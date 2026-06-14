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

IfNode::IfNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> thenBranch, std::unique_ptr<ASTNode> elseBranch)
    : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

std::string IfNode::getType() {
    return "Int";
}

void IfNode::generateASM(std::ofstream& out, CompilerContext& context) {
    condition->generateASM(out, context);
    int labelId = context.nextLabelId++;
    out << "    cmp rax, 1\n";
    out << "    je .L_else_" << labelId << "\n";
    thenBranch->generateASM(out, context);
    out << "    jmp .L_endif_" << labelId << "\n";
    out << ".L_else_" << labelId << ":\n";
    elseBranch->generateASM(out, context);
    out << ".L_endif_" << labelId << ":\n";
}

void IfNode::allocateLocals(int& nextOffset) {
    thenBranch->allocateLocals(nextOffset);
    elseBranch->allocateLocals(nextOffset);
}

BlockNode::BlockNode(std::vector<std::unique_ptr<ASTNode>> exprs)
    : expressions(std::move(exprs)) {}

std::string BlockNode::getType() {
    return expressions.back()->getType();
}

void BlockNode::generateASM(std::ofstream& out, CompilerContext& context) {
    for (const auto& expr : expressions) {
        if (expr) expr->generateASM(out, context);
    }
}

void BlockNode::allocateLocals(int& nextOffset) {
    for (const auto& expr : expressions) {
        if (expr) expr->allocateLocals(nextOffset);
    }
}

WhileNode::WhileNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> body)
    : condition(std::move(condition)), body(std::move(body)) {}

std::string WhileNode::getType() {
    return "Int";
}

void WhileNode::generateASM(std::ofstream& out, CompilerContext& context) {
    int labelId = context.nextLabelId++;
    out << ".L_while_cond_" << labelId << ":\n";
    condition->generateASM(out, context);
    out << "    cmp rax, 1\n";
    out << "    je .L_while_end_" << labelId << "\n";
    body->generateASM(out, context);
    out << "    jmp .L_while_cond_" << labelId << "\n";
    out << ".L_while_end_" << labelId << ":\n";
    out << "    mov rax, 1\n";
}

void WhileNode::allocateLocals(int& nextOffset) {
    body->allocateLocals(nextOffset);
}

ForNode::ForNode(std::unique_ptr<ASTNode> count, std::unique_ptr<ASTNode> body)
    : count(std::move(count)), body(std::move(body)) {}

std::string ForNode::getType() {
    return "Int";
}

void ForNode::generateASM(std::ofstream& out, CompilerContext& context) {
    count->generateASM(out, context);
    out << "    push rax\n";
    int labelId = context.nextLabelId++;
    out << ".L_for_cond_" << labelId << ":\n";
    out << "    mov rax, [rsp]\n";
    out << "    cmp rax, 1\n";
    out << "    jle .L_for_end_" << labelId << "\n";
    body->generateASM(out, context);
    out << "    mov rax, [rsp]\n";
    out << "    sub rax, 2\n";
    out << "    mov [rsp], rax\n";
    out << "    jmp .L_for_cond_" << labelId << "\n";
    out << ".L_for_end_" << labelId << ":\n";
    out << "    pop rax\n";
    out << "    mov rax, 1\n";
}

void ForNode::allocateLocals(int& nextOffset) {
    body->allocateLocals(nextOffset);
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
    int objectSize = 8 + static_cast<int>(args.size()) * 8;
    out << "    mov rbx, [heap_pointer]\n"
        << "    add qword [heap_pointer], " << objectSize << "\n"
        << "    push rbx\n"
        << "    mov qword [rbx], 0\n";
    int offset = 8;
    for (const auto& arg : args) {
        arg->generateASM(out, context);
        out << "    mov rbx, [rsp]\n    mov [rbx + " << offset << "], rax\n";
        offset += 8;
    }
    out << "    pop rax\n";
}

