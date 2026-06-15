#include "parser.hpp"
#include <fstream>
#include <sstream>

namespace {
bool isBuiltinTypeName(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" || type == "Double" ||
           type == "Bool" || type == "Char" || type == "String" || type == "Unit" || type == "IntArray" ||
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

void Parser::parseClassDefinition(std::unique_ptr<ProgramNode>& program) {
    Token classToken = consume(TokenType::KW_CLASS, "");
    Token classNameToken = consume(TokenType::IDENTIFIER, "Nom de classe attendu");
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
                typeParameterToken.value == "Unit" || typeParameterToken.value == "IntArray" ||
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
    currentParsingClass = className;
    auto previousTypeParameters = currentFunctionTypeParameters;
    currentFunctionTypeParameters = typeParameters;
    consume(TokenType::LPAREN, "");
    int offset = 8;
    while (peek().type != TokenType::RPAREN) {
        Token fieldToken = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu");
        std::string fieldName = fieldToken.value;
        consume(TokenType::COLON, "");
        auto [fieldType, fieldTypeLocation] = parseType("Type attendu");
        if (context.classLayouts[className].count(fieldName)) {
            throw CompilerError(ErrorKind::Parser, fieldToken.location, "champ déjà déclaré dans '" + className + "': " + fieldName);
        }
        context.classLayouts[className][fieldName] = offset;
        context.classes[className].fields.push_back({fieldName, fieldType, fieldTypeLocation});
        offset += 8;
        if (peek().type == TokenType::COMMA) consume(TokenType::COMMA, "");
    }
    consume(TokenType::RPAREN, "");
    if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "");
        while (peek().type != TokenType::RBRACE) {
            if (peek().type == TokenType::KW_DEF) program->elements.push_back(parseFunctionDef(className));
            else throw CompilerError(ErrorKind::Parser, peek().location, "instruction inattendue dans la classe '" + peek().value + "'");
        }
        consume(TokenType::RBRACE, "");
    }
    currentFunctionTypeParameters = previousTypeParameters;
    currentParsingClass.clear();
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
    consume(TokenType::LBRACE, "Bloc 'else' attendu après 'else'");
    auto elseBranch = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc 'else' attendue");
    return located(std::make_unique<IfNode>(std::move(condition), std::move(thenBranch), std::move(elseBranch)), start.location);
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
    if (peek().type == TokenType::KW_VAL || peek().type == TokenType::KW_VAR) {
        Token declarationToken = peek();
        bool isMutable = (peek().type == TokenType::KW_VAR);
        index++; // consume kw
        std::string name = consume(TokenType::IDENTIFIER, "Nom de variable attendu").value;
        consume(TokenType::EQUAL, "Initialisation requise pour 'val'/'var'");
        auto init = parseExpression();
        if (localScopes.empty()) {
            throw CompilerError(ErrorKind::Parser, declarationToken.location, "déclaration locale hors d'un bloc: " + name);
        }
        if (localScopes.back().count(name)) {
            throw CompilerError(ErrorKind::Parser, declarationToken.location, "variable déjà déclarée dans cette portée: " + name);
        }
        std::string symbolName = name + "#" + std::to_string(nextSymbolId++);
        localScopes.back()[name] = {symbolName, init->getType(), isMutable};
        return located(std::make_unique<VarDeclNode>(name, symbolName, std::move(init), isMutable), declarationToken.location);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        // possible assignment
        if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::EQUAL) {
            Token nameToken = consume(TokenType::IDENTIFIER, "");
            std::string name = nameToken.value;
            consume(TokenType::EQUAL, "");
            auto rhs = parseExpression();
            const ParsedSymbol* symbol = findLocal(name);
            return located(std::make_unique<AssignmentNode>(
                name, symbol ? symbol->internalName : name, symbol ? symbol->type : "Int",
                symbol ? symbol->isMutable : false, std::move(rhs)), nameToken.location);
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
    if (cursor >= tokens.size() || tokens[cursor].type == TokenType::RPAREN) return false;

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
    if (peek().type == TokenType::IDENTIFIER) {
        return index + 1 < tokens.size() && tokens[index + 1].type == TokenType::FAT_ARROW;
    }
    if (peek().type != TokenType::LPAREN) return false;
    size_t cursor = index + 1;
    if (cursor >= tokens.size() || tokens[cursor].type == TokenType::RPAREN) return false;

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
    if (parameters.empty()) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "les lambdas doivent avoir au moins un paramètre");
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

