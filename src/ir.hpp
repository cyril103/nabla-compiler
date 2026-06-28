#pragma once
#include "compiler_error.hpp"
#include <map>
#include <set>
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
    SingletonObjectRef,
    MethodCall,
    StaticMethodCall,
    NewObject,
    NewIntArray,
    IntArrayLength,
    IntArrayGet,
    IntArraySet,
    NewLongArray,
    LongArrayLength,
    LongArrayGet,
    LongArraySet,
    NewFloatArray,
    FloatArrayLength,
    FloatArrayGet,
    FloatArraySet,
    NewDoubleArray,
    DoubleArrayLength,
    DoubleArrayGet,
    DoubleArraySet,
    NewBoolArray,
    BoolArrayLength,
    BoolArrayGet,
    BoolArraySet,
    NewObjectArray,
    ObjectArrayLength,
    ObjectArrayGet,
    ObjectArraySet,
    FieldLoad,
    FieldStore,
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
    IRProgram build(const class ProgramNode& program, const class CompilerContext& context);

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
    std::string emitClosureLoad(const std::string& closure, int captureIndex, const std::string& type);
    std::string emitIndirectCall(
        const std::string& callee, const std::vector<std::string>& arguments,
        const std::string& type = "Int");
    std::string emitSingletonObjectRef(const std::string& objectName);
    std::string emitMethodCall(
        const std::string& className, const std::string& methodName, const std::string& receiver,
        const std::vector<std::string>& arguments, const std::string& type = "Int");
    std::string emitStaticMethodCall(
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
    std::string emitNewLongArray(const std::string& size);
    std::string emitLongArrayLength(const std::string& receiver);
    std::string emitLongArrayGet(const std::string& receiver, const std::string& index);
    std::string emitLongArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitNewFloatArray(const std::string& size);
    std::string emitFloatArrayLength(const std::string& receiver);
    std::string emitFloatArrayGet(const std::string& receiver, const std::string& index);
    std::string emitFloatArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitNewDoubleArray(const std::string& size);
    std::string emitDoubleArrayLength(const std::string& receiver);
    std::string emitDoubleArrayGet(const std::string& receiver, const std::string& index);
    std::string emitDoubleArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitNewBoolArray(const std::string& size);
    std::string emitBoolArrayLength(const std::string& receiver);
    std::string emitBoolArrayGet(const std::string& receiver, const std::string& index);
    std::string emitBoolArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitNewObjectArray(const std::string& size, const std::string& elementType);
    std::string emitObjectArrayLength(const std::string& receiver);
    std::string emitObjectArrayGet(
        const std::string& receiver, const std::string& index, const std::string& elementType);
    std::string emitObjectArraySet(
        const std::string& receiver, const std::string& index, const std::string& value);
    std::string emitFieldLoad(
        const SourceLocation& location, const std::string& className, const std::string& fieldName,
        const std::string& type);
    void emitFieldStore(
        const SourceLocation& location, const std::string& className, const std::string& fieldName,
        const std::string& value, const std::string& type);
    std::string emitLoad(const std::string& symbol, const std::string& type = "Int");
    void emitStore(const std::string& symbol, const std::string& value, const std::string& type = "Int");
    std::string emitPhi(const std::string& left, const std::string& right, const std::string& type = "Int");
    std::string emitPhi(const std::vector<std::string>& values, const std::string& type = "Int");
    void pushTypeSubstitution(const std::map<std::string, std::string>& substitution);
    void popTypeSubstitution();
    std::string substituteActiveType(const std::string& type) const;
    bool isTypeParameter(const std::string& type) const;
    std::string makeLabel(const std::string& prefix);
    std::string makeTemporarySymbol(const std::string& prefix);
    void emitLabel(const std::string& label);
    void emitBranchIfFalse(const std::string& condition, const std::string& targetLabel);
    void emitJump(const std::string& targetLabel);
    void bindParameter(const std::string& symbol, const std::string& parameterName);
    void bindThis();
    void bindClosure();
    void bindCapture(const std::string& symbol, int captureIndex, const std::string& type);
    const class CompilerContext& getContext() const;
    [[noreturn]] void unsupported(const SourceLocation& location, const std::string& feature) const;

private:
    IRProgram program;
    IRFunction* currentFunction = nullptr;
    std::map<std::string, std::string> parameterValues;
    struct CaptureValue {
        int index;
        std::string type;
    };
    std::map<std::string, CaptureValue> captureValues;
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
    const class CompilerContext* context = nullptr;

    std::string nextValue();
    void emit(IRInstruction instruction);
    void emitPendingSpecializations(const class ProgramNode& root);
    void emitMethodSpecialization(const MethodSpecialization& specialization, const class ProgramNode& root);
    void registerOverrideMethodSpecializations(const MethodSpecialization& specialization);
    void emitFunctionSpecialization(const FunctionSpecialization& specialization, const class ProgramNode& root);
    void registerAllMethodSpecializationsForClass(const std::string& className, std::set<std::string>& visited);
};
