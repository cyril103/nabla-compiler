#include "ir_codegen.hpp"
#include "compiler_error.hpp"
#include "runtime_asm.hpp"
#include "runtime_values.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
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

std::string asmSymbolName(const std::string& name) {
    std::string result;
    result.reserve(name.size() + 10);
    result = "nabla_sym_";
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            result += static_cast<char>(c);
        } else {
            result += "_";
            result += hex[(c >> 4) & 0x0f];
            result += hex[c & 0x0f];
        }
    }
    return result;
}

std::string asmFunctionName(const std::string& name) {
    if (name == "main") return "main";
    return asmSymbolName(name);
}

std::string asmLabelName(const std::string& functionName, const std::string& label) {
    return "L_ir_" + asmFunctionName(functionName) + "_" + asmFunctionName(label);
}

std::pair<std::string, std::string> splitQualifiedMember(const std::string& name) {
    size_t dot = name.find('.');
    if (dot == std::string::npos) codegenError("membre IR non qualifie: " + name);
    return {name.substr(0, dot), name.substr(dot + 1)};
}

std::string methodFunctionName(const std::string& className, const std::string& methodName) {
    return "method." + className + "." + methodName;
}

std::string methodFunctionName(const std::string& qualifiedMember) {
    auto [className, methodName] = splitQualifiedMember(qualifiedMember);
    return methodFunctionName(className, methodName);
}

