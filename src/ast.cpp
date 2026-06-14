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

bool isIntegerType(const std::string& type) {
    return type == "Int" || type == "Long";
}

bool isNumericType(const std::string& type) {
    return isIntegerType(type) || type == "Float" || type == "Double";
}

bool isBoolBinaryMethod(const std::string& methodName) {
    return methodName == "==" || methodName == "!=";
}

bool isKnownBuiltinType(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" || type == "Double" || type == "Bool" ||
           type == "String" || type == "Unit" || type == "IntArray";
}

bool isKnownTypeInContext(const std::string& type, const CompilerContext& context) {
    if (isKnownBuiltinType(type) || isFunctionTypeName(type)) return true;
    auto classIt = context.classes.find(type);
    if (classIt != context.classes.end()) return classIt->second.typeParameters.empty();
    if (isTypeParameterName(type, context.semanticTypeParameters)) return true;
    auto substitution = genericSubstitutionFor(context, type);
    if (!substitution) return false;
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return false;
    for (const auto& argument : parameterizedType->second) {
        if (!isKnownTypeInContext(argument, context)) return false;
    }
    return true;
}

std::vector<CompilerContext::ParameterInfo> substituteParameters(
    const std::vector<CompilerContext::ParameterInfo>& parameters,
    const std::map<std::string, std::string>& substitution) {
    std::vector<CompilerContext::ParameterInfo> substituted;
    for (auto parameter : parameters) {
        parameter.type = substituteType(parameter.type, substitution);
        substituted.push_back(std::move(parameter));
    }
    return substituted;
}

std::vector<std::string> orderedTypeArguments(
    const std::vector<std::string>& typeParameters,
    const std::map<std::string, std::string>& substitution) {
    std::vector<std::string> arguments;
    for (const auto& typeParameter : typeParameters) {
        auto argument = substitution.find(typeParameter);
        if (argument != substitution.end()) arguments.push_back(argument->second);
    }
    return arguments;
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

LongNode::LongNode(std::string val) : value(std::move(val)) {}

std::string LongNode::getType() {
    return "Long";
}

void LongNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string LongNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value, "Long");
}

DoubleNode::DoubleNode(std::string val) : value(std::move(val)) {}

std::string DoubleNode::getType() {
    return "Double";
}

void DoubleNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string DoubleNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value, "Double");
}

FloatNode::FloatNode(std::string val) : value(std::move(val)) {}

std::string FloatNode::getType() {
    return "Float";
}

void FloatNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string FloatNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value, "Float");
}

BoolNode::BoolNode(bool val) : value(val) {}

std::string BoolNode::getType() {
    return "Bool";
}

void BoolNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string BoolNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value ? "1" : "0", "Bool");
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
    std::string falseValue = builder.emitConstant("0", "Bool");
    return builder.emitBinary("==", loweredExpression, falseValue, "Bool");
}

LogicalNode::LogicalNode(
    std::string op, std::unique_ptr<ASTNode> leftExpr, std::unique_ptr<ASTNode> rightExpr)
    : operation(std::move(op)), left(std::move(leftExpr)), right(std::move(rightExpr)) {}

std::string LogicalNode::getType() {
    return "Bool";
}

void LogicalNode::validateSemantics(CompilerContext& context) {
    left->validateSemantics(context);
    right->validateSemantics(context);
    if (left->getType() != "Bool") {
        semanticError("l'opérateur '" + operation + "' attend une expression gauche de type Bool");
    }
    if (right->getType() != "Bool") {
        throw CompilerError(
            ErrorKind::Semantic, right->getLocation(),
            "l'opérateur '" + operation + "' attend une expression droite de type Bool");
    }
}

