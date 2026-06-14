#include "ast.hpp"
#include "ir.hpp"
#include <algorithm>

namespace {
bool isComparisonMethod(const std::string& methodName) {
    return methodName == "==" || methodName == "!=" || methodName == "<" || methodName == ">" ||
           methodName == "<=" || methodName == ">=";
}

bool isArithmeticMethod(const std::string& methodName) {
    return methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/";
}

bool isBoolBinaryMethod(const std::string& methodName) {
    return methodName == "&&" || methodName == "||" || methodName == "==" || methodName == "!=";
}

bool isKnownBuiltinType(const std::string& type) {
    return type == "Int" || type == "Bool" || type == "String" || type == "Unit" || type == "IntArray";
}

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
}

std::string ASTNode::lowerToIR(IRBuilder& builder) const {
    builder.unsupported(location, "ce nœud AST");
}

std::string ProgramNode::getType() {
    return "Unit";
}

void ProgramNode::validateSemantics(CompilerContext& context) {
    for (const auto& element : elements) {
        if (element) element->validateSemantics(context);
    }
}

std::string ProgramNode::lowerToIR(IRBuilder& builder) const {
    for (const auto& element : elements) {
        if (element) element->lowerToIR(builder);
    }
    return "";
}

IntNode::IntNode(std::string val) : value(std::move(val)) {}

std::string IntNode::getType() {
    return "Int";
}

void IntNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string IntNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value);
}

BoolNode::BoolNode(bool val) : value(val) {}

std::string BoolNode::getType() {
    return "Bool";
}

void BoolNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string BoolNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value ? "1" : "0");
}

NotNode::NotNode(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}

std::string NotNode::getType() {
    return "Bool";
}

void NotNode::validateSemantics(CompilerContext& context) {
    expression->validateSemantics(context);
    if (expression->getType() != "Bool") {
        semanticError("l'opérateur '!' attend une expression de type Bool");
    }
}

std::string NotNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredExpression = expression->lowerToIR(builder);
    std::string falseValue = builder.emitConstant("0");
    return builder.emitBinary("==", loweredExpression, falseValue);
}

StringNode::StringNode(std::string val) : value(std::move(val)) {}

std::string StringNode::getType() {
    return "String";
}

void StringNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string StringNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitStringLiteral(value);
}

NewNode::NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments)
    : className(std::move(clName)), args(std::move(arguments)) {}

std::string NewNode::getType() {
    return className;
}

void NewNode::validateSemantics(CompilerContext& context) {
    for (const auto& arg : args) arg->validateSemantics(context);

    if (className == "IntArray") {
        if (args.size() != 1) {
            semanticError(
                "Constructeur de 'IntArray': 1 argument(s) attendu(s), " +
                std::to_string(args.size()) + " reçu(s)");
        }
        if (args[0]->getType() != "Int") {
            throw CompilerError(
                ErrorKind::Semantic, args[0]->getLocation(),
                "Constructeur de 'IntArray', paramètre 'size': type 'Int' attendu, '" +
                args[0]->getType() + "' reçu");
        }
        return;
    }

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

std::string NewNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredArguments;
    for (const auto& argument : args) loweredArguments.push_back(argument->lowerToIR(builder));
    if (className == "IntArray") return builder.emitNewIntArray(loweredArguments[0]);
    return builder.emitNewObject(className, loweredArguments);
}

MethodCallNode::MethodCallNode(
    std::unique_ptr<ASTNode> rec, std::string method, std::vector<std::unique_ptr<ASTNode>> args)
    : receiver(std::move(rec)), methodName(std::move(method)), arguments(std::move(args)) {}

std::string MethodCallNode::getType() {
    const std::string receiverType = receiver->getType();
    if (receiverType == "Int") {
        if (methodName == "toString") return "String";
        if (isComparisonMethod(methodName)) return "Bool";
        return "Int";
    }
    if (receiverType == "String") {
        if (methodName == "length") return "Int";
    }
    if (receiverType == "Bool") {
        if (isBoolBinaryMethod(methodName)) return "Bool";
    }
    if (receiverType == "IntArray") {
        if (methodName == "set") return "Unit";
        if (methodName == "length" || methodName == "get") return "Int";
    }
    return resolvedType;
}

