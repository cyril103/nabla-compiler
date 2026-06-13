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
    currentParsingClass = className;
    consume(TokenType::LPAREN, "");
    int offset = 8;
    while (peek().type != TokenType::RPAREN) {
        std::string fieldName = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu").value;
        consume(TokenType::COLON, ""); consume(TokenType::IDENTIFIER, "Type attendu");
        context.classLayouts[className][fieldName] = offset;
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

std::unique_ptr<ASTNode> Parser::parsePrimary() {
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
        if (!currentParsingClass.empty() && context.classLayouts[currentParsingClass].count(name)) {
            return std::make_unique<FieldAccessNode>(currentParsingClass, name);
        }
        throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : identifiant inconnu '" + name + "'");
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

std::unique_ptr<ASTNode> Parser::parseExpression() {
    auto expr = parseMultiplicative();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        Token op = tokens[index++];
        auto right = parseMultiplicative();
        expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef(std::string clName) {
    consume(TokenType::KW_DEF, "");
    std::string name = consume(TokenType::IDENTIFIER, "Nom de fonction attendu").value;
    consume(TokenType::LPAREN, ""); consume(TokenType::RPAREN, "");
    consume(TokenType::COLON, ""); consume(TokenType::IDENTIFIER, "Type de retour attendu").value;
    consume(TokenType::EQUAL, ""); consume(TokenType::LBRACE, "");
    auto body = parseExpression();
    consume(TokenType::RBRACE, "");
    return std::make_unique<FunctionDefNode>(clName, name, std::move(body));
}