    const std::string returnType = body->getType();
    std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
    context.functions[lambdaName] = {
        signatureParameters, returnType, currentFunctionTypeParameters, start.location, start.location};

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

    const std::string returnType = body->getType();
    std::string lambdaName = "lambda." + std::to_string(context.nextLambdaId++);
    context.functions[lambdaName] = {
        signatureParameters, returnType, currentFunctionTypeParameters, start.location, start.location};

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

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    if (peek().type == TokenType::KW_IF) {
        return parseIfExpression();
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
        consume(TokenType::LPAREN, "");
        auto expr = parseExpression();
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
            auto [initialSymbol, initialScopeIndex] = findLocalWithScope(name);
            (void) initialScopeIndex;
            if (initialSymbol && !typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les arguments de type ne sont supportés que pour les fonctions nommées");
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
                        "' ne supporte pas ces arguments de type");
                }
            }
            if (initialSymbol) {
                auto functionType = functionTypeFromName(initialSymbol->type);
                if (functionType) expectedArgumentTypes = functionType->parameterTypes;
            } else {
                auto function = context.functions.find(functionLookupName);
                if (function != context.functions.end()) {
                    std::map<std::string, std::string> substitution;
                    if (auto genericSubstitution = genericFunctionSubstitutionFor(function->second, functionTypeArguments)) {
                        substitution = *genericSubstitution;
                    }
                    for (const auto& parameter : function->second.parameters) {
                        expectedArgumentTypes.push_back(substituteType(parameter.type, substitution));
                    }
                }
            }
            consume(TokenType::LPAREN, "");
            std::vector<std::unique_ptr<ASTNode>> arguments;
            auto namedFunction = context.functions.find(functionLookupName);
            if (!initialSymbol && namedFunction != context.functions.end()) {
                arguments = parseFunctionCallArguments(namedFunction->second, functionTypeArguments);
            } else {
                arguments = parseArguments(expectedArgumentTypes);
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
            std::string initialReturnType = "Int";
            auto function = context.functions.find(functionLookupName);
            if (function != context.functions.end()) {
                std::map<std::string, std::string> substitution;
                if (auto genericSubstitution = genericFunctionSubstitutionFor(function->second, functionTypeArguments)) {
                    substitution = *genericSubstitution;
                } else if (functionTypeArguments.empty() && !function->second.typeParameters.empty()) {
                    std::vector<std::string> actualArgumentTypes;
                    for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
                    if (auto inferredSubstitution =
                            inferGenericFunctionSubstitution(function->second, actualArgumentTypes)) {
                        substitution = *inferredSubstitution;
                    }
                }
                initialReturnType = substituteType(function->second.returnType, substitution);
            }
            return located(
                std::make_unique<FunctionCallNode>(
                    functionLookupName, std::move(arguments), std::move(functionTypeArguments), initialReturnType),
                nameToken.location);
        }
        auto [symbol, scopeIndex] = findLocalWithScope(name);
        if (symbol) {
            if (!typeArguments.empty()) {
                throw CompilerError(
                    ErrorKind::Parser, nameToken.location,
                    "les arguments de type ne sont supportés que pour les fonctions nommées");
            }
            captureIfNeeded(name, *symbol, scopeIndex);
            return located(std::make_unique<IdentifierNode>(name, symbol->internalName, symbol->type), nameToken.location);
        }
        if (!currentParsingClass.empty() && context.classLayouts[currentParsingClass].count(name)) {
            for (const auto& field : context.classes[currentParsingClass].fields) {
                if (field.name == name) {
                    return located(std::make_unique<FieldAccessNode>(currentParsingClass, name, field.type), nameToken.location);
                }
            }
        }
        if (context.functions.count(name)) {
            const auto& signature = context.functions[name];
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
                    std::make_unique<FunctionReferenceNode>(name, functionType, std::move(typeArguments)),
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
                    name, *functionType, std::vector<std::string>{}),
                nameToken.location);
        }
        if (!typeArguments.empty()) {
            throw CompilerError(ErrorKind::Parser, nameToken.location, "appel attendu après les arguments de type");
        }
        return located(std::make_unique<IdentifierNode>(name, name, "Int"), nameToken.location);
    }
    throw CompilerError(ErrorKind::Parser, peek().location, "expression primaire invalide '" + peek().value + "'");
}

