#include "ir.hpp"
#include "ast.hpp"
#include <sstream>

namespace {
std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string qualifiedMember(const std::string& className, const std::string& memberName) {
    return className + "." + memberName;
}

std::string quotedString(const std::string& value) {
    std::ostringstream out;
    out << "\"";
    for (char c : value) {
        if (c == '\n') out << "\\n";
        else if (c == '"') out << "\\\"";
        else if (c == '\\') out << "\\\\";
        else out << c;
    }
    out << "\"";
    return out.str();
}
}

std::string IRProgram::format() const {
    std::ostringstream out;
    for (size_t functionIndex = 0; functionIndex < functions.size(); ++functionIndex) {
        const auto& function = functions[functionIndex];
        out << "function " << function.name << "(";
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            if (i > 0) out << ", ";
            out << "%" << function.parameters[i].name << ": " << function.parameters[i].type;
        }
        out << ") -> " << function.returnType << "\n";

        for (const auto& instruction : function.instructions) {
            out << "  ";
            if (!instruction.result.empty()) {
                out << instruction.result;
                if (!instruction.type.empty()) out << ": " << instruction.type;
                out << " = ";
            }
            switch (instruction.opcode) {
                case IROpcode::Constant:
                    out << "const " << instruction.operands[0];
                    break;
                case IROpcode::StringLiteral:
                    out << "string " << quotedString(instruction.operands[0]);
                    break;
                case IROpcode::Binary:
                    out << instruction.operation << " " << join(instruction.operands, ", ");
                    break;
                case IROpcode::Call:
                    out << "call " << instruction.operation << "(" << join(instruction.operands, ", ") << ")";
                    break;
                case IROpcode::FunctionReference:
                    out << "closure_ref " << instruction.operation;
                    if (!instruction.operands.empty()) out << "(" << join(instruction.operands, ", ") << ")";
                    break;
                case IROpcode::ClosureLoad:
                    out << "closure_load " << instruction.operands[0] << ", " << instruction.operands[1];
                    break;
                case IROpcode::IndirectCall:
                    out << "indirect_call " << join(instruction.operands, ", ");
                    break;
                case IROpcode::MethodCall:
                    out << "call_method " << instruction.operation << "(" << join(instruction.operands, ", ") << ")";
                    break;
                case IROpcode::NewObject:
                    out << "new " << instruction.operation << "(" << join(instruction.operands, ", ") << ")";
                    break;
                case IROpcode::NewIntArray:
                    out << "new_int_array " << instruction.operands[0];
                    break;
                case IROpcode::IntArrayLength:
                    out << "int_array_length " << instruction.operands[0];
                    break;
                case IROpcode::IntArrayGet:
                    out << "int_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::IntArraySet:
                    out << "int_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::NewLongArray:
                    out << "new_long_array " << instruction.operands[0];
                    break;
                case IROpcode::LongArrayLength:
                    out << "long_array_length " << instruction.operands[0];
                    break;
                case IROpcode::LongArrayGet:
                    out << "long_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::LongArraySet:
                    out << "long_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::NewFloatArray:
                    out << "new_float_array " << instruction.operands[0];
                    break;
                case IROpcode::FloatArrayLength:
                    out << "float_array_length " << instruction.operands[0];
                    break;
                case IROpcode::FloatArrayGet:
                    out << "float_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::FloatArraySet:
                    out << "float_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::NewDoubleArray:
                    out << "new_double_array " << instruction.operands[0];
                    break;
                case IROpcode::DoubleArrayLength:
                    out << "double_array_length " << instruction.operands[0];
                    break;
                case IROpcode::DoubleArrayGet:
                    out << "double_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::DoubleArraySet:
                    out << "double_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::NewBoolArray:
                    out << "new_bool_array " << instruction.operands[0];
                    break;
                case IROpcode::BoolArrayLength:
                    out << "bool_array_length " << instruction.operands[0];
                    break;
                case IROpcode::BoolArrayGet:
                    out << "bool_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::BoolArraySet:
                    out << "bool_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::NewObjectArray:
                    out << "new_object_array " << instruction.operands[0];
                    break;
                case IROpcode::ObjectArrayLength:
                    out << "object_array_length " << instruction.operands[0];
                    break;
                case IROpcode::ObjectArrayGet:
                    out << "object_array_get " << join(instruction.operands, ", ");
                    break;
                case IROpcode::ObjectArraySet:
                    out << "object_array_set " << join(instruction.operands, ", ");
                    break;
                case IROpcode::FieldLoad:
                    out << "field " << instruction.operands[0] << ", " << instruction.operation;
                    break;
                case IROpcode::Load:
                    out << "load @" << instruction.operands[0];
                    break;
                case IROpcode::Store:
                    out << "store " << instruction.operands[1] << ", @" << instruction.operands[0];
                    break;
                case IROpcode::Label:
                    out << "label " << instruction.operands[0];
                    break;
                case IROpcode::BranchIfFalse:
                    out << "branch_if_false " << instruction.operands[0] << ", " << instruction.operands[1];
                    break;
                case IROpcode::Jump:
                    out << "jump " << instruction.operands[0];
                    break;
                case IROpcode::Phi:
                    out << "phi " << join(instruction.operands, ", ");
                    break;
                case IROpcode::Return:
                    out << "return " << instruction.operands[0];
                    break;
            }
            out << "\n";
        }
        if (functionIndex + 1 < functions.size()) out << "\n";
    }
    return out.str();
}

