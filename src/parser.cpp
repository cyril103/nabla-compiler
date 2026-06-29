#include "parser.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
bool isBuiltinTypeName(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" || type == "Double" ||
           type == "Bool" || type == "Char" || type == "String" || type == "Unit" ||
           type == "Nothing" || type == "IntArray" ||
           type == "LongArray" || type == "FloatArray" || type == "DoubleArray" ||
           type == "BoolArray";
}

bool isKnownConcreteTypeName(const std::string& type, const CompilerContext& context) {
    if (isBuiltinTypeName(type) || functionTypeFromName(type)) return true;
    auto parameterizedType = parameterizedTypeFromName(type);
    if (parameterizedType) {
        const auto& [baseName, arguments] = *parameterizedType;
        if (!isKnownConcreteTypeName(baseName, context)) return false;
        for (const auto& argument : arguments) {
            if (!isKnownConcreteTypeName(argument, context)) return false;
        }
        return true;
    }
    return context.classes.find(type) != context.classes.end();
}

bool shouldRetargetInferredFactoryAlias(const std::string& name) {
    return name == "arrayFill" || name == "ArrayFill" ||
           name == "Array.fill" || name == "Array.tabulate";
}

std::string formatFieldProviders(const std::set<std::string>& providers) {
    std::string message;
    bool first = true;
    for (const auto& provider : providers) {
        if (!first) message += ", ";
        message += provider;
        first = false;
    }
    return message;
}

std::string formatMethodProviders(const std::set<std::string>& providers) {
    std::string message;
    bool first = true;
    for (const auto& provider : providers) {
        if (!first) message += ", ";
        message += provider;
        first = false;
    }
    return message;
}

std::optional<ClassFieldLookupResult> resolveClassFieldInHierarchy(
    const CompilerContext& context, const std::string& className,
    const std::string& fieldName, const SourceLocation& location) {
    std::map<std::string, std::set<std::string>> fieldOwners;
    std::map<std::string, std::string> fieldTypes;
    std::map<std::string, bool> fieldMutability;
    std::set<std::string> visiting;
    collectVisibleFieldsInHierarchy(context, className, visiting, fieldOwners, fieldTypes, &fieldMutability);

    auto it = fieldOwners.find(fieldName);
    if (it == fieldOwners.end()) return std::nullopt;
    if (it->second.size() > 1) {
        throw CompilerError(
            ErrorKind::Parser, location,
            "conflit d'héritage pour le champ '" + fieldName + "' dans la classe '" +
            className + "': plusieurs définitions dans [" + formatFieldProviders(it->second) + "]");
    }
    auto fieldType = fieldTypes.find(fieldName);
    if (fieldType == fieldTypes.end()) return std::nullopt;
    return ClassFieldLookupResult{
        *it->second.begin(),
        fieldType->second,
        fieldMutability.count(fieldName) ? fieldMutability[fieldName] : false,
    };
}

std::optional<ClassMethodLookupResult> resolveZeroArgumentMethod(
    const CompilerContext& context, const std::string& receiverType,
    const std::string& methodName, const std::vector<std::string>& typeArguments) {
    if (!typeArguments.empty()) return std::nullopt;
    return resolveExactClassMethodOverload(context, receiverType, methodName, {});
}
}

Parser::Parser(const std::vector<Token>& tokens, CompilerContext& ctx, const std::filesystem::path& currentFile)
    : tokens(tokens), index(0), context(ctx), currentFile(currentFile), currentParsingClass("") {}

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = located(std::make_unique<ProgramNode>(), peek().location);
    while (peek().type != TokenType::EOF_TOKEN) {
        if (peek().type == TokenType::KW_IMPORT) {
            parseImport(program);
        } else if (peek().type == TokenType::KW_CLASS) {
            parseClassDefinition(program);
        } else if (peek().type == TokenType::KW_TRAIT) {
            parseClassDefinition(program, true);
        } else if (peek().type == TokenType::KW_OBJECT) {
            parseObjectDefinition(program);
        } else if (peek().type == TokenType::KW_DEF) {
            program->elements.push_back(parseFunctionDef(""));
        } else {
            throw CompilerError(ErrorKind::Parser, peek().location, "instruction inattendue '" + peek().value + "'");
        }
    }
    for (auto& function : generatedFunctions) {
        program->elements.push_back(std::move(function));
    }
    return program;
}

Token Parser::peek() const {
    return tokens[index];
}

Token Parser::consume(TokenType expected, const std::string& err) {
    if (peek().type == expected) return tokens[index++];
    std::string message = err.empty() ? "token inattendu" : err;
    throw CompilerError(ErrorKind::Parser, peek().location, message + " (reçu '" + peek().value + "')");
}

void Parser::parseImport(std::unique_ptr<ProgramNode>& currentProgram) {
    Token importToken = consume(TokenType::KW_IMPORT, "");
    std::string path = consume(TokenType::IDENTIFIER, "Nom de module attendu").value;
    while (peek().type == TokenType::DOT) {
        consume(TokenType::DOT, "");
        path += "/" + consume(TokenType::IDENTIFIER, "Sous-module attendu").value;
    }

    std::filesystem::path importPath = currentFile.parent_path() / (path + ".nabla");
    if (!std::filesystem::exists(importPath)) {
        importPath = context.rootDir / (path + ".nabla");
    }
    if (!std::filesystem::exists(importPath)) {
        importPath = context.stdlibDir / (path + ".nabla");
    }
    if (!std::filesystem::exists(importPath)) {
        throw CompilerError(ErrorKind::Parser, importToken.location, "impossible de charger '" + importPath.string() + "'");
    }

    std::string canonicalPath = std::filesystem::canonical(importPath).string();
    if (context.parsedFiles.find(canonicalPath) != context.parsedFiles.end()) return;
    context.parsedFiles.insert(canonicalPath);

    std::ifstream file(importPath);
    if (!file.is_open()) throw CompilerError(ErrorKind::Parser, importToken.location, "impossible d'ouvrir '" + importPath.string() + "'");
    std::stringstream buffer; buffer << file.rdbuf();

    Lexer subLexer(buffer.str(), importPath.string());
    Parser subParser(subLexer.tokenize(), context, importPath);
    auto subProgram = subParser.parseProgram();
    for (auto& el : subProgram->elements) currentProgram->elements.push_back(std::move(el));
}

void Parser::parseClassDefinition(std::unique_ptr<ProgramNode>& program, bool isTrait) {
    Token classToken = consume(isTrait ? TokenType::KW_TRAIT : TokenType::KW_CLASS, "");
    Token classNameToken = consume(TokenType::IDENTIFIER, isTrait ? "Nom de trait attendu" : "Nom de classe attendu");
    std::string className = classNameToken.value;
    std::vector<std::string> typeParameters;
    if (peek().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "");
        while (peek().type != TokenType::RBRACKET) {
            Token typeParameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre de type attendu");
            if (typeParameterToken.value == "Int" || typeParameterToken.value == "Long" ||
                typeParameterToken.value == "Float" || typeParameterToken.value == "Double" ||
                typeParameterToken.value == "Bool" || typeParameterToken.value == "Char" ||
                typeParameterToken.value == "String" ||
                typeParameterToken.value == "Unit" || typeParameterToken.value == "Nothing" ||
                typeParameterToken.value == "IntArray" ||
                typeParameterToken.value == "LongArray" || typeParameterToken.value == "FloatArray" ||
                typeParameterToken.value == "DoubleArray" || typeParameterToken.value == "BoolArray") {
                throw CompilerError(
                    ErrorKind::Parser, typeParameterToken.location,
                    "paramètre de type invalide: " + typeParameterToken.value);
            }
            if (isTypeParameterName(typeParameterToken.value, typeParameters)) {
                throw CompilerError(
                    ErrorKind::Parser, typeParameterToken.location,
                    "paramètre de type déjà déclaré: " + typeParameterToken.value);
            }
            typeParameters.push_back(typeParameterToken.value);
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RBRACKET) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu après le paramètre de type");
            }
        }
        consume(TokenType::RBRACKET, "']' attendu après les paramètres de type");
    }
    if (context.classes.count(className)) {
        throw CompilerError(ErrorKind::Parser, classNameToken.location, "classe déjà déclarée: " + className);
    }
    context.classes[className].typeParameters = typeParameters;
    context.classes[className].location = classToken.location;
    context.classes[className].isTrait = isTrait;

    bool hasExplicitParent = false;
    std::vector<std::string> parentConstructorArguments;
    std::vector<CompilerContext::FieldInfo> inheritedConstructorSignature;
    bool hasExplicitParentConstructorArguments = false;
    bool consumedHeaderList = false;
    bool headerListIsTyped = false;
    std::string directParentType;

    auto isTypedFieldHeader = [&]() -> bool {
        if (peek().type != TokenType::LPAREN) return false;
        if (index + 1 >= tokens.size()) return false;
        if (tokens[index + 1].type == TokenType::RPAREN) return true;
        if (tokens[index + 1].type == TokenType::KW_VAL ||
            tokens[index + 1].type == TokenType::KW_VAR) {
            return index + 3 < tokens.size() &&
                   tokens[index + 2].type == TokenType::IDENTIFIER &&
                   tokens[index + 3].type == TokenType::COLON;
        }
        return tokens[index + 1].type == TokenType::IDENTIFIER &&
               index + 2 < tokens.size() &&
               tokens[index + 2].type == TokenType::COLON;
    };

    auto registerConstructorValAccessor = [&](const Token& fieldToken, const std::string& fieldType) {
        auto& classInfo = context.classes[className];
        CompilerContext::FunctionSignature signature{
            {}, fieldType, {}, fieldToken.location, fieldToken.location, false, false, true, {}};
        auto& overloads = classInfo.methodOverloads[fieldToken.value];
        for (const auto& overloadName : overloads) {
            const auto existingMethod = classInfo.methods.find(overloadName);
            if (existingMethod != classInfo.methods.end() &&
                functionSignatureParametersMatch(existingMethod->second, signature)) {
                throw CompilerError(
                    ErrorKind::Parser, fieldToken.location,
                    "méthode déjà déclarée avec cette signature dans '" +
                    className + "': " + fieldToken.value);
            }
        }
        overloads.push_back(fieldToken.value);
        classInfo.methods[fieldToken.value] = signature;
    };

    auto parseClassFieldList = [&](int& offset) {
        consume(TokenType::LPAREN, "");
        while (peek().type != TokenType::RPAREN) {
            bool hasAccessor = false;
            bool isMutableField = false;
            if (peek().type == TokenType::KW_VAL || peek().type == TokenType::KW_VAR) {
                isMutableField = peek().type == TokenType::KW_VAR;
                index++;
                hasAccessor = true;
            }
            Token fieldToken = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu");
            std::string fieldName = fieldToken.value;
            consume(TokenType::COLON, "");
            auto [fieldType, fieldTypeLocation] = parseType("Type attendu");
            if (context.classLayouts[className].count(fieldName)) {
                throw CompilerError(
                    ErrorKind::Parser, fieldToken.location,
                    "champ déjà déclaré dans '" + className + "': " + fieldName);
            }
            context.classLayouts[className][fieldName] = offset;
            context.classes[className].fields.push_back({fieldName, fieldType, fieldTypeLocation, hasAccessor, isMutableField});
            if (hasAccessor) {
                registerConstructorValAccessor(fieldToken, fieldType);
            }
            offset += 8;
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RPAREN) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
            }
        }
        consume(TokenType::RPAREN, "");
    };

    if (isTrait && peek().type == TokenType::LPAREN) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "constructeur ou champs non autorisés dans le trait '" + className + "'");
    }

    if (!isTrait && isTypedFieldHeader()) {
        int offset = 8;
        parseClassFieldList(offset);
        consumedHeaderList = true;
        headerListIsTyped = true;
    }

    if (!isTrait && peek().type == TokenType::KW_EXTENDS) {
        hasExplicitParent = true;
        consume(TokenType::KW_EXTENDS, "");
        auto [baseParentType, baseParentTypeLocation] = parseType("Type de parent attendu");
        (void) baseParentTypeLocation;
        directParentType = baseParentType;
        context.classes[className].parentTypes.push_back(baseParentType);
        if (peek().type == TokenType::COMMA) {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                "syntaxe d'héritage multiple supprimée: utilisez 'with' pour les mixins");
        }
        while (peek().type == TokenType::KW_WITH) {
            consume(TokenType::KW_WITH, "");
            auto [mixinType, mixinTypeLocation] = parseType("Type de mixin attendu");
            (void) mixinTypeLocation;
            context.classes[className].parentTypes.push_back(mixinType);
        }
    }

    if (peek().type == TokenType::KW_WITH) {
        if (!isTrait && !hasExplicitParent && className != "Any" && className != "AnyVal" && className != "AnyRef") {
            context.classes[className].parentTypes.push_back("AnyRef");
        }
        while (peek().type == TokenType::KW_WITH) {
            consume(TokenType::KW_WITH, "");
            auto [mixinType, mixinTypeLocation] = parseType("Type de mixin attendu");
            (void) mixinTypeLocation;
            context.classes[className].parentTypes.push_back(mixinType);
        }
    }

    context.classes[className].hasExplicitParent = hasExplicitParent;
    if (!isTrait && !hasExplicitParent && context.classes[className].parentTypes.empty() &&
        className != "Any" && className != "AnyVal" && className != "AnyRef") {
        context.classes[className].parentTypes.push_back("AnyRef");
    }

    if (hasExplicitParent && peek().type == TokenType::LPAREN) {
        consume(TokenType::LPAREN, "");
        bool sawTypeAnnotation = false;
        bool sawBareArgument = false;
        std::vector<std::string> explicitArguments;
        int offset = 8;
        while (peek().type != TokenType::RPAREN) {
            Token argumentToken = consume(TokenType::IDENTIFIER, "Nom d'argument attendu");
            if (peek().type == TokenType::COLON) {
                if (sawBareArgument) {
                    throw CompilerError(
                        ErrorKind::Parser, argumentToken.location,
                        "arguments parent explicites ou signature héritée mélangés dans 'extends'");
                }
                sawTypeAnnotation = true;
                consume(TokenType::COLON, "");
                auto [fieldType, fieldTypeLocation] = parseType("Type attendu");
                const auto duplicateSignatureField = std::find_if(
                    inheritedConstructorSignature.begin(), inheritedConstructorSignature.end(),
                    [&](const CompilerContext::FieldInfo& field) {
                        return field.name == argumentToken.value;
                    });
                if (duplicateSignatureField != inheritedConstructorSignature.end()) {
                    throw CompilerError(
                        ErrorKind::Parser, argumentToken.location,
                        "champ déjà déclaré dans '" + className + "': " + argumentToken.value);
                }
                inheritedConstructorSignature.push_back(
                    {argumentToken.value, fieldType, fieldTypeLocation});
                offset += 8;
            } else {
                if (peek().type == TokenType::COMMA || peek().type == TokenType::RPAREN) {
                    if (sawTypeAnnotation) {
                        throw CompilerError(
                            ErrorKind::Parser, argumentToken.location,
                            "arguments parent explicites ou signature héritée mélangés dans 'extends'");
                    }
                    sawBareArgument = true;
                    explicitArguments.push_back(argumentToken.value);
                    if (peek().type == TokenType::COMMA) {
                        consume(TokenType::COMMA, "");
                        continue;
                    }
                } else {
                    throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu");
                }
            }
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RPAREN) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu");
            }
        }
        consume(TokenType::RPAREN, "");
        consumedHeaderList = true;
        if (sawTypeAnnotation) {
            if (sawBareArgument) {
                throw CompilerError(
                    ErrorKind::Parser, classNameToken.location,
                    "arguments parent explicites ou signature héritée mélangés dans 'extends'");
            }
            size_t firstOwnField = 0;
            if (!directParentType.empty() && context.classes.count(genericBaseName(directParentType))) {
                firstOwnField = collectClassFieldsInHierarchyForLayout(context, directParentType).size();
            }
            int ownFieldOffset = 8;
            for (size_t i = firstOwnField; i < inheritedConstructorSignature.size(); ++i) {
                const auto& field = inheritedConstructorSignature[i];
                if (context.classLayouts[className].count(field.name)) {
                    throw CompilerError(
                        ErrorKind::Parser, field.location,
                        "champ déjà déclaré dans '" + className + "': " + field.name);
                }
                context.classLayouts[className][field.name] = ownFieldOffset;
                context.classes[className].fields.push_back(field);
                ownFieldOffset += 8;
            }
            headerListIsTyped = true;
        } else if (sawBareArgument || (peek().type == TokenType::LPAREN)) {
            hasExplicitParentConstructorArguments = true;
            parentConstructorArguments = std::move(explicitArguments);
        }
    }

    if (isTrait) {
        // V1 traits are stateless: no constructor signature and no instance fields.
    } else if (!hasExplicitParent && !consumedHeaderList) {
        int offset = 8;
        parseClassFieldList(offset);
    } else if (hasExplicitParent && headerListIsTyped) {
        // field signature was already parsed in the extends list
    } else if (hasExplicitParent && hasExplicitParentConstructorArguments) {
        if (peek().type == TokenType::LPAREN) {
            int offset = 8;
            parseClassFieldList(offset);
        }
    }

    context.classes[className].parentConstructorArguments = parentConstructorArguments;
    context.classes[className].inheritedConstructorSignature = inheritedConstructorSignature;
    currentParsingClass = className;
    auto previousTypeParameters = currentFunctionTypeParameters;
    currentFunctionTypeParameters = typeParameters;

    if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "");
        while (peek().type != TokenType::RBRACE) {
            if (peek().type == TokenType::KW_OVERRIDE || peek().type == TokenType::KW_DEF) {
                auto method = parseFunctionDef(className, "", isTrait);
                if (method) program->elements.push_back(std::move(method));
            }
            else throw CompilerError(
                ErrorKind::Parser, peek().location,
                std::string("instruction inattendue dans ") +
                    (isTrait ? "le trait '" : "la classe '") + peek().value + "'");
        }
        consume(TokenType::RBRACE, "");
    }
    if (!isTrait) {
        for (const auto& field : context.classes[className].fields) {
            if (!field.hasAccessor) continue;
            auto body = located(
                std::make_unique<FieldAccessNode>(className, field.name, field.type),
                field.location);
            std::vector<std::string> ownerTypeParameters = context.classes[className].typeParameters;
            generatedFunctions.push_back(located(std::make_unique<FunctionDefNode>(
                className, field.name, field.type, std::vector<std::string>{},
                std::vector<FunctionDefNode::Parameter>{}, std::move(body),
                std::vector<FunctionDefNode::Capture>{}, std::move(ownerTypeParameters)),
                field.location));
        }
    }
    currentFunctionTypeParameters = previousTypeParameters;
    currentParsingClass.clear();
}

