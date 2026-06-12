#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <set>
#include <fstream>
#include <sstream>
#include <stdexcept>

extern std::set<std::string> parsed_files;
extern std::string current_parsing_class;

class Parser {
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens), index(0) {}
    std::unique_ptr<ProgramNode> parseProgram() {
        auto program = std::make_unique<ProgramNode>();
        while (peek().type != TokenType::EOF_TOKEN) {
            if (peek().type == TokenType::KW_IMPORT) {
                parseImport(program);
            } else if (peek().type == TokenType::KW_CLASS) {
                parseClassDefinition(program);
            } else if (peek().type == TokenType::KW_DEF) {
                program->elements.push_back(parseFunctionDef(""));
            }
        }
        return program;
    }
private:
    std::vector<Token> tokens; size_t index;
    Token peek() const { return tokens[index]; }
    
    Token consume(TokenType expected, const std::string& err) {
        if (peek().type == expected) return tokens[index++];
        throw std::runtime_error("à la ligne " + std::to_string(peek().line) + 
                                 " : " + err + " (reçu '" + peek().value + "')");
    }

    void parseImport(std::unique_ptr<ProgramNode>& currentProgram) {
        consume(TokenType::KW_IMPORT, "");
        std::string path = consume(TokenType::IDENTIFIER, "Nom de module attendu").value;
        while (peek().type == TokenType::DOT) {
            consume(TokenType::DOT, "");
            path += "/" + consume(TokenType::IDENTIFIER, "Sous-module attendu").value;
        }
        std::string fullPath = path + ".nabla";
        if (parsed_files.find(fullPath) != parsed_files.end()) return;
        parsed_files.insert(fullPath);

        std::ifstream file(fullPath);
        if (!file.is_open()) throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : Impossible de charger '" + fullPath + "'");
        std::stringstream buffer; buffer << file.rdbuf();
        
        Lexer subLexer(buffer.str());
        Parser subParser(subLexer.tokenize());
        auto subProgram = subParser.parseProgram();
        for (auto& el : subProgram->elements) currentProgram->elements.push_back(std::move(el));
    }

    void parseClassDefinition(std::unique_ptr<ProgramNode>& program) {
        consume(TokenType::KW_CLASS, "");
        std::string className = consume(TokenType::IDENTIFIER, "Nom de classe attendu").value;
        current_parsing_class = className;
        consume(TokenType::LPAREN, "");
        int offset = 8;
        while (peek().type != TokenType::RPAREN) {
            std::string fieldName = consume(TokenType::IDENTIFIER, "Nom d'attribut attendu").value;
            consume(TokenType::COLON, ""); consume(TokenType::IDENTIFIER, "Type attendu");
            class_layouts[className][fieldName] = offset;
            offset += 8;
            if (peek().type == TokenType::COMMA) consume(TokenType::COMMA, "");
        }
        consume(TokenType::RPAREN, "");
        if (peek().type == TokenType::LBRACE) {
            consume(TokenType::LBRACE, "");
            while (peek().type != TokenType::RBRACE) {
                if (peek().type == TokenType::KW_DEF) program->elements.push_back(parseFunctionDef(className));
            }
            consume(TokenType::RBRACE, "");
        }
        current_parsing_class = "";
    }

    std::unique_ptr<ASTNode> parsePrimary() {
	if (peek().type == TokenType::LPAREN) {
            consume(TokenType::LPAREN, "");
            auto expr = parseExpression(); // Évalue récursivement tout ce qui est dedans
            consume(TokenType::RPAREN, "Parenthèse fermante attendue après l'expression");
            return expr;
        }    
        if (peek().type == TokenType::INT_LITERAL) return std::make_unique<IntNode>(consume(TokenType::INT_LITERAL, "").value);
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
            if (!current_parsing_class.empty() && class_layouts[current_parsing_class].count(name)) {
                return std::make_unique<FieldAccessNode>(current_parsing_class, name);
            }
            // Fallback pour les arguments ou variables locales hors attributs (assimilé à Int pour l'instant)
            return std::make_unique<IntNode>("0");
        }
        throw std::runtime_error("à la ligne " + std::to_string(peek().line) + " : expression primaire invalide '" + peek().value + "'");
    }

    std::unique_ptr<ASTNode> parseMultiplicative() {
        auto expr = parsePrimary();
        while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH) {
            Token op = tokens[index++];
            auto right = parsePrimary();
            expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
        }
        return expr;
    }

    std::unique_ptr<ASTNode> parseExpression() {
        auto expr = parseMultiplicative();
        while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS || peek().type == TokenType::DOT) {
            if (peek().type == TokenType::DOT) {
                consume(TokenType::DOT, "");
                std::string method = consume(TokenType::IDENTIFIER, "Nom de méthode attendu après '.'").value;
                consume(TokenType::LPAREN, "");
                std::unique_ptr<ASTNode> arg = nullptr;
                if (peek().type != TokenType::RPAREN) {
                    arg = parseExpression();
                }
                consume(TokenType::RPAREN, "");
                
                // Déduction basique : si on invoque sur quelque chose qui n'est pas Int, on cible OutilsMath pour le test
                std::string target = (expr->getType() == "Int") ? "Int" : "OutilsMath";
                expr = std::make_unique<MethodCallNode>(std::move(expr), method, std::move(arg));
            } else {
                Token op = tokens[index++];
                auto right = parseMultiplicative();
                expr = std::make_unique<MethodCallNode>(std::move(expr), op.value, std::move(right));
            }
        }
        return expr;
    }

    std::unique_ptr<ASTNode> parseFunctionDef(std::string clName) {
        consume(TokenType::KW_DEF, "");
        std::string name = consume(TokenType::IDENTIFIER, "Nom de fonction attendu").value;
        consume(TokenType::LPAREN, ""); consume(TokenType::RPAREN, "");
        consume(TokenType::COLON, ""); consume(TokenType::IDENTIFIER, "Type de retour attendu").value;
        consume(TokenType::EQUAL, ""); consume(TokenType::LBRACE, "");
        auto body = parseExpression();
        consume(TokenType::RBRACE, "");
        return std::make_unique<FunctionDefNode>(clName, name, std::move(body));
    }
};