void MethodCallNode::validateSemantics(CompilerContext& context) {
    receiver->validateSemantics(context);
    for (const auto& argument : arguments) argument->validateSemantics(context);

    const std::string receiverType = receiver->getType();
    if (receiverType == "Int") {
        const bool binaryMethod = isArithmeticMethod(methodName) || isComparisonMethod(methodName);
        if (binaryMethod) {
            if (arguments.size() != 1) {
                semanticError("la méthode Int." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode Int." + methodName + " attend un argument de type Int");
            }
            resolvedType = isComparisonMethod(methodName) ? "Bool" : "Int";
            return;
        }
        if (methodName == "toString") {
            if (!arguments.empty()) semanticError("la méthode Int.toString n'accepte aucun argument");
            resolvedType = "String";
            return;
        }
        semanticError("méthode inconnue: Int." + methodName);
    }
    if (receiverType == "String") {
        if (methodName == "length") {
            if (!arguments.empty()) semanticError("la méthode String.length n'accepte aucun argument");
            resolvedType = "Int";
            return;
        }
        semanticError("méthode inconnue: String." + methodName);
    }
    if (receiverType == "Bool") {
        if (isBoolBinaryMethod(methodName)) {
            if (arguments.size() != 1) {
                semanticError("la méthode Bool." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "Bool") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode Bool." + methodName + " attend un argument de type Bool");
            }
            resolvedType = "Bool";
            return;
        }
        semanticError("méthode inconnue: Bool." + methodName);
    }
    if (receiverType == "IntArray") {
        if (methodName == "length") {
            if (!arguments.empty()) semanticError("la méthode IntArray.length n'accepte aucun argument");
            resolvedType = "Int";
            return;
        }
        if (methodName == "get") {
            if (arguments.size() != 1) semanticError("la méthode IntArray.get attend un argument");
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode IntArray.get attend un index de type Int");
            }
            resolvedType = "Int";
            return;
        }
        if (methodName == "set") {
            if (arguments.size() != 2) semanticError("la méthode IntArray.set attend deux arguments");
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode IntArray.set attend un index de type Int");
            }
            if (arguments[1]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[1]->getLocation(),
                    "la méthode IntArray.set attend une valeur de type Int");
            }
            resolvedType = "Unit";
            return;
        }
        semanticError("méthode inconnue: IntArray." + methodName);
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

std::string MethodCallNode::lowerToIR(IRBuilder& builder) const {
    const std::string receiverType = receiver->getType();
    if (receiverType == "Int") {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode Int.toString");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("Int", "toString", loweredReceiver, {});
        }
        const std::vector<std::string> supported = {
            "+", "-", "*", "/", "==", "!=", "<", ">", "<=", ">="
        };
        if (std::find(supported.begin(), supported.end(), methodName) == supported.end()) {
            builder.unsupported(location, "la méthode Int." + methodName);
        }
        if (arguments.size() != 1) {
            builder.unsupported(location, "l'appel de méthode '" + methodName + "'");
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right);
    }
    if (receiverType == "String") {
        if (methodName != "length" || !arguments.empty()) {
            builder.unsupported(location, "la méthode String." + methodName);
        }
        std::string loweredReceiver = receiver->lowerToIR(builder);
        return builder.emitMethodCall("String", "length", loweredReceiver, {});
    }
    if (receiverType == "Bool") {
        if (!isBoolBinaryMethod(methodName) || arguments.size() != 1) {
            builder.unsupported(location, "la méthode Bool." + methodName);
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right);
    }
    if (receiverType == "IntArray") {
        std::string loweredReceiver = receiver->lowerToIR(builder);
        if (methodName == "length" && arguments.empty()) {
            return builder.emitIntArrayLength(loweredReceiver);
        }
        if (methodName == "get" && arguments.size() == 1) {
            return builder.emitIntArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
        }
        if (methodName == "set" && arguments.size() == 2) {
            return builder.emitIntArraySet(
                loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
        }
        builder.unsupported(location, "la méthode IntArray." + methodName);
    }

    std::string loweredReceiver = receiver->lowerToIR(builder);
    std::vector<std::string> loweredArguments;
    for (const auto& argument : arguments) loweredArguments.push_back(argument->lowerToIR(builder));
    return builder.emitMethodCall(receiverType, methodName, loweredReceiver, loweredArguments);
}