void Parser::parseObjectDefinition(std::unique_ptr<ProgramNode>& program) {
    consume(TokenType::KW_OBJECT, "");
    Token objectNameToken = consume(TokenType::IDENTIFIER, "Nom d'objet attendu");
    const std::string objectName = objectNameToken.value;
    if (context.objects.count(objectName)) {
        throw CompilerError(ErrorKind::Parser, objectNameToken.location, "objet déjà déclaré: " + objectName);
    }
    context.objects.insert(objectName);

    if (peek().type == TokenType::KW_EXTENDS) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "un objet runtime V0 ne supporte pas 'extends'; utilisez 'with Trait'");
    }

    std::vector<std::string> parentTypes;
    while (peek().type == TokenType::KW_WITH) {
        consume(TokenType::KW_WITH, "");
        auto [parentType, parentTypeLocation] = parseType("Type de trait attendu après 'with'");
        (void) parentTypeLocation;
        parentTypes.push_back(parentType);
    }

    const bool isRuntimeObject = !parentTypes.empty();
    if (isRuntimeObject) {
        if (context.classes.count(objectName)) {
            throw CompilerError(
                ErrorKind::Parser, objectNameToken.location,
                "objet runtime '" + objectName + "' en conflit avec une classe ou un trait du même nom");
        }
        context.runtimeObjects.insert(objectName);
        auto& classInfo = context.classes[objectName];
        classInfo.location = objectNameToken.location;
        classInfo.isTrait = false;
        classInfo.hasExplicitParent = false;
        classInfo.parentTypes.push_back("AnyRef");
        classInfo.parentTypes.insert(classInfo.parentTypes.end(), parentTypes.begin(), parentTypes.end());
    }

    const std::string previousParsingClass = currentParsingClass;
    if (isRuntimeObject) currentParsingClass = objectName;

    consume(TokenType::LBRACE, "'{' attendu après le nom de l'objet");
    while (peek().type != TokenType::RBRACE) {
        if (peek().type == TokenType::KW_DEF || (isRuntimeObject && peek().type == TokenType::KW_OVERRIDE)) {
            if (isRuntimeObject) {
                auto method = parseFunctionDef(objectName, "", false);
                if (method) program->elements.push_back(std::move(method));
            } else {
                program->elements.push_back(parseFunctionDef("", objectName));
            }
        } else if (peek().type == TokenType::KW_OVERRIDE) {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                "mot-clé 'override' non autorisé dans un objet statique");
        } else {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                isRuntimeObject
                    ? "seules les déclarations 'def' sont autorisées dans un objet runtime"
                    : "seules les déclarations 'def' sont autorisées dans un objet statique");
        }
    }
    consume(TokenType::RBRACE, "'}' attendu après l'objet");
    currentParsingClass = previousParsingClass;
}

std::unique_ptr<ASTNode> Parser::parseIfExpression() {
    Token start = consume(TokenType::KW_IF, "");
    if (peek().type == TokenType::LPAREN) {
        consume(TokenType::LPAREN, "Parenthèse ouvrante attendue après 'if'");
    }
    auto condition = parseExpression();
    if (peek().type == TokenType::RPAREN) {
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après la condition");
    }
    consume(TokenType::LBRACE, "Bloc 'then' attendu après la condition");
    auto thenBranch = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc 'then' attendue");
    consume(TokenType::KW_ELSE, "mot-clé 'else' attendu");
    std::unique_ptr<ASTNode> elseBranch;
    if (peek().type == TokenType::KW_IF) {
        elseBranch = parseIfExpression();
    } else {
        consume(TokenType::LBRACE, "Bloc 'else' attendu après 'else'");
        elseBranch = parseBlock();
        consume(TokenType::RBRACE, "Fin du bloc 'else' attendue");
    }
    return located(std::make_unique<IfNode>(std::move(condition), std::move(thenBranch), std::move(elseBranch)), start.location);
}

std::unique_ptr<ASTNode> Parser::parseMatchExpression() {
    Token start = consume(TokenType::KW_MATCH, "");
    auto scrutinee = parseExpression();
    consume(TokenType::LBRACE, "Bloc 'match' attendu après l'expression");

    std::vector<MatchNode::Branch> branches;
    bool sawWildcard = false;
    while (peek().type != TokenType::RBRACE) {
        Token branchToken = peek();
        if (sawWildcard) {
            throw CompilerError(
                ErrorKind::Parser, branchToken.location,
                "aucune branche n'est autorisée après '_' dans un match");
        }
        bool isWildcard = false;
        bool isNamedPattern = false;
        std::string boundSymbol;
        std::unique_ptr<ASTNode> pattern;
        std::unique_ptr<ASTNode> guard;
        if (peek().type == TokenType::IDENTIFIER && peek().value == "_") {
            consume(TokenType::IDENTIFIER, "");
            isWildcard = true;
        } else if (peek().type == TokenType::IDENTIFIER) {
            Token token = consume(TokenType::IDENTIFIER, "Nom de variable attendu dans le motif");
            isNamedPattern = true;
            boundSymbol = token.value + "#" + std::to_string(nextSymbolId++);
            localScopes.emplace_back();
            localScopes.back()[token.value] = {boundSymbol, "Unit", false};
        } else {
            if (peek().type == TokenType::INT_LITERAL) {
                Token token = consume(TokenType::INT_LITERAL, "");
                pattern = located(std::make_unique<IntNode>(token.value), token.location);
            } else if (peek().type == TokenType::LONG_LITERAL) {
                Token token = consume(TokenType::LONG_LITERAL, "");
                pattern = located(std::make_unique<LongNode>(token.value), token.location);
            } else if (peek().type == TokenType::DOUBLE_LITERAL) {
                Token token = consume(TokenType::DOUBLE_LITERAL, "");
                pattern = located(std::make_unique<DoubleNode>(token.value), token.location);
            } else if (peek().type == TokenType::FLOAT_LITERAL) {
                Token token = consume(TokenType::FLOAT_LITERAL, "");
                pattern = located(std::make_unique<FloatNode>(token.value), token.location);
            } else if (peek().type == TokenType::KW_TRUE) {
                Token token = consume(TokenType::KW_TRUE, "");
                pattern = located(std::make_unique<BoolNode>(true), token.location);
            } else if (peek().type == TokenType::KW_FALSE) {
                Token token = consume(TokenType::KW_FALSE, "");
                pattern = located(std::make_unique<BoolNode>(false), token.location);
            } else if (peek().type == TokenType::STRING_LITERAL) {
                Token token = consume(TokenType::STRING_LITERAL, "");
                pattern = located(std::make_unique<StringNode>(token.value), token.location);
            } else if (peek().type == TokenType::CHAR_LITERAL) {
                Token token = consume(TokenType::CHAR_LITERAL, "");
                pattern = located(std::make_unique<CharNode>(token.value), token.location);
            } else {
                throw CompilerError(
                    ErrorKind::Parser, peek().location,
                    "motif de match invalide: littéral, identifiant ou '_' attendu");
            }
        }
        if (peek().type == TokenType::KW_IF) {
            consume(TokenType::KW_IF, "");
            guard = parseExpression();
        }
        consume(TokenType::FAT_ARROW, "'=>' attendu après le motif de match");
        std::unique_ptr<ASTNode> body;
        if (peek().type == TokenType::LBRACE) {
            consume(TokenType::LBRACE, "");
            body = parseBlock();
            consume(TokenType::RBRACE, "Fin du bloc de branche match attendue");
        } else {
            body = parseExpression();
        }
        if (isNamedPattern) {
            localScopes.pop_back();
        }
        if (isWildcard && !guard) {
            sawWildcard = true;
        }
        branches.push_back({isWildcard, isNamedPattern, std::move(pattern), std::move(guard), boundSymbol, std::move(body), branchToken.location});
    }
    consume(TokenType::RBRACE, "Fin du bloc 'match' attendue");
    if (branches.empty()) {
        throw CompilerError(ErrorKind::Parser, start.location, "match sans branches");
    }
    if (!branches.back().isWildcard) {
        throw CompilerError(ErrorKind::Parser, branches.back().location, "branche '_' finale attendue dans un match");
    }
    return located(std::make_unique<MatchNode>(std::move(scrutinee), std::move(branches)), start.location);
}

std::unique_ptr<ASTNode> Parser::parseWhileExpression() {
    Token start = consume(TokenType::KW_WHILE, "");
    if (peek().type == TokenType::LPAREN) {
        consume(TokenType::LPAREN, "Parenthèse ouvrante attendue après 'while'");
    }
    auto condition = parseExpression();
    if (peek().type == TokenType::RPAREN) {
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après la condition");
    }
    consume(TokenType::LBRACE, "Bloc 'while' attendu après la condition");
    auto body = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc 'while' attendue");
    return located(std::make_unique<WhileNode>(std::move(condition), std::move(body)), start.location);
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    localScopes.emplace_back();
    std::vector<std::unique_ptr<ASTNode>> expressions;
    while (peek().type != TokenType::RBRACE) {
        expressions.push_back(parseStatement());
    }
    if (expressions.empty()) {
        throw CompilerError(ErrorKind::Parser, peek().location, "bloc vide non autorisé");
    }
    localScopes.pop_back();
    if (expressions.size() == 1) {
        return std::move(expressions[0]);
    }
    SourceLocation location = expressions.front()->getLocation();
    return located(std::make_unique<BlockNode>(std::move(expressions)), location);
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (peek().type == TokenType::KW_DEF) {
        return parseLocalFunctionDef();
    }
    if (peek().type == TokenType::KW_VAL || peek().type == TokenType::KW_VAR) {
        Token declarationToken = peek();
        bool isMutable = (peek().type == TokenType::KW_VAR);
        index++; // consume kw
        std::string name = consume(TokenType::IDENTIFIER, "Nom de variable attendu").value;
        std::string declaredType;
        if (peek().type == TokenType::COLON) {
            consume(TokenType::COLON, "");
            auto [typeName, typeLocation] = parseType("Type de variable attendu");
            (void) typeLocation;
            declaredType = typeName;
        }
        consume(TokenType::EQUAL, "Initialisation requise pour 'val'/'var'");
        auto init = declaredType.empty() ? parseExpression() : parseArgument(declaredType);
        if (localScopes.empty()) {
            throw CompilerError(ErrorKind::Parser, declarationToken.location, "déclaration locale hors d'un bloc: " + name);
        }
        if (localScopes.back().count(name)) {
            throw CompilerError(ErrorKind::Parser, declarationToken.location, "variable déjà déclarée dans cette portée: " + name);
        }
        std::string symbolName = name + "#" + std::to_string(nextSymbolId++);
        localScopes.back()[name] = {symbolName, declaredType.empty() ? init->getType() : declaredType, isMutable};
        return located(
            std::make_unique<VarDeclNode>(
                name, symbolName, std::move(init), isMutable, declaredType),
            declarationToken.location);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        // possible assignment
        if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::EQUAL) {
            Token nameToken = consume(TokenType::IDENTIFIER, "");
            std::string name = nameToken.value;
            consume(TokenType::EQUAL, "");
            auto rhs = parseExpression();
            const ParsedSymbol* symbol = findLocal(name);
            if (symbol) {
                return located(std::make_unique<AssignmentNode>(
                    name, symbol->internalName, symbol->type, symbol->isMutable, std::move(rhs)),
                    nameToken.location);
            }
            if (!currentParsingClass.empty() && lambdaCaptureScopes.empty()) {
                if (auto field = resolveClassFieldInHierarchy(context, currentParsingClass, name, nameToken.location)) {
                    return located(std::make_unique<FieldAssignmentNode>(
                        field->ownerClassName, name, field->type, field->isMutable, std::move(rhs)),
                        nameToken.location);
                }
            }
            return located(std::make_unique<AssignmentNode>(
                name, name, "Int", false, std::move(rhs)), nameToken.location);
        }
    }
    return parseExpression();
}

