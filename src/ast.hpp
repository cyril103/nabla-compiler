#pragma once
#include "compiler_context.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

class IRBuilder;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::string getType() = 0;
    virtual void validateSemantics(CompilerContext& context) = 0;
    virtual std::string lowerToIR(IRBuilder& builder) const;
    void setLocation(SourceLocation sourceLocation) { location = std::move(sourceLocation); }
    const SourceLocation& getLocation() const { return location; }

protected:
    SourceLocation location;
    [[noreturn]] void semanticError(const std::string& message) const {
        throw CompilerError(ErrorKind::Semantic, location, message);
    }
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class IntNode : public ASTNode {
    std::string value;
public:
    IntNode(std::string val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class LongNode : public ASTNode {
    std::string value;
public:
    LongNode(std::string val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class DoubleNode : public ASTNode {
    std::string value;
public:
    DoubleNode(std::string val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FloatNode : public ASTNode {
    std::string value;
public:
    FloatNode(std::string val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class BoolNode : public ASTNode {
    bool value;
public:
    BoolNode(bool val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class NotNode : public ASTNode {
    std::unique_ptr<ASTNode> expression;
public:
    NotNode(std::unique_ptr<ASTNode> expr);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class LogicalNode : public ASTNode {
    std::string operation;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
public:
    LogicalNode(std::string op, std::unique_ptr<ASTNode> leftExpr, std::unique_ptr<ASTNode> rightExpr);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class StringNode : public ASTNode {
    std::string value;
public:
    StringNode(std::string val);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class NewNode : public ASTNode {
    std::string className;
    std::vector<std::unique_ptr<ASTNode>> args;
public:
    NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class MethodCallNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string methodName;
    std::vector<std::unique_ptr<ASTNode>> arguments;
    std::vector<std::string> typeArguments;
    std::vector<std::string> resolvedTypeArguments;
    std::string resolvedType = "Int";
    std::string resolvedOwnerType;
public:
    MethodCallNode(
        std::unique_ptr<ASTNode> rec, std::string method, std::vector<std::unique_ptr<ASTNode>> args,
        std::vector<std::string> genericTypeArguments = {},
        std::string initialResolvedType = "Int", std::string initialOwnerType = "");
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FunctionCallNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> arguments;
    std::vector<std::string> typeArguments;
    std::vector<std::string> resolvedTypeArguments;
    std::string resolvedType = "Int";
public:
    FunctionCallNode(
        std::string functionName, std::vector<std::unique_ptr<ASTNode>> args,
        std::vector<std::string> genericTypeArguments = {},
        std::string initialResolvedType = "Int");
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FunctionReferenceNode : public ASTNode {
public:
    struct Capture {
        std::string name;
        std::string symbolName;
        std::string type;
    };

private:
    std::string name;
    std::string resolvedType;
    std::vector<std::string> typeArguments;
    std::vector<std::string> resolvedTypeArguments;
    std::vector<Capture> captures;
public:
    FunctionReferenceNode(
        std::string functionName, std::string functionType,
        std::vector<std::string> genericTypeArguments = {},
        std::vector<Capture> capturedValues = {});
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FunctionValueCallNode : public ASTNode {
    std::string name;
    std::string symbolName;
    std::vector<std::unique_ptr<ASTNode>> arguments;
    std::string resolvedType = "Int";
public:
    FunctionValueCallNode(
        std::string functionName, std::string symbol, std::vector<std::unique_ptr<ASTNode>> args,
        std::string initialResolvedType = "Int");
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FieldAccessNode : public ASTNode {
    std::string className;
    std::string fieldName;
    std::string type;
public:
    FieldAccessNode(std::string clName, std::string field, std::string fieldType);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class IfNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBranch;
    std::unique_ptr<ASTNode> elseBranch;
    std::string resolvedType = "Int";
public:
    IfNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> thenBranch, std::unique_ptr<ASTNode> elseBranch);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> expressions;
public:
    BlockNode(std::vector<std::unique_ptr<ASTNode>> exprs);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;
public:
    WhileNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> body);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class ForNode : public ASTNode {
    std::unique_ptr<ASTNode> count;
    std::unique_ptr<ASTNode> body;
public:
    ForNode(std::unique_ptr<ASTNode> count, std::unique_ptr<ASTNode> body);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class FunctionDefNode : public ASTNode {
public:
    struct Parameter {
        std::string name;
        std::string symbolName;
        std::string type;
    };
    struct Capture {
        std::string name;
        std::string symbolName;
        std::string type;
    };

private:
    std::string className;
    std::string name;
    std::string returnType;
    std::vector<std::string> typeParameters;
    std::vector<std::string> ownerTypeParameters;
    std::vector<Parameter> parameters;
    std::unique_ptr<ASTNode> body;
    std::vector<Capture> captures;
public:
    FunctionDefNode(
        std::string clName, std::string name, std::string declaredReturnType,
        std::vector<std::string> genericTypeParameters,
        std::vector<Parameter> params, std::unique_ptr<ASTNode> body,
        std::vector<Capture> capturedValues = {},
        std::vector<std::string> genericOwnerTypeParameters = {});
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
    std::string lowerSpecializedMethodToIR(
        IRBuilder& builder, const std::string& concreteClassName,
        const std::vector<std::string>& concreteMethodTypeArguments) const;
    std::string lowerSpecializedFunctionToIR(
        IRBuilder& builder, const std::vector<std::string>& concreteTypeArguments) const;
    const std::string& getClassName() const { return className; }
    const std::string& getName() const { return name; }
};

class IdentifierNode : public ASTNode {
    std::string name;
    std::string symbolName;
    std::string type;
public:
    IdentifierNode(std::string n, std::string symbol, std::string resolvedType);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
    const std::string& getName() const { return name; }
};

class VarDeclNode : public ASTNode {
    std::string name;
    std::string symbolName;
    std::unique_ptr<ASTNode> initializer;
    bool isMutable;
public:
    VarDeclNode(std::string n, std::string symbol, std::unique_ptr<ASTNode> init, bool mut);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};

class AssignmentNode : public ASTNode {
    std::string name;
    std::string symbolName;
    std::string targetType;
    bool targetMutable;
    std::unique_ptr<ASTNode> value;
public:
    AssignmentNode(std::string n, std::string symbol, std::string type, bool isMutable, std::unique_ptr<ASTNode> v);
    std::string getType() override;
    void validateSemantics(CompilerContext& context) override;
    std::string lowerToIR(IRBuilder& builder) const override;
};
