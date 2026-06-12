#pragma once
#include <string>
#include <vector>
#include <cctype>

enum class TokenType {
    KW_DEF, KW_CLASS, KW_NEW, KW_IMPORT, IDENTIFIER, LPAREN, RPAREN, COLON, EQUAL, LBRACE, RBRACE, 
    COMMA, DOT, INT_LITERAL, PLUS, MINUS, STAR, SLASH, EOF_TOKEN
};

struct Token { 
    TokenType type; 
    std::string value; 
    int line; 
};

class Lexer {
public:
    Lexer(const std::string& source) : src(source), index(0), current_line(1) {}
    
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (index < src.size()) {
            char current = src[index];
            
            if (current == '\n') { 
                current_line++; 
                index++; 
                continue; 
            }
            if (isspace(current)) { index++; continue; }
            
            if (current == '(') { tokens.push_back({TokenType::LPAREN, "(", current_line}); index++; continue; }
            if (current == ')') { tokens.push_back({TokenType::RPAREN, ")", current_line}); index++; continue; }
            if (current == ':') { tokens.push_back({TokenType::COLON, ":", current_line});  index++; continue; }
            if (current == '=') { tokens.push_back({TokenType::EQUAL, "=", current_line});  index++; continue; }
            if (current == '{') { tokens.push_back({TokenType::LBRACE, "{", current_line}); index++; continue; }
            if (current == '}') { tokens.push_back({TokenType::RBRACE, "}", current_line}); index++; continue; }
            if (current == ',') { tokens.push_back({TokenType::COMMA, ",", current_line});  index++; continue; }
            if (current == '.') { tokens.push_back({TokenType::DOT, ".", current_line});    index++; continue; }
            if (current == '+') { tokens.push_back({TokenType::PLUS, "+", current_line});   index++; continue; }
            if (current == '-') { tokens.push_back({TokenType::MINUS, "-", current_line});  index++; continue; }
            if (current == '*') { tokens.push_back({TokenType::STAR, "*", current_line});   index++; continue; }
            if (current == '/') { tokens.push_back({TokenType::SLASH, "/", current_line});  index++; continue; }

            if (isalpha(current)) {
                std::string ident;
                while (index < src.size() && isalnum(src[index])) { ident += src[index++]; }
                TokenType t = TokenType::IDENTIFIER;
                if (ident == "def") t = TokenType::KW_DEF;
                else if (ident == "class") t = TokenType::KW_CLASS;
                else if (ident == "new") t = TokenType::KW_NEW;
                else if (ident == "import") t = TokenType::KW_IMPORT;
                
                tokens.push_back({t, ident, current_line});
                continue;
            }
            if (isdigit(current)) {
                std::string num;
                while (index < src.size() && isdigit(src[index])) { num += src[index++]; }
                tokens.push_back({TokenType::INT_LITERAL, num, current_line}); 
                continue;
            }
            index++;
        }
        tokens.push_back({TokenType::EOF_TOKEN, "", current_line}); 
        return tokens;
    }
private:
    std::string src; 
    size_t index;
    int current_line;
};