std::unique_ptr<ASTNode> Parser::parseForExpression() {
    Token start = consume(TokenType::KW_FOR, "");
    if (peek().type == TokenType::LPAREN) {
        consume(TokenType::LPAREN, "Parenthèse ouvrante attendue après 'for'");
    }
    auto count = parseExpression();
    if (peek().type == TokenType::RPAREN) {
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après la condition");
    }
    consume(TokenType::LBRACE, "Bloc 'for' attendu après la condition");
    auto body = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc 'for' attendue");
    return located(std::make_unique<ForNode>(std::move(count), std::move(body)), start.location);
}

bool Parser::startsLambdaExpression() const {
    if (peek().type != TokenType::LPAREN) return false;
    size_t cursor = index + 1;
    if (cursor >= tokens.size()) return false;
    if (tokens[cursor].type == TokenType::RPAREN) {
        ++cursor;
        return cursor < tokens.size() && tokens[cursor].type == TokenType::FAT_ARROW;
    }

    while (cursor < tokens.size()) {
        if (tokens[cursor].type != TokenType::IDENTIFIER) return false;
        ++cursor;
        if (cursor >= tokens.size() || tokens[cursor].type != TokenType::COLON) return false;
        ++cursor;
        if (!skipTypeAt(cursor)) return false;
        if (cursor >= tokens.size()) return false;
        if (tokens[cursor].type == TokenType::COMMA) {
            ++cursor;
            continue;
        }
        if (tokens[cursor].type == TokenType::RPAREN) {
            ++cursor;
            return cursor < tokens.size() && tokens[cursor].type == TokenType::FAT_ARROW;
        }
        return false;
    }
    return false;
}

bool Parser::skipTypeAt(size_t& cursor) const {
    if (cursor >= tokens.size() || tokens[cursor].type != TokenType::IDENTIFIER) return false;
    ++cursor;
    if (cursor >= tokens.size() || tokens[cursor].type != TokenType::LBRACKET) return true;

    ++cursor;
    while (cursor < tokens.size()) {
        if (!skipTypeAt(cursor)) return false;
        if (cursor >= tokens.size()) return false;
        if (tokens[cursor].type == TokenType::COMMA) {
            ++cursor;
            continue;
        }
        if (tokens[cursor].type == TokenType::RBRACKET) {
            ++cursor;
            return true;
        }
        return false;
    }
    return false;
}

bool Parser::startsInferredLambdaExpression() const {
    return startsInferredLambdaExpressionAt(index);
}

bool Parser::startsInferredLambdaExpressionAt(size_t cursor) const {
    if (cursor >= tokens.size()) return false;
    if (tokens[cursor].type == TokenType::IDENTIFIER) {
        return cursor + 1 < tokens.size() && tokens[cursor + 1].type == TokenType::FAT_ARROW;
    }
    if (tokens[cursor].type != TokenType::LPAREN) return false;
    ++cursor;
    if (cursor >= tokens.size()) return false;
    if (tokens[cursor].type == TokenType::RPAREN) {
        ++cursor;
        return cursor < tokens.size() && tokens[cursor].type == TokenType::FAT_ARROW;
    }

    while (cursor < tokens.size()) {
        if (tokens[cursor].type != TokenType::IDENTIFIER) return false;
        ++cursor;
        if (cursor >= tokens.size()) return false;
        if (tokens[cursor].type == TokenType::COMMA) {
            ++cursor;
            continue;
        }
        if (tokens[cursor].type == TokenType::RPAREN) {
            ++cursor;
            return cursor < tokens.size() && tokens[cursor].type == TokenType::FAT_ARROW;
        }
        return false;
    }
    return false;
}

std::unique_ptr<ASTNode> Parser::parseLambdaExpression() {
    Token start = consume(TokenType::LPAREN, "");
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    std::vector<ParsedSymbol> scopedParameters;
    const size_t outerScopeCount = localScopes.size();

    while (peek().type != TokenType::RPAREN) {
        Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu dans la lambda");
        std::string parameterName = parameterToken.value;
        consume(TokenType::COLON, "':' attendu après le nom du paramètre de lambda");
        auto [parameterType, parameterTypeLocation] = parseType("Type de paramètre attendu dans la lambda");
        if (isFunctionTypeName(parameterType)) {
            throw CompilerError(
                ErrorKind::Parser, parameterTypeLocation,
                "les paramètres de lambda ne peuvent pas encore être des fonctions");
        }
        std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTypeLocation});
        scopedParameters.push_back({symbolName, parameterType, false});
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
        }
    }
    consume(TokenType::RPAREN, "Parenthèse fermante attendue après le paramètre de lambda");
    consume(TokenType::FAT_ARROW, "'=>' attendu après le paramètre de lambda");
    consume(TokenType::LBRACE, "Bloc attendu après '=>'");

    lambdaCaptureScopes.push_back({outerScopeCount, {}});
    localScopes.emplace_back();
    for (size_t i = 0; i < scopedParameters.size(); ++i) {
        localScopes.back()[parameters[i].name] = scopedParameters[i];
    }
    auto body = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc de lambda attendue");
    localScopes.pop_back();
    std::vector<FunctionDefNode::Capture> captures;
    for (const auto& [symbolName, capture] : lambdaCaptureScopes.back().capturesBySymbol) {
        (void) symbolName;
        captures.push_back({capture.name, capture.symbolName, capture.type});
    }
    lambdaCaptureScopes.pop_back();

    const std::string returnType = inferLambdaReturnType(*body, parameters, captures);
    std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
    context.functions[lambdaName] = {
        signatureParameters, returnType, currentFunctionTypeParameters, start.location, start.location, false, false, false, {}};

    auto function = located(std::make_unique<FunctionDefNode>(
        "", lambdaName, returnType, currentFunctionTypeParameters,
        std::move(parameters), std::move(body), captures), start.location);
    generatedFunctions.push_back(std::move(function));
    CompilerContext::FunctionType lambdaType;
    for (const auto& parameter : signatureParameters) {
        lambdaType.parameterTypes.push_back(parameter.type);
    }
    lambdaType.returnType = returnType;
    auto functionType = functionNameForType(lambdaType);
    if (!functionType) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "type fonction de lambda non supporté pour l'instant");
    }
    std::vector<FunctionReferenceNode::Capture> referenceCaptures;
    for (const auto& capture : captures) {
        referenceCaptures.push_back({capture.name, capture.symbolName, capture.type});
    }
    return located(
        std::make_unique<FunctionReferenceNode>(
            lambdaName, *functionType, currentFunctionTypeParameters, std::move(referenceCaptures)),
        start.location);
}

std::unique_ptr<ASTNode> Parser::parseInferredLambdaExpression(const std::string& expectedType) {
    Token start = peek();
    std::vector<Token> parameterTokens;
    if (peek().type == TokenType::IDENTIFIER) {
        parameterTokens.push_back(consume(TokenType::IDENTIFIER, "Nom de paramètre attendu dans la lambda"));
    } else {
        consume(TokenType::LPAREN, "");
        while (peek().type != TokenType::RPAREN) {
            parameterTokens.push_back(consume(TokenType::IDENTIFIER, "Nom de paramètre attendu dans la lambda"));
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RPAREN) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
            }
        }
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après les paramètres de lambda");
    }

    auto expectedFunctionType = functionTypeFromName(expectedType);
    if (!expectedFunctionType || expectedFunctionType->parameterTypes.size() != parameterTokens.size()) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "impossible d'inférer les types des paramètres de lambda sans type fonction attendu");
    }

    consume(TokenType::FAT_ARROW, "'=>' attendu après le paramètre de lambda");

    const size_t outerScopeCount = localScopes.size();
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    std::vector<ParsedSymbol> scopedParameters;
    for (size_t i = 0; i < parameterTokens.size(); ++i) {
        const std::string parameterType = expectedFunctionType->parameterTypes[i];
        if (isFunctionTypeName(parameterType)) {
            throw CompilerError(
                ErrorKind::Parser, parameterTokens[i].location,
                "les paramètres de lambda ne peuvent pas encore être des fonctions");
        }
        std::string parameterName = parameterTokens[i].value;
        std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTokens[i].location});
        scopedParameters.push_back({symbolName, parameterType, false});
    }

    lambdaCaptureScopes.push_back({outerScopeCount, {}});
    localScopes.emplace_back();
    for (size_t i = 0; i < scopedParameters.size(); ++i) {
        localScopes.back()[parameters[i].name] = scopedParameters[i];
    }
    std::unique_ptr<ASTNode> body;
    if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "Bloc attendu après '=>'");
        body = parseBlock();
        consume(TokenType::RBRACE, "Fin du bloc de lambda attendue");
    } else {
        body = parseExpression();
    }
    localScopes.pop_back();

    std::vector<FunctionDefNode::Capture> captures;
    for (const auto& [capturedSymbolName, capture] : lambdaCaptureScopes.back().capturesBySymbol) {
        (void) capturedSymbolName;
        captures.push_back({capture.name, capture.symbolName, capture.type});
    }
    lambdaCaptureScopes.pop_back();

    const std::string returnType = inferLambdaReturnType(*body, parameters, captures);
    std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
    context.functions[lambdaName] = {
        signatureParameters, returnType, currentFunctionTypeParameters, start.location, start.location, false, false, false, {}};

    auto function = located(std::make_unique<FunctionDefNode>(
        "", lambdaName, returnType, currentFunctionTypeParameters,
        std::move(parameters), std::move(body), captures), start.location);
    generatedFunctions.push_back(std::move(function));

    CompilerContext::FunctionType lambdaType;
    for (const auto& parameter : signatureParameters) {
        lambdaType.parameterTypes.push_back(parameter.type);
    }
    lambdaType.returnType = returnType;
    auto functionType = functionNameForType(lambdaType);
    if (!functionType) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "type fonction de lambda non supporté pour l'instant");
    }

    std::vector<FunctionReferenceNode::Capture> referenceCaptures;
    for (const auto& capture : captures) {
        referenceCaptures.push_back({capture.name, capture.symbolName, capture.type});
    }
    return located(
        std::make_unique<FunctionReferenceNode>(
            lambdaName, *functionType, currentFunctionTypeParameters, std::move(referenceCaptures)),
        start.location);
}

std::string Parser::zeroArgumentFunctionTypeFor(const std::string& returnType) const {
    CompilerContext::FunctionType functionType;
    functionType.returnType = returnType;
    auto functionTypeName = functionNameForType(functionType);
    if (!functionTypeName) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "type fonction zero-argument non supporté pour le paramètre par nom");
    }
    return *functionTypeName;
}

std::unique_ptr<ASTNode> Parser::parseByNameArgument(
    const std::string& expectedFunctionType,
    const CompilerContext::FunctionType& expectedFunction) {
    const Token start = peek();
    const size_t outerScopeCount = localScopes.size();
    lambdaCaptureScopes.push_back({outerScopeCount, {}});
    auto body = parseExpression();

    std::vector<FunctionDefNode::Capture> captures;
    for (const auto& [capturedSymbolName, capture] : lambdaCaptureScopes.back().capturesBySymbol) {
        (void) capturedSymbolName;
        captures.push_back({capture.name, capture.symbolName, capture.type});
    }
    lambdaCaptureScopes.pop_back();

    if (isTypeAssignable(context, body->getType(), expectedFunctionType)) {
        return body;
    }
    if (start.type == TokenType::IDENTIFIER) {
        for (const auto& overloadName : functionOverloadNames(context, start.value)) {
            const auto functionIt = context.functions.find(overloadName);
            if (functionIt != context.functions.end() &&
                isTypeAssignable(context, functionIt->second.returnType, expectedFunctionType)) {
                return body;
            }
        }
    }
    if (!isTypeAssignable(context, body->getType(), expectedFunction.returnType)) {
        return body;
    }

    std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
    context.functions[lambdaName] = {
        std::vector<CompilerContext::ParameterInfo>{}, body->getType(), currentFunctionTypeParameters,
        start.location, start.location, false, false, false, {}};

    generatedFunctions.push_back(located(std::make_unique<FunctionDefNode>(
        "", lambdaName, body->getType(), currentFunctionTypeParameters,
        std::vector<FunctionDefNode::Parameter>{}, std::move(body), captures), start.location));

    std::vector<FunctionReferenceNode::Capture> referenceCaptures;
    for (const auto& capture : captures) {
        referenceCaptures.push_back({capture.name, capture.symbolName, capture.type});
    }
    return located(
        std::make_unique<FunctionReferenceNode>(
            lambdaName, expectedFunctionType, currentFunctionTypeParameters, std::move(referenceCaptures)),
        start.location);
}