std::unique_ptr<ASTNode> Parser::parsePostfix() {
    auto expr = parsePrimary();
    while (peek().type == TokenType::DOT) {
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
        const std::string receiverType = expr->getType();
        auto expectedArgumentTypes = expectedArgumentTypesForMethodCall(receiverType, method);
        std::string initialReturnType = "Int";
        std::string initialOwnerType = genericBaseName(receiverType);
        auto classIt = context.classes.find(initialOwnerType);
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
        if (classIt != context.classes.end()) {
            auto methodIt = classIt->second.methods.find(method);
            if (methodIt != classIt->second.methods.end()) {
                methodSignature = &methodIt->second;
                std::map<std::string, std::string> substitution;
                if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
                    substitution = *genericSubstitution;
                    classSubstitution = *genericSubstitution;
                }
                if (auto methodSubstitution = genericFunctionSubstitutionFor(methodIt->second, typeArguments)) {
                    substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
                }
                initialReturnType = substituteType(methodIt->second.returnType, substitution);
                expectedArgumentTypes.clear();
                for (const auto& parameter : methodIt->second.parameters) {
                    expectedArgumentTypes.push_back(substituteType(parameter.type, substitution));
                }
            }
        }
        consume(TokenType::LPAREN, "");
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (methodSignature) {
            arguments = parseFunctionCallArguments(*methodSignature, typeArguments, classSubstitution);
        } else {
            arguments = parseArguments(expectedArgumentTypes);
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
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative() {
    auto expr = parseUnary();
    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH) {
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

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseLogicalOr();
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef(std::string clName) {
    Token defToken = consume(TokenType::KW_DEF, "");
    Token nameToken = consume(TokenType::IDENTIFIER, "Nom de fonction attendu");
    std::string name = nameToken.value;
    std::vector<std::string> typeParameters;
    if (peek().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "");
        while (peek().type != TokenType::RBRACKET) {
            Token typeParameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre de type attendu");
            if (typeParameterToken.value == "Int" || typeParameterToken.value == "Long" ||
                typeParameterToken.value == "Float" || typeParameterToken.value == "Double" ||
                typeParameterToken.value == "Bool" || typeParameterToken.value == "Char" ||
                typeParameterToken.value == "String" ||
                typeParameterToken.value == "Unit" || typeParameterToken.value == "IntArray" ||
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
    consume(TokenType::LPAREN, "");
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    localScopes.emplace_back();
    while (peek().type != TokenType::RPAREN) {
        Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu");
        std::string parameterName = parameterToken.value;
        consume(TokenType::COLON, "':' attendu après le nom du paramètre");
        auto [parameterType, parameterTypeLocation] = parseType("Type de paramètre attendu");
        if (localScopes.back().count(parameterName)) {
            throw CompilerError(ErrorKind::Parser, parameterToken.location, "paramètre déjà déclaré: " + parameterName);
        }
        std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        localScopes.back()[parameterName] = {symbolName, parameterType, false};
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTypeLocation});
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
        }
    }
    consume(TokenType::RPAREN, "");
    consume(TokenType::COLON, "");
    auto [returnType, returnTypeLocation] = parseType("Type de retour attendu");
    CompilerContext::FunctionSignature signature{
        signatureParameters, returnType, typeParameters, defToken.location, returnTypeLocation};
    if (!clName.empty()) {
        if (context.classes[clName].methods.count(name)) {
            throw CompilerError(ErrorKind::Parser, nameToken.location, "méthode déjà déclarée dans '" + clName + "': " + name);
        }
        context.classes[clName].methods[name] = signature;
    } else {
        if (context.functions.count(name)) {
            throw CompilerError(ErrorKind::Parser, nameToken.location, "fonction déjà déclarée: " + name);
        }
        context.functions[name] = signature;
    }
    consume(TokenType::EQUAL, ""); consume(TokenType::LBRACE, "");
    auto previousFunctionTypeParameters = currentFunctionTypeParameters;
    currentFunctionTypeParameters.clear();
    if (!clName.empty()) {
        currentFunctionTypeParameters = context.classes[clName].typeParameters;
    }
    currentFunctionTypeParameters.insert(
        currentFunctionTypeParameters.end(), typeParameters.begin(), typeParameters.end());
    auto body = parseBlock();
    currentFunctionTypeParameters = previousFunctionTypeParameters;
    consume(TokenType::RBRACE, "");
    localScopes.pop_back();
    std::vector<std::string> ownerTypeParameters;
    if (!clName.empty()) ownerTypeParameters = context.classes[clName].typeParameters;
    return located(std::make_unique<FunctionDefNode>(
        clName, name, returnType, std::move(typeParameters),
        std::move(parameters), std::move(body), std::vector<FunctionDefNode::Capture>{},
        std::move(ownerTypeParameters)), defToken.location);
}

std::pair<std::string, SourceLocation> Parser::parseType(const std::string& expectedMessage) {
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
    CompilerContext::FunctionType functionType;
    while (peek().type != TokenType::RPAREN) {
        auto [parameterType, parameterLocation] = parseType("Type de paramètre attendu dans le type fonction");
        (void) parameterLocation;
        functionType.parameterTypes.push_back(parameterType);
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu dans le type fonction");
        }
    }
    consume(TokenType::RPAREN, "Parenthèse fermante attendue dans le type fonction");
    consume(TokenType::FAT_ARROW, "'=>' attendu dans le type fonction");
    auto [returnType, returnTypeLocation] = parseType("Type de retour attendu dans le type fonction");
    (void) returnTypeLocation;
    functionType.returnType = returnType;

    auto functionName = functionNameForType(functionType);
    if (!functionName) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "type fonction non supporté pour l'instant");
    }
    return {*functionName, start.location};
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseArguments(const std::vector<std::string>& expectedTypes) {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    while (peek().type != TokenType::RPAREN) {
        std::string expectedType;
        if (arguments.size() < expectedTypes.size()) expectedType = expectedTypes[arguments.size()];
        arguments.push_back(parseArgument(expectedType));
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
        if (arguments.size() < signature.parameters.size()) {
            expectedPattern = substituteType(signature.parameters[arguments.size()].type, substitution);
            expectedType = expectedPattern;
        }
        auto argument = parseArgument(expectedType);
        if (typeArguments.empty() && arguments.size() < signature.parameters.size()) {
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

std::unique_ptr<ASTNode> Parser::parseArgument(const std::string& expectedType) {
    if (startsInferredLambdaExpression()) {
        return parseInferredLambdaExpression(expectedType);
    }
    return parseExpression();
}

std::vector<std::string> Parser::expectedArgumentTypesForMethodCall(
    const std::string& receiverType, const std::string& methodName) const {
    if (receiverType == "Int" || receiverType == "Long" || receiverType == "Float" || receiverType == "Double") {
        const bool binaryMethod =
            methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/" ||
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
    const std::string classLookupName = genericBaseName(receiverType);
    auto classIt = context.classes.find(classLookupName);
    if (auto signature = stdlibTypeAliasMethodSignature(receiverType, methodName)) {
        std::vector<std::string> expectedTypes;
        for (const auto& parameter : signature->parameters) {
            expectedTypes.push_back(parameter.type);
        }
        return expectedTypes;
    }
    if (classIt == context.classes.end()) return {};
    auto methodIt = classIt->second.methods.find(methodName);
    if (methodIt == classIt->second.methods.end()) return {};
    std::map<std::string, std::string> substitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        substitution = *genericSubstitution;
    }
    std::vector<std::string> expectedTypes;
    for (const auto& parameter : methodIt->second.parameters) {
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
    auto& lambda = lambdaCaptureScopes.back();
    if (scopeIndex >= lambda.outerScopeCount) return;
    lambda.capturesBySymbol[symbol.internalName] = {name, symbol.internalName, symbol.type};
}
