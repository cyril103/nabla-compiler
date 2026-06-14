#pragma once

#include "compiler_error.hpp"
#include <map>
#include <string>
#include <vector>

enum class IROpcode {
    Constant,
    StringLiteral,
    Binary,
    Call,
    FunctionReference,
    ClosureLoad,
    IndirectCall,
    MethodCall,
    NewObject,
    NewIntArray,
    IntArrayLength,
    IntArrayGet,
    IntArraySet,
    FieldLoad,
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
    std::string type;
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
    std::string emitConstant(const std::string& value, const std::string& type = "Int");
    std::string emitStringLiteral(const std::string& value);
    std::string emitBinary(
        const std::string& operation, const std::string& left, const std::string& right,
        const std::string& type = "Int");
    std::string emitCall(
        const std::string& name, const std::vector<std::string>& arguments,
        const std::string& type = "Int");
    std::string emitFunctionReference(
        const std::string& name, const std::vector<std::string>& captures = {});
    std::string emitClosureLoad(const std::string& closure, int captureIndex);
    std::string emitIndirectCall(
        const std::string& callee, const std::vector<std::string>& arguments,
        const std::string& type = "Int");
    std::string emitMethodCall(
        const std::string& className, const std::string& methodName, const std::string& receiver,
        const std::vector<std::string>& arguments, const std::string& type = "Int");
    void registerMethodSpecialization(
        const std::string& concreteClassName, const std::string& templateClassName,
        const std::string& methodName, const std::vector<std::string>& methodTypeArguments,
        const std::vector<std::string>& argumentTypes, const std::string& returnType);
    void registerFunctionSpecialization(
        const std::string& functionName, const std::vector<std::string>& typeArguments,
        const std::string& returnType);
    std::string emitNewObject(
        const std::string& className, const std::vector<std::string>& arguments,
        const std::string& resultType = "");
    std::string emitNewIntArray(const std::string& size);
    std::string emitIntArrayLength(const std::string& receiver);
    std::string emitIntArrayGet(const std::string& receiver, const std::string& index);
    std::string emitIntArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitFieldLoad(
        const SourceLocation& location, const std::string& className, const std::string& fieldName,
        const std::string& type);
    std::string emitLoad(const std::string& symbol, const std::string& type = "Int");
    void emitStore(const std::string& symbol, const std::string& value, const std::string& type = "Int");
    std::string emitPhi(const std::string& left, const std::string& right, const std::string& type = "Int");
    void pushTypeSubstitution(const std::map<std::string, std::string>& substitution);
    void popTypeSubstitution();
    std::string substituteActiveType(const std::string& type) const;
    std::string makeLabel(const std::string& prefix);
    std::string makeTemporarySymbol(const std::string& prefix);
    void emitLabel(const std::string& label);
    void emitBranchIfFalse(const std::string& condition, const std::string& targetLabel);
    void emitJump(const std::string& targetLabel);
    void bindParameter(const std::string& symbol, const std::string& parameterName);
    void bindThis();
    void bindClosure();
    void bindCapture(const std::string& symbol, int captureIndex);
    [[noreturn]] void unsupported(const SourceLocation& location, const std::string& feature) const;

private:
    IRProgram program;
    IRFunction* currentFunction = nullptr;
    std::map<std::string, std::string> parameterValues;
    std::map<std::string, int> captureValues;
    std::vector<std::map<std::string, std::string>> typeSubstitutionStack;
    std::map<std::string, std::string> activeTypeSubstitution;
    std::string thisValue;
    std::string closureValue;
    int nextValueId = 0;
    int nextLabelId = 0;
    int nextTemporarySymbolId = 0;
    struct MethodSpecialization {
        std::string concreteClassName;
        std::string templateClassName;
        std::string methodName;
        std::vector<std::string> methodTypeArguments;
        std::vector<std::string> argumentTypes;
        std::string returnType;
    };
    struct FunctionSpecialization {
        std::string functionName;
        std::vector<std::string> typeArguments;
        std::string returnType;
    };
    std::vector<MethodSpecialization> methodSpecializations;
    std::vector<FunctionSpecialization> functionSpecializations;

    std::string nextValue();
    void emit(IRInstruction instruction);
    void emitPendingSpecializations(const class ProgramNode& root);
    void emitMethodSpecialization(const MethodSpecialization& specialization, const class ProgramNode& root);
    void emitFunctionSpecialization(const FunctionSpecialization& specialization, const class ProgramNode& root);
};