std::string Parser::inferLambdaReturnType(
    ASTNode& body, const std::vector<FunctionDefNode::Parameter>& parameters,
    const std::vector<FunctionDefNode::Capture>& captures) {
    const auto previousSymbolTypes = context.semanticSymbolTypes;
    const auto previousTypeParameters = context.semanticTypeParameters;

    context.semanticSymbolTypes.clear();
    context.semanticTypeParameters = currentFunctionTypeParameters;
    for (const auto& capture : captures) {
        context.semanticSymbolTypes[capture.symbolName] = capture.type;
    }
    for (const auto& parameter : parameters) {
        context.semanticSymbolTypes[parameter.symbolName] = parameter.type;
    }

    body.validateSemantics(context);
    const std::string returnType = body.getType();

    context.semanticSymbolTypes = previousSymbolTypes;
    context.semanticTypeParameters = previousTypeParameters;
    return returnType;
}

void Parser::requireTupleModule() {
    const std::filesystem::path modulePath = context.stdlibDir / "core" / "tuple.nabla";
    if (!std::filesystem::exists(modulePath)) return;
    const std::string canonicalPath = std::filesystem::canonical(modulePath).string();
    if (context.parsedFiles.find(canonicalPath) != context.parsedFiles.end()) return;
    context.parsedFiles.insert(canonicalPath);

    std::ifstream file(modulePath);
    if (!file.is_open()) {
        throw CompilerError(
            ErrorKind::Parser, {modulePath.string(), 1, 1},
            "impossible d'ouvrir '" + modulePath.string() + "'");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    Lexer lexer(buffer.str(), modulePath.string());
    Parser parser(lexer.tokenize(), context, modulePath);
    auto moduleAST = parser.parseProgram();
    for (auto& element : moduleAST->elements) {
        generatedFunctions.push_back(std::move(element));
    }
}

std::unique_ptr<ASTNode> Parser::parseFunctionReferenceWithExpectedType(const std::string& expectedType) {
    if (!functionTypeFromName(expectedType) || peek().type != TokenType::IDENTIFIER) {
        return nullptr;
    }

    Token nameToken = peek();
    std::string name = nameToken.value;
    if (findLocal(name)) {
        return nullptr;
    }
    if (!currentParsingClass.empty() && lambdaCaptureScopes.empty() &&
        resolveClassFieldInHierarchy(context, currentParsingClass, name, nameToken.location)) {
        return nullptr;
    }
    const auto overloadNames = functionOverloadNames(context, name);
    if (overloadNames.empty()) {
        return nullptr;
    }

    const size_t referenceStartIndex = index;
    consume(TokenType::IDENTIFIER, "");
    std::vector<std::string> typeArguments;
    if (peek().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "");
        while (peek().type != TokenType::RBRACKET) {
            auto [typeArgument, typeLocation] = parseType("Type d'argument générique attendu");
            (void) typeLocation;
            typeArguments.push_back(typeArgument);
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RBRACKET) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu après l'argument de type");
            }
        }
        consume(TokenType::RBRACKET, "']' attendu après les arguments de type");
    }
    if (peek().type == TokenType::LPAREN) {
        index = referenceStartIndex;
        return nullptr;
    }

    struct FunctionReferenceOverloadMatch {
        std::string overloadName;
        std::vector<std::string> resolvedTypeArguments;
    };
    std::vector<FunctionReferenceOverloadMatch> concreteMatches;
    std::vector<FunctionReferenceOverloadMatch> genericMatches;
    for (const auto& overloadName : overloadNames) {
        const auto* signature = findFunctionSignature(context, overloadName);
        if (!signature) continue;
        if (signature->typeParameters.empty()) {
            if (!typeArguments.empty()) continue;
            if (functionTypeNameMatchesSignature(expectedType, *signature)) {
                concreteMatches.push_back({overloadName, {}});
            }
            continue;
        }

        std::optional<std::map<std::string, std::string>> substitution;
        if (typeArguments.empty()) {
            substitution = inferGenericFunctionSubstitutionForFunctionType(*signature, expectedType);
        } else {
            substitution = genericFunctionSubstitutionFor(*signature, typeArguments);
            if (!substitution) continue;
        }
        if (!substitution) continue;

        CompilerContext::FunctionSignature candidate = *signature;
        for (auto& parameter : candidate.parameters) {
            parameter.type = substituteType(parameter.type, *substitution);
        }
        candidate.returnType = substituteType(candidate.returnType, *substitution);
        if (functionTypeNameMatchesSignature(expectedType, candidate)) {
            genericMatches.push_back({
                overloadName,
                orderedTypeArguments(signature->typeParameters, *substitution),
            });
        }
    }

    std::optional<FunctionReferenceOverloadMatch> match;
    if (concreteMatches.size() == 1) {
        match = concreteMatches[0];
    } else if (concreteMatches.empty() && genericMatches.size() == 1) {
        match = genericMatches[0];
    }
    if (!match) {
        const auto expectedFunctionType = functionTypeFromName(expectedType);
        if (overloadNames.size() == 1 ||
            (concreteMatches.empty() && genericMatches.empty() &&
             expectedFunctionType && expectedFunctionType->parameterTypes.empty())) {
            index = referenceStartIndex;
            return nullptr;
        }
        std::string candidates = formatFunctionOverloadCandidates(context, name);
        if (concreteMatches.size() > 1 ||
            (concreteMatches.empty() && genericMatches.size() > 1)) {
            throw CompilerError(
                ErrorKind::Parser, nameToken.location,
                "référence de fonction surchargée ambiguë pour '" + name +
                "' avec le type attendu " + expectedType +
                (candidates.empty() ? "" : "\ncandidats:" + candidates));
        }
        throw CompilerError(
            ErrorKind::Parser, nameToken.location,
            "aucune surcharge compatible pour la référence '" + name +
            "' avec le type attendu " + expectedType +
            (candidates.empty() ? "" : "\ncandidats:" + candidates));
    }
    return located(
        std::make_unique<FunctionReferenceNode>(
            name, expectedType, std::move(match->resolvedTypeArguments),
            std::vector<FunctionReferenceNode::Capture>{}, match->overloadName),
        nameToken.location);
}

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    if (peek().type == TokenType::KW_IF) {
        return parseIfExpression();
    }
    if (peek().type == TokenType::KW_MATCH) {
        return parseMatchExpression();
    }
    if (peek().type == TokenType::KW_WHILE) {
        return parseWhileExpression();
    }
    if (peek().type == TokenType::KW_FOR) {
        return parseForExpression();
    }
    if (startsLambdaExpression()) {
        return parseLambdaExpression();
    }
    if (startsInferredLambdaExpression()) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "type attendu requis pour inférer le paramètre de lambda");
    }
    if (peek().type == TokenType::LPAREN) {
        Token start = consume(TokenType::LPAREN, "");
        if (peek().type == TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, start.location, "tuple vide non supporté");
        }
        auto expr = parseExpression();
        if (peek().type == TokenType::COMMA) {
            requireTupleModule();
            consume(TokenType::COMMA, "");
            auto second = parseExpression();
            if (peek().type == TokenType::COMMA) {
                throw CompilerError(
                    ErrorKind::Parser, peek().location,
                    "tuples de plus de 2 éléments non supportés pour l'instant");
            }
            consume(TokenType::RPAREN, "Parenthèse fermante attendue après le tuple");
            std::vector<std::unique_ptr<ASTNode>> arguments;
            arguments.push_back(std::move(expr));
            arguments.push_back(std::move(second));
            const std::string tupleType =
                "Tuple2[" + arguments[0]->getType() + ", " + arguments[1]->getType() + "]";
            return located(
                std::make_unique<FunctionCallNode>(
                    "Tuple2.apply", std::move(arguments), std::vector<std::string>{},
                    tupleType, "Tuple2"),
                start.location);
        }
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'expression");
        return expr;
    }
    if (peek().type == TokenType::INT_LITERAL) {
        Token token = consume(TokenType::INT_LITERAL, "");
        return located(std::make_unique<IntNode>(token.value), token.location);
    }
    if (peek().type == TokenType::LONG_LITERAL) {
        Token token = consume(TokenType::LONG_LITERAL, "");
        return located(std::make_unique<LongNode>(token.value), token.location);
    }
    if (peek().type == TokenType::DOUBLE_LITERAL) {
        Token token = consume(TokenType::DOUBLE_LITERAL, "");
        return located(std::make_unique<DoubleNode>(token.value), token.location);
    }
    if (peek().type == TokenType::FLOAT_LITERAL) {
        Token token = consume(TokenType::FLOAT_LITERAL, "");
        return located(std::make_unique<FloatNode>(token.value), token.location);
    }
    if (peek().type == TokenType::KW_TRUE) {
        Token token = consume(TokenType::KW_TRUE, "");
        return located(std::make_unique<BoolNode>(true), token.location);
    }
    if (peek().type == TokenType::KW_FALSE) {
        Token token = consume(TokenType::KW_FALSE, "");
        return located(std::make_unique<BoolNode>(false), token.location);
    }
    if (peek().type == TokenType::STRING_LITERAL) {
        Token token = consume(TokenType::STRING_LITERAL, "");
        return located(std::make_unique<StringNode>(token.value), token.location);
    }
    if (peek().type == TokenType::CHAR_LITERAL) {
        Token token = consume(TokenType::CHAR_LITERAL, "");
        return located(std::make_unique<CharNode>(token.value), token.location);
    }
    if (peek().type == TokenType::KW_NEW) {
        Token start = consume(TokenType::KW_NEW, "");
        auto [clName, typeLocation] = parseType("Nom de classe attendu après 'new'");
        (void) typeLocation;
        consume(TokenType::LPAREN, "");
        auto args = parseArguments();
        consume(TokenType::RPAREN, "");
        return located(std::make_unique<NewNode>(clName, std::move(args)), start.location);
    }
    if (peek().type == TokenType::KW_THIS) {
        if (currentParsingClass.empty()) {
            throw CompilerError(ErrorKind::Parser, peek().location, "'this' non autorisé en dehors d'une méthode de classe");
        }
        Token thisToken = consume(TokenType::KW_THIS, "'this' invalide");
        std::string thisType = currentParsingClass;
        const auto classIt = context.classes.find(currentParsingClass);
        if (classIt != context.classes.end() && !classIt->second.typeParameters.empty()) {
            thisType = formatParameterizedType(currentParsingClass, classIt->second.typeParameters);
        }
        return located(
            std::make_unique<IdentifierNode>(
                thisToken.value, "this", thisType),
            thisToken.location);
    }
    if (peek().type == TokenType::KW_SUPER) {
        if (currentParsingClass.empty()) {
            throw CompilerError(ErrorKind::Parser, peek().location, "'super' non autorisé en dehors d'une méthode de classe");
        }
        Token superToken = consume(TokenType::KW_SUPER, "'super' invalide");
        const auto classIt = context.classes.find(currentParsingClass);
        if (classIt != context.classes.end() && classIt->second.isTrait) {
            throw CompilerError(
                ErrorKind::Parser, superToken.location,
                "'super' non autorisé dans un trait");
        }
        if (classIt == context.classes.end() || classIt->second.parentTypes.empty() ||
            !classIt->second.hasExplicitParent) {
            const std::string className = currentParsingClass.empty()
                ? "<hors classe>"
                : currentParsingClass;
            throw CompilerError(
                ErrorKind::Parser, superToken.location,
                "'super' non autorisé dans la classe '" + className +
                "' sans parent explicite; ajoutez 'extends Parent(...)' avant d'appeler super");
        }
        return located(
            std::make_unique<SuperNode>(classIt->second.parentTypes[0]),
            superToken.location);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        Token nameToken = consume(TokenType::IDENTIFIER, "");
        std::string name = nameToken.value;
        std::vector<std::string> typeArguments;
        if (peek().type == TokenType::LBRACKET) {
            consume(TokenType::LBRACKET, "");
            while (peek().type != TokenType::RBRACKET) {
                auto [typeArgument, typeLocation] = parseType("Type d'argument générique attendu");
                (void) typeLocation;
                typeArguments.push_back(typeArgument);
                if (peek().type == TokenType::COMMA) {
                    consume(TokenType::COMMA, "");
                } else if (peek().type != TokenType::RBRACKET) {
                    throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu après l'argument de type");
                }
            }
            consume(TokenType::RBRACKET, "']' attendu après les arguments de type");
        }
        if (peek().type == TokenType::LPAREN) {
            std::vector<std::string> expectedArgumentTypes;
            std::vector<bool> byNameParameters;
            auto [initialSymbol, initialScopeIndex] = findLocalWithScope(name);
            (void) initialScopeIndex;
            std::string fieldFunctionType;
            std::string fieldOwnerType;
            if (initialSymbol && initialSymbol->isForbiddenCapture) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les fonctions locales ne capturent pas encore les variables ou paramètres englobants: " + name);
            }
            if (initialSymbol && !typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les arguments de type ne sont supportés que pour les fonctions nommées");
            }
            if (initialSymbol && initialSymbol->isLocalFunction) {
                auto functionType = functionTypeFromName(initialSymbol->type);
                if (functionType) expectedArgumentTypes = functionType->parameterTypes;
                consume(TokenType::LPAREN, "");
                auto arguments = parseArguments(expectedArgumentTypes, initialSymbol->parameterByNameParameters);
                consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de fonction");
                return located(
                    std::make_unique<FunctionCallNode>(
                        initialSymbol->internalName, std::move(arguments),
                        std::vector<std::string>{},
                        functionType ? functionType->returnType : "Int", name,
                        initialSymbol->returnFunctionByNameParameters),
                    nameToken.location);
            }
            std::string functionLookupName = name;
            std::vector<std::string> functionTypeArguments = typeArguments;
            if (!initialSymbol) {
                if (auto alias = resolveStdlibFunctionAlias(name, typeArguments)) {
                    auto aliasFunction = context.functions.find(*alias);
                    if (aliasFunction != context.functions.end()) {
                        functionLookupName = *alias;
                        if (aliasFunction->second.typeParameters.empty()) {
                            functionTypeArguments.clear();
                        }
                    }
                } else if (isStdlibFunctionAliasName(name) && !typeArguments.empty()) {
                    throw CompilerError(
                        ErrorKind::Parser, nameToken.location,
                        "la fonction standard générique '" + name +
                        "' ne supporte pas ces arguments de type" +
                        recommendedStdlibFunctionSuffix(name));
                } else if (context.objects.count(name) &&
                           !functionOverloadNames(context, name + ".apply").empty() &&
                           functionOverloadNames(context, name).empty()) {
                    functionLookupName = name + ".apply";
                }
            }
            if (initialSymbol) {
                auto functionType = functionTypeFromName(initialSymbol->type);
                if (functionType) expectedArgumentTypes = functionType->parameterTypes;
            } else {
                const auto* function = uniqueFunctionSignatureForName(context, functionLookupName);
                if (function) {
                    std::map<std::string, std::string> substitution;
                    if (auto genericSubstitution = genericFunctionSubstitutionFor(*function, functionTypeArguments)) {
                        substitution = *genericSubstitution;
                    }
                    for (const auto& parameter : function->parameters) {
                        expectedArgumentTypes.push_back(substituteType(parameter.type, substitution));
                        byNameParameters.push_back(parameter.isByName);
                    }
                }
            }
            if (!initialSymbol && !currentParsingClass.empty() && lambdaCaptureScopes.empty()) {
                if (auto field = resolveClassFieldInHierarchy(context, currentParsingClass, name, nameToken.location)) {
                    if (auto functionType = functionTypeFromName(field->type)) {
                        fieldFunctionType = field->type;
                        fieldOwnerType = field->ownerClassName;
                        expectedArgumentTypes = functionType->parameterTypes;
                    }
                }
            }
            consume(TokenType::LPAREN, "");
            std::vector<std::unique_ptr<ASTNode>> arguments;
            const auto* namedFunction = uniqueFunctionSignatureForName(context, functionLookupName);
            const CompilerContext::FunctionSignature* selectedOverloadedFunction = nullptr;
            std::string selectedOverloadedFunctionName;
            if (!initialSymbol && !namedFunction) {
                std::vector<std::string> byNameOverloadNames;
                for (const auto& overloadName : functionOverloadNames(context, functionLookupName)) {
                    auto functionIt = context.functions.find(overloadName);
                    if (functionIt == context.functions.end()) continue;
                    const auto& candidate = functionIt->second;
                    const bool hasByNameParameter = std::any_of(
                        candidate.parameters.begin(), candidate.parameters.end(),
                        [](const auto& parameter) { return parameter.isByName; });
                    if (hasByNameParameter) byNameOverloadNames.push_back(overloadName);
                }
                if (byNameOverloadNames.size() == 1) {
                    selectedOverloadedFunctionName = byNameOverloadNames.front();
                    selectedOverloadedFunction = &context.functions[selectedOverloadedFunctionName];
                    functionLookupName = selectedOverloadedFunctionName;
                    arguments = parseFunctionCallArguments(*selectedOverloadedFunction, functionTypeArguments);
                } else {
                    arguments = parseArguments(expectedArgumentTypes, byNameParameters);
                }
            } else if (!initialSymbol && namedFunction) {
                arguments = parseFunctionCallArguments(*namedFunction, functionTypeArguments);
            } else {
                arguments = parseArguments(expectedArgumentTypes, byNameParameters);
            }
            consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de fonction");
            auto [symbol, scopeIndex] = findLocalWithScope(name);
            if (symbol) {
                auto functionType = functionTypeFromName(symbol->type);
                if (functionType) {
                    captureIfNeeded(name, *symbol, scopeIndex);
                    return located(
                        std::make_unique<FunctionValueCallNode>(
                        name, symbol->internalName, std::move(arguments), functionType->returnType),
                        nameToken.location);
                }
            }
            if (!fieldFunctionType.empty()) {
                auto functionType = functionTypeFromName(fieldFunctionType);
                auto fieldAccess = located(
                    std::make_unique<FieldAccessNode>(fieldOwnerType, name, fieldFunctionType),
                    nameToken.location);
                return located(
                    std::make_unique<FunctionExpressionCallNode>(
                        name, std::move(fieldAccess), std::move(arguments),
                        functionType ? functionType->returnType : "Int"),
                    nameToken.location);
            }
            std::string initialReturnType = "Int";
            const auto* function = selectedOverloadedFunction
                ? selectedOverloadedFunction
                : uniqueFunctionSignatureForName(context, functionLookupName);
            if (function) {
                std::map<std::string, std::string> substitution;
                if (auto genericSubstitution = genericFunctionSubstitutionFor(*function, functionTypeArguments)) {
                    substitution = *genericSubstitution;
                } else if (functionTypeArguments.empty() && !function->typeParameters.empty()) {
                    std::vector<std::string> actualArgumentTypes;
                    for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
                    if (auto inferredSubstitution =
                            inferGenericFunctionSubstitution(*function, actualArgumentTypes)) {
                        substitution = *inferredSubstitution;
                    }
                }
                if (functionTypeArguments.empty() && !substitution.empty() &&
                    shouldRetargetInferredFactoryAlias(functionLookupName)) {
                    const auto inferredTypeArguments =
                        orderedTypeArguments(function->typeParameters, substitution);
                    if (auto alias = resolveStdlibFunctionAlias(functionLookupName, inferredTypeArguments)) {
                        auto aliasFunction = context.functions.find(*alias);
                        if (aliasFunction != context.functions.end()) {
                            functionLookupName = *alias;
                            functionTypeArguments = aliasFunction->second.typeParameters.empty()
                                ? std::vector<std::string>{}
                                : inferredTypeArguments;
                            function = &aliasFunction->second;
                            substitution.clear();
                            if (auto aliasSubstitution =
                                    genericFunctionSubstitutionFor(*function, functionTypeArguments)) {
                                substitution = *aliasSubstitution;
                            }
                        }
                    }
                }
                initialReturnType = substituteType(function->returnType, substitution);
            }
            const std::string diagnosticFunctionName =
                functionLookupName == name ? std::string() : name;
            return located(
                std::make_unique<FunctionCallNode>(
                    functionLookupName, std::move(arguments), std::move(functionTypeArguments),
                    initialReturnType, diagnosticFunctionName,
                    function ? function->returnFunctionByNameParameters : std::vector<bool>{}),
                nameToken.location);
        }
        auto [symbol, scopeIndex] = findLocalWithScope(name);
        if (symbol) {
            if (symbol->isForbiddenCapture) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les fonctions locales ne capturent pas encore les variables ou paramètres englobants: " + name);
            }
            if (!typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les arguments de type ne sont supportés que pour les fonctions nommées");
            }
            if (symbol->isLocalFunction) {
                return located(
                    std::make_unique<FunctionReferenceNode>(
                        name, symbol->type, std::vector<std::string>{},
                        std::vector<FunctionReferenceNode::Capture>{}, symbol->internalName),
                    nameToken.location);
            }
            if (symbol->isByName) {
                captureIfNeeded(name, *symbol, scopeIndex);
                auto functionType = functionTypeFromName(symbol->type);
                return located(
                    std::make_unique<FunctionValueCallNode>(
                        name, symbol->internalName, std::vector<std::unique_ptr<ASTNode>>{},
                        functionType ? functionType->returnType : "Int"),
                    nameToken.location);
            }
            captureIfNeeded(name, *symbol, scopeIndex);
            return located(std::make_unique<IdentifierNode>(name, symbol->internalName, symbol->type), nameToken.location);
        }
        if (!currentParsingClass.empty() && lambdaCaptureScopes.empty()) {
            if (auto field = resolveClassFieldInHierarchy(context, currentParsingClass, name, nameToken.location)) {
                return located(
                    std::make_unique<FieldAccessNode>(
                        field->ownerClassName, name, field->type),
                    nameToken.location);
            }
        }
        if (context.runtimeObjects.count(name)) {
            if (!typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "un objet runtime ne supporte pas d'arguments de type: " + name);
            }
            return located(std::make_unique<SingletonObjectNode>(name), nameToken.location);
        }
        auto overloadNames = functionOverloadNames(context, name);
        if (!overloadNames.empty()) {
            if (auto zeroArgOverload = resolveExactFunctionOverload(context, name, {}, typeArguments)) {
                const auto& signature = context.functions[*zeroArgOverload];
                if (signature.parameters.empty() && signature.typeParameters.empty()) {
                    if (!typeArguments.empty()) {
                        throw CompilerError(
                            ErrorKind::Parser, nameToken.location,
                            "la fonction '" + name + "' n'accepte pas d'arguments de type");
                    }
                    return located(
                        std::make_unique<FunctionCallNode>(
                            name, std::vector<std::unique_ptr<ASTNode>>{},
                            std::vector<std::string>{}, signature.returnType, std::string{},
                            signature.returnFunctionByNameParameters),
                        nameToken.location);
                }
            }
            if (overloadNames.size() > 1) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "référence de fonction surchargée non supportée sans type attendu: " + name);
            }
            const auto& signature = context.functions[overloadNames[0]];
            if (!signature.typeParameters.empty()) {
                if (typeArguments.empty()) {
                    throw CompilerError(
                        ErrorKind::Parser, nameToken.location,
                        "la fonction générique '" + name + "' doit être référencée avec des arguments de type");
                }
                std::string functionType = "Int";
                if (auto substitution = genericFunctionSubstitutionFor(signature, typeArguments)) {
                    CompilerContext::FunctionSignature substitutedSignature = signature;
                    for (auto& parameter : substitutedSignature.parameters) {
                        parameter.type = substituteType(parameter.type, *substitution);
                    }
                    substitutedSignature.returnType = substituteType(substitutedSignature.returnType, *substitution);
                    if (auto substitutedFunctionType = functionNameForSignature(substitutedSignature)) {
                        functionType = *substitutedFunctionType;
                    }
                }
                return located(
                    std::make_unique<FunctionReferenceNode>(
                        name, functionType, std::move(typeArguments),
                        std::vector<FunctionReferenceNode::Capture>{}, overloadNames[0]),
                    nameToken.location);
            }
            if (!typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "la fonction '" + name + "' n'accepte pas d'arguments de type");
            }
            auto functionType = functionNameForSignature(signature);
            if (!functionType) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "la fonction '" + name + "' n'a pas encore de type fonction valeur supporté");
            }
            return located(
                std::make_unique<FunctionReferenceNode>(
                    name, *functionType, std::vector<std::string>{},
                    std::vector<FunctionReferenceNode::Capture>{}, overloadNames[0]),
                nameToken.location);
        }
        if (!typeArguments.empty()) {
            throw CompilerError(ErrorKind::Parser, nameToken.location, "appel attendu après les arguments de type");
        }
        return located(std::make_unique<IdentifierNode>(name, name, "<unresolved>"), nameToken.location);
    }
    throw CompilerError(ErrorKind::Parser, peek().location, "expression primaire invalide '" + peek().value + "'");
}