FunctionCallNode::FunctionCallNode(
    std::string functionName, std::vector<std::unique_ptr<ASTNode>> args, std::string initialResolvedType)
    : name(std::move(functionName)), arguments(std::move(args)),
      resolvedType(std::move(initialResolvedType)) {}

std::string FunctionCallNode::getType() {
    if (name == "print") return "Unit";
    return resolvedType;
}

void FunctionCallNode::validateSemantics(CompilerContext& context) {
    for (const auto& argument : arguments) argument->validateSemantics(context);
    if (name == "print") {
        if (arguments.size() != 1) {
            semanticError("print: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "print, paramètre 'value': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "Unit";
        return;
    }
    auto function = context.functions.find(name);
    if (function == context.functions.end()) {
        semanticError("fonction inconnue: " + name);
    }
    validateArguments(name, arguments, function->second.parameters, location);
    resolvedType = function->second.returnType;
}

std::string FunctionCallNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredArguments;
    for (const auto& argument : arguments) loweredArguments.push_back(argument->lowerToIR(builder));
    return builder.emitCall(name, loweredArguments);
}

FunctionReferenceNode::FunctionReferenceNode(
    std::string functionName, std::string functionType, std::vector<Capture> capturedValues)
    : name(std::move(functionName)), resolvedType(std::move(functionType)),
      captures(std::move(capturedValues)) {}

std::string FunctionReferenceNode::getType() {
    return resolvedType;
}

void FunctionReferenceNode::validateSemantics(CompilerContext& context) {
    auto function = context.functions.find(name);
    if (function == context.functions.end()) {
        semanticError("fonction inconnue: " + name);
    }
    if (!functionTypeNameMatchesSignature(resolvedType, function->second)) {
        semanticError("la fonction '" + name + "' n'est pas compatible avec " + resolvedType);
    }
}

std::string FunctionReferenceNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredCaptures;
    for (const auto& capture : captures) {
        loweredCaptures.push_back(builder.emitLoad(capture.symbolName));
    }
    return builder.emitFunctionReference(name, loweredCaptures);
}

FunctionValueCallNode::FunctionValueCallNode(
    std::string functionName, std::string symbol, std::vector<std::unique_ptr<ASTNode>> args,
    std::string initialResolvedType)
    : name(std::move(functionName)), symbolName(std::move(symbol)), arguments(std::move(args)),
      resolvedType(std::move(initialResolvedType)) {}

std::string FunctionValueCallNode::getType() {
    return resolvedType;
}

void FunctionValueCallNode::validateSemantics(CompilerContext& context) {
    for (const auto& argument : arguments) argument->validateSemantics(context);
    auto symbol = context.semanticSymbolTypes.find(symbolName);
    if (symbol == context.semanticSymbolTypes.end()) {
        semanticError("fonction utilisée hors de sa portée: " + name);
    }
    auto functionType = functionTypeFromName(symbol->second);
    if (!functionType) {
        semanticError("la valeur '" + name + "' n'est pas appelable");
    }
    const size_t expectedArgumentCount = functionType->parameterTypes.size();
    if (arguments.size() != expectedArgumentCount) {
        semanticError(
            symbol->second + ": " + std::to_string(expectedArgumentCount) +
            " argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
    }
    resolvedType = functionType->returnType;
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i]->getType() != functionType->parameterTypes[i]) {
            throw CompilerError(
                ErrorKind::Semantic, arguments[i]->getLocation(),
                symbol->second + ", paramètre: type '" + functionType->parameterTypes[i] +
                "' attendu, '" + arguments[i]->getType() + "' reçu");
        }
    }
}

std::string FunctionValueCallNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredFunction = builder.emitLoad(symbolName);
    std::vector<std::string> loweredArguments;
    for (const auto& argument : arguments) loweredArguments.push_back(argument->lowerToIR(builder));
    return builder.emitIndirectCall(loweredFunction, loweredArguments);
}

FieldAccessNode::FieldAccessNode(std::string clName, std::string field, std::string fieldType)
    : className(std::move(clName)), fieldName(std::move(field)), type(std::move(fieldType)) {}

