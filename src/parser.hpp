#pragma once
#include "compiler_context.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
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
    struct ParsedSymbol {
        std::string internalName;
        std::string type;
        bool isMutable;
    };
    struct CapturedSymbol {
        std::string name;
        std::string symbolName;
        std::string type;
    };
    struct LambdaCaptureScope {
        size_t outerScopeCount;
        std::map<std::string, CapturedSymbol> capturesBySymbol;
    };
    std::vector<std::map<std::string, ParsedSymbol>> localScopes;
    std::vector<LambdaCaptureScope> lambdaCaptureScopes;
    std::vector<std::unique_ptr<ASTNode>> generatedFunctions;
    std::vector<std::string> currentFunctionTypeParameters;
    int nextSymbolId = 0;

    Token peek() const;
    Token consume(TokenType expected, const std::string& err);

    void parseImport(std::unique_ptr<ProgramNode>& currentProgram);
    void parseClassDefinition(std::unique_ptr<ProgramNode>& program, bool isTrait = false);
    void parseObjectDefinition(std::unique_ptr<ProgramNode>& program);
    std::unique_ptr<ASTNode> parseIfExpression();
    std::unique_ptr<ASTNode> parseMatchExpression();
    std::unique_ptr<ASTNode> parseWhileExpression();
    std::unique_ptr<ASTNode> parseForExpression();
    bool startsLambdaExpression() const;
    bool startsInferredLambdaExpression() const;
    bool skipTypeAt(size_t& cursor) const;
    std::unique_ptr<ASTNode> parseLambdaExpression();
    std::unique_ptr<ASTNode> parseInferredLambdaExpression(const std::string& expectedType);
    std::string inferLambdaReturnType(
        ASTNode& body, const std::vector<FunctionDefNode::Parameter>& parameters,
        const std::vector<FunctionDefNode::Capture>& captures);
    void requireTupleModule();
    std::unique_ptr<ASTNode> parseFunctionReferenceWithExpectedType(const std::string& expectedType);
    std::unique_ptr<ASTNode> parsePrimary();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseMultiplicative();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseComparison();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseTupleArrow();
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseFunctionDef(
        std::string clName, std::string objectName = "", bool allowAbstractMethod = false);
    std::vector<std::unique_ptr<ASTNode>> parseArguments(
        const std::vector<std::string>& expectedTypes = {});
    std::vector<std::unique_ptr<ASTNode>> parseFunctionCallArguments(
        const CompilerContext::FunctionSignature& signature,
        const std::vector<std::string>& typeArguments,
        std::map<std::string, std::string> initialSubstitution = {});
    std::unique_ptr<ASTNode> parseArgument(const std::string& expectedType);
    std::vector<std::string> expectedArgumentTypesForMethodCall(
        const std::string& receiverType, const std::string& methodName,
        const SourceLocation& location) const;
    std::optional<ClassMethodLookupResult> selectedOverloadedMethodCallForArgumentShape(
        const std::string& receiverType, const std::string& methodName,
        const std::vector<std::string>& typeArguments, size_t callParenIndex,
        const SourceLocation& location) const;
    bool startsInferredLambdaExpressionAt(size_t cursor) const;
    std::optional<std::vector<bool>> argumentLambdaShapeAtCall(size_t callParenIndex) const;
    std::pair<std::string, SourceLocation> parseType(const std::string& expectedMessage);
    const ParsedSymbol* findLocal(const std::string& name) const;
    std::pair<const ParsedSymbol*, size_t> findLocalWithScope(const std::string& name) const;
    void captureIfNeeded(const std::string& name, const ParsedSymbol& symbol, size_t scopeIndex);

    template<typename T>
    std::unique_ptr<T> located(std::unique_ptr<T> node, const SourceLocation& location) {
        node->setLocation(location);
        return node;
    }
};