std::string LogicalNode::lowerToIR(IRBuilder& builder) const {
    const std::string skipLabel = builder.makeLabel(operation == "&&" ? "and.false" : "or.true");
    const std::string rightLabel = builder.makeLabel(operation == "&&" ? "and.right" : "or.right");
    const std::string endLabel = builder.makeLabel(operation == "&&" ? "and.end" : "or.end");

    std::string leftValue = left->lowerToIR(builder);
    if (operation == "&&") {
        builder.emitBranchIfFalse(leftValue, skipLabel);
        builder.emitJump(rightLabel);
        builder.emitLabel(skipLabel);
        std::string falseValue = builder.emitConstant("0", "Bool");
        builder.emitJump(endLabel);
        builder.emitLabel(rightLabel);
        std::string rightValue = right->lowerToIR(builder);
        builder.emitJump(endLabel);
        builder.emitLabel(endLabel);
        return builder.emitPhi(falseValue, rightValue, "Bool");
    }

    builder.emitBranchIfFalse(leftValue, rightLabel);
    builder.emitJump(skipLabel);
    builder.emitLabel(rightLabel);
    std::string rightValue = right->lowerToIR(builder);
    builder.emitJump(endLabel);
    builder.emitLabel(skipLabel);
    std::string trueValue = builder.emitConstant("1", "Bool");
    builder.emitJump(endLabel);
    builder.emitLabel(endLabel);
    return builder.emitPhi(rightValue, trueValue, "Bool");
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

    const std::string classLookupName = genericBaseName(className);
    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        semanticError("classe inconnue dans 'new': " + className);
    }
    std::map<std::string, std::string> substitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, className)) {
        substitution = *genericSubstitution;
    } else if (!classIt->second.typeParameters.empty()) {
        semanticError(
            "classe générique '" + classLookupName + "' utilisée sans arguments de type");
    }
    const auto& fields = classIt->second.fields;
    if (args.size() != fields.size()) {
        semanticError(
            "Constructeur de '" + className + "': " + std::to_string(fields.size()) +
            " argument(s) attendu(s), " + std::to_string(args.size()) + " reçu(s)");
    }
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string expectedType = substituteType(fields[i].type, substitution);
        if (args[i]->getType() != expectedType) {
            throw CompilerError(ErrorKind::Semantic, args[i]->getLocation(),
                "Constructeur de '" + className + "', champ '" + fields[i].name +
                "': type '" + expectedType + "' attendu, '" + args[i]->getType() + "' reçu");
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
    std::unique_ptr<ASTNode> rec, std::string method, std::vector<std::unique_ptr<ASTNode>> args,
    std::vector<std::string> genericTypeArguments,
    std::string initialResolvedType, std::string initialOwnerType)
    : receiver(std::move(rec)), methodName(std::move(method)), arguments(std::move(args)),
      typeArguments(std::move(genericTypeArguments)),
      resolvedType(std::move(initialResolvedType)), resolvedOwnerType(std::move(initialOwnerType)) {}