std::unique_ptr<ASTNode> Parser::parsePostfix() {
    auto expr = parsePrimary();
    while (peek().type == TokenType::DOT || peek().type == TokenType::LPAREN) {
        if (peek().type == TokenType::LPAREN) {
            Token callToken = consume(TokenType::LPAREN, "");
            std::vector<std::string> expectedArgumentTypes;
            std::vector<bool> byNameParameters;
            std::string initialReturnType = "Int";
            if (auto functionType = functionTypeFromName(expr->getType())) {
                expectedArgumentTypes = functionType->parameterTypes;
                initialReturnType = functionType->returnType;
            }
            if (auto* functionCall = dynamic_cast<FunctionCallNode*>(expr.get())) {
                byNameParameters = functionCall->getReturnFunctionByNameParameters();
            } else if (auto* expressionCall = dynamic_cast<FunctionExpressionCallNode*>(expr.get())) {
                byNameParameters = expressionCall->getReturnFunctionByNameParameters();
            }
            auto arguments = parseArguments(expectedArgumentTypes, byNameParameters);
            consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de fonction");
            expr = located(
                std::make_unique<FunctionExpressionCallNode>(
                    "expression", std::move(expr), std::move(arguments), initialReturnType),
                callToken.location);
            continue;
        }
        consume(TokenType::DOT, "");
        Token methodToken = consume(TokenType::IDENTIFIER, "Nom de méthode attendu après '.'");
        std::string method = methodToken.value;
        std::vector<std::string> typeArguments;
        if (peek().type == TokenType::LBRACKET) {
            consume(TokenType::LBRACKET, "");
            while (peek().type != TokenType::RBRACKET) {
                auto [typeArgument, typeLocation] = parseType("Type d'argument générique attendu");
                (void) typeLocation;
                typeArguments.push_back(typeArgument);
                if (peek().type == TokenType::COMMA) {
                    consume(TokenType::COMMA, "");
                } else if (peek().type != TokenType::RBRACKET) {
                    throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu après l'argument de type");
                }
            }
            consume(TokenType::RBRACKET, "']' attendu après les arguments de type");
            if (peek().type != TokenType::LPAREN) {
                throw CompilerError(ErrorKind::Parser, methodToken.location, "appel attendu après les arguments de type");
            }
        }
        if (auto* identifier = dynamic_cast<IdentifierNode*>(expr.get());
            identifier && identifier->getType() == "<unresolved>") {
            const std::string qualifiedFunctionName = identifier->getName() + "." + method;
            std::string functionLookupName = qualifiedFunctionName;
            std::vector<std::string> functionTypeArguments = typeArguments;
            auto countImmediateArguments = [&]() -> std::optional<size_t> {
                if (peek().type != TokenType::LPAREN) return std::nullopt;
                size_t cursor = index + 1;
                if (cursor < tokens.size() && tokens[cursor].type == TokenType::RPAREN) return 0;
                size_t count = 1;
                int parenDepth = 0;
                int bracketDepth = 0;
                int braceDepth = 0;
                while (cursor < tokens.size()) {
                    const Token& token = tokens[cursor];
                    if (token.type == TokenType::LPAREN) {
                        ++parenDepth;
                    } else if (token.type == TokenType::RPAREN) {
                        if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) return count;
                        --parenDepth;
                    } else if (token.type == TokenType::LBRACKET) {
                        ++bracketDepth;
                    } else if (token.type == TokenType::RBRACKET) {
                        --bracketDepth;
                    } else if (token.type == TokenType::LBRACE) {
                        ++braceDepth;
                    } else if (token.type == TokenType::RBRACE) {
                        --braceDepth;
                    } else if (token.type == TokenType::COMMA && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
                        ++count;
                    }
                    ++cursor;
                }
                return std::nullopt;
            };
            bool selectedQualifiedOverload = false;
            if (auto argumentCount = countImmediateArguments()) {
                for (const auto& overloadName : functionOverloadNames(context, qualifiedFunctionName)) {
                    auto overloadIt = context.functions.find(overloadName);
                    if (overloadIt == context.functions.end()) continue;
                    if (overloadIt->second.parameters.size() != *argumentCount) continue;
                    if (overloadIt->second.returnFunctionByNameParameters.empty()) continue;
                    if (!genericFunctionSubstitutionFor(overloadIt->second, typeArguments)) continue;
                    functionLookupName = overloadName;
                    selectedQualifiedOverload = true;
                    break;
                }
            }
            if (!selectedQualifiedOverload) {
                if (auto alias = resolveStdlibFunctionAlias(qualifiedFunctionName, typeArguments)) {
                    auto aliasFunction = context.functions.find(*alias);
                    if (aliasFunction != context.functions.end()) {
                        functionLookupName = *alias;
                        if (aliasFunction->second.typeParameters.empty()) {
                            functionTypeArguments.clear();
                        }
                    }
                }
            }
            if (!selectedQualifiedOverload && isStdlibFunctionAliasName(qualifiedFunctionName) && !typeArguments.empty() &&
                functionLookupName == qualifiedFunctionName) {
                throw CompilerError(
                    ErrorKind::Parser, methodToken.location,
                    "la fonction standard générique '" + qualifiedFunctionName +
                    "' ne supporte pas ces arguments de type" +
                    recommendedStdlibFunctionSuffix(qualifiedFunctionName));
            }
            if (peek().type != TokenType::LPAREN) {
                if (auto zeroArgOverload = resolveExactFunctionOverload(
                        context, functionLookupName, {}, functionTypeArguments)) {
                    const auto* zeroArgSignature = findFunctionSignature(context, *zeroArgOverload);
                    if (zeroArgSignature && zeroArgSignature->parameters.empty() &&
                        zeroArgSignature->typeParameters.empty()) {
                        functionLookupName = *zeroArgOverload;
                        functionTypeArguments.clear();
                    }
                }
            }
            auto qualifiedFunction = context.functions.find(functionLookupName);
            if (qualifiedFunction != context.functions.end()) {
                std::vector<std::unique_ptr<ASTNode>> arguments;
                if (peek().type == TokenType::LPAREN) {
                    consume(TokenType::LPAREN, "");
                    arguments = parseFunctionCallArguments(qualifiedFunction->second, functionTypeArguments);
                    consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de fonction");
                } else if (functionTypeArguments.empty() && qualifiedFunction->second.parameters.empty() &&
                           qualifiedFunction->second.typeParameters.empty()) {
                    // Static namespace property sugar: `Object.name` is `Object.name()` for
                    // non-generic zero-argument defs, mirroring bare `name` and `receiver.name`.
                } else {
                    consume(TokenType::LPAREN, "");
                }

                std::map<std::string, std::string> substitution;
                if (auto genericSubstitution =
                        genericFunctionSubstitutionFor(qualifiedFunction->second, functionTypeArguments)) {
                    substitution = *genericSubstitution;
                } else if (functionTypeArguments.empty() && !qualifiedFunction->second.typeParameters.empty()) {
                    std::vector<std::string> actualArgumentTypes;
                    for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
                    if (auto inferredSubstitution =
                            inferGenericFunctionSubstitution(qualifiedFunction->second, actualArgumentTypes)) {
                        substitution = *inferredSubstitution;
                    }
                }
                if (functionTypeArguments.empty() && !substitution.empty() &&
                    shouldRetargetInferredFactoryAlias(functionLookupName)) {
                    const auto inferredTypeArguments =
                        orderedTypeArguments(qualifiedFunction->second.typeParameters, substitution);
                    if (auto alias = resolveStdlibFunctionAlias(functionLookupName, inferredTypeArguments)) {
                        auto aliasFunction = context.functions.find(*alias);
                        if (aliasFunction != context.functions.end()) {
                            functionLookupName = *alias;
                            functionTypeArguments = aliasFunction->second.typeParameters.empty()
                                ? std::vector<std::string>{}
                                : inferredTypeArguments;
                            qualifiedFunction = aliasFunction;
                            substitution.clear();
                            if (auto aliasSubstitution =
                                    genericFunctionSubstitutionFor(qualifiedFunction->second, functionTypeArguments)) {
                                substitution = *aliasSubstitution;
                            }
                        }
                    }
                }
                const std::string initialReturnType =
                    substituteType(qualifiedFunction->second.returnType, substitution);
                expr = located(
                    std::make_unique<FunctionCallNode>(
                        functionLookupName, std::move(arguments), std::move(functionTypeArguments),
                        initialReturnType,
                        functionLookupName == qualifiedFunctionName ? std::string() : qualifiedFunctionName,
                        qualifiedFunction->second.returnFunctionByNameParameters),
                    methodToken.location);
                continue;
            }
        }
        const std::string receiverType = expr->getType();
        auto expectedArgumentTypes = expectedArgumentTypesForMethodCall(receiverType, method, methodToken.location);
        std::string initialReturnType = "Int";
        std::string initialOwnerType = genericBaseName(receiverType);
        const CompilerContext::FunctionSignature* methodSignature = nullptr;
        std::optional<CompilerContext::FunctionSignature> stdlibMethodSignature;
        std::map<std::string, std::string> classSubstitution;
        if (auto signature = stdlibTypeAliasMethodSignature(receiverType, method)) {
            stdlibMethodSignature = *signature;
            methodSignature = &*stdlibMethodSignature;
            initialReturnType = methodSignature->returnType;
            initialOwnerType.clear();
            expectedArgumentTypes.clear();
            for (const auto& parameter : methodSignature->parameters) {
                expectedArgumentTypes.push_back(parameter.type);
            }
        }
        const auto methodLookupCandidates =
            collectClassMethodLookupCandidates(context, receiverType, method);
        if (methodLookupCandidates.size() == 1) {
            const auto& methodLookup = methodLookupCandidates.front();
            methodSignature = methodLookup.signature;
            initialOwnerType = methodLookup.ownerClassName;
            classSubstitution = methodLookup.classSubstitution;
            std::map<std::string, std::string> substitution = methodLookup.classSubstitution;
            if (auto methodSubstitution = genericFunctionSubstitutionFor(*methodLookup.signature, typeArguments)) {
                substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
            }
            initialReturnType = substituteType(methodLookup.signature->returnType, substitution);
            expectedArgumentTypes.clear();
            for (const auto& parameter : methodLookup.signature->parameters) {
                expectedArgumentTypes.push_back(substituteType(parameter.type, substitution));
            }
        }
        if (!methodSignature && isTypeParameterName(receiverType, currentFunctionTypeParameters)) {
            if (method == "toString") {
                initialReturnType = "String";
            } else if (method == "hashCode") {
                initialReturnType = "Int";
            } else if (method == "==" || method == "!=") {
                initialReturnType = "Bool";
                expectedArgumentTypes = {receiverType};
            }
        }
        if (peek().type != TokenType::LPAREN) {
            if (!typeArguments.empty()) {
                throw CompilerError(ErrorKind::Parser, methodToken.location, "appel attendu après les arguments de type");
            }
            if (methodSignature && methodSignature->parameters.empty()) {
                expr = located(
                    std::make_unique<MethodCallNode>(
                        std::move(expr), method, std::vector<std::unique_ptr<ASTNode>>{},
                        std::vector<std::string>{}, initialReturnType, initialOwnerType),
                    methodToken.location);
                continue;
            }
            if (auto zeroArgMethod = resolveZeroArgumentMethod(context, receiverType, method, typeArguments)) {
                methodSignature = zeroArgMethod->signature;
                initialOwnerType = zeroArgMethod->ownerClassName;
                classSubstitution = zeroArgMethod->classSubstitution;
                initialReturnType = substituteType(methodSignature->returnType, classSubstitution);
                expr = located(
                    std::make_unique<MethodCallNode>(
                        std::move(expr), method, std::vector<std::unique_ptr<ASTNode>>{},
                        std::vector<std::string>{}, initialReturnType, initialOwnerType),
                    methodToken.location);
                continue;
            }
            if (method.empty() || method[0] != '_') {
                throw CompilerError(
                    ErrorKind::Parser, methodToken.location,
                    "appel de méthode attendu après '" + method + "'");
            }
            expr = located(
                std::make_unique<MethodCallNode>(
                    std::move(expr), method, std::vector<std::unique_ptr<ASTNode>>{},
                    std::vector<std::string>{}, initialReturnType, initialOwnerType),
                methodToken.location);
            continue;
        }
        const size_t callParenIndex = index;
        consume(TokenType::LPAREN, "");
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (methodSignature) {
            arguments = parseFunctionCallArguments(*methodSignature, typeArguments, classSubstitution);
        } else {
            auto selectedOverload = selectedOverloadedMethodCallForArgumentShape(
                receiverType, method, typeArguments, callParenIndex, methodToken.location);
            if (selectedOverload) {
                methodSignature = selectedOverload->signature;
                initialOwnerType = selectedOverload->ownerClassName;
                classSubstitution = selectedOverload->classSubstitution;
                arguments = parseFunctionCallArguments(*methodSignature, typeArguments, classSubstitution);
            } else {
                arguments = parseArguments(expectedArgumentTypes);
            }
        }
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de méthode");
        if (methodSignature) {
            std::map<std::string, std::string> substitution = classSubstitution;
            if (auto methodSubstitution = genericFunctionSubstitutionFor(*methodSignature, typeArguments)) {
                substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
            } else if (typeArguments.empty() && !methodSignature->typeParameters.empty()) {
                std::vector<std::string> actualArgumentTypes;
                for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
                CompilerContext::FunctionSignature substitutedSignature = *methodSignature;
                for (auto& parameter : substitutedSignature.parameters) {
                    parameter.type = substituteType(parameter.type, classSubstitution);
                }
                substitutedSignature.returnType =
                    substituteType(substitutedSignature.returnType, classSubstitution);
                if (auto inferredSubstitution =
                        inferGenericFunctionSubstitution(substitutedSignature, actualArgumentTypes)) {
                    substitution.insert(inferredSubstitution->begin(), inferredSubstitution->end());
                }
            }
            initialReturnType = substituteType(methodSignature->returnType, substitution);
        }
        expr = located(
            std::make_unique<MethodCallNode>(
                std::move(expr), method, std::move(arguments), std::move(typeArguments),
                initialReturnType, initialOwnerType),
            methodToken.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseUnary() {
    if (peek().type == TokenType::BANG) {
        Token op = consume(TokenType::BANG, "");
        return located(std::make_unique<NotNode>(parseUnary()), op.location);
    }
    if (peek().type == TokenType::MINUS) {
        Token op = consume(TokenType::MINUS, "");
        return located(std::make_unique<UnaryMinusNode>(parseUnary()), op.location);
    }
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative() {
    auto expr = parseUnary();
    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH ||
           peek().type == TokenType::PERCENT) {
        Token op = tokens[index++];
        auto right = parseUnary();
        std::vector<std::unique_ptr<ASTNode>> arguments;
        arguments.push_back(std::move(right));
        expr = located(std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(arguments)), op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        Token op = tokens[index++];
        auto right = parseMultiplicative();
        std::vector<std::unique_ptr<ASTNode>> arguments;
        arguments.push_back(std::move(right));
        expr = located(std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(arguments)), op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseComparison() {
    auto expr = parseAdditive();
    while (peek().type == TokenType::EQEQ || peek().type == TokenType::NEQ || peek().type == TokenType::LT || peek().type == TokenType::GT || peek().type == TokenType::LTE || peek().type == TokenType::GTE) {
        Token op = tokens[index++];
        auto right = parseAdditive();
        std::vector<std::unique_ptr<ASTNode>> arguments;
        arguments.push_back(std::move(right));
        expr = located(std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(arguments)), op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAnd() {
    auto expr = parseComparison();
    while (peek().type == TokenType::AND_AND) {
        Token op = tokens[index++];
        auto right = parseComparison();
        expr = located(std::make_unique<LogicalNode>(op.value, std::move(expr), std::move(right)), op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (peek().type == TokenType::OR_OR) {
        Token op = tokens[index++];
        auto right = parseLogicalAnd();
        expr = located(std::make_unique<LogicalNode>(op.value, std::move(expr), std::move(right)), op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseTupleArrow() {
    auto expr = parseLogicalOr();
    while (peek().type == TokenType::THIN_ARROW) {
        Token op = tokens[index++];
        requireTupleModule();
        auto right = parseLogicalOr();
        std::vector<std::unique_ptr<ASTNode>> arguments;
        arguments.push_back(std::move(expr));
        arguments.push_back(std::move(right));
        const std::string tupleType =
            "Tuple2[" + arguments[0]->getType() + ", " + arguments[1]->getType() + "]";
        expr = located(
            std::make_unique<FunctionCallNode>(
                "Tuple2.apply", std::move(arguments), std::vector<std::string>{},
                tupleType, "Tuple2"),
            op.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseTupleArrow();
}

std::unique_ptr<ASTNode> Parser::parseLocalFunctionDef() {
    if (localScopes.empty()) {
        throw CompilerError(ErrorKind::Parser, peek().location, "déclaration 'def' locale hors d'un bloc");
    }

    Token defToken = consume(TokenType::KW_DEF, "");
    Token nameToken = consume(TokenType::IDENTIFIER, "Nom de fonction locale attendu");
    const std::string name = nameToken.value;
    if (localScopes.back().count(name)) {
        throw CompilerError(ErrorKind::Parser, nameToken.location, "symbole déjà déclaré dans cette portée: " + name);
    }
    if (peek().type == TokenType::LBRACKET) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "les fonctions locales génériques ne sont pas encore supportées");
    }
    if (!currentFunctionTypeParameters.empty()) {
        throw CompilerError(
            ErrorKind::Parser, nameToken.location,
            "les fonctions locales dans un contexte générique ne sont pas encore supportées");
    }

    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    std::map<std::string, ParsedSymbol> functionScope;

    consume(TokenType::LPAREN, "'(' attendu après le nom de fonction locale");
    while (peek().type != TokenType::RPAREN) {
        Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu");
        const std::string parameterName = parameterToken.value;
        consume(TokenType::COLON, "':' attendu après le nom du paramètre");
        bool isByName = false;
        SourceLocation parameterTypeLocation = peek().location;
        std::string parameterType;
        if (peek().type == TokenType::FAT_ARROW) {
            consume(TokenType::FAT_ARROW, "");
            auto [byNameReturnType, byNameReturnLocation] = parseType("Type de retour du paramètre par nom attendu");
            parameterType = zeroArgumentFunctionTypeFor(byNameReturnType);
            parameterTypeLocation = byNameReturnLocation;
            isByName = true;
        } else {
            auto parsedType = parseType("Type de paramètre attendu");
            parameterType = parsedType.first;
            parameterTypeLocation = parsedType.second;
        }
        if (peek().type == TokenType::STAR) {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                "les paramètres répétés ne sont pas encore supportés sur les fonctions locales");
        }
        if (functionScope.count(parameterName)) {
            throw CompilerError(ErrorKind::Parser, parameterToken.location, "paramètre déjà déclaré: " + parameterName);
        }
        const std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        functionScope[parameterName] = {symbolName, parameterType, false, false, false, isByName};
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTypeLocation, false, "", isByName});
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
        }
    }
    consume(TokenType::RPAREN, "Parenthèse fermante attendue après les paramètres de fonction locale");
    consume(TokenType::COLON, "':' attendu avant le type de retour de fonction locale");
    auto [returnType, returnTypeLocation] = parseType("Type de retour attendu");

    CompilerContext::FunctionType functionTypeInfo;
    for (const auto& parameter : signatureParameters) functionTypeInfo.parameterTypes.push_back(parameter.type);
    functionTypeInfo.returnType = returnType;
    auto functionType = functionNameForType(functionTypeInfo);
    if (!functionType) {
        throw CompilerError(
            ErrorKind::Parser, nameToken.location,
            "type fonction local non supporté pour l'instant");
    }

    const std::string mangledName =
        "local." + name + "." + std::to_string(context.nextLambdaId++);
    CompilerContext::FunctionSignature signature{
        signatureParameters, returnType, std::vector<std::string>{},
        defToken.location, returnTypeLocation, false, false, false, {}};
    context.functions[mangledName] = signature;
    std::vector<bool> localFunctionByNameParameters;
    for (const auto& parameter : signatureParameters) {
        localFunctionByNameParameters.push_back(parameter.isByName);
    }
    localScopes.back()[name] = {
        mangledName, *functionType, false, true, false, false,
        localFunctionByNameParameters, signature.returnFunctionByNameParameters};

    functionScope[name] = {
        mangledName, *functionType, false, true, false, false,
        localFunctionByNameParameters, signature.returnFunctionByNameParameters};

    consume(TokenType::EQUAL, "'=' attendu après la signature de fonction locale");

    auto savedLocalScopes = std::move(localScopes);
    auto savedTypeParameters = currentFunctionTypeParameters;
    auto savedParsingClass = currentParsingClass;
    localScopes.clear();
    for (const auto& savedScope : savedLocalScopes) {
        std::map<std::string, ParsedSymbol> visibleScope;
        for (const auto& [sourceName, symbol] : savedScope) {
            if (symbol.isLocalFunction) {
                visibleScope[sourceName] = symbol;
            } else {
                visibleScope[sourceName] = {
                    symbol.internalName, symbol.type, symbol.isMutable, false, true, symbol.isByName,
                    symbol.parameterByNameParameters, symbol.returnFunctionByNameParameters};
            }
        }
        if (!visibleScope.empty()) localScopes.push_back(std::move(visibleScope));
    }
    localScopes.push_back(std::move(functionScope));
    currentFunctionTypeParameters.clear();
    currentParsingClass.clear();

    std::unique_ptr<ASTNode> body;
    if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "");
        body = parseBlock();
        consume(TokenType::RBRACE, "Fin du bloc de fonction locale attendue");
    } else {
        body = parseExpression();
    }

    localScopes = std::move(savedLocalScopes);
    currentFunctionTypeParameters = std::move(savedTypeParameters);
    currentParsingClass = std::move(savedParsingClass);

    generatedFunctions.push_back(located(std::make_unique<FunctionDefNode>(
        "", mangledName, returnType, std::vector<std::string>{},
        std::move(parameters), std::move(body), std::vector<FunctionDefNode::Capture>{}),
        defToken.location));

    return located(std::make_unique<LocalFunctionDeclNode>(name), defToken.location);
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef(
    std::string clName, std::string objectName, bool allowAbstractMethod) {
    bool isOverride = false;
    if (peek().type == TokenType::KW_OVERRIDE) {
        if (clName.empty()) {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                "mot-clé 'override' uniquement autorisé dans une déclaration de méthode de classe");
        }
        consume(TokenType::KW_OVERRIDE, "");
        isOverride = true;
    }
    Token defToken = consume(TokenType::KW_DEF, "");
    Token nameToken = consume(TokenType::IDENTIFIER, "Nom de fonction attendu");
    std::string name = nameToken.value;
    const std::string functionName = objectName.empty() ? name : objectName + "." + name;
    std::vector<std::string> typeParameters;
    if (peek().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "");
        while (peek().type != TokenType::RBRACKET) {
            Token typeParameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre de type attendu");
            if (typeParameterToken.value == "Int" || typeParameterToken.value == "Long" ||
                typeParameterToken.value == "Float" || typeParameterToken.value == "Double" ||
                typeParameterToken.value == "Bool" || typeParameterToken.value == "Char" ||
                typeParameterToken.value == "String" ||
                typeParameterToken.value == "Unit" || typeParameterToken.value == "Nothing" ||
                typeParameterToken.value == "IntArray" ||
                typeParameterToken.value == "LongArray" || typeParameterToken.value == "FloatArray" ||
                typeParameterToken.value == "DoubleArray" || typeParameterToken.value == "BoolArray") {
                throw CompilerError(
                    ErrorKind::Parser, typeParameterToken.location,
                    "paramètre de type invalide: " + typeParameterToken.value);
            }
            if (isTypeParameterName(typeParameterToken.value, typeParameters)) {
                throw CompilerError(
                    ErrorKind::Parser, typeParameterToken.location,
                    "paramètre de type déjà déclaré: " + typeParameterToken.value);
            }
            typeParameters.push_back(typeParameterToken.value);
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RBRACKET) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu après le paramètre de type");
            }
        }
        consume(TokenType::RBRACKET, "']' attendu après les paramètres de type");
    }
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    std::vector<FunctionDefNode::Parameter> curriedParameters;
    std::vector<CompilerContext::ParameterInfo> curriedSignatureParameters;
    std::vector<ParsedSymbol> curriedScopedParameters;
    localScopes.emplace_back();
    auto parseParameterList = [&](std::vector<FunctionDefNode::Parameter>& parsedParameters,
                                  std::vector<CompilerContext::ParameterInfo>& parsedSignatureParameters,
                                  std::vector<ParsedSymbol>* scopedParameters,
                                  std::map<std::string, ParsedSymbol>* targetScope,
                                  bool allowRepeated,
                                  const std::string& repeatedError) {
        consume(TokenType::LPAREN, "");
        while (peek().type != TokenType::RPAREN) {
            Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu");
            std::string parameterName = parameterToken.value;
            consume(TokenType::COLON, "':' attendu après le nom du paramètre");
            bool isByName = false;
            SourceLocation parameterTypeLocation = peek().location;
            std::string declaredParameterType;
            if (peek().type == TokenType::FAT_ARROW) {
                consume(TokenType::FAT_ARROW, "");
                auto [byNameReturnType, byNameReturnLocation] = parseType("Type de retour du paramètre par nom attendu");
                declaredParameterType = zeroArgumentFunctionTypeFor(byNameReturnType);
                parameterTypeLocation = byNameReturnLocation;
                isByName = true;
            } else {
                auto parsedType = parseType("Type de paramètre attendu");
                declaredParameterType = parsedType.first;
                parameterTypeLocation = parsedType.second;
            }
            bool isRepeated = false;
            std::string repeatedElementType;
            std::string parameterType = declaredParameterType;
            if (peek().type == TokenType::STAR) {
                if (!allowRepeated) {
                    throw CompilerError(ErrorKind::Parser, peek().location, repeatedError);
                }
                consume(TokenType::STAR, "");
                isRepeated = true;
                repeatedElementType = declaredParameterType;
                parameterType = canonicalTypeName(formatParameterizedType("Array", {declaredParameterType}));
                if (peek().type == TokenType::COMMA) {
                    throw CompilerError(
                        ErrorKind::Parser, parameterToken.location,
                        "un paramètre répété doit être le dernier paramètre");
                }
            }
            if (targetScope && targetScope->count(parameterName)) {
                throw CompilerError(ErrorKind::Parser, parameterToken.location, "paramètre déjà déclaré: " + parameterName);
            }
            std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
            if (targetScope) (*targetScope)[parameterName] = {symbolName, parameterType, false, false, false, isByName};
            if (scopedParameters) scopedParameters->push_back({symbolName, parameterType, false, false, false, isByName});
            parsedParameters.push_back({parameterName, symbolName, parameterType});
            parsedSignatureParameters.push_back({
                parameterName, parameterType, parameterTypeLocation, isRepeated, repeatedElementType, isByName});
            if (peek().type == TokenType::COMMA) {
                consume(TokenType::COMMA, "");
            } else if (peek().type != TokenType::RPAREN) {
                throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
            }
        }
        consume(TokenType::RPAREN, "");
    };
    if (peek().type == TokenType::LPAREN) {
        parseParameterList(
            parameters, signatureParameters, nullptr, &localScopes.back(), true,
            "paramètre répété non supporté ici");
    }
    if (peek().type == TokenType::LPAREN) {
        parseParameterList(
            curriedParameters, curriedSignatureParameters, &curriedScopedParameters, nullptr, false,
            "les paramètres répétés ne sont pas encore supportés dans une liste curryfiée");
        if (!clName.empty()) {
            throw CompilerError(
                ErrorKind::Parser, nameToken.location,
                "les méthodes curryfiées ne sont pas encore supportées");
        }
        if (peek().type == TokenType::LPAREN) {
            throw CompilerError(
                ErrorKind::Parser, peek().location,
                "une seule liste de paramètres curryfiée est supportée pour l'instant");
        }
    }
    consume(TokenType::COLON, "");
    auto [declaredReturnType, returnTypeLocation] = parseType("Type de retour attendu");
    std::string returnType = declaredReturnType;
    if (!curriedSignatureParameters.empty()) {
        CompilerContext::FunctionType curriedFunctionType;
        for (const auto& parameter : curriedSignatureParameters) {
            curriedFunctionType.parameterTypes.push_back(parameter.type);
        }
        curriedFunctionType.returnType = declaredReturnType;
        auto functionReturnType = functionNameForType(curriedFunctionType);
        if (!functionReturnType) {
            throw CompilerError(
                ErrorKind::Parser, nameToken.location,
                "type fonction curryfié non supporté pour l'instant");
        }
        returnType = *functionReturnType;
    }
    const bool isAbstract = allowAbstractMethod && peek().type != TokenType::EQUAL;
    CompilerContext::FunctionSignature signature{
        signatureParameters, returnType, typeParameters, defToken.location, returnTypeLocation, isOverride, isAbstract, false, {}};
    if (!curriedSignatureParameters.empty()) {
        for (const auto& parameter : curriedSignatureParameters) {
            signature.returnFunctionByNameParameters.push_back(parameter.isByName);
        }
    }
    std::string registeredFunctionName = functionName;
    if (!clName.empty()) {
        auto& overloads = context.classes[clName].methodOverloads[name];
        for (const auto& overloadName : overloads) {
            const auto existingMethod = context.classes[clName].methods.find(overloadName);
            if (existingMethod != context.classes[clName].methods.end() &&
                functionSignatureParametersMatch(existingMethod->second, signature)) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "méthode déjà déclarée avec cette signature dans '" + clName + "': " + name);
            }
        }
        registeredFunctionName = name;
        if (!overloads.empty()) {
            registeredFunctionName = overloadedMethodName(name, signature);
            if (context.classes[clName].methods.count(registeredFunctionName)) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "méthode déjà déclarée avec cette signature dans '" + clName + "': " + name);
            }
        }
        overloads.push_back(registeredFunctionName);
        context.classes[clName].methods[registeredFunctionName] = signature;
    } else {
        auto& overloads = context.functionOverloads[functionName];
        for (const auto& overloadName : overloads) {
            const auto existingFunction = context.functions.find(overloadName);
            if (existingFunction != context.functions.end() &&
                functionSignatureParametersMatch(existingFunction->second, signature)) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "fonction déjà déclarée avec cette signature: " + functionName);
            }
        }
        if (!overloads.empty()) {
            registeredFunctionName = overloadedFunctionName(functionName, signature);
            if (context.functions.count(registeredFunctionName)) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "fonction déjà déclarée avec cette signature: " + functionName);
            }
        }
        overloads.push_back(registeredFunctionName);
        context.functions[registeredFunctionName] = signature;
    }
    if (isAbstract) {
        localScopes.pop_back();
        return nullptr;
    }
    consume(TokenType::EQUAL, "");
    auto previousFunctionTypeParameters = currentFunctionTypeParameters;
    currentFunctionTypeParameters.clear();
    if (!clName.empty()) {
        currentFunctionTypeParameters = context.classes[clName].typeParameters;
    }
    currentFunctionTypeParameters.insert(
        currentFunctionTypeParameters.end(), typeParameters.begin(), typeParameters.end());
    std::unique_ptr<ASTNode> body;
    if (!curriedSignatureParameters.empty()) {
        const size_t outerScopeCount = localScopes.size();
        lambdaCaptureScopes.push_back({outerScopeCount, {}});
        localScopes.emplace_back();
        for (size_t i = 0; i < curriedScopedParameters.size(); ++i) {
            localScopes.back()[curriedParameters[i].name] = curriedScopedParameters[i];
        }
        std::unique_ptr<ASTNode> curriedBody;
        if (peek().type == TokenType::LBRACE) {
            consume(TokenType::LBRACE, "");
            curriedBody = parseBlock();
            consume(TokenType::RBRACE, "");
        } else {
            curriedBody = parseExpression();
        }
        localScopes.pop_back();

        std::vector<FunctionDefNode::Capture> captures;
        for (const auto& [capturedSymbolName, capture] : lambdaCaptureScopes.back().capturesBySymbol) {
            (void) capturedSymbolName;
            captures.push_back({capture.name, capture.symbolName, capture.type});
        }
        lambdaCaptureScopes.pop_back();

        std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
        context.functions[lambdaName] = {
            curriedSignatureParameters, declaredReturnType, currentFunctionTypeParameters,
            nameToken.location, returnTypeLocation, false, false, false, {}};
        generatedFunctions.push_back(located(std::make_unique<FunctionDefNode>(
            "", lambdaName, declaredReturnType, currentFunctionTypeParameters,
            std::move(curriedParameters), std::move(curriedBody), captures), nameToken.location));

        std::vector<FunctionReferenceNode::Capture> referenceCaptures;
        for (const auto& capture : captures) {
            referenceCaptures.push_back({capture.name, capture.symbolName, capture.type});
        }
        body = located(
            std::make_unique<FunctionReferenceNode>(
                lambdaName, returnType, currentFunctionTypeParameters, std::move(referenceCaptures)),
            nameToken.location);
    } else if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "");
        body = parseBlock();
        consume(TokenType::RBRACE, "");
    } else {
        body = parseExpression();
    }
    currentFunctionTypeParameters = previousFunctionTypeParameters;
    localScopes.pop_back();
    std::vector<std::string> ownerTypeParameters;
    if (!clName.empty()) ownerTypeParameters = context.classes[clName].typeParameters;
    return located(std::make_unique<FunctionDefNode>(
        clName, registeredFunctionName, returnType, std::move(typeParameters),
        std::move(parameters), std::move(body), std::vector<FunctionDefNode::Capture>{},
        std::move(ownerTypeParameters)), defToken.location);
}

