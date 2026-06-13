#pragma once
#include "compiler_context.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Parser {
public:
    Parser(const std::vector<Token>& tokens, CompilerContext& ctx, const std::filesystem::path& currentFile);
    std::unique_ptr<ProgramNode> parseProgram();

private:
    std::vector<Token> tokens;
    size_t index;
    CompilerContext& context;
    std::filesystem::path currentFile;
    std::string currentParsingClass;

    Token peek() const;
    Token consume(TokenType expected, const std::string& err);

    void parseImport(std::unique_ptr<ProgramNode>& currentProgram);
    void parseClassDefinition(std::unique_ptr<ProgramNode>& program);
    std::unique_ptr<ASTNode> parseIfExpression();
    std::unique_ptr<ASTNode> parseWhileExpression();
    std::unique_ptr<ASTNode> parseForExpression();
    std::unique_ptr<ASTNode> parsePrimary();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseMultiplicative();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseComparison();
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseFunctionDef(std::string clName);
};