std::string MethodCallNode::getType() {
    const std::string receiverType = receiver->getType();
    if (isNumericType(receiverType)) {
        if (isIntegerType(receiverType) && methodName == "toString") return "String";
        if (isComparisonMethod(methodName)) return "Bool";
        return receiverType;
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
    if (isNumericType(receiverType)) {
        const bool binaryMethod = isArithmeticMethod(methodName) || isComparisonMethod(methodName);
        if (binaryMethod) {
            if (arguments.size() != 1) {
                semanticError("la méthode " + receiverType + "." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != receiverType) {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode " + receiverType + "." + methodName +
                    " attend un argument de type " + receiverType);
            }
            resolvedType = isComparisonMethod(methodName) ? "Bool" : receiverType;
            return;
        }
        if (isIntegerType(receiverType) && methodName == "toString") {
            if (!arguments.empty()) {
                semanticError("la méthode " + receiverType + ".toString n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
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

    const std::string classLookupName = genericBaseName(receiverType);
    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        semanticError("type receveur inconnu pour l'appel de méthode: " + receiverType);
    }
    std::map<std::string, std::string> substitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        substitution = *genericSubstitution;
    } else if (!classIt->second.typeParameters.empty()) {
        semanticError(
            "classe générique '" + classLookupName + "' utilisée sans arguments de type");
    }
    auto methodIt = classIt->second.methods.find(methodName);
    if (methodIt == classIt->second.methods.end()) {
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    if (methodIt->second.typeParameters.empty()) {
        if (!typeArguments.empty()) {
            semanticError("la méthode '" + receiverType + "." + methodName + "' n'accepte pas d'arguments de type");
        }
        resolvedTypeArguments.clear();
    } else {
        auto methodSubstitution = genericFunctionSubstitutionFor(methodIt->second, typeArguments);
        if (!methodSubstitution && typeArguments.empty()) {
            std::vector<std::string> actualArgumentTypes;
            for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
            CompilerContext::FunctionSignature substitutedSignature = methodIt->second;
            for (auto& parameter : substitutedSignature.parameters) {
                parameter.type = substituteType(parameter.type, substitution);
            }
            substitutedSignature.returnType = substituteType(substitutedSignature.returnType, substitution);
            methodSubstitution =
                inferGenericFunctionSubstitution(substitutedSignature, actualArgumentTypes);
        }
        if (!methodSubstitution) {
            if (typeArguments.empty()) {
                semanticError(
                    "impossible d'inférer les arguments de type pour la méthode générique '" +
                    receiverType + "." + methodName + "'");
            }
            semanticError(
                "la méthode générique '" + receiverType + "." + methodName + "' attend " +
                std::to_string(methodIt->second.typeParameters.size()) + " argument(s) de type");
        }
        substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
        resolvedTypeArguments = orderedTypeArguments(methodIt->second.typeParameters, *methodSubstitution);
    }
    auto parameters = substituteParameters(methodIt->second.parameters, substitution);
    validateArguments(receiverType + "." + methodName, arguments, parameters, location);
    resolvedType = substituteType(methodIt->second.returnType, substitution);
    resolvedOwnerType = classLookupName;
}

std::string MethodCallNode::lowerToIR(IRBuilder& builder) const {
    const std::string receiverType = receiver->getType();
    if (isNumericType(receiverType)) {
        if (isIntegerType(receiverType) && methodName == "toString") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + ".toString");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall(receiverType, "toString", loweredReceiver, {}, "String");
        }
        const std::vector<std::string> supported = {
            "+", "-", "*", "/", "==", "!=", "<", ">", "<=", ">="
        };
        if (std::find(supported.begin(), supported.end(), methodName) == supported.end()) {
            builder.unsupported(location, "la méthode " + receiverType + "." + methodName);
        }
        if (arguments.size() != 1) {
            builder.unsupported(location, "l'appel de méthode '" + methodName + "'");
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right, isComparisonMethod(methodName) ? "Bool" : receiverType);
    }
    if (receiverType == "String") {
        if (methodName != "length" || !arguments.empty()) {
            builder.unsupported(location, "la méthode String." + methodName);
        }
        std::string loweredReceiver = receiver->lowerToIR(builder);
        return builder.emitMethodCall("String", "length", loweredReceiver, {}, "Int");
    }
    if (receiverType == "Bool") {
        if (!isBoolBinaryMethod(methodName) || arguments.size() != 1) {
            builder.unsupported(location, "la méthode Bool." + methodName);
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right, "Bool");
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
    std::vector<std::string> argumentTypes;
    for (const auto& argument : arguments) argumentTypes.push_back(argument->getType());
    std::vector<std::string> concreteTypeArguments;
    for (const auto& typeArgument : resolvedTypeArguments) {
        concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
    }
    const std::string concreteMethodName = concreteTypeArguments.empty()
        ? methodName
        : formatParameterizedType(methodName, concreteTypeArguments);
    const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
    if ((!concreteTypeArguments.empty() || receiverType != resolvedOwnerType) && !resolvedOwnerType.empty()) {
        builder.registerMethodSpecialization(
            receiverType, resolvedOwnerType, methodName, concreteTypeArguments,
            argumentTypes, concreteReturnType);
    }
    return builder.emitMethodCall(
        receiverType,
        concreteMethodName, loweredReceiver, loweredArguments, concreteReturnType);
}

FunctionCallNode::FunctionCallNode(
    std::string functionName, std::vector<std::unique_ptr<ASTNode>> args,
    std::vector<std::string> genericTypeArguments, std::string initialResolvedType)
    : name(std::move(functionName)), arguments(std::move(args)), typeArguments(std::move(genericTypeArguments)),
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
    if (function->second.typeParameters.empty()) {
        if (!typeArguments.empty()) {
            semanticError("la fonction '" + name + "' n'accepte pas d'arguments de type");
        }
        validateArguments(name, arguments, function->second.parameters, location);
        resolvedType = function->second.returnType;
        resolvedTypeArguments.clear();
        return;
    }
    std::optional<std::map<std::string, std::string>> substitution;
    if (typeArguments.empty()) {
        std::vector<std::string> actualArgumentTypes;
        for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
        substitution = inferGenericFunctionSubstitution(function->second, actualArgumentTypes);
    } else {
        substitution = genericFunctionSubstitutionFor(function->second, typeArguments);
    }
    if (!substitution) {
        if (typeArguments.empty()) {
            semanticError("impossible d'inférer les arguments de type pour la fonction générique '" + name + "'");
        }
        semanticError(
            "la fonction générique '" + name + "' attend " +
            std::to_string(function->second.typeParameters.size()) + " argument(s) de type");
    }
    auto parameters = substituteParameters(function->second.parameters, *substitution);
    validateArguments(name, arguments, parameters, location);
    resolvedType = substituteType(function->second.returnType, *substitution);
    resolvedTypeArguments = orderedTypeArguments(function->second.typeParameters, *substitution);
}

std::string FunctionCallNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredArguments;
    for (const auto& argument : arguments) loweredArguments.push_back(argument->lowerToIR(builder));
    std::vector<std::string> concreteTypeArguments;
    for (const auto& typeArgument : resolvedTypeArguments) {
        concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
    }
    if (!concreteTypeArguments.empty()) {
        const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
        builder.registerFunctionSpecialization(name, concreteTypeArguments, concreteReturnType);
        return builder.emitCall(
            formatParameterizedType(name, concreteTypeArguments),
            loweredArguments, concreteReturnType);
    }
    return builder.emitCall(name, loweredArguments, resolvedType);
}

FunctionReferenceNode::FunctionReferenceNode(
    std::string functionName, std::string functionType,
    std::vector<std::string> genericTypeArguments, std::vector<Capture> capturedValues)
    : name(std::move(functionName)), resolvedType(std::move(functionType)),
      typeArguments(std::move(genericTypeArguments)),
      captures(std::move(capturedValues)) {}

std::string FunctionReferenceNode::getType() {
    return resolvedType;
}

void FunctionReferenceNode::validateSemantics(CompilerContext& context) {
    auto function = context.functions.find(name);
    if (function == context.functions.end()) {
        semanticError("fonction inconnue: " + name);
    }
    if (function->second.typeParameters.empty()) {
        if (!typeArguments.empty()) {
            semanticError("la fonction '" + name + "' n'accepte pas d'arguments de type");
        }
        if (!functionTypeNameMatchesSignature(resolvedType, function->second)) {
            semanticError("la fonction '" + name + "' n'est pas compatible avec " + resolvedType);
        }
        resolvedTypeArguments.clear();
        return;
    }
    auto substitution = genericFunctionSubstitutionFor(function->second, typeArguments);
    if (!substitution) {
        if (typeArguments.empty()) {
            semanticError(
                "la fonction générique '" + name + "' doit être référencée avec des arguments de type");
        }
        semanticError(
            "la fonction générique '" + name + "' attend " +
            std::to_string(function->second.typeParameters.size()) + " argument(s) de type");
    }
    auto substitutedSignature = function->second;
    for (auto& parameter : substitutedSignature.parameters) {
        parameter.type = substituteType(parameter.type, *substitution);
    }
    substitutedSignature.returnType = substituteType(substitutedSignature.returnType, *substitution);
    if (!functionTypeNameMatchesSignature(resolvedType, substitutedSignature)) {
        semanticError("la fonction '" + name + "' n'est pas compatible avec " + resolvedType);
    }
    resolvedTypeArguments = orderedTypeArguments(function->second.typeParameters, *substitution);
}

std::string FunctionReferenceNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredCaptures;
    for (const auto& capture : captures) {
        loweredCaptures.push_back(builder.emitLoad(capture.symbolName, capture.type));
    }
    std::vector<std::string> concreteTypeArguments;
    for (const auto& typeArgument : resolvedTypeArguments) {
        concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
    }
    if (!concreteTypeArguments.empty()) {
        auto functionType = functionTypeFromName(resolvedType);
        const std::string concreteReturnType = functionType ? functionType->returnType : "";
        builder.registerFunctionSpecialization(name, concreteTypeArguments, concreteReturnType);
        return builder.emitFunctionReference(
            formatParameterizedType(name, concreteTypeArguments), loweredCaptures);
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
    std::string loweredFunction = builder.emitLoad(symbolName, "");
    std::vector<std::string> loweredArguments;
    for (const auto& argument : arguments) loweredArguments.push_back(argument->lowerToIR(builder));
    return builder.emitIndirectCall(loweredFunction, loweredArguments, resolvedType);
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
    return builder.emitFieldLoad(location, className, fieldName, type);
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
    return builder.emitPhi(thenValue, elseValue, resolvedType);
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
    return builder.emitConstant("0", "Unit");
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
    builder.emitStore(counterSymbol, initialCount, "Int");
    builder.emitJump(conditionLabel);

    builder.emitLabel(conditionLabel);
    std::string currentCount = builder.emitLoad(counterSymbol, "Int");
    std::string zero = builder.emitConstant("0", "Int");
    std::string shouldContinue = builder.emitBinary(">", currentCount, zero, "Bool");
    builder.emitBranchIfFalse(shouldContinue, endLabel);

    builder.emitLabel(bodyLabel);
    body->lowerToIR(builder);
    std::string countBeforeDecrement = builder.emitLoad(counterSymbol, "Int");
    std::string one = builder.emitConstant("1", "Int");
    std::string decrementedCount = builder.emitBinary("-", countBeforeDecrement, one, "Int");
    builder.emitStore(counterSymbol, decrementedCount, "Int");
    builder.emitJump(conditionLabel);

    builder.emitLabel(endLabel);
    return builder.emitConstant("0", "Unit");
}

FunctionDefNode::FunctionDefNode(
    std::string clName, std::string name, std::string declaredReturnType,
    std::vector<std::string> genericTypeParameters,
    std::vector<Parameter> params, std::unique_ptr<ASTNode> body,
    std::vector<Capture> capturedValues,
    std::vector<std::string> genericOwnerTypeParameters)
    : className(std::move(clName)), name(std::move(name)), returnType(std::move(declaredReturnType)),
      typeParameters(std::move(genericTypeParameters)),
      ownerTypeParameters(std::move(genericOwnerTypeParameters)), parameters(std::move(params)),
      body(std::move(body)), captures(std::move(capturedValues)) {}

std::string FunctionDefNode::getType() {
    return "Unit";
}

void FunctionDefNode::validateSemantics(CompilerContext& context) {
    context.semanticSymbolTypes.clear();
    context.semanticTypeParameters.clear();
    if (!className.empty()) {
        auto classIt = context.classes.find(className);
        if (classIt != context.classes.end()) {
            context.semanticTypeParameters = classIt->second.typeParameters;
            context.semanticTypeParameters.insert(
                context.semanticTypeParameters.end(), typeParameters.begin(), typeParameters.end());
            context.semanticSymbolTypes["this"] = classIt->second.typeParameters.empty()
                ? className
                : formatParameterizedType(className, classIt->second.typeParameters);
        } else {
            context.semanticSymbolTypes["this"] = className;
        }
    } else {
        context.semanticTypeParameters = typeParameters;
    }
    for (const auto& capture : captures) {
        context.semanticSymbolTypes[capture.symbolName] = capture.type;
    }
    for (const auto& parameter : parameters) {
        context.semanticSymbolTypes[parameter.symbolName] = parameter.type;
    }
    if (body) body->validateSemantics(context);
    const bool knownType = isKnownTypeInContext(returnType, context);
    if (!knownType) {
        semanticError("type de retour inconnu '" + returnType + "' pour la fonction '" + name + "'");
    }
    if (body && body->getType() != returnType) {
        semanticError(
            "Type de retour invalide pour '" + name + "': '" + returnType +
            "' attendu, '" + body->getType() + "' reçu");
    }
    context.semanticTypeParameters.clear();
}

std::string FunctionDefNode::lowerToIR(IRBuilder& builder) const {
    if (!typeParameters.empty()) return "";

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
        builder.bindParameter("this", "this");
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

std::string FunctionDefNode::lowerSpecializedMethodToIR(
    IRBuilder& builder, const std::string& concreteClassName,
    const std::vector<std::string>& concreteMethodTypeArguments) const {
    auto parameterizedType = parameterizedTypeFromName(concreteClassName);
    if (!parameterizedType || parameterizedType->first != className ||
        parameterizedType->second.size() != ownerTypeParameters.size()) {
        builder.unsupported(location, "la spécialisation de méthode " + concreteClassName + "." + name);
    }
    if (concreteMethodTypeArguments.size() != typeParameters.size()) {
        builder.unsupported(location, "la spécialisation de méthode " + concreteClassName + "." + name);
    }

    std::map<std::string, std::string> substitution;
    substitution[className] = concreteClassName;
    for (size_t i = 0; i < ownerTypeParameters.size(); ++i) {
        substitution[ownerTypeParameters[i]] = parameterizedType->second[i];
    }
    for (size_t i = 0; i < typeParameters.size(); ++i) {
        substitution[typeParameters[i]] = concreteMethodTypeArguments[i];
    }

    builder.pushTypeSubstitution(substitution);

    std::vector<IRParameter> irParameters;
    irParameters.push_back({"this", concreteClassName});
    for (const auto& parameter : parameters) {
        irParameters.push_back({parameter.name, parameter.type});
    }

    const std::string concreteMethodName = concreteMethodTypeArguments.empty()
        ? name
        : formatParameterizedType(name, concreteMethodTypeArguments);
    builder.beginFunction(concreteClassName + "." + concreteMethodName, irParameters, returnType);
    builder.bindThis();
    builder.bindParameter("this", "this");
    for (const auto& parameter : parameters) {
        builder.bindParameter(parameter.symbolName, parameter.name);
    }
    std::string result = body->lowerToIR(builder);
    builder.endFunction(result);

    builder.popTypeSubstitution();
    return "";
}

std::string FunctionDefNode::lowerSpecializedFunctionToIR(
    IRBuilder& builder, const std::vector<std::string>& concreteTypeArguments) const {
    if (!className.empty() || concreteTypeArguments.size() != typeParameters.size()) {
        builder.unsupported(location, "la spécialisation de fonction " + name);
    }

    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < typeParameters.size(); ++i) {
        substitution[typeParameters[i]] = concreteTypeArguments[i];
    }

    builder.pushTypeSubstitution(substitution);

    std::vector<IRParameter> irParameters;
    for (const auto& parameter : parameters) {
        irParameters.push_back({parameter.name, parameter.type});
    }

    builder.beginFunction(formatParameterizedType(name, concreteTypeArguments), irParameters, returnType);
    for (const auto& parameter : parameters) {
        builder.bindParameter(parameter.symbolName, parameter.name);
    }
    std::string result = body->lowerToIR(builder);
    builder.endFunction(result);

    builder.popTypeSubstitution();
    return "";
}

IdentifierNode::IdentifierNode(std::string n, std::string symbol, std::string resolvedType)
    : name(std::move(n)), symbolName(std::move(symbol)), type(std::move(resolvedType)) {}

std::string IdentifierNode::getType() {
    return type;
}

void IdentifierNode::validateSemantics(CompilerContext& context) {
    if (symbolName == name && symbolName != "this") {
        semanticError("variable non déclarée: " + name);
    }
    auto symbol = context.semanticSymbolTypes.find(symbolName);
    if (symbol == context.semanticSymbolTypes.end()) {
        semanticError("variable utilisée hors de sa portée: " + name);
    }
    type = symbol->second;
}

std::string IdentifierNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitLoad(symbolName, type);
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
    builder.emitStore(symbolName, value, const_cast<ASTNode*>(initializer.get())->getType());
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
    builder.emitStore(symbolName, loweredValue, targetType);
    return loweredValue;
}
