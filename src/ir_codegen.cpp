#include "ir_codegen.hpp"
#include "compiler_error.hpp"
#include "runtime_asm.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::vector<std::string> GLOBAL_ARGUMENT_REGISTERS = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
const std::vector<std::string> METHOD_ARGUMENT_REGISTERS = {"rsi", "rdx", "rcx", "r8", "r9"};

[[noreturn]] void codegenError(const std::string& message) {
    throw CompilerError(ErrorKind::Codegen, {"<ir>", 1, 1}, message);
}

std::string asmFunctionName(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) result += c;
        else result += "_";
    }
    return result;
}

std::string asmLabelName(const std::string& functionName, const std::string& label) {
    return "L_ir_" + asmFunctionName(functionName) + "_" + asmFunctionName(label);
}

std::pair<std::string, std::string> splitQualifiedMember(const std::string& name) {
    size_t dot = name.find('.');
    if (dot == std::string::npos) codegenError("membre IR non qualifie: " + name);
    return {name.substr(0, dot), name.substr(dot + 1)};
}

long long boxedInt(const std::string& value) {
    return (std::stoll(value) << 1) | 1;
}

std::string asmDataBytes(const std::string& value) {
    if (value.empty()) return "";
    std::ostringstream out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) out << ", ";
        out << static_cast<int>(static_cast<unsigned char>(value[i]));
    }
    return out.str();
}

std::string asmSymbolPart(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c))) result += c;
        else result += "_";
    }
    return result;
}

struct PhiInfo {
    std::string result;
    std::string type;
    std::vector<std::string> operands;
};

struct CallingConvention {
    std::vector<std::string> globalArgumentRegisters = GLOBAL_ARGUMENT_REGISTERS;
    std::vector<std::string> methodArgumentRegisters = METHOD_ARGUMENT_REGISTERS;
};

class StackFrame {
public:
    void addSlot(const std::string& name) {
        if (slotOffsets.count(name)) return;
        slotOrder.push_back(name);
        slotOffsets[name] = static_cast<int>(slotOrder.size()) * 8;
    }

    bool empty() const {
        return slotOffsets.empty();
    }

    size_t byteSize() const {
        return slotOffsets.size() * 8;
    }

    int offsetFor(const std::string& name) const {
        auto slot = slotOffsets.find(name);
        if (slot == slotOffsets.end()) codegenError("valeur IR inconnue: " + name);
        return slot->second;
    }

private:
    std::map<std::string, int> slotOffsets;
    std::vector<std::string> slotOrder;
};

class FunctionEmitter {
public:
    FunctionEmitter(
        const IRFunction& function, const CompilerContext& context, std::ostream& out)
        : function(function), context(context), out(out) {
        collectPhiLabels();
        collectSlots();
    }

    void emit() {
        if (function.parameters.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("le backend IR limite les fonctions globales a 6 parametres");
        }

        out << asmFunctionName(function.name) << ":\n";
        out << "    push rbp\n    mov rbp, rsp\n";
        if (!frame.empty()) out << "    sub rsp, " << frame.byteSize() << "\n";
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            storeRegister("%" + function.parameters[i].name, callingConvention.globalArgumentRegisters[i]);
        }
        for (const auto& instruction : function.instructions) emitInstruction(instruction);
        out << "\n";
    }

