#include "ast.hpp"

std::string ProgramNode::getType() {
    return "Unit";
}

void ProgramNode::generateASM(std::ofstream& out, CompilerContext& context) {
    for (const auto& el : elements) if (el) el->generateASM(out, context);
}

IntNode::IntNode(std::string val) : value(std::move(val)) {}

std::string IntNode::getType() {
    return "Int";
}

NewNode::NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments)
    : className(std::move(clName)), args(std::move(arguments)) {}

std::string NewNode::getType() {
    return className;
}

MethodCallNode::MethodCallNode(std::unique_ptr<ASTNode> rec, std::string method, std::unique_ptr<ASTNode> arg)
    : receiver(std::move(rec)), methodName(std::move(method)), argument(std::move(arg)) {}

std::string MethodCallNode::getType() {
    if (methodName == "toString") return "String";
    return "Int";
}

FieldAccessNode::FieldAccessNode(std::string clName, std::string field)
    : className(std::move(clName)), fieldName(std::move(field)) {}

std::string FieldAccessNode::getType() {
    return "Int";
}

FunctionDefNode::FunctionDefNode(std::string clName, std::string name, std::unique_ptr<ASTNode> body)
    : className(std::move(clName)), name(std::move(name)), body(std::move(body)) {}

std::string FunctionDefNode::getType() {
    return "Unit";
}

void IntNode::generateASM(std::ofstream& out, CompilerContext& context) {
    (void) context;
    long long boxed = (std::stoll(value) << 1) | 1;
    out << "    mov rax, " << boxed << " ; Int " << value << "\n";
}

void NewNode::generateASM(std::ofstream& out, CompilerContext& context) {
    out << "    ; --- Instanciation new " << className << " ---\n";
    out << "    mov rbx, [heap_pointer]\n    push rbx\n    mov qword [rbx], 0\n";
    int offset = 8;
    for (const auto& arg : args) {
        arg->generateASM(out, context);
        out << "    mov rbx, [heap_pointer]\n    mov [rbx + " << offset << "], rax\n";
        offset += 8;
    }
    out << "    add qword [heap_pointer], " << offset << "\n    pop rax\n";
}

void MethodCallNode::generateASM(std::ofstream& out, CompilerContext& context) {
    std::string targetClass = receiver->getType();
    if (argument) { argument->generateASM(out, context); out << "    push rax\n"; }
    receiver->generateASM(out, context);
    out << "    mov rdi, rax\n";
    if (argument) { out << "    pop rsi\n"; }
    if (targetClass == "Int") {
        out << "    call Int_method_" << (methodName == "+" ? "add" : methodName == "-" ? "sub" : methodName == "*" ? "mul" : methodName == "/" ? "div" : "toString") << "\n";
    } else {
        out << "    call " << targetClass << "_method_" << methodName << "\n";
    }
}

void FieldAccessNode::generateASM(std::ofstream& out, CompilerContext& context) {
    int offset = context.classLayouts[className][fieldName];
    out << "    mov rax, [rdi + " << offset << "]\n";
}

void FunctionDefNode::generateASM(std::ofstream& out, CompilerContext& context) {
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
    if (body) body->generateASM(out, context);
    out << "    mov rsp, rbp\n    pop rbp\n    ret\n\n";
}