long long boxedInt(const std::string& value) {
    long long parsed = 0;
    try {
        parsed = std::stoll(value);
    } catch (...) {
        codegenError("entier constant invalide ou trop grand: " + value);
    }

    if (parsed > RuntimeValues::kMaxTaggedIntPayload || parsed < RuntimeValues::kMinTaggedIntPayload) {
        codegenError("entier constant hors de la plage Int: " + value);
    }
    return RuntimeValues::encodeTaggedInteger(parsed);
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

std::string normalizeFloatConstant(const std::string& value) {
    if (value.find('.') != std::string::npos) return value;
    if (value.find('e') != std::string::npos || value.find('E') != std::string::npos) return value;
    return value + ".0";
}

std::string asmSymbolPart(const std::string& value) {
    return asmSymbolName(value);
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

struct DynamicDispatchTarget {
    std::string runtimeClassName;
    std::string targetOwnerName;
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
        out << tailEntryLabel() << ":\n";
        for (size_t i = 0; i < function.instructions.size();) {
            if (isTailRecursiveCallAt(i)) {
                emitTailRecursiveCall(function.instructions[i]);
                i += 2;
                continue;
            }
            emitInstruction(function.instructions[i]);
            ++i;
        }
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

    std::string tailEntryLabel() const {
        return "L_tail_entry_" + asmSymbolPart(function.name);
    }

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
        if (type == valueTypes.end() || type->second.empty()) {
            codegenError("type IR inconnu pour la valeur '" + name + "'");
        }
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
                emitMethodCall(instruction, true);
                break;
            case IROpcode::StaticMethodCall:
                emitMethodCall(instruction, false);
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
            case IROpcode::NewFloatArray:
                emitNewFloatArray(instruction);
                break;
            case IROpcode::FloatArrayLength:
                emitFloatArrayLength(instruction);
                break;
            case IROpcode::FloatArrayGet:
                emitFloatArrayGet(instruction);
                break;
            case IROpcode::FloatArraySet:
                emitFloatArraySet(instruction);
                break;
            case IROpcode::NewDoubleArray:
                emitNewDoubleArray(instruction);
                break;
            case IROpcode::DoubleArrayLength:
                emitDoubleArrayLength(instruction);
                break;
            case IROpcode::DoubleArrayGet:
                emitDoubleArrayGet(instruction);
                break;
            case IROpcode::DoubleArraySet:
                emitDoubleArraySet(instruction);
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
            case IROpcode::NewObjectArray:
                emitNewObjectArray(instruction);
                break;
            case IROpcode::ObjectArrayLength:
                emitObjectArrayLength(instruction);
                break;
            case IROpcode::ObjectArrayGet:
                emitObjectArrayGet(instruction);
                break;
            case IROpcode::ObjectArraySet:
                emitObjectArraySet(instruction);
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
                out << RuntimeValues::asmCompareTaggedFalse("rax");
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

    bool isTailRecursiveCallAt(size_t index) const {
        if (index + 1 >= function.instructions.size()) return false;
        const IRInstruction& call = function.instructions[index];
        if (call.opcode != IROpcode::Call || call.operation != function.name) return false;
        if (call.operands.size() != function.parameters.size()) return false;

        const IRInstruction& next = function.instructions[index + 1];
        if (next.opcode == IROpcode::Return) {
            return next.operands.size() == 1 && next.operands[0] == call.result;
        }
        if (next.opcode != IROpcode::Jump || next.operands.size() != 1) return false;
        return jumpTargetReturnsCallResult(next.operands[0], call.result);
    }

    bool jumpTargetReturnsCallResult(const std::string& targetLabel, const std::string& callResult) const {
        for (size_t i = 0; i < function.instructions.size(); ++i) {
            const IRInstruction& label = function.instructions[i];
            if (label.opcode != IROpcode::Label || label.operands.empty() ||
                label.operands[0] != targetLabel) {
                continue;
            }

            size_t next = i + 1;
            std::string returningPhi;
            while (next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Phi) {
                const IRInstruction& phi = function.instructions[next];
                if (std::find(phi.operands.begin(), phi.operands.end(), callResult) != phi.operands.end()) {
                    returningPhi = phi.result;
                }
                ++next;
            }
            return !returningPhi.empty() &&
                   next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Return &&
                   function.instructions[next].operands.size() == 1 &&
                   function.instructions[next].operands[0] == returningPhi;
        }
        return false;
    }

    void emitTailRecursiveCall(const IRInstruction& instruction) {
        if (instruction.operands.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel tail-rec IR avec plus de 6 arguments");
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], callingConvention.globalArgumentRegisters[i]);
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            storeRegister("%" + function.parameters[i].name, callingConvention.globalArgumentRegisters[i]);
        }
        out << "    jmp " << tailEntryLabel() << "\n";
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
            out << "    sar rax, 1\n    sar rbx, 1\n    test rbx, rbx\n    je Runtime_division_by_zero\n    cqo\n    idiv rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "%") {
            out << "    sar rax, 1\n    sar rbx, 1\n    test rbx, rbx\n    je Runtime_division_by_zero\n    cqo\n    idiv rbx\n    mov rax, rdx\n    shl rax, 1\n    or rax, 1\n";
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
            out << label << ": dq __float64__(" << normalizeFloatConstant(instruction.operands[0]) << ")\n";
            out << "section .text\n";
            out << "    mov rax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        if (instruction.type == "Float") {
            const std::string label = "nabla_float_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result);
            out << "section .data\n";
            out << label << ": dd __float32__(" << normalizeFloatConstant(instruction.operands[0]) << ")\n";
            out << "section .text\n";
            out << "    xor rax, rax\n";
            out << "    mov eax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        if (instruction.type == "Bool") {
            long long parsed = 0;
            try {
                parsed = std::stoll(instruction.operands[0]);
            } catch (...) {
                codegenError("constante Bool IR invalide: " + instruction.operands[0]);
            }
            if (parsed != RuntimeValues::kTaggedFalse && parsed != RuntimeValues::kTaggedTrue) {
                codegenError("constante Bool IR non taggee: " + instruction.operands[0]);
            }
            out << "    mov rax, " << parsed << "\n";
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
        } else if (instruction.operation == "readLine") {
            out << "    call Runtime_readLine\n";
        } else if (instruction.operation == "readFile") {
            out << "    call Runtime_readFile\n";
        } else if (instruction.operation == "writeFile") {
            out << "    call Runtime_writeFile\n";
        } else if (instruction.operation == "appendFile") {
            out << "    call Runtime_appendFile\n";
        } else if (instruction.operation == "fileExists") {
            out << "    call Runtime_fileExists\n";
        } else if (instruction.operation == "deleteFile") {
            out << "    call Runtime_deleteFile\n";
        } else if (instruction.operation == "renameFile") {
            out << "    call Runtime_renameFile\n";
        } else if (instruction.operation == "createDir") {
            out << "    call Runtime_createDir\n";
        } else if (instruction.operation == "parseInt") {
            out << "    call Runtime_stringToInt\n";
        } else if (instruction.operation == "timeSeed") {
            out << "    call Runtime_timeSeed\n";
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
        if (layoutIt == context.classLayouts.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        std::vector<int> offsets;
        for (const auto& field : collectClassFieldsInHierarchyForLayout(context, layoutClassName)) {
            auto offsetIt = layoutIt->second.find(field.first);
            if (offsetIt == layoutIt->second.end()) {
                codegenError("offset de champ inconnu: " + layoutClassName + "." + field.first);
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

    long long classIdFor(const std::string& className) const {
        const std::string classBaseName = genericBaseName(className);
        long long nextClassId = 1000;
        for (const auto& [candidateName, _] : context.classes) {
            if (candidateName == classBaseName) return nextClassId;
            ++nextClassId;
        }
        return 0;
    }

    bool canUseRuntimeClassDispatch(const std::string& className, const std::string& methodName) const {
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        const std::string classBaseName = genericBaseName(className);
        auto classIt = context.classes.find(classBaseName);
        if (classIt == context.classes.end()) return false;
        auto methodIt = classIt->second.methods.find(methodBaseName);
        if (methodIt != classIt->second.methods.end() &&
            !methodIt->second.typeParameters.empty() && !parameterizedMethod) {
            return false;
        }
        return true;
    }

    bool isAbstractTraitMethod(const std::string& className, const std::string& methodName) const {
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        auto classIt = context.classes.find(genericBaseName(className));
        if (classIt == context.classes.end() || !classIt->second.isTrait) return false;
        auto methodIt = classIt->second.methods.find(methodBaseName);
        return methodIt != classIt->second.methods.end() && methodIt->second.isAbstract;
    }

    void emitTraitAbstractDispatchFallback(const std::string& className, const std::string& methodName) {
        if (isAbstractTraitMethod(className, methodName)) {
            out << "    jmp Runtime_trait_dispatch_error\n";
            return;
        }
        out << "    call " << asmFunctionName(methodFunctionName(className, methodName)) << "\n";
    }

    std::vector<DynamicDispatchTarget> dynamicDispatchTargets(
        const std::string& className, const std::string& methodName) const {
        std::vector<DynamicDispatchTarget> targets;
        if (!canUseRuntimeClassDispatch(className, methodName)) return targets;

        const std::string staticClassName = genericBaseName(className);
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        std::set<std::pair<std::string, std::string>> seen;
        for (const auto& [runtimeClassName, runtimeClass] : context.classes) {
            if (runtimeClassName == staticClassName) continue;
            if (runtimeClass.isTrait) continue;
            if (!runtimeClass.typeParameters.empty()) continue;
            if (!isTypeAssignable(context, runtimeClassName, className)) continue;

            auto methodLookup = resolveClassMethodInHierarchy(context, runtimeClassName, methodBaseName);
            if (!methodLookup) continue;
            const std::string targetOwnerName = genericBaseName(methodLookup->ownerClassName);
            if (targetOwnerName == staticClassName) continue;
            auto targetOwnerIt = context.classes.find(targetOwnerName);
            if (targetOwnerIt == context.classes.end()) continue;
            const bool staticOwnerIsTrait = [&]() {
                auto staticOwnerIt = context.classes.find(staticClassName);
                return staticOwnerIt != context.classes.end() && staticOwnerIt->second.isTrait;
            }();
            if (staticOwnerIsTrait && targetOwnerIt->second.isTrait) continue;
            auto targetMethodIt = targetOwnerIt->second.methods.find(methodBaseName);
            if (targetMethodIt == targetOwnerIt->second.methods.end()) continue;
            if (!targetMethodIt->second.typeParameters.empty() && !parameterizedMethod) continue;

            if (seen.insert({runtimeClassName, targetOwnerName}).second) {
                targets.push_back({runtimeClassName, targetOwnerName});
            }
        }
        return targets;
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
        out << "    mov qword [rbx], " << classIdFor(instruction.operation) << "\n";
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], "rax");
            out << "    mov [rbx + " << fieldOffsets[i] << "], rax\n";
        }
        storeRegister(instruction.result, "rbx");
    }

    std::string nativeArrayDefaultValue(IROpcode opcode) const {
        switch (opcode) {
            case IROpcode::NewFloatArray:
            case IROpcode::NewDoubleArray:
            case IROpcode::NewObjectArray:
                return std::to_string(RuntimeValues::kNullSlot);
            case IROpcode::NewIntArray:
            case IROpcode::NewLongArray:
            case IROpcode::NewBoolArray:
                return std::to_string(RuntimeValues::kTaggedZero);
            default:
                codegenError("opcode IR de tableau natif inattendu");
        }
    }

    void emitNewNativeArray(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << RuntimeValues::asmCompareTaggedFalse("rax");
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
        out << "    mov qword [rbx + 16 + rcx * 8], " << nativeArrayDefaultValue(instruction.opcode) << "\n";
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

    void emitNewFloatArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewDoubleArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewBoolArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewObjectArray(const IRInstruction& instruction) {
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

    void emitFloatArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitDoubleArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitBoolArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitObjectArrayLength(const IRInstruction& instruction) {
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

    void emitFloatArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitDoubleArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitBoolArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitObjectArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitNativeArraySet(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rcx");
        emitArrayBoundsCheck("rbx", "rcx");
        loadValue(instruction.operands[2], "rax");
        out << "    mov [rbx + 16 + rcx * 8], rax\n";
        out << RuntimeValues::asmMoveTaggedFalse("rax");
        storeRegister(instruction.result, "rax");
    }

    void emitIntArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitLongArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitFloatArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitDoubleArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitBoolArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitObjectArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitFieldLoad(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    mov rax, [rax + " << fieldOffsetFor(instruction.operation) << "]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitBoxedValue(const IRInstruction& instruction, long long tag) {
        if (instruction.operands.size() != 1) codegenError("boxing IR invalide");
        loadValue(instruction.operands[0], "r10");
        out << "    mov rdi, 16\n";
        out << "    call Runtime_alloc\n";
        out << "    mov qword [rax], " << tag << "\n";
        out << "    mov [rax + 8], r10\n";
        storeRegister(instruction.result, "rax");
    }

    void emitMethodCall(const IRInstruction& instruction, bool allowDynamicDispatch) {
        auto [className, methodName] = splitQualifiedMember(instruction.operation);
        if (instruction.operands.empty()) codegenError("appel de methode IR sans receveur");
        if (methodName == "box") {
            if (className == "Int") return emitBoxedValue(instruction, RuntimeValues::kBoxedIntTag);
            if (className == "Long") return emitBoxedValue(instruction, RuntimeValues::kBoxedLongTag);
            if (className == "Float") return emitBoxedValue(instruction, RuntimeValues::kBoxedFloatTag);
            if (className == "Double") return emitBoxedValue(instruction, RuntimeValues::kBoxedDoubleTag);
            if (className == "Bool") return emitBoxedValue(instruction, RuntimeValues::kBoxedBoolTag);
            if (className == "Char") return emitBoxedValue(instruction, RuntimeValues::kBoxedCharTag);
            if (className == "Unit") return emitBoxedValue(instruction, RuntimeValues::kBoxedUnitTag);
            codegenError("boxing non supporté pour " + className);
        }

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
        } else if (className == "Float" && methodName == "toString") {
            out << "    call Float_method_toString\n";
        } else if (className == "Double" && methodName == "toString") {
            out << "    call Double_method_toString\n";
        } else if (className == "Bool" && methodName == "toString") {
            out << "    call Bool_method_toString\n";
        } else if (className == "Char" && methodName == "toString") {
            out << "    call Char_method_toString\n";
        } else if (className == "Any" &&
                   (methodName == "toString" || methodName == "hashCode" || methodName == "equals")) {
            const auto targets = allowDynamicDispatch
                ? dynamicDispatchTargets(className, methodName)
                : std::vector<DynamicDispatchTarget>{};
            const std::string fallbackMethod =
                methodName == "toString" ? "Any_toString" :
                methodName == "hashCode" ? "Any_hashCode" :
                "Any_equals";
            if (targets.empty()) {
                out << "    call " << fallbackMethod << "\n";
            } else {
                const std::string dispatchPrefix =
                    ".L_dispatch_" + asmSymbolPart(function.name) + "_" +
                    asmSymbolPart(instruction.result);
                const std::string fallbackLabel = dispatchPrefix + "_fallback";
                const std::string doneLabel = dispatchPrefix + "_done";
                out << "    test rdi, 1\n";
                out << "    jnz " << fallbackLabel << "\n";
                out << "    mov r10, [rdi]\n";
                for (size_t i = 0; i < targets.size(); ++i) {
                    out << "    cmp r10, " << classIdFor(targets[i].runtimeClassName) << "\n";
                    out << "    je " << dispatchPrefix << "_" << i << "\n";
                }
                out << "    jmp " << fallbackLabel << "\n";
                for (size_t i = 0; i < targets.size(); ++i) {
                    out << dispatchPrefix << "_" << i << ":\n";
                    out << "    call " << asmFunctionName(methodFunctionName(targets[i].targetOwnerName, methodName)) << "\n";
                    out << "    jmp " << doneLabel << "\n";
                }
                out << fallbackLabel << ":\n";
                out << "    call " << fallbackMethod << "\n";
                out << doneLabel << ":\n";
            }
        } else if (className == "Int" && methodName == "toLong") {
            out << "    mov rax, rdi\n";
        } else if (className == "String" && methodName == "length") {
            out << "    mov rax, [rdi + 8]\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "+") {
            out << "    call Runtime_stringConcat\n";
        } else if (className == "String" && methodName == "hashCode") {
            out << "    call Runtime_stringHashCode\n";
        } else if (className == "String" && methodName == "isEmpty") {
            out << "    cmp qword [rdi + 8], 0\n";
            out << "    sete al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "nonEmpty") {
            out << "    cmp qword [rdi + 8], 0\n";
            out << "    setne al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "charAt") {
            out << "    mov rax, rsi\n";
            out << "    sar rax, 1\n";
            out << "    cmp rax, 0\n";
            out << "    jl Runtime_bounds_error\n";
            out << "    cmp rax, [rdi + 8]\n";
            out << "    jge Runtime_bounds_error\n";
            out << "    mov rbx, [rdi + 16]\n";
            out << "    movzx rax, byte [rbx + rax]\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && (methodName == "==" || methodName == "!=")) {
            out << "    call Runtime_stringEquals\n";
            if (methodName == "!=") {
                out << "    xor rax, 2\n";
            }
        } else if (className == "String" && methodName == "startsWith") {
            out << "    call Runtime_stringStartsWith\n";
        } else if (className == "String" && methodName == "endsWith") {
            out << "    call Runtime_stringEndsWith\n";
        } else if (className == "String" && methodName == "indexOf") {
            out << "    call Runtime_stringIndexOf\n";
        } else if (className == "String" && methodName == "contains") {
            out << "    call Runtime_stringIndexOf\n";
            out << "    cmp rax, -1\n";
            out << "    setne al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "toInt") {
            out << "    call Runtime_stringToInt\n";
        } else if (className == "String" && methodName == "toCharArray") {
            out << "    call Runtime_stringToCharArray\n";
        } else if (className == "String" && methodName == "split") {
            out << "    call Runtime_stringSplit\n";
        } else if (className == "String" && methodName == "substring") {
            out << "    call Runtime_stringSubstring\n";
        } else if (className == "String" && methodName == "repeat") {
            out << "    call Runtime_stringRepeat\n";
        } else if (className == "String" && methodName == "trim") {
            out << "    call Runtime_stringTrim\n";
        } else if (allowDynamicDispatch) {
            const auto targets = dynamicDispatchTargets(className, methodName);
            if (targets.empty()) {
                emitTraitAbstractDispatchFallback(className, methodName);
            } else {
                const std::string dispatchPrefix =
                    ".L_dispatch_" + asmSymbolPart(function.name) + "_" +
                    asmSymbolPart(instruction.result);
                const std::string fallbackLabel = dispatchPrefix + "_fallback";
                const std::string doneLabel = dispatchPrefix + "_done";
                out << "    mov r10, [rdi]\n";
                for (size_t i = 0; i < targets.size(); ++i) {
                    out << "    cmp r10, " << classIdFor(targets[i].runtimeClassName) << "\n";
                    out << "    je " << dispatchPrefix << "_" << i << "\n";
                }
                out << "    jmp " << fallbackLabel << "\n";
                for (size_t i = 0; i < targets.size(); ++i) {
                    out << dispatchPrefix << "_" << i << ":\n";
                    out << "    call " << asmFunctionName(methodFunctionName(targets[i].targetOwnerName, methodName)) << "\n";
                    out << "    jmp " << doneLabel << "\n";
                }
                out << fallbackLabel << ":\n";
                emitTraitAbstractDispatchFallback(className, methodName);
                out << doneLabel << ":\n";
            }
        } else {
            out << "    call " << asmFunctionName(methodFunctionName(instruction.operation)) << "\n";
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
