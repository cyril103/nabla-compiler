#pragma once

#include "compiler_error.hpp"
#include <cctype>
#include <string>
#include <vector>

enum class TokenType {
    KW_DEF, KW_CLASS, KW_NEW, KW_IMPORT, KW_IF, KW_ELSE, KW_MATCH, KW_WHILE, KW_FOR, KW_VAL, KW_VAR, KW_THIS, KW_SUPER, KW_OVERRIDE,
    KW_EXTENDS, KW_WITH,
    KW_TRUE, KW_FALSE,
    IDENTIFIER, LPAREN, RPAREN, LBRACKET, RBRACKET, COLON, EQUAL, LBRACE, RBRACE, COMMA, DOT,
    INT_LITERAL, LONG_LITERAL, FLOAT_LITERAL, DOUBLE_LITERAL, STRING_LITERAL, CHAR_LITERAL,
    PLUS, MINUS, STAR, SLASH, PERCENT, BANG, AND_AND, OR_OR, FAT_ARROW, EQEQ, NEQ, LT, GT, LTE, GTE, EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string value;
    SourceLocation location;
};

class Lexer {
public:
    Lexer(const std::string& source, std::string file)
        : src(source), file(std::move(file)), index(0), currentLine(1), currentColumn(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (index < src.size()) {
            char current = src[index];

            if (current == '\n') {
                advance();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(current))) {
                advance();
                continue;
            }

            SourceLocation start = location();
            if (current == '(') { add(tokens, TokenType::LPAREN, "(", start, 1); continue; }
            if (current == ')') { add(tokens, TokenType::RPAREN, ")", start, 1); continue; }
            if (current == '[') { add(tokens, TokenType::LBRACKET, "[", start, 1); continue; }
            if (current == ']') { add(tokens, TokenType::RBRACKET, "]", start, 1); continue; }
            if (current == ':') { add(tokens, TokenType::COLON, ":", start, 1); continue; }
            if (current == '{') { add(tokens, TokenType::LBRACE, "{", start, 1); continue; }
            if (current == '}') { add(tokens, TokenType::RBRACE, "}", start, 1); continue; }
            if (current == ',') { add(tokens, TokenType::COMMA, ",", start, 1); continue; }
            if (current == '.') { add(tokens, TokenType::DOT, ".", start, 1); continue; }
            if (current == '+') { add(tokens, TokenType::PLUS, "+", start, 1); continue; }
            if (current == '-') { add(tokens, TokenType::MINUS, "-", start, 1); continue; }
            if (current == '*') { add(tokens, TokenType::STAR, "*", start, 1); continue; }
            if (current == '/') {
                if (nextIs('/')) {
                    while (index < src.size() && src[index] != '\n') advance();
                    continue;
                }
                add(tokens, TokenType::SLASH, "/", start, 1);
                continue;
            }
            if (current == '%') { add(tokens, TokenType::PERCENT, "%", start, 1); continue; }
            if (current == '&') {
                if (nextIs('&')) {
                    add(tokens, TokenType::AND_AND, "&&", start, 2);
                    continue;
                }
                throw CompilerError(ErrorKind::Lexer, start, "caractère inattendu '&'");
            }
            if (current == '|') {
                if (nextIs('|')) {
                    add(tokens, TokenType::OR_OR, "||", start, 2);
                    continue;
                }
                throw CompilerError(ErrorKind::Lexer, start, "caractère inattendu '|'");
            }
            if (current == '=') {
                if (nextIs('>')) add(tokens, TokenType::FAT_ARROW, "=>", start, 2);
                else if (nextIs('=')) add(tokens, TokenType::EQEQ, "==", start, 2);
                else add(tokens, TokenType::EQUAL, "=", start, 1);
                continue;
            }
            if (current == '!') {
                if (nextIs('=')) {
                    add(tokens, TokenType::NEQ, "!=", start, 2);
                    continue;
                }
                add(tokens, TokenType::BANG, "!", start, 1);
                continue;
            }
            if (current == '<') {
                if (nextIs('=')) add(tokens, TokenType::LTE, "<=", start, 2);
                else add(tokens, TokenType::LT, "<", start, 1);
                continue;
            }
            if (current == '>') {
                if (nextIs('=')) add(tokens, TokenType::GTE, ">=", start, 2);
                else add(tokens, TokenType::GT, ">", start, 1);
                continue;
            }
            if (current == '"') {
                advance();
                std::string value;
                while (index < src.size() && src[index] != '"') {
                    if (src[index] == '\n') {
                        throw CompilerError(ErrorKind::Lexer, start, "chaîne de caractères non terminée");
                    }
                    if (src[index] == '\\') {
                        advance();
                        if (index >= src.size()) {
                            throw CompilerError(ErrorKind::Lexer, start, "chaîne de caractères non terminée");
                        }
                        char escaped = src[index];
                        if (escaped == 'n') value += '\n';
                        else if (escaped == '"') value += '"';
                        else if (escaped == '\\') value += '\\';
                        else {
                            throw CompilerError(
                                ErrorKind::Lexer, location(),
                                "échappement inconnu '\\" + std::string(1, escaped) + "'");
                        }
                        advance();
                        continue;
                    }
                    value += src[index];
                    advance();
                }
                if (index >= src.size()) {
                    throw CompilerError(ErrorKind::Lexer, start, "chaîne de caractères non terminée");
                }
                advance();
                tokens.push_back({TokenType::STRING_LITERAL, value, start});
                continue;
            }
            if (current == '\'') {
                advance();
                if (index >= src.size() || src[index] == '\n' || src[index] == '\'') {
                    throw CompilerError(ErrorKind::Lexer, start, "caractère littéral invalide");
                }
                unsigned char value = 0;
                if (src[index] == '\\') {
                    advance();
                    if (index >= src.size()) {
                        throw CompilerError(ErrorKind::Lexer, start, "caractère littéral non terminé");
                    }
                    char escaped = src[index];
                    if (escaped == 'n') value = '\n';
                    else if (escaped == '\'') value = '\'';
                    else if (escaped == '\\') value = '\\';
                    else {
                        throw CompilerError(
                            ErrorKind::Lexer, location(),
                            "échappement inconnu '\\" + std::string(1, escaped) + "'");
                    }
                    advance();
                } else {
                    value = static_cast<unsigned char>(src[index]);
                    advance();
                }
                if (index >= src.size() || src[index] != '\'') {
                    throw CompilerError(ErrorKind::Lexer, start, "caractère littéral non terminé");
                }
                advance();
                tokens.push_back({TokenType::CHAR_LITERAL, std::to_string(static_cast<int>(value)), start});
                continue;
            }

            if (isIdentifierStart(current)) {
                std::string ident;
                while (index < src.size() && isIdentifierPart(src[index])) {
                    ident += src[index];
                    advance();
                }
                TokenType type = TokenType::IDENTIFIER;
                if (ident == "def") type = TokenType::KW_DEF;
                else if (ident == "class") type = TokenType::KW_CLASS;
                else if (ident == "new") type = TokenType::KW_NEW;
                else if (ident == "import") type = TokenType::KW_IMPORT;
                else if (ident == "if") type = TokenType::KW_IF;
                else if (ident == "else") type = TokenType::KW_ELSE;
                else if (ident == "match") type = TokenType::KW_MATCH;
                else if (ident == "while") type = TokenType::KW_WHILE;
                else if (ident == "for") type = TokenType::KW_FOR;
                else if (ident == "extends") type = TokenType::KW_EXTENDS;
                else if (ident == "with") type = TokenType::KW_WITH;
                else if (ident == "val") type = TokenType::KW_VAL;
                else if (ident == "var") type = TokenType::KW_VAR;
                else if (ident == "this") type = TokenType::KW_THIS;
                else if (ident == "super") type = TokenType::KW_SUPER;
                else if (ident == "override") type = TokenType::KW_OVERRIDE;
                else if (ident == "true") type = TokenType::KW_TRUE;
                else if (ident == "false") type = TokenType::KW_FALSE;
                tokens.push_back({type, ident, start});
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(current))) {
                std::string number;
                while (index < src.size() && std::isdigit(static_cast<unsigned char>(src[index]))) {
                    number += src[index];
                    advance();
                }
                if (index < src.size() && src[index] == '.' &&
                    index + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[index + 1]))) {
                    number += src[index];
                    advance();
                    while (index < src.size() && std::isdigit(static_cast<unsigned char>(src[index]))) {
                        number += src[index];
                        advance();
                    }
                    if (index < src.size() && (src[index] == 'F' || src[index] == 'f')) {
                        advance();
                        tokens.push_back({TokenType::FLOAT_LITERAL, number, start});
                        continue;
                    }
                    tokens.push_back({TokenType::DOUBLE_LITERAL, number, start});
                    continue;
                }
                if (index < src.size() && (src[index] == 'L' || src[index] == 'l')) {
                    advance();
                    tokens.push_back({TokenType::LONG_LITERAL, number, start});
                    continue;
                }
                tokens.push_back({TokenType::INT_LITERAL, number, start});
                continue;
            }

            throw CompilerError(
                ErrorKind::Lexer, start, "caractère inattendu '" + std::string(1, current) + "'");
        }
        tokens.push_back({TokenType::EOF_TOKEN, "", location()});
        return tokens;
    }

private:
    std::string src;
    std::string file;
    size_t index;
    int currentLine;
    int currentColumn;

    SourceLocation location() const {
        return {file, currentLine, currentColumn};
    }

    bool nextIs(char expected) const {
        return index + 1 < src.size() && src[index + 1] == expected;
    }

    bool isIdentifierStart(char c) const {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool isIdentifierPart(char c) const {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    void advance() {
        if (src[index] == '\n') {
            currentLine++;
            currentColumn = 1;
        } else {
            currentColumn++;
        }
        index++;
    }

    void add(
        std::vector<Token>& tokens, TokenType type, const std::string& value,
        const SourceLocation& start, int length) {
        tokens.push_back({type, value, start});
        for (int i = 0; i < length; ++i) advance();
    }
};
