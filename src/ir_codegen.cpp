#include "ir_codegen.hpp"
#include "compiler_error.hpp"
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::vector<std::string> GLOBAL_ARGUMENT_REGISTERS = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

[[noreturn]] void codegenError(const std::string& message) {
    throw CompilerError(ErrorKind::Codegen, {"<ir>", 1, 1}, message);
}

std::string asmFunctionName(const std::string& name) {
    std::string result = name;
    for (char& c : result) {
        if (c == '.') c = '_';
    }
    return result;
}

std::string asmLabelName(const std::string& functionName, const std::string& label) {
    return ".L_ir_" + asmFunctionName(functionName) + "_" + asmFunctionName(label);
}

long long boxedInt(const std::string& value) {
    return (std::stoll(value) << 1) | 1;
}

struct PhiInfo {
    std::string result;
    std::vector<std::string> operands;
};

class FunctionEmitter {
public:
    FunctionEmitter(const IRFunction& function, std::ostream& out)
        : function(function), out(out) {
        collectPhiLabels();
        collectSlots();
    }

    void emit() {
        if (function.parameters.size() > GLOBAL_ARGUMENT_REGISTERS.size()) {
            codegenError("le backend IR limite les fonctions globales a 6 parametres");
        }

        out << asmFunctionName(function.name) << ":\n";
        out << "    push rbp\n    mov rbp, rsp\n";
        if (!slotOffsets.empty()) out << "    sub rsp, " << (slotOffsets.size() * 8) << "\n";
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            storeRegister("%" + function.parameters[i].name, GLOBAL_ARGUMENT_REGISTERS[i]);
        }
        for (const auto& instruction : function.instructions) emitInstruction(instruction);
        out << "\n";
    }

private:
    const IRFunction& function;
    std::ostream& out;
    std::map<std::string, int> slotOffsets;
    std::vector<std::string> slotOrder;
    std::map<std::string, std::vector<PhiInfo>> phiLabels;
    std::map<std::string, int> emittedIncomingJumps;

    void collectPhiLabels() {
        for (size_t i = 0; i < function.instructions.size(); ++i) {
            const auto& instruction = function.instructions[i];
            if (instruction.opcode != IROpcode::Label) continue;

            std::vector<PhiInfo> phis;
            size_t next = i + 1;
            while (next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Phi) {
                const auto& phi = function.instructions[next];
                phis.push_back({phi.result, phi.operands});
                ++next;
            }
            if (!phis.empty()) phiLabels[instruction.operands[0]] = phis;
        }
    }

    void collectSlots() {
        for (const auto& parameter : function.parameters) addSlot("%" + parameter.name);
        for (const auto& instruction : function.instructions) {
            if (!instruction.result.empty()) addSlot(instruction.result);
            if (instruction.opcode == IROpcode::Store) addSlot(instruction.operands[0]);
        }
    }

    void addSlot(const std::string& name) {
        if (slotOffsets.count(name)) return;
        slotOrder.push_back(name);
        slotOffsets[name] = static_cast<int>(slotOrder.size()) * 8;
    }

    int offsetFor(const std::string& name) const {
        auto slot = slotOffsets.find(name);
        if (slot == slotOffsets.end()) codegenError("valeur IR inconnue: " + name);
        return slot->second;
    }

    void loadValue(const std::string& name, const std::string& reg) const {
        out << "    mov " << reg << ", [rbp - " << offsetFor(name) << "]\n";
    }

    void storeRegister(const std::string& name, const std::string& reg) const {
        out << "    mov [rbp - " << offsetFor(name) << "], " << reg << "\n";
    }

    void emitInstruction(const IRInstruction& instruction) {
        switch (instruction.opcode) {
            case IROpcode::Constant:
                out << "    mov rax, " << boxedInt(instruction.operands[0]) << "\n";
                storeRegister(instruction.result, "rax");
                break;
            case IROpcode::Binary:
                emitBinary(instruction);
                break;
            case IROpcode::Call:
                emitCall(instruction);
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
            case IROpcode::MethodCall:
            case IROpcode::NewObject:
            case IROpcode::FieldLoad:
                codegenError("instruction IR non supportee par le backend ASM initial");
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
        loadValue(instruction.operands[0], "rax");
        loadValue(instruction.operands[1], "rbx");
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    add rax, rbx\n    sub rax, 1\n";
        } else if (op == "-") {
            out << "    sub rax, rbx\n    add rax, 1\n";
        } else if (op == "*") {
            out << "    shr rax, 1\n    shr rbx, 1\n    imul rax, rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "/") {
            out << "    shr rax, 1\n    shr rbx, 1\n    cqo\n    idiv rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    shr rax, 1\n    shr rbx, 1\n    cmp rax, rbx\n";
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

    void emitCall(const IRInstruction& instruction) {
        if (instruction.operands.size() > GLOBAL_ARGUMENT_REGISTERS.size()) {
            codegenError("appel IR avec plus de 6 arguments");
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], GLOBAL_ARGUMENT_REGISTERS[i]);
        }
        out << "    call " << asmFunctionName(instruction.operation) << "\n";
        storeRegister(instruction.result, "rax");
    }
};
}

void IRCodeGenerator::generateASM(const IRProgram& program, std::ostream& out) const {
    out << "section .text\nglobal _start\n\n";
    out << "_start:\n    call main\n    mov rdi, rax\n    shr rdi, 1\n    mov rax, 60\n    syscall\n\n";
    for (const auto& function : program.functions) {
        FunctionEmitter(function, out).emit();
    }
}