IRProgram IRBuilder::build(const ProgramNode& root) {
    program = {};
    methodSpecializations.clear();
    functionSpecializations.clear();
    typeSubstitutionStack.clear();
    activeTypeSubstitution.clear();
    root.lowerToIR(*this);
    emitPendingSpecializations(root);
    return program;
}

void IRBuilder::beginFunction(
    const std::string& name, const std::vector<IRParameter>& parameters,
    const std::string& returnType) {
    std::vector<IRParameter> substitutedParameters;
    for (auto parameter : parameters) {
        parameter.type = substituteActiveType(parameter.type);
        substitutedParameters.push_back(std::move(parameter));
    }
    program.functions.push_back({name, substitutedParameters, substituteActiveType(returnType), {}});
    currentFunction = &program.functions.back();
    parameterValues.clear();
    captureValues.clear();
    thisValue.clear();
    closureValue.clear();
    nextValueId = 0;
    nextLabelId = 0;
    nextTemporarySymbolId = 0;
}

void IRBuilder::endFunction(const std::string& returnValue) {
    emit({IROpcode::Return, "", "", "", {returnValue}});
    currentFunction = nullptr;
}

std::string IRBuilder::emitConstant(const std::string& value, const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::Constant, result, substituteActiveType(type), "", {value}});
    return result;
}

std::string IRBuilder::emitStringLiteral(const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::StringLiteral, result, "String", "", {value}});
    return result;
}

std::string IRBuilder::emitBinary(
    const std::string& operation, const std::string& left, const std::string& right,
    const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::Binary, result, substituteActiveType(type), operation, {left, right}});
    return result;
}

std::string IRBuilder::emitCall(
    const std::string& name, const std::vector<std::string>& arguments, const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::Call, result, substituteActiveType(type), name, arguments});
    return result;
}

std::string IRBuilder::emitFunctionReference(
    const std::string& name, const std::vector<std::string>& captures) {
    std::string result = nextValue();
    emit({IROpcode::FunctionReference, result, "Closure", name, captures});
    return result;
}

std::string IRBuilder::emitClosureLoad(const std::string& closure, int captureIndex) {
    std::string result = nextValue();
    emit({IROpcode::ClosureLoad, result, "", "", {closure, std::to_string(captureIndex)}});
    return result;
}

std::string IRBuilder::emitClosureLoad(const std::string& closure, int captureIndex, const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::ClosureLoad, result, substituteActiveType(type), "", {closure, std::to_string(captureIndex)}});
    return result;
}

std::string IRBuilder::emitIndirectCall(
    const std::string& callee, const std::vector<std::string>& arguments, const std::string& type) {
    std::vector<std::string> operands = {callee};
    operands.insert(operands.end(), arguments.begin(), arguments.end());
    std::string result = nextValue();
    emit({IROpcode::IndirectCall, result, substituteActiveType(type), "", operands});
    return result;
}

std::string IRBuilder::emitMethodCall(
    const std::string& className, const std::string& methodName, const std::string& receiver,
    const std::vector<std::string>& arguments, const std::string& type) {
    std::vector<std::string> operands = {receiver};
    operands.insert(operands.end(), arguments.begin(), arguments.end());
    std::string result = nextValue();
    emit({
        IROpcode::MethodCall, result, substituteActiveType(type),
        qualifiedMember(substituteActiveType(className), methodName), operands});
    return result;
}