std::string FieldAccessNode::getType() {
    return type;
}

void FieldAccessNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string FieldAccessNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitFieldLoad(location, className, fieldName);
}

IfNode::IfNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> thenBranch, std::unique_ptr<ASTNode> elseBranch)
    : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

std::string IfNode::getType() {
    return resolvedType;
}

void IfNode::validateSemantics(CompilerContext& context) {
    condition->validateSemantics(context);
    thenBranch->validateSemantics(context);
    elseBranch->validateSemantics(context);
    if (condition->getType() != "Bool") {
        semanticError("la condition d'un 'if' doit être de type Bool");
    }
    if (thenBranch->getType() != elseBranch->getType()) {
        semanticError("les branches d'un 'if' doivent avoir le même type");
    }
    resolvedType = thenBranch->getType();
}

std::string IfNode::lowerToIR(IRBuilder& builder) const {
    std::string elseLabel = builder.makeLabel("if.else");
    std::string endLabel = builder.makeLabel("if.end");

    std::string loweredCondition = condition->lowerToIR(builder);
    builder.emitBranchIfFalse(loweredCondition, elseLabel);
    std::string thenValue = thenBranch->lowerToIR(builder);
    builder.emitJump(endLabel);
    builder.emitLabel(elseLabel);
    std::string elseValue = elseBranch->lowerToIR(builder);
    builder.emitJump(endLabel);
    builder.emitLabel(endLabel);
    return builder.emitPhi(thenValue, elseValue);
}

BlockNode::BlockNode(std::vector<std::unique_ptr<ASTNode>> exprs)
    : expressions(std::move(exprs)) {}

std::string BlockNode::getType() {
    return expressions.back()->getType();
}

void BlockNode::validateSemantics(CompilerContext& context) {
    for (const auto& expr : expressions) {
        if (expr) expr->validateSemantics(context);
    }
}

std::string BlockNode::lowerToIR(IRBuilder& builder) const {
    std::string result;
    for (const auto& expression : expressions) {
        if (expression) result = expression->lowerToIR(builder);
    }
    return result;
}

WhileNode::WhileNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTNode> body)
    : condition(std::move(condition)), body(std::move(body)) {}

std::string WhileNode::getType() {
    return "Unit";
}

void WhileNode::validateSemantics(CompilerContext& context) {
    condition->validateSemantics(context);
    body->validateSemantics(context);
    if (condition->getType() != "Bool") {
        semanticError("la condition d'un 'while' doit être de type Bool");
    }
}

std::string WhileNode::lowerToIR(IRBuilder& builder) const {
    std::string conditionLabel = builder.makeLabel("while.cond");
    std::string bodyLabel = builder.makeLabel("while.body");
    std::string endLabel = builder.makeLabel("while.end");

    builder.emitJump(conditionLabel);
    builder.emitLabel(conditionLabel);
    std::string loweredCondition = condition->lowerToIR(builder);
    builder.emitBranchIfFalse(loweredCondition, endLabel);
    builder.emitLabel(bodyLabel);
    body->lowerToIR(builder);
    builder.emitJump(conditionLabel);
    builder.emitLabel(endLabel);
    return builder.emitConstant("0");
}

ForNode::ForNode(std::unique_ptr<ASTNode> count, std::unique_ptr<ASTNode> body)
    : count(std::move(count)), body(std::move(body)) {}

std::string ForNode::getType() {
    return "Unit";
}

void ForNode::validateSemantics(CompilerContext& context) {
    count->validateSemantics(context);
    body->validateSemantics(context);
    if (count->getType() != "Int") {
        semanticError("le compteur d'un 'for' doit être de type Int");
    }
}