std::pair<std::string, SourceLocation> Parser::parseType(const std::string& expectedMessage) {
    if (peek().type == TokenType::FAT_ARROW) {
        throw CompilerError(
            ErrorKind::Parser, peek().location,
            "type par nom autorisé uniquement en position paramètre");
    }
    if (peek().type == TokenType::IDENTIFIER) {
        Token typeToken = consume(TokenType::IDENTIFIER, expectedMessage);
        std::string typeName = typeToken.value;
        if (peek().type == TokenType::LBRACKET) {
            consume(TokenType::LBRACKET, "");
            std::vector<std::string> typeArguments;
            while (peek().type != TokenType::RBRACKET) {
                auto [argumentType, argumentLocation] = parseType("Type d'argument générique attendu");
                (void) argumentLocation;
                typeArguments.push_back(argumentType);
                if (peek().type == TokenType::COMMA) {
                    consume(TokenType::COMMA, "");
                } else if (peek().type != TokenType::RBRACKET) {
                    throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ']' attendu dans le type générique");
                }
            }
            consume(TokenType::RBRACKET, "']' attendu après les arguments génériques");
            typeName = formatParameterizedType(typeName, typeArguments);
        }
        std::string canonicalType = canonicalTypeName(typeName);
        auto parameterizedType = parameterizedTypeFromName(canonicalType);
        if (parameterizedType && parameterizedType->first == "Array" &&
            parameterizedType->second.size() == 1 &&
            !isTypeParameterName(parameterizedType->second[0], currentFunctionTypeParameters) &&
            isKnownConcreteTypeName(parameterizedType->second[0], context)) {
            canonicalType = formatParameterizedType("ArrayObject", parameterizedType->second);
        }
        return {canonicalType, typeToken.location};
    }
    if (peek().type != TokenType::LPAREN) {
        throw CompilerError(ErrorKind::Parser, peek().location, expectedMessage + " (reçu '" + peek().value + "')");
    }

    Token start = consume(TokenType::LPAREN, "");
    std::vector<std::string> parenthesizedTypes;
    while (peek().type != TokenType::RPAREN) {
        auto [parameterType, parameterLocation] = parseType("Type de paramètre attendu dans le type fonction");
        (void) parameterLocation;
        parenthesizedTypes.push_back(parameterType);
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu dans le type fonction");
        }
    }
    consume(TokenType::RPAREN, "Parenthèse fermante attendue dans le type fonction");
    if (peek().type != TokenType::FAT_ARROW) {
        if (parenthesizedTypes.empty()) {
            throw CompilerError(ErrorKind::Parser, start.location, "type tuple vide non supporté");
        }
        if (parenthesizedTypes.size() == 1) {
            return {parenthesizedTypes[0], start.location};
        }
        if (parenthesizedTypes.size() == 2) {
            requireTupleModule();
            return {formatParameterizedType("Tuple2", parenthesizedTypes), start.location};
        }
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "types tuples de plus de 2 éléments non supportés pour l'instant");
    }
    consume(TokenType::FAT_ARROW, "'=>' attendu dans le type fonction");
    auto [returnType, returnTypeLocation] = parseType("Type de retour attendu dans le type fonction");
    (void) returnTypeLocation;
    CompilerContext::FunctionType functionType;
    functionType.parameterTypes = std::move(parenthesizedTypes);
    functionType.returnType = returnType;

    auto functionName = functionNameForType(functionType);
    if (!functionName) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "type fonction non supporté pour l'instant");
    }
    return {*functionName, start.location};
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseArguments(
    const std::vector<std::string>& expectedTypes,
    const std::vector<bool>& byNameParameters) {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    while (peek().type != TokenType::RPAREN) {
        std::string expectedType;
        bool allowByName = false;
        if (arguments.size() < expectedTypes.size()) expectedType = expectedTypes[arguments.size()];
        if (arguments.size() < byNameParameters.size()) allowByName = byNameParameters[arguments.size()];
        arguments.push_back(parseArgument(expectedType, allowByName));
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après l'argument");
        }
    }
    return arguments;
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseFunctionCallArguments(
    const CompilerContext::FunctionSignature& signature,
    const std::vector<std::string>& typeArguments,
    std::map<std::string, std::string> initialSubstitution) {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    std::map<std::string, std::string> substitution = std::move(initialSubstitution);
    if (auto genericSubstitution = genericFunctionSubstitutionFor(signature, typeArguments)) {
        substitution.insert(genericSubstitution->begin(), genericSubstitution->end());
    }

    while (peek().type != TokenType::RPAREN) {
        std::string expectedType;
        std::string expectedPattern;
        const size_t parameterIndex = arguments.size();
        if (parameterIndex < signature.parameters.size() ||
            (hasRepeatedParameter(signature) && parameterIndex >= signature.parameters.size() - 1)) {
            if (hasRepeatedParameter(signature) && parameterIndex >= signature.parameters.size() - 1) {
                expectedPattern = substituteType(signature.parameters.back().repeatedElementType, substitution);
            } else {
                expectedPattern = substituteType(signature.parameters[parameterIndex].type, substitution);
            }
            expectedType = expectedPattern;
        }
        auto argument = parseArgument(expectedType, parameterIndex < signature.parameters.size() && signature.parameters[parameterIndex].isByName);
        if (typeArguments.empty() && !expectedPattern.empty()) {
            inferTypeArgumentsFromTypes(
                expectedPattern, argument->getType(),
                signature.typeParameters, substitution);
        }
        arguments.push_back(std::move(argument));
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après l'argument");
        }
    }
    return arguments;
}