void IRBuilder::registerMethodSpecialization(
    const std::string& concreteClassName, const std::string& templateClassName,
    const std::string& methodName, const std::vector<std::string>& methodTypeArguments,
    const std::vector<std::string>& argumentTypes, const std::string& returnType) {
    if (concreteClassName == templateClassName && methodTypeArguments.empty()) return;
    for (const auto& specialization : methodSpecializations) {
        if (specialization.concreteClassName == concreteClassName &&
            specialization.templateClassName == templateClassName &&
            specialization.methodName == methodName &&
            specialization.methodTypeArguments == methodTypeArguments &&
            specialization.argumentTypes == argumentTypes &&
            specialization.returnType == returnType) {
            return;
        }
    }
    methodSpecializations.push_back({
        concreteClassName, templateClassName, methodName, methodTypeArguments,
        argumentTypes, returnType});
}

void IRBuilder::registerFunctionSpecialization(
    const std::string& functionName, const std::vector<std::string>& typeArguments,
    const std::string& returnType) {
    if (typeArguments.empty()) return;
    for (const auto& specialization : functionSpecializations) {
        if (specialization.functionName == functionName &&
            specialization.typeArguments == typeArguments &&
            specialization.returnType == returnType) {
            return;
        }
    }
    functionSpecializations.push_back({functionName, typeArguments, returnType});
}

void IRBuilder::emitPendingSpecializations(const ProgramNode& root) {
    size_t nextFunctionSpecialization = 0;
    size_t nextMethodSpecialization = 0;
    while (nextFunctionSpecialization < functionSpecializations.size() ||
           nextMethodSpecialization < methodSpecializations.size()) {
        while (nextFunctionSpecialization < functionSpecializations.size()) {
            emitFunctionSpecialization(functionSpecializations[nextFunctionSpecialization], root);
            ++nextFunctionSpecialization;
        }
        while (nextMethodSpecialization < methodSpecializations.size()) {
            emitMethodSpecialization(methodSpecializations[nextMethodSpecialization], root);
            ++nextMethodSpecialization;
        }
    }
}

void IRBuilder::emitFunctionSpecialization(
    const FunctionSpecialization& specialization, const ProgramNode& root) {
    for (const auto& element : root.elements) {
        const auto* function = dynamic_cast<const FunctionDefNode*>(element.get());
        if (!function) continue;
        if (function->getClassName().empty() && function->getName() == specialization.functionName) {
            function->lowerSpecializedFunctionToIR(*this, specialization.typeArguments);
            return;
        }
    }
    throw CompilerError(
        ErrorKind::Codegen, SourceLocation{},
        "fonction générique introuvable pour spécialisation: " + specialization.functionName);
}

void IRBuilder::emitMethodSpecialization(
    const MethodSpecialization& specialization, const ProgramNode& root) {
    for (const auto& element : root.elements) {
        const auto* function = dynamic_cast<const FunctionDefNode*>(element.get());
        if (!function) continue;
        if (function->getClassName() == specialization.templateClassName &&
            function->getName() == specialization.methodName) {
            function->lowerSpecializedMethodToIR(
                *this, specialization.concreteClassName, specialization.methodTypeArguments);
            return;
        }
    }
    throw CompilerError(
        ErrorKind::Codegen, SourceLocation{},
        "méthode générique introuvable pour spécialisation: " +
        qualifiedMember(specialization.templateClassName, specialization.methodName));
}

std::string IRBuilder::emitNewObject(
    const std::string& className, const std::vector<std::string>& arguments,
    const std::string& resultType) {
    std::string result = nextValue();
    const std::string type = resultType.empty() ? className : resultType;
    const std::string substitutedType = substituteActiveType(type);
    emit({IROpcode::NewObject, result, substitutedType, substitutedType, arguments});
    return result;
}

std::string IRBuilder::emitNewIntArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewIntArray, result, "IntArray", "", {size}});
    return result;
}

std::string IRBuilder::emitIntArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::IntArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitIntArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::IntArrayGet, result, "Int", "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitIntArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::IntArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitNewLongArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewLongArray, result, "LongArray", "", {size}});
    return result;
}

std::string IRBuilder::emitLongArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::LongArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitLongArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::LongArrayGet, result, "Long", "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitLongArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::LongArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitNewFloatArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewFloatArray, result, "FloatArray", "", {size}});
    return result;
}

std::string IRBuilder::emitFloatArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::FloatArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitFloatArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::FloatArrayGet, result, "Float", "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitFloatArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::FloatArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitNewDoubleArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewDoubleArray, result, "DoubleArray", "", {size}});
    return result;
}

std::string IRBuilder::emitDoubleArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::DoubleArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitDoubleArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::DoubleArrayGet, result, "Double", "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitDoubleArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::DoubleArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitNewBoolArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewBoolArray, result, "BoolArray", "", {size}});
    return result;
}