std::string ForNode::lowerToIR(IRBuilder& builder) const {
    std::string counterSymbol = builder.makeTemporarySymbol("for.count");
    std::string conditionLabel = builder.makeLabel("for.cond");
    std::string bodyLabel = builder.makeLabel("for.body");
    std::string endLabel = builder.makeLabel("for.end");

    std::string initialCount = count->lowerToIR(builder);
    builder.emitStore(counterSymbol, initialCount);
    builder.emitJump(conditionLabel);

    builder.emitLabel(conditionLabel);
    std::string currentCount = builder.emitLoad(counterSymbol);
    std::string zero = builder.emitConstant("0");
    std::string shouldContinue = builder.emitBinary(">", currentCount, zero);
    builder.emitBranchIfFalse(shouldContinue, endLabel);

    builder.emitLabel(bodyLabel);
    body->lowerToIR(builder);
    std::string countBeforeDecrement = builder.emitLoad(counterSymbol);
    std::string one = builder.emitConstant("1");
    std::string decrementedCount = builder.emitBinary("-", countBeforeDecrement, one);
    builder.emitStore(counterSymbol, decrementedCount);
    builder.emitJump(conditionLabel);

    builder.emitLabel(endLabel);
    return builder.emitConstant("0");
}

FunctionDefNode::FunctionDefNode(
    std::string clName, std::string name, std::string declaredReturnType,
    std::vector<Parameter> params, std::unique_ptr<ASTNode> body, std::vector<Capture> capturedValues)
    : className(std::move(clName)), name(std::move(name)), returnType(std::move(declaredReturnType)),
      parameters(std::move(params)), body(std::move(body)), captures(std::move(capturedValues)) {}

std::string FunctionDefNode::getType() {
    return "Unit";
}

void FunctionDefNode::validateSemantics(CompilerContext& context) {
    context.semanticSymbolTypes.clear();
    for (const auto& capture : captures) {
        context.semanticSymbolTypes[capture.symbolName] = capture.type;
    }
    for (const auto& parameter : parameters) {
        context.semanticSymbolTypes[parameter.symbolName] = parameter.type;
    }
    if (body) body->validateSemantics(context);
    const bool knownType =
        isKnownBuiltinType(returnType) || isFunctionTypeName(returnType) ||
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

std::string FunctionDefNode::lowerToIR(IRBuilder& builder) const {
    std::vector<IRParameter> irParameters;
    if (!className.empty()) {
        irParameters.push_back({"this", className});
    } else if (!captures.empty()) {
        irParameters.push_back({"closure", "Closure"});
    }
    for (const auto& parameter : parameters) {
        irParameters.push_back({parameter.name, parameter.type});
    }
    const std::string functionName = className.empty() ? name : className + "." + name;
    builder.beginFunction(functionName, irParameters, returnType);
    if (!className.empty()) {
        builder.bindThis();
    } else if (!captures.empty()) {
        builder.bindClosure();
        for (size_t i = 0; i < captures.size(); ++i) {
            builder.bindCapture(captures[i].symbolName, static_cast<int>(i));
        }
    }
    for (const auto& parameter : parameters) {
        builder.bindParameter(parameter.symbolName, parameter.name);
    }
    std::string result = body->lowerToIR(builder);
    builder.endFunction(result);
    return "";
}

IdentifierNode::IdentifierNode(std::string n, std::string symbol, std::string resolvedType)
    : name(std::move(n)), symbolName(std::move(symbol)), type(std::move(resolvedType)) {}

std::string IdentifierNode::getType() {
    return type;
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

std::string IdentifierNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitLoad(symbolName);
}

VarDeclNode::VarDeclNode(std::string n, std::string symbol, std::unique_ptr<ASTNode> init, bool mut)
    : name(std::move(n)), symbolName(std::move(symbol)), initializer(std::move(init)), isMutable(mut) {}

std::string VarDeclNode::getType() {
    return initializer ? initializer->getType() : "Unit";
}

void VarDeclNode::validateSemantics(CompilerContext& context) {
    initializer->validateSemantics(context);
    context.semanticSymbolTypes[symbolName] = initializer->getType();
}

std::string VarDeclNode::lowerToIR(IRBuilder& builder) const {
    std::string value = initializer->lowerToIR(builder);
    builder.emitStore(symbolName, value);
    return value;
}

AssignmentNode::AssignmentNode(
    std::string n, std::string symbol, std::string type, bool isMutable, std::unique_ptr<ASTNode> v)
    : name(std::move(n)), symbolName(std::move(symbol)), targetType(std::move(type)),
      targetMutable(isMutable), value(std::move(v)) {}

std::string AssignmentNode::getType() {
    return value ? value->getType() : "Unit";
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

std::string AssignmentNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredValue = value->lowerToIR(builder);
    builder.emitStore(symbolName, loweredValue);
    return loweredValue;
}
