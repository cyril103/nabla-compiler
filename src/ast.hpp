#pragma once
#include "compiler_context.hpp"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <map>

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void generateASM(std::ofstream& out, CompilerContext& context) = 0;
    virtual std::string getType() = 0;
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class IntNode : public ASTNode {
    std::string value;
public:
    IntNode(std::string val);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class NewNode : public ASTNode {
    std::string className;
    std::vector<std::unique_ptr<ASTNode>> args;
public:
    NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class MethodCallNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string methodName;
    std::unique_ptr<ASTNode> argument;
public:
    MethodCallNode(std::unique_ptr<ASTNode> rec, std::string method, std::unique_ptr<ASTNode> arg);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class FieldAccessNode : public ASTNode {
    std::string className;
    std::string fieldName;
public:
    FieldAccessNode(std::string clName, std::string field);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class IfNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBranch;
    std::unique_ptr<ASTNode> elseBranch;
public:
    IfNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> thenBranch, std::unique_ptr<ASTNode> elseBranch);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> expressions;
public:
    BlockNode(std::vector<std::unique_ptr<ASTNode>> exprs);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;
public:
    WhileNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> body);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class ForNode : public ASTNode {
    std::unique_ptr<ASTNode> count;
    std::unique_ptr<ASTNode> body;
public:
    ForNode(std::unique_ptr<ASTNode> count, std::unique_ptr<ASTNode> body);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class FunctionDefNode : public ASTNode {
    std::string className;
    std::string name;
    std::unique_ptr<ASTNode> body;
public:
    FunctionDefNode(std::string clName, std::string name, std::unique_ptr<ASTNode> body);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class IdentifierNode : public ASTNode {
    std::string name;
public:
    IdentifierNode(std::string n);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
    const std::string& getName() const { return name; }
};

class VarDeclNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> initializer;
    bool isMutable;
public:
    VarDeclNode(std::string n, std::unique_ptr<ASTNode> init, bool mut);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};

class AssignmentNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> value;
public:
    AssignmentNode(std::string n, std::unique_ptr<ASTNode> v);
    std::string getType() override;
    void generateASM(std::ofstream& out, CompilerContext& context) override;
};
