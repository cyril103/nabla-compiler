#include "parser.hpp"
#include <fstream>
#include <sstream>

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
    if (context.classes.count(className)) {
        throw CompilerError(ErrorKind::Parser, classNameToken.location, "classe déjà déclarée: " + className);
    }
    context.classes[className].location = classToken.location;
    currentParsingClass = className;
    consume(TokenType::LPAREN, "");
    int offset = 8;
    while (peek().type != TokenType::RPAREN) {
        Token fieldToken = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu");
        std::string fieldName = fieldToken.value;
        consume(TokenType::COLON, "");
        Token fieldTypeToken = consume(TokenType::IDENTIFIER, "Type attendu");
        std::string fieldType = fieldTypeToken.value;
        if (context.classLayouts[className].count(fieldName)) {
            throw CompilerError(ErrorKind::Parser, fieldToken.location, "champ déjà déclaré dans '" + className + "': " + fieldName);
        }
        context.classLayouts[className][fieldName] = offset;
        context.classes[className].fields.push_back({fieldName, fieldType, fieldTypeToken.location});
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
    if (peek().type != TokenType::LPAREN || index + 5 >= tokens.size()) return false;
    if (tokens[index + 1].type != TokenType::IDENTIFIER ||
        tokens[index + 2].type != TokenType::COLON ||
        tokens[index + 3].type != TokenType::IDENTIFIER) {
        return false;
    }
    if (tokens[index + 4].type == TokenType::RPAREN) {
        return tokens[index + 5].type == TokenType::FAT_ARROW;
    }
    return index + 9 < tokens.size() &&
           tokens[index + 4].type == TokenType::COMMA &&
           tokens[index + 5].type == TokenType::IDENTIFIER &&
           tokens[index + 6].type == TokenType::COLON &&
           tokens[index + 7].type == TokenType::IDENTIFIER &&
           tokens[index + 8].type == TokenType::RPAREN &&
           tokens[index + 9].type == TokenType::FAT_ARROW;
}