private:
    const IRFunction& function;
    const CompilerContext& context;
    std::ostream& out;
    CallingConvention callingConvention;
    StackFrame frame;
    std::map<std::string, std::vector<PhiInfo>> phiLabels;
    std::map<std::string, std::string> valueTypes;
    std::map<std::string, int> emittedIncomingJumps;
    int nextIndirectCallId = 0;

    void collectPhiLabels() {
        for (size_t i = 0; i < function.instructions.size(); ++i) {
            const auto& instruction = function.instructions[i];
            if (instruction.opcode != IROpcode::Label) continue;

            std::vector<PhiInfo> phis;
            size_t next = i + 1;
            while (next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Phi) {
                const auto& phi = function.instructions[next];
                phis.push_back({phi.result, phi.type, phi.operands});
                ++next;
            }
            if (!phis.empty()) phiLabels[instruction.operands[0]] = phis;
        }
    }

    void collectSlots() {
        for (const auto& parameter : function.parameters) {
            const std::string name = "%" + parameter.name;
            frame.addSlot(name);
            valueTypes[name] = parameter.type;
        }
        for (const auto& instruction : function.instructions) {
            if (!instruction.result.empty()) {
                frame.addSlot(instruction.result);
                valueTypes[instruction.result] = instruction.type;
            }
            if (instruction.opcode == IROpcode::Store) {
                frame.addSlot(instruction.operands[0]);
                valueTypes[instruction.operands[0]] = instruction.type;
            }
        }
    }

    void loadValue(const std::string& name, const std::string& reg) const {
        out << "    mov " << reg << ", [rbp - " << frame.offsetFor(name) << "]\n";
    }

    void storeRegister(const std::string& name, const std::string& reg) const {
        out << "    mov [rbp - " << frame.offsetFor(name) << "], " << reg << "\n";
    }

    std::string typeOf(const std::string& name) const {
        auto type = valueTypes.find(name);
        if (type == valueTypes.end() || type->second.empty()) return "Int";
        return type->second;
    }

    void emitInstruction(const IRInstruction& instruction) {
        switch (instruction.opcode) {
            case IROpcode::Constant:
                emitConstant(instruction);
                break;
            case IROpcode::StringLiteral:
                emitStringLiteral(instruction);
                break;
            case IROpcode::Binary:
                emitBinary(instruction);
                break;
            case IROpcode::Call:
                emitCall(instruction);
                break;
            case IROpcode::FunctionReference:
                emitFunctionReference(instruction);
                break;
            case IROpcode::ClosureLoad:
                emitClosureLoad(instruction);
                break;
            case IROpcode::IndirectCall:
                emitIndirectCall(instruction);
                break;
            case IROpcode::MethodCall:
                emitMethodCall(instruction);
                break;
            case IROpcode::NewObject:
                emitNewObject(instruction);
                break;
            case IROpcode::NewIntArray:
                emitNewIntArray(instruction);
                break;
            case IROpcode::IntArrayLength:
                emitIntArrayLength(instruction);
                break;
            case IROpcode::IntArrayGet:
                emitIntArrayGet(instruction);
                break;
            case IROpcode::IntArraySet:
                emitIntArraySet(instruction);
                break;
            case IROpcode::NewLongArray:
                emitNewLongArray(instruction);
                break;
            case IROpcode::LongArrayLength:
                emitLongArrayLength(instruction);
                break;
            case IROpcode::LongArrayGet:
                emitLongArrayGet(instruction);
                break;
            case IROpcode::LongArraySet:
                emitLongArraySet(instruction);
                break;
            case IROpcode::NewBoolArray:
                emitNewBoolArray(instruction);
                break;
            case IROpcode::BoolArrayLength:
                emitBoolArrayLength(instruction);
                break;
            case IROpcode::BoolArrayGet:
                emitBoolArrayGet(instruction);
                break;
            case IROpcode::BoolArraySet:
                emitBoolArraySet(instruction);
                break;
            case IROpcode::FieldLoad:
                emitFieldLoad(instruction);
                break;
            case IROpcode::Load:
                loadValue(instruction.operands[0], "rax");
                storeRegister(instruction.result, "rax");
                break;
            case IROpcode::Store:
                loadValue(instruction.operands[1], "rax");
                storeRegister(instruction.operands[0], "rax");
                break;
            case IROpcode::Return:
                loadValue(instruction.operands[0], "rax");
                out << "    mov rsp, rbp\n    pop rbp\n    ret\n";
                break;
            case IROpcode::Label:
                out << asmLabelName(function.name, instruction.operands[0]) << ":\n";
                break;
            case IROpcode::BranchIfFalse:
                loadValue(instruction.operands[0], "rax");
                out << "    cmp rax, 1\n";
                out << "    je " << asmLabelName(function.name, instruction.operands[1]) << "\n";
                break;
            case IROpcode::Jump:
                emitPhiTransfers(instruction.operands[0]);
                out << "    jmp " << asmLabelName(function.name, instruction.operands[0]) << "\n";
                break;
            case IROpcode::Phi:
                break;
        }
    }

    void emitPhiTransfers(const std::string& targetLabel) {
        auto phis = phiLabels.find(targetLabel);
        if (phis == phiLabels.end()) return;

        int incomingIndex = emittedIncomingJumps[targetLabel]++;
        for (const auto& phi : phis->second) {
            if (incomingIndex >= static_cast<int>(phi.operands.size())) {
                codegenError("phi IR sans operande pour le saut vers " + targetLabel);
            }
            loadValue(phi.operands[incomingIndex], "rax");
            storeRegister(phi.result, "rax");
        }
    }

    void emitBinary(const IRInstruction& instruction) {
        if (instruction.type == "Float" || typeOf(instruction.operands[0]) == "Float") {
            emitFloatBinary(instruction);
            return;
        }
        if (instruction.type == "Double" || typeOf(instruction.operands[0]) == "Double") {
            emitDoubleBinary(instruction);
            return;
        }
        loadValue(instruction.operands[0], "rax");
        loadValue(instruction.operands[1], "rbx");
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    add rax, rbx\n    sub rax, 1\n";
        } else if (op == "-") {
            out << "    sub rax, rbx\n    add rax, 1\n";
        } else if (op == "*") {
            out << "    sar rax, 1\n    sar rbx, 1\n    imul rax, rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "/") {
            out << "    sar rax, 1\n    sar rbx, 1\n    cqo\n    idiv rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    sar rax, 1\n    sar rbx, 1\n    cmp rax, rbx\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setl al\n";
            else if (op == ">") out << "    setg al\n";
            else if (op == "<=") out << "    setle al\n";
            else out << "    setge al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitConstant(const IRInstruction& instruction) {
        if (instruction.type == "Double") {
            const std::string label = "nabla_double_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result);
            out << "section .data\n";
            out << label << ": dq __float64__(" << instruction.operands[0] << ")\n";
            out << "section .text\n";
            out << "    mov rax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        if (instruction.type == "Float") {
            const std::string label = "nabla_float_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result);
            out << "section .data\n";
            out << label << ": dd __float32__(" << instruction.operands[0] << ")\n";
            out << "section .text\n";
            out << "    xor rax, rax\n";
            out << "    mov eax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        out << "    mov rax, " << boxedInt(instruction.operands[0]) << "\n";
        storeRegister(instruction.result, "rax");
    }

    void emitFloatBinary(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        loadValue(instruction.operands[1], "rbx");
        out << "    movd xmm0, eax\n";
        out << "    movd xmm1, ebx\n";
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    addss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "-") {
            out << "    subss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "*") {
            out << "    mulss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "/") {
            out << "    divss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    ucomiss xmm0, xmm1\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setb al\n";
            else if (op == ">") out << "    seta al\n";
            else if (op == "<=") out << "    setbe al\n";
            else out << "    setae al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR Float inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitDoubleBinary(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        loadValue(instruction.operands[1], "rbx");
        out << "    movq xmm0, rax\n";
        out << "    movq xmm1, rbx\n";
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    addsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "-") {
            out << "    subsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "*") {
            out << "    mulsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "/") {
            out << "    divsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    ucomisd xmm0, xmm1\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setb al\n";
            else if (op == ">") out << "    seta al\n";
            else if (op == "<=") out << "    setbe al\n";
            else out << "    setae al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR Double inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitStringLiteral(const IRInstruction& instruction) {
        const std::string label = "nabla_string_" + asmSymbolPart(function.name) + "_" +
                                  asmSymbolPart(instruction.result);
        out << "section .data\n";
        if (!instruction.operands[0].empty()) {
            out << label << "_chars: db " << asmDataBytes(instruction.operands[0]) << "\n";
        } else {
            out << label << "_chars: db 0\n";
        }
        out << label << "_obj: dq 0, " << instruction.operands[0].size() << ", " << label << "_chars\n";
        out << "section .text\n";
        out << "    mov rax, " << label << "_obj\n";
        storeRegister(instruction.result, "rax");
    }

    void emitCall(const IRInstruction& instruction) {
        if (instruction.operands.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel IR avec plus de 6 arguments");
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], callingConvention.globalArgumentRegisters[i]);
        }
        if (instruction.operation == "print") {
            out << "    call Runtime_print\n";
        } else {
            out << "    call " << asmFunctionName(instruction.operation) << "\n";
        }
        storeRegister(instruction.result, "rax");
    }

    void emitFunctionReference(const IRInstruction& instruction) {
        const size_t objectSize = 16 + instruction.operands.size() * 8;
        out << "    mov rdi, " << objectSize << "\n";
        out << "    call Runtime_alloc\n";
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], " << asmFunctionName(instruction.operation) << "\n";
        out << "    mov qword [rbx + 8], " << (instruction.operands.empty() ? 0 : 1) << "\n";
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], "rax");
            out << "    mov [rbx + " << (16 + i * 8) << "], rax\n";
        }
        storeRegister(instruction.result, "rbx");
    }

    void emitClosureLoad(const IRInstruction& instruction) {
        if (instruction.operands.size() != 2) codegenError("lecture de closure IR invalide");
        loadValue(instruction.operands[0], "rax");
        const int captureIndex = std::stoi(instruction.operands[1]);
        out << "    mov rax, [rax + " << (16 + captureIndex * 8) << "]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitIndirectCall(const IRInstruction& instruction) {
        if (instruction.operands.empty()) codegenError("appel indirect IR sans callee");
        const size_t argumentCount = instruction.operands.size() - 1;
        if (argumentCount > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel indirect IR avec plus de 6 arguments");
        }
        if (argumentCount + 1 > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel indirect capturant IR avec plus de 5 arguments");
        }
        const int callId = nextIndirectCallId++;
        const std::string capturedLabel = ".L_closure_captured_" + asmSymbolPart(function.name) + "_" +
                                          std::to_string(callId);
        const std::string doneLabel = ".L_closure_done_" + asmSymbolPart(function.name) + "_" +
                                      std::to_string(callId);
        loadValue(instruction.operands[0], "r11");
        out << "    cmp qword [r11 + 8], 0\n";
        out << "    jne " << capturedLabel << "\n";
        for (size_t i = 0; i < argumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.globalArgumentRegisters[i]);
        }
        out << "    call qword [r11]\n";
        out << "    jmp " << doneLabel << "\n";
        out << capturedLabel << ":\n";
        out << "    mov rdi, r11\n";
        for (size_t i = 0; i < argumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.globalArgumentRegisters[i + 1]);
        }
        out << "    call qword [r11]\n";
        out << doneLabel << ":\n";
        storeRegister(instruction.result, "rax");
    }

    std::vector<int> fieldOffsetsByConstructorOrder(const std::string& className) const {
        const std::string layoutClassName = genericBaseName(className);
        auto classIt = context.classes.find(layoutClassName);
        if (classIt == context.classes.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        auto layoutIt = context.classLayouts.find(layoutClassName);
        if (classIt->second.fields.empty()) return {};
        if (layoutIt == context.classLayouts.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        std::vector<int> offsets;
        for (const auto& field : classIt->second.fields) {
            auto offsetIt = layoutIt->second.find(field.name);
            if (offsetIt == layoutIt->second.end()) {
                codegenError("offset de champ inconnu: " + layoutClassName + "." + field.name);
            }
            offsets.push_back(offsetIt->second);
        }
        return offsets;
    }

    int objectSizeFor(const std::string& className) const {
        std::vector<int> offsets = fieldOffsetsByConstructorOrder(className);
        if (offsets.empty()) return 8;
        return *std::max_element(offsets.begin(), offsets.end()) + 8;
    }

    int fieldOffsetFor(const std::string& qualifiedField) const {
        auto [className, fieldName] = splitQualifiedMember(qualifiedField);
        const std::string layoutClassName = genericBaseName(className);
        auto layoutIt = context.classLayouts.find(layoutClassName);
        if (layoutIt == context.classLayouts.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        auto fieldIt = layoutIt->second.find(fieldName);
        if (fieldIt == layoutIt->second.end()) {
            codegenError("offset de champ inconnu: " + qualifiedField);
        }
        return fieldIt->second;
    }

    void emitNewObject(const IRInstruction& instruction) {
        std::vector<int> fieldOffsets = fieldOffsetsByConstructorOrder(instruction.operation);
        if (instruction.operands.size() != fieldOffsets.size()) {
            codegenError("nombre d'arguments invalide pour new " + instruction.operation);
        }

        out << "    mov rdi, " << objectSizeFor(instruction.operation) << "\n";
        out << "    call Runtime_alloc\n";
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], 0\n";
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], "rax");
            out << "    mov [rbx + " << fieldOffsets[i] << "], rax\n";
        }
        storeRegister(instruction.result, "rbx");
    }

    void emitNewNativeArray(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    cmp rax, 1\n";
        out << "    jl Runtime_bounds_error\n";
        out << "    mov r8, rax\n";
        out << "    sar rax, 1\n";
        out << "    lea rdi, [rax * 8 + 16]\n";
        out << "    call Runtime_alloc\n";
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], 0\n";
        out << "    mov [rbx + 8], r8\n";
        out << "    sar r8, 1\n";
        out << "    xor rcx, rcx\n";
        out << ".L_array_init_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        out << "    cmp rcx, r8\n";
        out << "    jge .L_array_init_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << "    mov qword [rbx + 16 + rcx * 8], 1\n";
        out << "    inc rcx\n";
        out << "    jmp .L_array_init_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << ".L_array_init_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        storeRegister(instruction.result, "rbx");
    }

    void emitNewIntArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewLongArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewBoolArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNativeArrayLength(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    mov rax, [rax + 8]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitArrayBoundsCheck(const std::string& arrayReg, const std::string& indexReg) {
        out << "    sar " << indexReg << ", 1\n";
        out << "    cmp " << indexReg << ", 0\n";
        out << "    jl Runtime_bounds_error\n";
        out << "    mov rdx, [" << arrayReg << " + 8]\n";
        out << "    sar rdx, 1\n";
        out << "    cmp " << indexReg << ", rdx\n";
        out << "    jge Runtime_bounds_error\n";
    }

    void emitIntArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitLongArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitBoolArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitNativeArrayGet(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rcx");
        emitArrayBoundsCheck("rbx", "rcx");
        out << "    mov rax, [rbx + 16 + rcx * 8]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitIntArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitLongArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitBoolArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitNativeArraySet(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rcx");
        emitArrayBoundsCheck("rbx", "rcx");
        loadValue(instruction.operands[2], "rax");
        out << "    mov [rbx + 16 + rcx * 8], rax\n";
        out << "    mov rax, 1\n";
        storeRegister(instruction.result, "rax");
    }

    void emitIntArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitLongArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitBoolArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitFieldLoad(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    mov rax, [rax + " << fieldOffsetFor(instruction.operation) << "]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitMethodCall(const IRInstruction& instruction) {
        auto [className, methodName] = splitQualifiedMember(instruction.operation);
        if (instruction.operands.empty()) codegenError("appel de methode IR sans receveur");

        loadValue(instruction.operands[0], "rdi");
        const size_t methodArgumentCount = instruction.operands.size() - 1;
        if (methodArgumentCount > callingConvention.methodArgumentRegisters.size()) {
            codegenError("appel de methode IR avec plus de 5 arguments");
        }
        for (size_t i = 0; i < methodArgumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.methodArgumentRegisters[i]);
        }

        if ((className == "Int" || className == "Long") && methodName == "toString") {
            out << "    call Int_method_toString\n";
        } else if (className == "String" && methodName == "length") {
            out << "    mov rax, [rdi + 8]\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else {
            out << "    call " << asmFunctionName(instruction.operation) << "\n";
        }
        storeRegister(instruction.result, "rax");
    }
};
}

void IRCodeGenerator::generateASM(
    const IRProgram& program, const CompilerContext& context, std::ostream& out) const {
    RuntimeASM::emit(out);
    for (const auto& function : program.functions) {
        FunctionEmitter(function, context, out).emit();
    }
}