std::string IRBuilder::emitBoolArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::BoolArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitBoolArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::BoolArrayGet, result, "Bool", "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitBoolArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::BoolArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitNewObjectArray(const std::string& size, const std::string& elementType) {
    std::string result = nextValue();
    emit({IROpcode::NewObjectArray, result, formatParameterizedType("ObjectArray", {elementType}), "", {size}});
    return result;
}

std::string IRBuilder::emitObjectArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::ObjectArrayLength, result, "Int", "", {receiver}});
    return result;
}

std::string IRBuilder::emitObjectArrayGet(
    const std::string& receiver, const std::string& index, const std::string& elementType) {
    std::string result = nextValue();
    emit({IROpcode::ObjectArrayGet, result, elementType, "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitObjectArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::ObjectArraySet, result, "Unit", "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitFieldLoad(
    const SourceLocation& location, const std::string& className, const std::string& fieldName,
    const std::string& type) {
    if (thisValue.empty()) {
        unsupported(location, "l'accès au champ '" + qualifiedMember(className, fieldName) + "'");
    }
    std::string result = nextValue();
    emit({
        IROpcode::FieldLoad, result, substituteActiveType(type),
        qualifiedMember(substituteActiveType(className), fieldName), {thisValue}});
    return result;
}

std::string IRBuilder::emitLoad(const std::string& symbol, const std::string& type) {
    auto capture = captureValues.find(symbol);
    if (capture != captureValues.end()) {
        return emitClosureLoad(closureValue, capture->second.index, capture->second.type);
    }
    auto parameter = parameterValues.find(symbol);
    if (parameter != parameterValues.end()) return parameter->second;
    std::string result = nextValue();
    emit({IROpcode::Load, result, substituteActiveType(type), "", {symbol}});
    return result;
}

void IRBuilder::emitStore(const std::string& symbol, const std::string& value, const std::string& type) {
    emit({IROpcode::Store, "", substituteActiveType(type), "", {symbol, value}});
}

std::string IRBuilder::emitPhi(const std::string& left, const std::string& right, const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::Phi, result, substituteActiveType(type), "", {left, right}});
    return result;
}

std::string IRBuilder::emitPhi(const std::vector<std::string>& values, const std::string& type) {
    std::string result = nextValue();
    emit({IROpcode::Phi, result, substituteActiveType(type), "", values});
    return result;
}

void IRBuilder::pushTypeSubstitution(const std::map<std::string, std::string>& substitution) {
    typeSubstitutionStack.push_back(activeTypeSubstitution);
    activeTypeSubstitution = substitution;
}

void IRBuilder::popTypeSubstitution() {
    if (typeSubstitutionStack.empty()) {
        activeTypeSubstitution.clear();
        return;
    }
    activeTypeSubstitution = typeSubstitutionStack.back();
    typeSubstitutionStack.pop_back();
}

std::string IRBuilder::substituteActiveType(const std::string& type) const {
    if (activeTypeSubstitution.empty()) return type;
    return substituteType(type, activeTypeSubstitution);
}

std::string IRBuilder::makeLabel(const std::string& prefix) {
    return prefix + "." + std::to_string(nextLabelId++);
}

std::string IRBuilder::makeTemporarySymbol(const std::string& prefix) {
    return prefix + "." + std::to_string(nextTemporarySymbolId++);
}

void IRBuilder::emitLabel(const std::string& label) {
    emit({IROpcode::Label, "", "", "", {label}});
}

void IRBuilder::emitBranchIfFalse(const std::string& condition, const std::string& targetLabel) {
    emit({IROpcode::BranchIfFalse, "", "", "", {condition, targetLabel}});
}

void IRBuilder::emitJump(const std::string& targetLabel) {
    emit({IROpcode::Jump, "", "", "", {targetLabel}});
}

void IRBuilder::bindParameter(const std::string& symbol, const std::string& parameterName) {
    parameterValues[symbol] = "%" + parameterName;
}

void IRBuilder::bindThis() {
    thisValue = "%this";
}

void IRBuilder::bindClosure() {
    closureValue = "%closure";
}

void IRBuilder::bindCapture(const std::string& symbol, int captureIndex, const std::string& type) {
    captureValues[symbol] = {captureIndex, substituteActiveType(type)};
}

[[noreturn]] void IRBuilder::unsupported(
    const SourceLocation& location, const std::string& feature) const {
    throw CompilerError(ErrorKind::Codegen, location, "IR non supportée pour " + feature);
}

std::string IRBuilder::nextValue() {
    return "%" + std::to_string(nextValueId++);
}

void IRBuilder::emit(IRInstruction instruction) {
    currentFunction->instructions.push_back(std::move(instruction));
}
