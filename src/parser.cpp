#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens, CompilerContext& ctx, const std::filesystem::path& currentFile)
    : tokens(tokens), index(0), context(ctx), currentFile(currentFile), currentParsingClass("") {}

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = std::make_unique<ProgramNode>();
    while (peek().type != TokenType::EOF_TOKEN) {
        if (peek().type == TokenType::KW_IMPORT) {
            parseImport(program);
        } else if (peek().type == TokenType::KW_CLASS) {
            parseClassDefinition(program);
        } else if (peek().type == TokenType::KW_DEF) {
            program->elements.push_back(parseFunctionDef(""));
        } else {
            throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : instruction inattendue '" + peek().value + "'");
        }
    }
    return program;
}

Token Parser::peek() const {
    return tokens[index];
}

Token Parser::consume(TokenType expected, const std::string& err) {
    if (peek().type == expected) return tokens[index++];
    throw std::runtime_error("à la ligne " + std::to_string(peek().line) + ": " + err + " (reçu '" + peek().value + "')");
}

void Parser::parseImport(std::unique_ptr<ProgramNode>& currentProgram) {
    consume(TokenType::KW_IMPORT, "");
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
        throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : Impossible de charger '" + importPath.string() + "'");
    }

    std::string canonicalPath = std::filesystem::canonical(importPath).string();
    if (context.parsedFiles.find(canonicalPath) != context.parsedFiles.end()) return;
    context.parsedFiles.insert(canonicalPath);

    std::ifstream file(importPath);
    if (!file.is_open()) throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : Impossible d'ouvrir '" + importPath.string() + "'");
    std::stringstream buffer; buffer << file.rdbuf();

    Lexer subLexer(buffer.str());
    Parser subParser(subLexer.tokenize(), context, importPath);
    auto subProgram = subParser.parseProgram();
    for (auto& el : subProgram->elements) currentProgram->elements.push_back(std::move(el));
}

void Parser::parseClassDefinition(std::unique_ptr<ProgramNode>& program) {
    consume(TokenType::KW_CLASS, "");
    std::string className = consume(TokenType::IDENTIFIER, "Nom de classe attendu").value;
    if (context.classes.count(className)) {
        throw std::runtime_error("Classe déjà déclarée: " + className);
    }
    context.classes[className] = {};
    currentParsingClass = className;
    consume(TokenType::LPAREN, "");
    int offset = 8;
    while (peek().type != TokenType::RPAREN) {
        std::string fieldName = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu").value;
        consume(TokenType::COLON, "");
        std::string fieldType = consume(TokenType::IDENTIFIER, "Type attendu").value;
        if (context.classLayouts[className].count(fieldName)) {
            throw std::runtime_error("Champ déjà déclaré dans '" + className + "': " + fieldName);
        }
        context.classLayouts[className][fieldName] = offset;
        context.classes[className].fields.push_back({fieldName, fieldType});
        offset += 8;
        if (peek().type == TokenType::COMMA) consume(TokenType::COMMA, "");
    }
    consume(TokenType::RPAREN, "");
    if (peek().type == TokenType::LBRACE) {
        consume(TokenType::LBRACE, "");
        while (peek().type != TokenType::RBRACE) {
            if (peek().type == TokenType::KW_DEF) program->elements.push_back(parseFunctionDef(className));
            else throw std::runtime_error("à la ligne " + std::to_string(peek().line) + ": instruction inattendue dans la classe '" + peek().value + "'");
        }
        consume(TokenType::RBRACE, "");
    }
    currentParsingClass.clear();
}

std::unique_ptr<ASTNode> Parser::parseIfExpression() {
    consume(TokenType::KW_IF, "");
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
    return std::make_unique<IfNode>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<ASTNode> Parser::parseWhileExpression() {
    consume(TokenType::KW_WHILE, "");
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
    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    localScopes.emplace_back();
    std::vector<std::unique_ptr<ASTNode>> expressions;
    while (peek().type != TokenType::RBRACE) {
        expressions.push_back(parseStatement());
    }
    if (expressions.empty()) {
        throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : bloc vide non autorisé");
    }
    localScopes.pop_back();
    if (expressions.size() == 1) {
        return std::move(expressions[0]);
    }
    return std::make_unique<BlockNode>(std::move(expressions));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (peek().type == TokenType::KW_VAL || peek().type == TokenType::KW_VAR) {
        bool isMutable = (peek().type == TokenType::KW_VAR);
        index++; // consume kw
        std::string name = consume(TokenType::IDENTIFIER, "Nom de variable attendu").value;
        consume(TokenType::EQUAL, "Initialisation requise pour 'val'/'var'");
        auto init = parseExpression();
        if (localScopes.empty()) {
            throw std::runtime_error("Déclaration locale hors d'un bloc: " + name);
        }
        if (localScopes.back().count(name)) {
            throw std::runtime_error("Variable déjà déclarée dans cette portée: " + name);
        }
        std::string symbolName = name + "#" + std::to_string(nextSymbolId++);
        localScopes.back()[name] = {symbolName, init->getType(), isMutable};
        return std::make_unique<VarDeclNode>(name, symbolName, std::move(init), isMutable);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        // possible assignment
        if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::EQUAL) {
            std::string name = consume(TokenType::IDENTIFIER, "").value;
            consume(TokenType::EQUAL, "");
            auto rhs = parseExpression();
            const ParsedSymbol* symbol = findLocal(name);
            return std::make_unique<AssignmentNode>(
                name, symbol ? symbol->internalName : name, symbol ? symbol->type : "Int",
                symbol ? symbol->isMutable : false, std::move(rhs));
        }
    }
    return parseExpression();
}

