#include "ast.hpp"

namespace {
const std::vector<std::string> GLOBAL_ARGUMENT_REGISTERS = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
const std::vector<std::string> METHOD_ARGUMENT_REGISTERS = {"rsi", "rdx", "rcx", "r8", "r9"};

void validateArguments(
    const std::string& callableName,
    const std::vector<std::unique_ptr<ASTNode>>& arguments,
    const std::vector<CompilerContext::ParameterInfo>& parameters,
    const SourceLocation& location) {
    if (arguments.size() != parameters.size()) {
        throw CompilerError(ErrorKind::Semantic, location,
            callableName + ": " + std::to_string(parameters.size()) +
            " argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
    }
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i]->getType() != parameters[i].type) {
            throw CompilerError(ErrorKind::Semantic, arguments[i]->getLocation(),
                callableName + ", paramètre '" + parameters[i].name + "': type '" +
                parameters[i].type + "' attendu, '" + arguments[i]->getType() + "' reçu");
        }
    }
}

void pushArguments(
    std::ofstream& out,
    CompilerContext& context,
    const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    for (auto argument = arguments.rbegin(); argument != arguments.rend(); ++argument) {
        (*argument)->generateASM(out, context);
        out << "    push rax\n";
    }
}

void popArguments(std::ofstream& out, const std::vector<std::string>& registers, size_t argumentCount) {
    for (size_t i = 0; i < argumentCount; ++i) {
        out << "    pop " << registers[i] << "\n";
    }
}
}

std::string ProgramNode::getType() {
    return "Unit";
}

void ProgramNode::generateASM(std::ofstream& out, CompilerContext& context) {
    for (const auto& el : elements) if (el) el->generateASM(out, context);
}

void ProgramNode::validateSemantics(CompilerContext& context) {
    for (const auto& element : elements) {
        if (element) element->validateSemantics(context);
    }
}

IntNode::IntNode(std::string val) : value(std::move(val)) {}

std::string IntNode::getType() {
    return "Int";
}

void IntNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

NewNode::NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments)
    : className(std::move(clName)), args(std::move(arguments)) {}

std::string NewNode::getType() {
    return className;
}

void NewNode::validateSemantics(CompilerContext& context) {
    for (const auto& arg : args) arg->validateSemantics(context);

    auto classIt = context.classes.find(className);
    if (classIt == context.classes.end()) {
        semanticError("classe inconnue dans 'new': " + className);
    }
    const auto& fields = classIt->second.fields;
    if (args.size() != fields.size()) {
        semanticError(
            "Constructeur de '" + className + "': " + std::to_string(fields.size()) +
            " argument(s) attendu(s), " + std::to_string(args.size()) + " reçu(s)");
    }
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i]->getType() != fields[i].type) {
            throw CompilerError(ErrorKind::Semantic, args[i]->getLocation(),
                "Constructeur de '" + className + "', champ '" + fields[i].name +
                "': type '" + fields[i].type + "' attendu, '" + args[i]->getType() + "' reçu");
        }
    }
}

MethodCallNode::MethodCallNode(
    std::unique_ptr<ASTNode> rec, std::string method, std::vector<std::unique_ptr<ASTNode>> args)
    : receiver(std::move(rec)), methodName(std::move(method)), arguments(std::move(args)) {}

std::string MethodCallNode::getType() {
    return resolvedType;
}

