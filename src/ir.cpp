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
            if (!instruction.result.empty()) out << instruction.result << " = ";
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
                    out << "function_ref " << instruction.operation;
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
    root.lowerToIR(*this);
    return program;
}

void IRBuilder::beginFunction(
    const std::string& name, const std::vector<IRParameter>& parameters,
    const std::string& returnType) {
    program.functions.push_back({name, parameters, returnType, {}});
    currentFunction = &program.functions.back();
    parameterValues.clear();
    thisValue.clear();
    nextValueId = 0;
    nextLabelId = 0;
    nextTemporarySymbolId = 0;
}

void IRBuilder::endFunction(const std::string& returnValue) {
    emit({IROpcode::Return, "", "", {returnValue}});
    currentFunction = nullptr;
}

std::string IRBuilder::emitConstant(const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::Constant, result, "", {value}});
    return result;
}

std::string IRBuilder::emitStringLiteral(const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::StringLiteral, result, "", {value}});
    return result;
}

std::string IRBuilder::emitBinary(
    const std::string& operation, const std::string& left, const std::string& right) {
    std::string result = nextValue();
    emit({IROpcode::Binary, result, operation, {left, right}});
    return result;
}

std::string IRBuilder::emitCall(const std::string& name, const std::vector<std::string>& arguments) {
    std::string result = nextValue();
    emit({IROpcode::Call, result, name, arguments});
    return result;
}

std::string IRBuilder::emitFunctionReference(const std::string& name) {
    std::string result = nextValue();
    emit({IROpcode::FunctionReference, result, name, {}});
    return result;
}

std::string IRBuilder::emitIndirectCall(
    const std::string& callee, const std::vector<std::string>& arguments) {
    std::vector<std::string> operands = {callee};
    operands.insert(operands.end(), arguments.begin(), arguments.end());
    std::string result = nextValue();
    emit({IROpcode::IndirectCall, result, "", operands});
    return result;
}

std::string IRBuilder::emitMethodCall(
    const std::string& className, const std::string& methodName, const std::string& receiver,
    const std::vector<std::string>& arguments) {
    std::vector<std::string> operands = {receiver};
    operands.insert(operands.end(), arguments.begin(), arguments.end());
    std::string result = nextValue();
    emit({IROpcode::MethodCall, result, qualifiedMember(className, methodName), operands});
    return result;
}

std::string IRBuilder::emitNewObject(
    const std::string& className, const std::vector<std::string>& arguments) {
    std::string result = nextValue();
    emit({IROpcode::NewObject, result, className, arguments});
    return result;
}

std::string IRBuilder::emitNewIntArray(const std::string& size) {
    std::string result = nextValue();
    emit({IROpcode::NewIntArray, result, "", {size}});
    return result;
}

std::string IRBuilder::emitIntArrayLength(const std::string& receiver) {
    std::string result = nextValue();
    emit({IROpcode::IntArrayLength, result, "", {receiver}});
    return result;
}

std::string IRBuilder::emitIntArrayGet(const std::string& receiver, const std::string& index) {
    std::string result = nextValue();
    emit({IROpcode::IntArrayGet, result, "", {receiver, index}});
    return result;
}

std::string IRBuilder::emitIntArraySet(
    const std::string& receiver, const std::string& index, const std::string& value) {
    std::string result = nextValue();
    emit({IROpcode::IntArraySet, result, "", {receiver, index, value}});
    return result;
}

std::string IRBuilder::emitFieldLoad(
    const SourceLocation& location, const std::string& className, const std::string& fieldName) {
    if (thisValue.empty()) {
        unsupported(location, "l'accès au champ '" + qualifiedMember(className, fieldName) + "'");
    }
    std::string result = nextValue();
    emit({IROpcode::FieldLoad, result, qualifiedMember(className, fieldName), {thisValue}});
    return result;
}

std::string IRBuilder::emitLoad(const std::string& symbol) {
    auto parameter = parameterValues.find(symbol);
    if (parameter != parameterValues.end()) return parameter->second;
    std::string result = nextValue();
    emit({IROpcode::Load, result, "", {symbol}});
    return result;
}

void IRBuilder::emitStore(const std::string& symbol, const std::string& value) {
    emit({IROpcode::Store, "", "", {symbol, value}});
}

std::string IRBuilder::emitPhi(const std::string& left, const std::string& right) {
    std::string result = nextValue();
    emit({IROpcode::Phi, result, "", {left, right}});
    return result;
}

std::string IRBuilder::makeLabel(const std::string& prefix) {
    return prefix + "." + std::to_string(nextLabelId++);
}

std::string IRBuilder::makeTemporarySymbol(const std::string& prefix) {
    return prefix + "." + std::to_string(nextTemporarySymbolId++);
}

void IRBuilder::emitLabel(const std::string& label) {
    emit({IROpcode::Label, "", "", {label}});
}

void IRBuilder::emitBranchIfFalse(const std::string& condition, const std::string& targetLabel) {
    emit({IROpcode::BranchIfFalse, "", "", {condition, targetLabel}});
}

void IRBuilder::emitJump(const std::string& targetLabel) {
    emit({IROpcode::Jump, "", "", {targetLabel}});
}

void IRBuilder::bindParameter(const std::string& symbol, const std::string& parameterName) {
    parameterValues[symbol] = "%" + parameterName;
}

void IRBuilder::bindThis() {
    thisValue = "%this";
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
