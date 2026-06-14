#pragma once

#include "compiler_error.hpp"
#include <map>
#include <string>
#include <vector>

enum class IROpcode {
    Constant,
    Binary,
    Call,
    Load,
    Store,
    Label,
    BranchIfFalse,
    Jump,
    Phi,
    Return
};

struct IRInstruction {
    IROpcode opcode;
    std::string result;
    std::string operation;
    std::vector<std::string> operands;
};

struct IRParameter {
    std::string name;
    std::string type;
};

struct IRFunction {
    std::string name;
    std::vector<IRParameter> parameters;
    std::string returnType;
    std::vector<IRInstruction> instructions;
};

struct IRProgram {
    std::vector<IRFunction> functions;
    std::string format() const;
};

class IRBuilder {
public:
    IRProgram build(const class ProgramNode& program);

    void beginFunction(
        const std::string& name, const std::vector<IRParameter>& parameters,
        const std::string& returnType);
    void endFunction(const std::string& returnValue);
    std::string emitConstant(const std::string& value);
    std::string emitBinary(
        const std::string& operation, const std::string& left, const std::string& right);
    std::string emitCall(const std::string& name, const std::vector<std::string>& arguments);
    std::string emitLoad(const std::string& symbol);
    void emitStore(const std::string& symbol, const std::string& value);
    std::string emitPhi(const std::string& left, const std::string& right);
    std::string makeLabel(const std::string& prefix);
    std::string makeTemporarySymbol(const std::string& prefix);
    void emitLabel(const std::string& label);
    void emitBranchIfFalse(const std::string& condition, const std::string& targetLabel);
    void emitJump(const std::string& targetLabel);
    void bindParameter(const std::string& symbol, const std::string& parameterName);
    [[noreturn]] void unsupported(const SourceLocation& location, const std::string& feature) const;

private:
    IRProgram program;
    IRFunction* currentFunction = nullptr;
    std::map<std::string, std::string> parameterValues;
    int nextValueId = 0;
    int nextLabelId = 0;
    int nextTemporarySymbolId = 0;

    std::string nextValue();
    void emit(IRInstruction instruction);
};