void MethodCallNode::validateSemantics(CompilerContext& context) {
    receiver->validateSemantics(context);
    for (const auto& argument : arguments) argument->validateSemantics(context);

    const std::string receiverType = receiver->getType();
    if (receiverType == "Int") {
        const bool binaryMethod =
            methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/" ||
            methodName == "==" || methodName == "!=" || methodName == "<" || methodName == ">" ||
            methodName == "<=" || methodName == ">=";
        if (binaryMethod) {
            if (arguments.size() != 1) {
                semanticError("la méthode Int." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode Int." + methodName + " attend un argument de type Int");
            }
            resolvedType = "Int";
            return;
        }
        if (methodName == "toString") {
            if (!arguments.empty()) semanticError("la méthode Int.toString n'accepte aucun argument");
            resolvedType = "String";
            return;
        }
        semanticError("méthode inconnue: Int." + methodName);
    }

    auto classIt = context.classes.find(receiverType);
    if (classIt == context.classes.end()) {
        semanticError("type receveur inconnu pour l'appel de méthode: " + receiverType);
    }
    auto methodIt = classIt->second.methods.find(methodName);
    if (methodIt == classIt->second.methods.end()) {
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    validateArguments(receiverType + "." + methodName, arguments, methodIt->second.parameters, location);
    resolvedType = methodIt->second.returnType;
}

FunctionCallNode::FunctionCallNode(std::string functionName, std::vector<std::unique_ptr<ASTNode>> args)
    : name(std::move(functionName)), arguments(std::move(args)) {}

std::string FunctionCallNode::getType() {
    return resolvedType;
}

void FunctionCallNode::validateSemantics(CompilerContext& context) {
    for (const auto& argument : arguments) argument->validateSemantics(context);
    auto function = context.functions.find(name);
    if (function == context.functions.end()) {
        semanticError("fonction inconnue: " + name);
    }
    validateArguments(name, arguments, function->second.parameters, location);
    resolvedType = function->second.returnType;
}

FieldAccessNode::FieldAccessNode(std::string clName, std::string field, std::string fieldType)
    : className(std::move(clName)), fieldName(std::move(field)), type(std::move(fieldType)) {}

std::string FieldAccessNode::getType() {
    return type;
}

void FieldAccessNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

IfNode::IfNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> thenBranch, std::unique_ptr<ASTNode> elseBranch)
    : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

std::string IfNode::getType() {
    return resolvedType;
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

void IfNode::validateSemantics(CompilerContext& context) {
    condition->validateSemantics(context);
    thenBranch->validateSemantics(context);
    elseBranch->validateSemantics(context);
    if (condition->getType() != "Int") {
        semanticError("la condition d'un 'if' doit être de type Int");
    }
    if (thenBranch->getType() != elseBranch->getType()) {
        semanticError("les branches d'un 'if' doivent avoir le même type");
    }
    resolvedType = thenBranch->getType();
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

void BlockNode::validateSemantics(CompilerContext& context) {
    for (const auto& expr : expressions) {
        if (expr) expr->validateSemantics(context);
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

void WhileNode::validateSemantics(CompilerContext& context) {
    condition->validateSemantics(context);
    body->validateSemantics(context);
    if (condition->getType() != "Int") {
        semanticError("la condition d'un 'while' doit être de type Int");
    }
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

void ForNode::validateSemantics(CompilerContext& context) {
    count->validateSemantics(context);
    body->validateSemantics(context);
    if (count->getType() != "Int") {
        semanticError("le compteur d'un 'for' doit être de type Int");
    }
}

FunctionDefNode::FunctionDefNode(
    std::string clName, std::string name, std::string declaredReturnType,
    std::vector<Parameter> params, std::unique_ptr<ASTNode> body)
    : className(std::move(clName)), name(std::move(name)), returnType(std::move(declaredReturnType)),
      parameters(std::move(params)), body(std::move(body)) {}

std::string FunctionDefNode::getType() {
    return "Unit";
}

void FunctionDefNode::validateSemantics(CompilerContext& context) {
    context.semanticSymbolTypes.clear();
    for (const auto& parameter : parameters) {
        context.semanticSymbolTypes[parameter.symbolName] = parameter.type;
    }
    if (body) body->validateSemantics(context);
    const bool knownType =
        returnType == "Int" || returnType == "String" || returnType == "Unit" ||
        context.classes.count(returnType) != 0;
    if (!knownType) {
        semanticError("type de retour inconnu '" + returnType + "' pour la fonction '" + name + "'");
    }
    if (body && body->getType() != returnType) {
        semanticError(
            "Type de retour invalide pour '" + name + "': '" + returnType +
            "' attendu, '" + body->getType() + "' reçu");
    }
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
    pushArguments(out, context, arguments);
    receiver->generateASM(out, context);
    out << "    mov rdi, rax\n";
    popArguments(out, METHOD_ARGUMENT_REGISTERS, arguments.size());
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

void FunctionCallNode::generateASM(std::ofstream& out, CompilerContext& context) {
    pushArguments(out, context, arguments);
    popArguments(out, GLOBAL_ARGUMENT_REGISTERS, arguments.size());
    out << "    call " << name << "\n";
}

void FieldAccessNode::generateASM(std::ofstream& out, CompilerContext& context) {
    int offset = context.classLayouts[className][fieldName];
    out << "    mov rax, [rbp - " << context.currentThisOffsetFromRbp << "]\n";
    out << "    mov rax, [rax + " << offset << "]\n";
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
    context.currentThisOffsetFromRbp = 0;
    if (!className.empty()) {
        localStackSize += 8;
        context.currentThisOffsetFromRbp = localStackSize;
    }
    for (auto& parameter : parameters) {
        localStackSize += 8;
        parameter.offsetFromRbp = localStackSize;
    }
    if (body) body->allocateLocals(localStackSize);
    if (localStackSize > 0) out << "    sub rsp, " << localStackSize << "\n";
    if (!className.empty()) {
        out << "    mov [rbp - " << context.currentThisOffsetFromRbp << "], rdi\n";
    }
    const auto& parameterRegisters = className.empty() ? GLOBAL_ARGUMENT_REGISTERS : METHOD_ARGUMENT_REGISTERS;
    for (size_t i = 0; i < parameters.size(); ++i) {
        const auto& parameter = parameters[i];
        context.localVars[parameter.symbolName] = {parameter.offsetFromRbp, false, parameter.type};
        out << "    mov [rbp - " << parameter.offsetFromRbp << "], " << parameterRegisters[i] << "\n";
    }
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
        codegenError("variable non déclarée: " + name);
    }
    int offset = it->second.offsetFromRbp;
    out << "    mov rax, [rbp - " << offset << "]\n";
}

void IdentifierNode::validateSemantics(CompilerContext& context) {
    if (symbolName == name) {
        semanticError("variable non déclarée: " + name);
    }
    auto symbol = context.semanticSymbolTypes.find(symbolName);
    if (symbol == context.semanticSymbolTypes.end()) {
        semanticError("variable utilisée hors de sa portée: " + name);
    }
    type = symbol->second;
}

VarDeclNode::VarDeclNode(std::string n, std::string symbol, std::unique_ptr<ASTNode> init, bool mut)
    : name(std::move(n)), symbolName(std::move(symbol)), initializer(std::move(init)), isMutable(mut) {}

std::string VarDeclNode::getType() {
    return initializer ? initializer->getType() : "Unit";
}

void VarDeclNode::generateASM(std::ofstream& out, CompilerContext& context) {
    if (!initializer) codegenError("initialisateur requis pour la déclaration de variable: " + name);
    initializer->generateASM(out, context);
    context.localVars[symbolName] = { offsetFromRbp, isMutable, initializer->getType() };
    out << "    mov [rbp - " << offsetFromRbp << "], rax\n";
}

void VarDeclNode::allocateLocals(int& nextOffset) {
    nextOffset += 8;
    offsetFromRbp = nextOffset;
}

void VarDeclNode::validateSemantics(CompilerContext& context) {
    initializer->validateSemantics(context);
    context.semanticSymbolTypes[symbolName] = initializer->getType();
}

AssignmentNode::AssignmentNode(
    std::string n, std::string symbol, std::string type, bool isMutable, std::unique_ptr<ASTNode> v)
    : name(std::move(n)), symbolName(std::move(symbol)), targetType(std::move(type)),
      targetMutable(isMutable), value(std::move(v)) {}

std::string AssignmentNode::getType() {
    return value ? value->getType() : "Unit";
}

void AssignmentNode::generateASM(std::ofstream& out, CompilerContext& context) {
    auto it = context.localVars.find(symbolName);
    if (it == context.localVars.end()) codegenError("affectation sur variable non déclarée: " + name);
    if (!it->second.isMutable) codegenError("impossible d'affecter à une 'val' immuable: " + name);
    value->generateASM(out, context);
    int offset = it->second.offsetFromRbp;
    out << "    mov [rbp - " << offset << "], rax\n";
}

void AssignmentNode::validateSemantics(CompilerContext& context) {
    value->validateSemantics(context);
    if (symbolName == name) {
        semanticError("affectation sur variable non déclarée: " + name);
    }
    if (!targetMutable) {
        semanticError("impossible d'affecter à une 'val' immuable: " + name);
    }
    auto symbol = context.semanticSymbolTypes.find(symbolName);
    if (symbol == context.semanticSymbolTypes.end()) {
        semanticError("affectation sur variable hors de sa portée: " + name);
    }
    targetType = symbol->second;
    if (value->getType() != targetType) {
        semanticError(
            "Affectation invalide pour '" + name + "': type '" + targetType +
            "' attendu, '" + value->getType() + "' reçu");
    }
}