std::unique_ptr<ASTNode> Parser::parseForExpression() {
    consume(TokenType::KW_FOR, "");
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
    return std::make_unique<ForNode>(std::move(count), std::move(body));
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
    if (peek().type == TokenType::LPAREN) {
        consume(TokenType::LPAREN, "");
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'expression");
        return expr;
    }
    if (peek().type == TokenType::INT_LITERAL) {
        return std::make_unique<IntNode>(consume(TokenType::INT_LITERAL, "").value);
    }
    if (peek().type == TokenType::KW_NEW) {
        consume(TokenType::KW_NEW, "");
        std::string clName = consume(TokenType::IDENTIFIER, "Nom de classe attendu après 'new'").value;
        consume(TokenType::LPAREN, "");
        std::vector<std::unique_ptr<ASTNode>> args;
        while (peek().type != TokenType::RPAREN) {
            args.push_back(parseExpression());
            if (peek().type == TokenType::COMMA) consume(TokenType::COMMA, "");
        }
        consume(TokenType::RPAREN, "");
        return std::make_unique<NewNode>(clName, std::move(args));
    }
    if (peek().type == TokenType::IDENTIFIER) {
        std::string name = consume(TokenType::IDENTIFIER, "").value;
        if (const ParsedSymbol* symbol = findLocal(name)) {
            return std::make_unique<IdentifierNode>(name, symbol->internalName, symbol->type);
        }
        if (!currentParsingClass.empty() && context.classLayouts[currentParsingClass].count(name)) {
            for (const auto& [fieldName, fieldType] : context.classes[currentParsingClass].fields) {
                if (fieldName == name) {
                    return std::make_unique<FieldAccessNode>(currentParsingClass, name, fieldType);
                }
            }
        }
        return std::make_unique<IdentifierNode>(name, name, "Int");
    }
    throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : expression primaire invalide '" + peek().value + "'");
}

std::unique_ptr<ASTNode> Parser::parsePostfix() {
    auto expr = parsePrimary();
    while (peek().type == TokenType::DOT) {
        consume(TokenType::DOT, "");
        std::string method = consume(TokenType::IDENTIFIER, "Nom de méthode attendu après '.'").value;
        consume(TokenType::LPAREN, "");
        std::unique_ptr<ASTNode> arg;
        if (peek().type != TokenType::RPAREN) {
            arg = parseExpression();
        }
        consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'appel de méthode");
        expr = std::make_unique<MethodCallNode>(std::move(expr), method, std::move(arg));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative() {
    auto expr = parsePostfix();
    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH) {
        Token op = tokens[index++];
        auto right = parsePostfix();
        expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        Token op = tokens[index++];
        auto right = parseMultiplicative();
        expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseComparison() {
    auto expr = parseAdditive();
    while (peek().type == TokenType::EQEQ || peek().type == TokenType::NEQ || peek().type == TokenType::LT || peek().type == TokenType::GT || peek().type == TokenType::LTE || peek().type == TokenType::GTE) {
        Token op = tokens[index++];
        auto right = parseAdditive();
        expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseComparison();
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef(std::string clName) {
    consume(TokenType::KW_DEF, "");
    std::string name = consume(TokenType::IDENTIFIER, "Nom de fonction attendu").value;
    consume(TokenType::LPAREN, ""); consume(TokenType::RPAREN, "");
    consume(TokenType::COLON, "");
    std::string returnType = consume(TokenType::IDENTIFIER, "Type de retour attendu").value;
    if (!clName.empty()) {
        if (context.classes[clName].methods.count(name)) {
            throw std::runtime_error("Méthode déjà déclarée dans '" + clName + "': " + name);
        }
        context.classes[clName].methods[name] = returnType;
    }
    consume(TokenType::EQUAL, ""); consume(TokenType::LBRACE, "");
    auto body = parseBlock();
    consume(TokenType::RBRACE, "");
    return std::make_unique<FunctionDefNode>(clName, name, returnType, std::move(body));
}

const Parser::ParsedSymbol* Parser::findLocal(const std::string& name) const {
    for (auto scope = localScopes.rbegin(); scope != localScopes.rend(); ++scope) {
        auto symbol = scope->find(name);
        if (symbol != scope->end()) return &symbol->second;
    }
    return nullptr;
}