void MethodCallNode::generateASM(std::ofstream& out, CompilerContext& context) {
    std::string targetClass = receiver->getType();
    if (argument) { argument->generateASM(out, context); out << "    push rax\n"; }
    receiver->generateASM(out, context);
    out << "    mov rdi, rax\n";
    if (argument) { out << "    pop rsi\n"; }
    if (targetClass == "Int") {
        if (methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/") {
            out << "    call Int_method_" << (methodName == "+" ? "add" : methodName == "-" ? "sub" : methodName == "*" ? "mul" : "div") << "\n";
        } else if (methodName == "==" || methodName == "!=" || methodName == "<" || methodName == ">" || methodName == "<=" || methodName == ">=") {
            out << "    mov rax, rdi\n";
            out << "    shr rax, 1\n";
            out << "    mov rbx, rsi\n";
            out << "    shr rbx, 1\n";
            out << "    cmp rax, rbx\n";
            if (methodName == "==") out << "    sete al\n";
            else if (methodName == "!=") out << "    setne al\n";
            else if (methodName == "<") out << "    setl al\n";
            else if (methodName == ">") out << "    setg al\n";
            else if (methodName == "<=") out << "    setle al\n";
            else if (methodName == ">=") out << "    setge al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else {
            out << "    call Int_method_toString\n";
        }
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
    context.localVars.clear();
    int localStackSize = 0;
    if (body) body->allocateLocals(localStackSize);
    if (localStackSize > 0) out << "    sub rsp, " << localStackSize << "\n";
    if (body) body->generateASM(out, context);
    out << "    mov rsp, rbp\n    pop rbp\n    ret\n\n";
}

IdentifierNode::IdentifierNode(std::string n, std::string symbol, std::string resolvedType)
    : name(std::move(n)), symbolName(std::move(symbol)), type(std::move(resolvedType)) {}

std::string IdentifierNode::getType() {
    return type;
}

void IdentifierNode::generateASM(std::ofstream& out, CompilerContext& context) {
    auto it = context.localVars.find(symbolName);
    if (it == context.localVars.end()) {
        throw std::runtime_error("Variable non déclarée: " + name);
    }
    int offset = it->second.offsetFromRbp;
    out << "    mov rax, [rbp - " << offset << "]\n";
}

VarDeclNode::VarDeclNode(std::string n, std::string symbol, std::unique_ptr<ASTNode> init, bool mut)
    : name(std::move(n)), symbolName(std::move(symbol)), initializer(std::move(init)), isMutable(mut) {}

std::string VarDeclNode::getType() {
    return initializer ? initializer->getType() : "Unit";
}

void VarDeclNode::generateASM(std::ofstream& out, CompilerContext& context) {
    if (!initializer) throw std::runtime_error("Initialisateur requis pour la declaration de variable: " + name);
    initializer->generateASM(out, context);
    context.localVars[symbolName] = { offsetFromRbp, isMutable, initializer->getType() };
    out << "    mov [rbp - " << offsetFromRbp << "], rax\n";
}

void VarDeclNode::allocateLocals(int& nextOffset) {
    nextOffset += 8;
    offsetFromRbp = nextOffset;
}

AssignmentNode::AssignmentNode(std::string n, std::string symbol, std::unique_ptr<ASTNode> v)
    : name(std::move(n)), symbolName(std::move(symbol)), value(std::move(v)) {}

std::string AssignmentNode::getType() {
    return value ? value->getType() : "Unit";
}

void AssignmentNode::generateASM(std::ofstream& out, CompilerContext& context) {
    auto it = context.localVars.find(symbolName);
    if (it == context.localVars.end()) throw std::runtime_error("Affectation sur variable non déclarée: " + name);
    if (!it->second.isMutable) throw std::runtime_error("Impossible d'affecter à une 'val' immuable: " + name);
    value->generateASM(out, context);
    int offset = it->second.offsetFromRbp;
    out << "    mov [rbp - " << offset << "], rax\n";
}