std::optional<std::vector<bool>> Parser::argumentLambdaShapeAtCall(size_t callParenIndex) const {
    if (callParenIndex >= tokens.size() || tokens[callParenIndex].type != TokenType::LPAREN) {
        return std::nullopt;
    }
    std::vector<bool> shape;
    size_t cursor = callParenIndex + 1;
    if (cursor < tokens.size() && tokens[cursor].type == TokenType::RPAREN) {
        return shape;
    }

    while (cursor < tokens.size()) {
        shape.push_back(startsInferredLambdaExpressionAt(cursor));
        int parenDepth = 0;
        int bracketDepth = 0;
        int braceDepth = 0;
        while (cursor < tokens.size()) {
            const auto tokenType = tokens[cursor].type;
            if (tokenType == TokenType::LPAREN) {
                ++parenDepth;
            } else if (tokenType == TokenType::RPAREN) {
                if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
                    return shape;
                }
                --parenDepth;
            } else if (tokenType == TokenType::LBRACKET) {
                ++bracketDepth;
            } else if (tokenType == TokenType::RBRACKET) {
                --bracketDepth;
            } else if (tokenType == TokenType::LBRACE) {
                ++braceDepth;
            } else if (tokenType == TokenType::RBRACE) {
                --braceDepth;
            } else if (tokenType == TokenType::COMMA &&
                       parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
                ++cursor;
                break;
            }
            ++cursor;
        }
    }
    return std::nullopt;
}

