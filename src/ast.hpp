#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <map>

extern std::map<std::string, std::map<std::string, int>> class_layouts;

class ASTNode { 
public: 
    virtual ~ASTNode() = default; 
    virtual void generateASM(std::ofstream& out) = 0; 
    virtual std::string getType() = 0; 
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    std::string getType() override { return "Unit"; }
    void generateASM(std::ofstream& out) override { for (const auto& el : elements) if (el) el->generateASM(out); }
};

class IntNode : public ASTNode {
    std::string value;
public:
    IntNode(std::string val) : value(val) {}
    std::string getType() override { return "Int"; }
    void generateASM(std::ofstream& out) override {
        long long boxed = (std::stoll(value) << 1) | 1;
        out << "    mov rax, " << boxed << " ; Int " << value << "\n";
    }
};

class NewNode : public ASTNode {
    std::string className;
    std::vector<std::unique_ptr<ASTNode>> args;
public:
    NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments)
        : className(clName), args(std::move(arguments)) {}
    std::string getType() override { return className; }
    void generateASM(std::ofstream& out) override {
        out << "    ; --- Instanciation new " << className << " ---\n";
        out << "    mov rbx, [heap_pointer]\n    push rbx\n    mov qword [rbx], 0\n";
        int offset = 8;
        for (const auto& arg : args) {
            arg->generateASM(out);
            out << "    mov rbx, [heap_pointer]\n    mov [rbx + " << offset << "], rax\n";
            offset += 8;
        }
        out << "    add qword [heap_pointer], " << offset << "\n    pop rax\n";
    }
};

class MethodCallNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string methodName;
    std::unique_ptr<ASTNode> argument;
public:
    MethodCallNode(std::unique_ptr<ASTNode> rec, std::string method, std::unique_ptr<ASTNode> arg)
        : receiver(std::move(rec)), methodName(method), argument(std::move(arg)) {}
    std::string getType() override {
        if (methodName == "toString") return "String";
        return "Int";
    }
    void generateASM(std::ofstream& out) override {
        std::string targetClass = receiver->getType();
        if (argument) { argument->generateASM(out); out << "    push rax\n"; }
        receiver->generateASM(out);
        out << "    mov rdi, rax\n";
        if (argument) { out << "    pop rsi\n"; }
        if (targetClass == "Int") {
            out << "    call Int_method_" << (methodName == "+" ? "add" : methodName == "-" ? "sub" : methodName == "*" ? "mul" : methodName == "/" ? "div" : "toString") << "\n";
        } else {
            out << "    call " << targetClass << "_method_" << methodName << "\n";
        }
    }
};

class FieldAccessNode : public ASTNode {
    std::string className; std::string fieldName;
public:
    FieldAccessNode(std::string clName, std::string field) : className(clName), fieldName(field) {}
    std::string getType() override { return "Int"; }
    void generateASM(std::ofstream& out) override {
        int offset = class_layouts[className][fieldName];
        out << "    mov rax, [rdi + " << offset << "]\n";
    }
};

class FunctionDefNode : public ASTNode {
    std::string className; std::string name; std::unique_ptr<ASTNode> body;
public:
    FunctionDefNode(std::string clName, std::string name, std::unique_ptr<ASTNode> body)
        : className(clName), name(name), body(std::move(body)) {}
    std::string getType() override { return "Unit"; }
    void generateASM(std::ofstream& out) override {
        if (name == "main" && className.empty()) {
            out << "section .data\n    global_heap: times 4096 db 0\n    heap_pointer: dq global_heap\n"
                << "    Int_str_obj: dq 0, 0, Int_str_buffer\n    Int_str_buffer: times 32 db 0\n\n"
                << "section .text\nglobal _start\n\n_start:\n    call main\n    mov rdi, rax\n    shr rdi, 1\n    mov rax, 60\n    syscall\n\n"
                << "Int_method_add:\n    mov rax, rdi\n    add rax, rsi\n    sub rax, 1\n    ret\n\n"
                << "Int_method_sub:\n    mov rax, rdi\n    sub rax, rsi\n    add rax, 1\n    ret\n\n"
                << "Int_method_mul:\n    mov rax, rdi\n    shr rax, 1\n    mov rbx, rsi\n    shr rbx, 1\n    imul rax, rbx\n    shl rax, 1\n    or rax, 1\n    ret\n\n"
                << "Int_method_div:\n    mov rax, rdi\n    shr rax, 1\n    mov rbx, rsi\n    shr rbx, 1\n    cqo\n    idiv rbx\n    shl rax, 1\n    or rax, 1\n    ret\n\n"
                << "Int_method_toString:\n    mov rax, rdi\n    shr rax, 1\n    mov rcx, 10\n    mov rsi, Int_str_buffer + 30\n    mov r8, 0\n"
                << ".L_loop_str:\n    dec rsi\n    inc r8\n    cqo\n    idiv rcx\n    add dl, 0x30\n    mov [rsi], dl\n    test rax, rax\n    jnz .L_loop_str\n"
                << "    mov [Int_str_obj + 8], r8\n    mov [Int_str_obj + 16], rsi\n    mov rax, Int_str_obj\n    ret\n\n";
        }
        if (!className.empty()) out << className << "_method_" << name << ":\n";
        else out << name << ":\n";
        out << "    push rbp\n    mov rbp, rsp\n";
        if (body) body->generateASM(out);
        out << "    mov rsp, rbp\n    pop rbp\n    ret\n\n";
    }
};