std::unique_ptr<ASTNode> Parser::parseLambdaExpression() {
    Token start = consume(TokenType::LPAREN, "");
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    std::vector<std::pair<std::string, std::string>> scopedParameters;

    while (peek().type != TokenType::RPAREN) {
        Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu dans la lambda");
        std::string parameterName = parameterToken.value;
        consume(TokenType::COLON, "':' attendu après le nom du paramètre de lambda");
        Token parameterTypeToken = consume(TokenType::IDENTIFIER, "Type de paramètre attendu dans la lambda");
        std::string parameterType = parameterTypeToken.value;
        if (parameterType != "Int") {
            throw CompilerError(
                ErrorKind::Parser, parameterTypeToken.location,
                "seules les lambdas Int => Int, Int => Unit et (Int, Int) => Int sont supportées pour l'instant");
        }
        std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTypeToken.location});
        scopedParameters.push_back({parameterName, symbolName});
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
        }
    }
    if (parameters.empty() || parameters.size() > 2) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "seules les lambdas à un ou deux paramètres Int sont supportées pour l'instant");
    }
    consume(TokenType::RPAREN, "Parenthèse fermante attendue après le paramètre de lambda");
    consume(TokenType::FAT_ARROW, "'=>' attendu après le paramètre de lambda");
    consume(TokenType::LBRACE, "Bloc attendu après '=>'");

    localScopes.emplace_back();
    for (const auto& [parameterName, symbolName] : scopedParameters) {
        localScopes.back()[parameterName] = {symbolName, "Int", false};
    }
    auto body = parseBlock();
    consume(TokenType::RBRACE, "Fin du bloc de lambda attendue");
    localScopes.pop_back();

    const std::string returnType = body->getType();
    if ((signatureParameters.size() == 1 && returnType != "Int" && returnType != "Unit") ||
        (signatureParameters.size() == 2 && returnType != "Int")) {
        throw CompilerError(
            ErrorKind::Parser, start.location,
            "seules les lambdas Int => Int, Int => Unit et (Int, Int) => Int sont supportées pour l'instant");
    }

    std::string lambdaName = "lambda." + std::to_string(nextLambdaId++);
    context.functions[lambdaName] = {signatureParameters, returnType, start.location, start.location};

    auto function = located(std::make_unique<FunctionDefNode>(
        "", lambdaName, returnType, std::move(parameters), std::move(body)), start.location);
    generatedFunctions.push_back(std::move(function));
    const std::string functionType = signatureParameters.size() == 2
        ? "IntBinaryFn"
        : (returnType == "Unit" ? "IntConsumerFn" : "IntUnaryFn");
    return located(std::make_unique<FunctionReferenceNode>(lambdaName, functionType), start.location);
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
    if (peek().type == TokenType::STRING_LITERAL) {
        Token token = consume(TokenType::STRING_LITERAL, "");
        return located(std::make_unique<StringNode>(token.value), token.location);
    }
    if (peek().type == TokenType::KW_NEW) {
        Token start = consume(TokenType::KW_NEW, "");
        std::string clName = consume(TokenType::IDENTIFIER, "Nom de classe attendu après 'new'").value;
        consume(TokenType::LPAREN, "");
        auto args = parseArguments();
        consume(TokenType::RPAREN, "");
        return located(std::make_unique<NewNode>(clName, std::move(args)), start.location);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        Token nameToken = consume(TokenType::IDENTIFIER, "");
        std::string name = nameToken.value;
        if (peek().type == TokenType::LPAREN) {
            consume(TokenType::LPAREN, "");
            auto arguments = parseArguments();
            consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de fonction");
            if (const ParsedSymbol* symbol = findLocal(name)) {
                if (symbol->type == "IntUnaryFn" || symbol->type == "IntConsumerFn" ||
                    symbol->type == "IntBinaryFn") {
                    return located(
                        std::make_unique<FunctionValueCallNode>(
                            name, symbol->internalName, std::move(arguments)),
                        nameToken.location);
                }
            }
            return located(std::make_unique<FunctionCallNode>(name, std::move(arguments)), nameToken.location);
        }
        if (const ParsedSymbol* symbol = findLocal(name)) {
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
            std::string functionType = "IntUnaryFn";
            if (signature.parameters.size() == 2) {
                functionType = "IntBinaryFn";
            } else if (signature.parameters.size() == 1 && signature.returnType == "Unit") {
                functionType = "IntConsumerFn";
            }
            return located(std::make_unique<FunctionReferenceNode>(name, functionType), nameToken.location);
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
        consume(TokenType::LPAREN, "");
        auto arguments = parseArguments();
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de méthode");
        expr = located(std::make_unique<MethodCallNode>(std::move(expr), method, std::move(arguments)), methodToken.location);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative() {
    auto expr = parsePostfix();
    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH) {
        Token op = tokens[index++];
        auto right = parsePostfix();
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

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseComparison();
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef(std::string clName) {
    Token defToken = consume(TokenType::KW_DEF, "");
    Token nameToken = consume(TokenType::IDENTIFIER, "Nom de fonction attendu");
    std::string name = nameToken.value;
    consume(TokenType::LPAREN, "");
    std::vector<FunctionDefNode::Parameter> parameters;
    std::vector<CompilerContext::ParameterInfo> signatureParameters;
    localScopes.emplace_back();
    while (peek().type != TokenType::RPAREN) {
        Token parameterToken = consume(TokenType::IDENTIFIER, "Nom de paramètre attendu");
        std::string parameterName = parameterToken.value;
        consume(TokenType::COLON, "':' attendu après le nom du paramètre");
        Token parameterTypeToken = consume(TokenType::IDENTIFIER, "Type de paramètre attendu");
        std::string parameterType = parameterTypeToken.value;
        if (localScopes.back().count(parameterName)) {
            throw CompilerError(ErrorKind::Parser, parameterToken.location, "paramètre déjà déclaré: " + parameterName);
        }
        std::string symbolName = parameterName + "#" + std::to_string(nextSymbolId++);
        localScopes.back()[parameterName] = {symbolName, parameterType, false};
        parameters.push_back({parameterName, symbolName, parameterType});
        signatureParameters.push_back({parameterName, parameterType, parameterTypeToken.location});
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après le paramètre");
        }
    }
    consume(TokenType::RPAREN, "");
    consume(TokenType::COLON, "");
    Token returnTypeToken = consume(TokenType::IDENTIFIER, "Type de retour attendu");
    std::string returnType = returnTypeToken.value;
    CompilerContext::FunctionSignature signature{signatureParameters, returnType, defToken.location, returnTypeToken.location};
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
    auto body = parseBlock();
    consume(TokenType::RBRACE, "");
    localScopes.pop_back();
    return located(std::make_unique<FunctionDefNode>(
        clName, name, returnType, std::move(parameters), std::move(body)), defToken.location);
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseArguments() {
    std::vector<std::unique_ptr<ASTNode>> arguments;
    while (peek().type != TokenType::RPAREN) {
        arguments.push_back(parseExpression());
        if (peek().type == TokenType::COMMA) {
            consume(TokenType::COMMA, "");
        } else if (peek().type != TokenType::RPAREN) {
            throw CompilerError(ErrorKind::Parser, peek().location, "',' ou ')' attendu après l'argument");
        }
    }
    return arguments;
}

const Parser::ParsedSymbol* Parser::findLocal(const std::string& name) const {
    for (auto scope = localScopes.rbegin(); scope != localScopes.rend(); ++scope) {
        auto symbol = scope->find(name);
        if (symbol != scope->end()) return &symbol->second;
    }
    return nullptr;
}