std::optional<ClassMethodLookupResult> Parser::selectedOverloadedMethodCallForArgumentShape(
    const std::string& receiverType, const std::string& methodName,
    const std::vector<std::string>& typeArguments, size_t callParenIndex,
    const SourceLocation& location) const {
    auto argumentShape = argumentLambdaShapeAtCall(callParenIndex);
    if (!argumentShape) return std::nullopt;

    const auto methodCandidates = collectClassMethodLookupCandidates(context, receiverType, methodName);
    if (methodCandidates.size() <= 1) return std::nullopt;

    std::vector<ClassMethodLookupResult> concreteMatches;
    std::vector<ClassMethodLookupResult> genericMatches;
    for (const auto& candidate : methodCandidates) {
        if (!candidate.signature) continue;
        const auto& signature = *candidate.signature;
        if (!acceptsArgumentCount(signature, argumentShape->size())) continue;
        std::map<std::string, std::string> substitution = candidate.classSubstitution;
        if (!typeArguments.empty()) {
            auto methodSubstitution = genericFunctionSubstitutionFor(signature, typeArguments);
            if (!methodSubstitution) continue;
            substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
        }
        bool lambdaCompatible = true;
        for (size_t i = 0; i < argumentShape->size(); ++i) {
            if (!(*argumentShape)[i]) continue;
            const std::string expectedType = expectedArgumentTypeAt(signature, i, substitution);
            if (!functionTypeFromName(expectedType)) {
                lambdaCompatible = false;
                break;
            }
        }
        if (lambdaCompatible) {
            if (signature.typeParameters.empty()) {
                concreteMatches.push_back(candidate);
            } else {
                genericMatches.push_back(candidate);
            }
        }
    }

    const bool hasInferredLambdaArgument =
        std::find(argumentShape->begin(), argumentShape->end(), true) != argumentShape->end();
    std::vector<ClassMethodLookupResult> matchedCandidates = concreteMatches.empty() ? genericMatches : concreteMatches;
    std::set<std::string> providers;
    for (const auto& candidate : matchedCandidates) {
        providers.insert(substituteType(candidate.ownerClassName, candidate.classSubstitution));
    }
    if (providers.size() > 1) {
        throw CompilerError(
            ErrorKind::Parser, location,
            "conflit d'héritage pour la méthode '" + methodName + "' dans la classe '" +
            genericBaseName(receiverType) + "': plusieurs définitions dans [" +
            formatMethodProviders(providers) + "]");
    }
    const size_t matchCount = matchedCandidates.size();
    if (matchCount > 1 && hasInferredLambdaArgument) {
        throw CompilerError(
            ErrorKind::Parser, location,
            "appel de méthode surchargée ambigu pour '" + receiverType + "." + methodName +
            "' avec lambda inférée: ajoutez des types explicites aux paramètres de lambda ou annotez la valeur fonction" +
            "\ncandidats:" + formatMethodOverloadCandidates(context, receiverType, methodName));
    }
    if (concreteMatches.size() == 1) return concreteMatches.front();
    if (concreteMatches.empty() && genericMatches.size() == 1) {
        return genericMatches.front();
    }
    return std::nullopt;
}

std::unique_ptr<ASTNode> Parser::parseArgument(const std::string& expectedType, bool allowByName) {
    if (startsInferredLambdaExpression()) {
        return parseInferredLambdaExpression(expectedType);
    }
    if (auto functionReference = parseFunctionReferenceWithExpectedType(expectedType)) {
        return functionReference;
    }
    auto expectedFunction = functionTypeFromName(expectedType);
    if (allowByName && expectedFunction && expectedFunction->parameterTypes.empty()) {
        return parseByNameArgument(expectedType, *expectedFunction);
    }
    auto expression = parseExpression();
    if (peek().type == TokenType::COLON) {
        const size_t colonIndex = index;
        consume(TokenType::COLON, "");
        if (peek().type == TokenType::IDENTIFIER && peek().value == "_") {
            consume(TokenType::IDENTIFIER, "");
            consume(TokenType::STAR, "'*' attendu après ': _'");
            return located(std::make_unique<SplatNode>(std::move(expression)), tokens[colonIndex].location);
        }
        index = colonIndex;
    }
    return expression;
}

std::vector<std::string> Parser::expectedArgumentTypesForMethodCall(
    const std::string& receiverType, const std::string& methodName,
    const SourceLocation& location) const {
    (void) location;
    if (receiverType == "Int" || receiverType == "Long" || receiverType == "Float" || receiverType == "Double") {
        const bool binaryMethod =
            methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/" ||
            ((receiverType == "Int" || receiverType == "Long") && methodName == "%") ||
            methodName == "==" || methodName == "!=" || methodName == "<" || methodName == ">" ||
            methodName == "<=" || methodName == ">=";
        if (binaryMethod) return {receiverType};
        return {};
    }
    if (receiverType == "IntArray") {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", "Int"};
        return {};
    }
    if (receiverType == "LongArray") {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", "Long"};
        return {};
    }
    if (receiverType == "FloatArray") {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", "Float"};
        return {};
    }
    if (receiverType == "DoubleArray") {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", "Double"};
        return {};
    }
    if (receiverType == "BoolArray") {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", "Bool"};
        return {};
    }
    if (auto parameterizedType = parameterizedTypeFromName(receiverType);
        parameterizedType && parameterizedType->first == "ObjectArray" &&
        parameterizedType->second.size() == 1) {
        if (methodName == "get") return {"Int"};
        if (methodName == "set") return {"Int", parameterizedType->second[0]};
        return {};
    }
    if (auto signature = stdlibTypeAliasMethodSignature(receiverType, methodName)) {
        std::vector<std::string> expectedTypes;
        for (const auto& parameter : signature->parameters) {
            expectedTypes.push_back(parameter.type);
        }
        return expectedTypes;
    }
    const auto methodCandidates = collectClassMethodLookupCandidates(
        context, receiverType, methodName);
    if (methodCandidates.size() > 1) {
        return {};
    }
    if (methodCandidates.empty()) return {};
    const auto& methodLookup = methodCandidates.front();
    const auto& methodSignature = *methodLookup.signature;
    std::map<std::string, std::string> substitution = methodLookup.classSubstitution;
    std::vector<std::string> expectedTypes;
    for (const auto& parameter : methodSignature.parameters) {
        expectedTypes.push_back(substituteType(parameter.type, substitution));
    }
    return expectedTypes;
}

const Parser::ParsedSymbol* Parser::findLocal(const std::string& name) const {
    return findLocalWithScope(name).first;
}

std::pair<const Parser::ParsedSymbol*, size_t> Parser::findLocalWithScope(const std::string& name) const {
    for (auto scope = localScopes.rbegin(); scope != localScopes.rend(); ++scope) {
        auto symbol = scope->find(name);
        if (symbol != scope->end()) {
            return {&symbol->second, static_cast<size_t>(std::distance(scope, localScopes.rend()) - 1)};
        }
    }
    return {nullptr, 0};
}

void Parser::captureIfNeeded(const std::string& name, const ParsedSymbol& symbol, size_t scopeIndex) {
    if (lambdaCaptureScopes.empty()) return;
    for (auto& lambda : lambdaCaptureScopes) {
        if (scopeIndex < lambda.outerScopeCount) {
            lambda.capturesBySymbol[symbol.internalName] = {name, symbol.internalName, symbol.type};
        }
    }
}
