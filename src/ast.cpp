#include "ast.hpp"
#include "ir.hpp"
#include "runtime_values.hpp"
#include <algorithm>

namespace {
bool isComparisonMethod(const std::string& methodName) {
    return methodName == "==" || methodName == "!=" || methodName == "<" || methodName == ">" ||
           methodName == "<=" || methodName == ">=";
}

bool isArithmeticMethod(const std::string& methodName) {
    return methodName == "+" || methodName == "-" || methodName == "*" || methodName == "/" ||
           methodName == "%";
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

std::string emitBoolConstant(IRBuilder& builder, bool value) {
    return builder.emitConstant(
        std::to_string(value ? RuntimeValues::kTaggedTrue : RuntimeValues::kTaggedFalse),
        "Bool");
}

bool isKnownBuiltinType(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" || type == "Double" || type == "Bool" ||
           type == "Char" || type == "String" || type == "Unit" || type == "Any" ||
           type == "AnyVal" || type == "AnyRef" || type == "IntArray" || type == "LongArray" ||
           type == "FloatArray" || type == "DoubleArray" || type == "BoolArray";
}

bool isObjectArrayType(const std::string& type) {
    auto parameterizedType = parameterizedTypeFromName(type);
    return parameterizedType && parameterizedType->first == "ObjectArray" &&
           parameterizedType->second.size() == 1;
}

bool isNativeArrayType(const std::string& type) {
    return type == "IntArray" || type == "LongArray" || type == "FloatArray" ||
           type == "DoubleArray" || type == "BoolArray" || isObjectArrayType(type);
}

bool isPrimitiveArrayFacadeType(const std::string& type) {
    return type == "ArrayInt" || type == "ArrayLong" || type == "ArrayFloat" ||
           type == "ArrayDouble" || type == "ArrayBool";
}

bool isObjectArrayFacadeType(const std::string& type) {
    auto parameterizedType = parameterizedTypeFromName(type);
    return parameterizedType && parameterizedType->first == "ArrayObject" &&
           parameterizedType->second.size() == 1;
}

bool isAbstractTraitMethod(
    const CompilerContext& context, const std::string& ownerType, const std::string& methodName) {
    auto ownerIt = context.classes.find(genericBaseName(ownerType));
    if (ownerIt == context.classes.end() || !ownerIt->second.isTrait) return false;
    auto methodIt = ownerIt->second.methods.find(methodName);
    return methodIt != ownerIt->second.methods.end() && methodIt->second.isAbstract;
}

std::string primitiveArrayFacadeStorageType(const std::string& type) {
    if (type == "ArrayLong") return "LongArray";
    if (type == "ArrayFloat") return "FloatArray";
    if (type == "ArrayDouble") return "DoubleArray";
    if (type == "ArrayBool") return "BoolArray";
    return "IntArray";
}

std::string nativeArrayElementType(const std::string& type) {
    if (auto parameterizedType = parameterizedTypeFromName(type);
        parameterizedType && parameterizedType->first == "ObjectArray" &&
        parameterizedType->second.size() == 1) {
        return parameterizedType->second[0];
    }
    if (type == "LongArray") return "Long";
    if (type == "FloatArray") return "Float";
    if (type == "DoubleArray") return "Double";
    if (type == "BoolArray") return "Bool";
    return "Int";
}

bool isKnownTypeInContext(const std::string& type, const CompilerContext& context) {
    if (isKnownBuiltinType(type) || isFunctionTypeName(type)) return true;
    auto classIt = context.classes.find(type);
    if (classIt != context.classes.end()) return classIt->second.typeParameters.empty();
    if (isTypeParameterName(type, context.semanticTypeParameters)) return true;
    auto substitution = genericSubstitutionFor(context, type);
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!substitution) {
        if (!parameterizedType) return false;
        const auto& [baseName, arguments] = *parameterizedType;
        if (baseName == "ObjectArray" && arguments.size() == 1) {
            return isKnownTypeInContext(arguments[0], context) ||
                   isTypeParameterName(arguments[0], context.semanticTypeParameters);
        }
        if (!isStdlibTypeAliasFamily(baseName, arguments.size())) return false;
        for (const auto& argument : arguments) {
            if (!isTypeParameterName(argument, context.semanticTypeParameters)) return false;
        }
        return true;
    }
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
        if (parameter.isRepeated) {
            parameter.repeatedElementType = substituteType(parameter.repeatedElementType, substitution);
        }
        substituted.push_back(std::move(parameter));
    }
    return substituted;
}

void validateArguments(
    const CompilerContext& context,
    const std::string& callableName,
    const std::vector<std::unique_ptr<ASTNode>>& arguments,
    const std::vector<CompilerContext::ParameterInfo>& parameters,
    const SourceLocation& location) {
    const bool hasRepeated = !parameters.empty() && parameters.back().isRepeated;
    const size_t minimumCount = hasRepeated ? parameters.size() - 1 : parameters.size();
    if ((!hasRepeated && arguments.size() != parameters.size()) ||
        (hasRepeated && arguments.size() < minimumCount)) {
        throw CompilerError(ErrorKind::Semantic, location,
            callableName + ": " + std::to_string(minimumCount) +
            " argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)" +
            recommendedStdlibFunctionSuffix(callableName));
    }
    for (size_t i = 0; i < arguments.size(); ++i) {
        const auto& parameter = hasRepeated && i >= parameters.size() - 1
            ? parameters.back()
            : parameters[i];
        auto* splat = dynamic_cast<SplatNode*>(arguments[i].get());
        if (splat) {
            const bool isOnlyRepeatedArgument =
                hasRepeated && i == parameters.size() - 1 && i + 1 == arguments.size();
            if (!isOnlyRepeatedArgument) {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[i]->getLocation(),
                    callableName + ": ': _*' est uniquement autorisé comme dernier argument d'un paramètre répété");
            }
            if (!isTypeAssignable(context, splat->getType(), parameter.type)) {
                throw CompilerError(ErrorKind::Semantic, arguments[i]->getLocation(),
                    callableName + ", paramètre '" + parameter.name + "': type '" +
                    parameter.type + "' attendu, '" + splat->getType() + "' reçu" +
                    recommendedStdlibFunctionSuffix(callableName));
            }
            continue;
        }
        const std::string expectedType = hasRepeated && i >= parameters.size() - 1
            ? parameter.repeatedElementType
            : parameter.type;
        if (!isTypeAssignable(context, arguments[i]->getType(), expectedType)) {
            throw CompilerError(ErrorKind::Semantic, arguments[i]->getLocation(),
                callableName + ", paramètre '" + parameter.name + "': type '" +
                expectedType + "' attendu, '" + arguments[i]->getType() + "' reçu" +
                recommendedStdlibFunctionSuffix(callableName));
        }
    }
}

std::vector<std::string> parameterTypes(const std::vector<CompilerContext::ParameterInfo>& parameters) {
    std::vector<std::string> types;
    for (const auto& parameter : parameters) types.push_back(parameter.type);
    return types;
}

bool shouldBoxArgumentForParameter(const std::string& actualType, const std::string& expectedType) {
    return (expectedType == "Any" || expectedType == "AnyVal") && isBuiltinValueType(actualType);
}

std::string boxValueForParameter(
    IRBuilder& builder, const std::string& loweredValue,
    const std::string& actualType, const std::string& expectedType) {
    if (!shouldBoxArgumentForParameter(actualType, expectedType)) return loweredValue;
    return builder.emitMethodCall(actualType, "box", loweredValue, {}, expectedType);
}

std::string emitPackedArrayArgument(
    IRBuilder& builder,
    const std::vector<std::unique_ptr<ASTNode>>& arguments,
    size_t startIndex,
    const CompilerContext::ParameterInfo& parameter) {
    const size_t repeatedCount = arguments.size() - startIndex;
    const std::string size = builder.emitConstant(std::to_string(repeatedCount), "Int");
    const std::string elementType = builder.substituteActiveType(parameter.repeatedElementType);
    const std::string arrayType = builder.substituteActiveType(parameter.type);

    std::string rawArray;
    if (arrayType == "ArrayLong") {
        rawArray = builder.emitNewLongArray(size);
    } else if (arrayType == "ArrayFloat") {
        rawArray = builder.emitNewFloatArray(size);
    } else if (arrayType == "ArrayDouble") {
        rawArray = builder.emitNewDoubleArray(size);
    } else if (arrayType == "ArrayBool") {
        rawArray = builder.emitNewBoolArray(size);
    } else if (arrayType == "ArrayInt") {
        rawArray = builder.emitNewIntArray(size);
    } else {
        rawArray = builder.emitNewObjectArray(size, elementType);
    }

    for (size_t i = 0; i < repeatedCount; ++i) {
        const auto& argument = arguments[startIndex + i];
        const std::string index = builder.emitConstant(std::to_string(i), "Int");
        std::string value = argument->lowerToIR(builder);
        value = boxValueForParameter(
            builder, value, argument->getType(), parameter.repeatedElementType);

        if (arrayType == "ArrayLong") {
            builder.emitLongArraySet(rawArray, index, value);
        } else if (arrayType == "ArrayFloat") {
            builder.emitFloatArraySet(rawArray, index, value);
        } else if (arrayType == "ArrayDouble") {
            builder.emitDoubleArraySet(rawArray, index, value);
        } else if (arrayType == "ArrayBool") {
            builder.emitBoolArraySet(rawArray, index, value);
        } else if (arrayType == "ArrayInt") {
            builder.emitIntArraySet(rawArray, index, value);
        } else {
            builder.emitObjectArraySet(rawArray, index, value);
        }
    }

    return builder.emitNewObject(arrayType, {rawArray}, arrayType);
}

std::vector<std::string> lowerCallArgumentsForParameters(
    IRBuilder& builder,
    const std::vector<std::unique_ptr<ASTNode>>& arguments,
    const std::vector<CompilerContext::ParameterInfo>& parameters) {
    std::vector<std::string> loweredArguments;
    const bool hasRepeated = !parameters.empty() && parameters.back().isRepeated;
    const size_t fixedCount = hasRepeated ? parameters.size() - 1 : parameters.size();

    for (size_t i = 0; i < arguments.size() && i < fixedCount; ++i) {
        std::string loweredArgument = arguments[i]->lowerToIR(builder);
        loweredArgument = boxValueForParameter(
            builder, loweredArgument, arguments[i]->getType(), parameters[i].type);
        loweredArguments.push_back(loweredArgument);
    }

    if (hasRepeated) {
        if (arguments.size() == fixedCount + 1) {
            if (auto* splat = dynamic_cast<SplatNode*>(arguments[fixedCount].get())) {
                std::string loweredArgument = splat->lowerToIR(builder);
                loweredArguments.push_back(loweredArgument);
                return loweredArguments;
            }
        }
        loweredArguments.push_back(
            emitPackedArrayArgument(builder, arguments, fixedCount, parameters.back()));
        return loweredArguments;
    }

    for (size_t i = fixedCount; i < arguments.size(); ++i) {
        loweredArguments.push_back(arguments[i]->lowerToIR(builder));
    }
    return loweredArguments;
}

std::optional<std::map<std::string, std::string>> inferGenericFunctionSubstitutionFromArguments(
    const CompilerContext::FunctionSignature& signature,
    const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    if (signature.typeParameters.empty()) return std::map<std::string, std::string>{};
    if (!acceptsArgumentCount(signature, arguments.size())) return std::nullopt;

    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < arguments.size(); ++i) {
        std::string expectedPattern;
        if (hasRepeatedParameter(signature) && i >= signature.parameters.size() - 1) {
            expectedPattern = dynamic_cast<SplatNode*>(arguments[i].get())
                ? signature.parameters.back().type
                : signature.parameters.back().repeatedElementType;
        } else {
            expectedPattern = signature.parameters[i].type;
        }
        if (!inferTypeArgumentsFromTypes(
                expectedPattern, arguments[i]->getType(),
                signature.typeParameters, substitution)) {
            return std::nullopt;
        }
    }
    for (const auto& typeParameter : signature.typeParameters) {
        if (substitution.count(typeParameter) == 0) return std::nullopt;
    }
    return substitution;
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
    return emitBoolConstant(builder, value);
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
    std::string falseValue = emitBoolConstant(builder, false);
    return builder.emitBinary("==", loweredExpression, falseValue, "Bool");
}

UnaryMinusNode::UnaryMinusNode(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}

std::string UnaryMinusNode::getType() {
    return expression->getType();
}

void UnaryMinusNode::validateSemantics(CompilerContext& context) {
    expression->validateSemantics(context);
    const std::string operandType = expression->getType();
    if (!isNumericType(operandType)) {
        semanticError("l'opérateur '-' attend une expression de type numérique");
    }
}

std::string UnaryMinusNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredExpression = expression->lowerToIR(builder);
    const std::string operandType = expression->getType();
    std::string zero = builder.emitConstant("0", operandType);
    return builder.emitBinary("-", zero, loweredExpression, operandType);
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
        std::string falseValue = emitBoolConstant(builder, false);
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
    std::string trueValue = emitBoolConstant(builder, true);
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

CharNode::CharNode(std::string val) : value(std::move(val)) {}

std::string CharNode::getType() {
    return "Char";
}

void CharNode::validateSemantics(CompilerContext& context) {
    (void) context;
}

std::string CharNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitConstant(value, "Char");
}

NewNode::NewNode(std::string clName, std::vector<std::unique_ptr<ASTNode>> arguments)
    : className(std::move(clName)), args(std::move(arguments)) {}

std::string NewNode::getType() {
    return className;
}

void NewNode::validateSemantics(CompilerContext& context) {
    for (const auto& arg : args) arg->validateSemantics(context);

    if (isPrimitiveArrayFacadeType(className) && args.size() == 1 && args[0]->getType() == "Int") {
        return;
    }
    if (isObjectArrayFacadeType(className) && args.size() == 1 && args[0]->getType() == "Int") {
        return;
    }

    if (isNativeArrayType(className)) {
        if (args.size() != 1) {
            semanticError(
                "Constructeur de '" + className + "': 1 argument(s) attendu(s), " +
                std::to_string(args.size()) + " reçu(s)");
        }
        if (args[0]->getType() != "Int") {
            throw CompilerError(
                ErrorKind::Semantic, args[0]->getLocation(),
                "Constructeur de '" + className + "', paramètre 'size': type 'Int' attendu, '" +
                args[0]->getType() + "' reçu");
        }
        return;
    }

    const std::string classLookupName = genericBaseName(className);
    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        semanticError("classe inconnue dans 'new': " + className);
    }
    if (classIt->second.isTrait) {
        semanticError("trait non instanciable dans 'new': " + className);
    }
    std::map<std::string, std::string> substitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, className)) {
        substitution = *genericSubstitution;
    } else if (!classIt->second.typeParameters.empty()) {
        semanticError(
            "classe générique '" + classLookupName + "' utilisée sans arguments de type");
    }
    auto fields = collectClassFieldsInHierarchyForLayout(context, className);
    const auto& parentConstructorArguments = classIt->second.parentConstructorArguments;
    if (!parentConstructorArguments.empty()) {
        const size_t expectedArgumentCount =
            parentConstructorArguments.size() + classIt->second.fields.size();
        if (args.size() != expectedArgumentCount) {
            semanticError(
                "Constructeur de '" + className + "': " + std::to_string(expectedArgumentCount) +
                " argument(s) attendu(s), " + std::to_string(args.size()) + " reçu(s)");
        }
        if (fields.size() != args.size()) {
            semanticError(
                "Constructeur de '" + className + "': " + std::to_string(fields.size()) +
                " champs attendus dans le layout, " + std::to_string(args.size()) + " reçu(s)");
        }

        std::map<std::string, size_t> fieldOrderByName;
        for (size_t i = 0; i < fields.size(); ++i) {
            fieldOrderByName[fields[i].first] = i;
        }

        std::vector<std::unique_ptr<ASTNode>> mappedArguments(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            std::string targetField;
            if (i < parentConstructorArguments.size()) {
                targetField = parentConstructorArguments[i];
            } else {
                targetField = classIt->second.fields[i - parentConstructorArguments.size()].name;
            }
            auto fieldIndex = fieldOrderByName.find(targetField);
            if (fieldIndex == fieldOrderByName.end()) {
                throw CompilerError(
                    ErrorKind::Semantic, args[i]->getLocation(),
                    "Constructeur de '" + className +
                    "': champ cible inconnu dans le parent explicite '" + targetField + "'");
            }
            if (mappedArguments[fieldIndex->second]) {
                throw CompilerError(
                    ErrorKind::Semantic, args[i]->getLocation(),
                    "Constructeur de '" + className +
                    "': cible de champ dupliquée dans le parent explicite '" + targetField + "'");
            }
            mappedArguments[fieldIndex->second] = std::move(args[i]);
        }
        args = std::move(mappedArguments);
        for (size_t i = 0; i < fields.size(); ++i) {
            const std::string expectedType = substituteType(fields[i].second, substitution);
            if (!args[i]) {
                semanticError(
                    "Constructeur de '" + className + "': champ '" + fields[i].first +
                    "' non initialisé");
            }
            if (args[i]->getType() != expectedType) {
                throw CompilerError(ErrorKind::Semantic, args[i]->getLocation(),
                    "Constructeur de '" + className + "', champ '" + fields[i].first +
                    "': type '" + expectedType + "' attendu, '" + args[i]->getType() + "' reçu");
            }
        }
        return;
    }

    if (args.size() != fields.size()) {
        semanticError(
            "Constructeur de '" + className + "': " + std::to_string(fields.size()) +
            " argument(s) attendu(s), " + std::to_string(args.size()) + " reçu(s)");
    }
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string expectedType = substituteType(fields[i].second, substitution);
        if (args[i]->getType() != expectedType) {
            throw CompilerError(ErrorKind::Semantic, args[i]->getLocation(),
                "Constructeur de '" + className + "', champ '" + fields[i].first +
                "': type '" + expectedType + "' attendu, '" + args[i]->getType() + "' reçu");
        }
    }
}

std::string NewNode::lowerToIR(IRBuilder& builder) const {
    std::vector<std::string> loweredArguments;
    for (const auto& argument : args) loweredArguments.push_back(argument->lowerToIR(builder));
    if (isPrimitiveArrayFacadeType(className) && args.size() == 1 && args[0]->getType() == "Int") {
        const std::string storageType = primitiveArrayFacadeStorageType(className);
        std::string storage;
        if (storageType == "LongArray") storage = builder.emitNewLongArray(loweredArguments[0]);
        else if (storageType == "FloatArray") storage = builder.emitNewFloatArray(loweredArguments[0]);
        else if (storageType == "DoubleArray") storage = builder.emitNewDoubleArray(loweredArguments[0]);
        else if (storageType == "BoolArray") storage = builder.emitNewBoolArray(loweredArguments[0]);
        else storage = builder.emitNewIntArray(loweredArguments[0]);
        return builder.emitNewObject(className, {storage});
    }
    if (isObjectArrayFacadeType(className) && args.size() == 1 && args[0]->getType() == "Int") {
        const std::string elementType = parameterizedTypeFromName(className)->second[0];
        const std::string storage = builder.emitNewObjectArray(loweredArguments[0], elementType);
        return builder.emitNewObject(className, {storage});
    }
    if (className == "IntArray") return builder.emitNewIntArray(loweredArguments[0]);
    if (className == "LongArray") return builder.emitNewLongArray(loweredArguments[0]);
    if (className == "FloatArray") return builder.emitNewFloatArray(loweredArguments[0]);
    if (className == "DoubleArray") return builder.emitNewDoubleArray(loweredArguments[0]);
    if (className == "BoolArray") return builder.emitNewBoolArray(loweredArguments[0]);
    if (isObjectArrayType(className)) return builder.emitNewObjectArray(loweredArguments[0], nativeArrayElementType(className));
    return builder.emitNewObject(className, loweredArguments);
}

SuperNode::SuperNode(std::string type) : parentType(std::move(type)) {}

std::string SuperNode::getType() {
    return parentType;
}

void SuperNode::validateSemantics(CompilerContext& context) {
    if (parentType.empty()) {
        semanticError("super non supporté dans une classe sans parent explicite");
    }
    if (context.semanticSymbolTypes.find("this") == context.semanticSymbolTypes.end()) {
        semanticError("'super' non autorisé en dehors d'une méthode de classe");
    }
}

std::string SuperNode::lowerToIR(IRBuilder& builder) const {
    return builder.emitLoad("this", builder.substituteActiveType(parentType));
}

MethodCallNode::MethodCallNode(
    std::unique_ptr<ASTNode> rec, std::string method, std::vector<std::unique_ptr<ASTNode>> args,
    std::vector<std::string> genericTypeArguments,
    std::string initialResolvedType, std::string initialOwnerType)
    : receiver(std::move(rec)), methodName(std::move(method)), resolvedMethodName(methodName), arguments(std::move(args)),
      typeArguments(std::move(genericTypeArguments)),
      resolvedType(std::move(initialResolvedType)), resolvedOwnerType(std::move(initialOwnerType)) {}

std::string MethodCallNode::getType() {
    const std::string receiverType = receiver->getType();
    if (isNumericType(receiverType)) {
        if (methodName == "toString") return "String";
        if (receiverType == "Int" && methodName == "toLong") return "Long";
        if (isComparisonMethod(methodName)) return "Bool";
        return receiverType;
    }
    if (receiverType == "Bool" && methodName == "toString") return "String";
    if (receiverType == "String") {
        if (methodName == "toString") return "String";
        if (methodName == "+") return "String";
        if (isBoolBinaryMethod(methodName)) return "Bool";
        if (methodName == "isEmpty" || methodName == "nonEmpty" || methodName == "startsWith" ||
            methodName == "endsWith" || methodName == "contains") return "Bool";
        if (methodName == "toInt") return "Int";
        if (methodName == "toCharArray") return "ArrayObject[Char]";
        if (methodName == "split") return "ArrayObject[String]";
        if (methodName == "substring" || methodName == "repeat" || methodName == "trim") return "String";
        if (methodName == "length" || methodName == "indexOf") return "Int";
        if (methodName == "charAt") return "Char";
    }
    if (receiverType == "Char") {
        if (methodName == "toString") return "String";
        if (isBoolBinaryMethod(methodName)) return "Bool";
    }
    if (receiverType == "Bool") {
        if (methodName == "toString") return "String";
        if (isBoolBinaryMethod(methodName)) return "Bool";
    }
    if (isNativeArrayType(receiverType)) {
        if (methodName == "set") return "Unit";
        if (methodName == "length") return "Int";
        if (methodName == "get") return nativeArrayElementType(receiverType);
    }
    if (receiverType == "ArrayObject[String]" && methodName == "mkString") return "String";
    if (methodName == "toString") return "String";
    if (methodName == "hashCode") return "Int";
    return resolvedType;
}

void MethodCallNode::validateSemantics(CompilerContext& context) {
    receiver->validateSemantics(context);
    for (const auto& argument : arguments) argument->validateSemantics(context);

    const std::string receiverType = receiver->getType();
    if (isTypeParameterName(receiverType, context.semanticTypeParameters)) {
        receiverIsTypeParameter = true;
        if (methodName == "toString" || methodName == "hashCode") {
            if (!arguments.empty()) {
                semanticError("la méthode " + receiverType + "." + methodName + " n'accepte aucun argument");
            }
            resolvedType = methodName == "toString" ? "String" : "Int";
            return;
        }
        if (methodName == "equals") {
            if (arguments.size() != 1) {
                semanticError("la méthode " + receiverType + ".equals attend un argument");
            }
            resolvedType = "Bool";
            resolvedParameterTypes = {"Any"};
            return;
        }
        if (methodName == "==" || methodName == "!=") {
            if (arguments.size() != 1) {
                semanticError("la méthode " + receiverType + "." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != receiverType) {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode " + receiverType + "." + methodName + " attend un argument de type " + receiverType);
            }
            resolvedType = "Bool";
            return;
        }
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    if (isNumericType(receiverType)) {
        const bool binaryMethod = isArithmeticMethod(methodName) || isComparisonMethod(methodName);
        if (binaryMethod) {
            if (methodName == "%" && !isIntegerType(receiverType)) {
                semanticError("méthode inconnue: " + receiverType + "." + methodName);
            }
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
        if (methodName == "toString") {
            if (!arguments.empty()) {
                semanticError("la méthode " + receiverType + ".toString n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
        if (receiverType == "Int" && methodName == "toLong") {
            if (!arguments.empty()) {
                semanticError("la méthode Int.toLong n'accepte aucun argument");
            }
            resolvedType = "Long";
            return;
        }
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    if (receiverType == "Bool") {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                semanticError("la méthode Bool.toString n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
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
    if (receiverType == "String") {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                semanticError("la méthode String.toString n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
        if (methodName == "+") {
            if (arguments.size() != 1) {
                semanticError("la méthode String.+ attend un argument");
            }
            if (arguments[0]->getType() != "String") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String.+ attend un argument de type String");
            }
            resolvedType = "String";
            return;
        }
        if (isBoolBinaryMethod(methodName)) {
            if (arguments.size() != 1) {
                semanticError("la méthode String." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "String") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String." + methodName + " attend un argument de type String");
            }
            resolvedType = "Bool";
            return;
        }
        if (methodName == "isEmpty" || methodName == "nonEmpty") {
            if (!arguments.empty()) {
                semanticError("la méthode String." + methodName + " n'accepte aucun argument");
            }
            resolvedType = "Bool";
            return;
        }
        if (methodName == "startsWith" || methodName == "endsWith") {
            if (arguments.size() != 1) {
                semanticError("la méthode String." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "String") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String." + methodName + " attend un argument de type String");
            }
            resolvedType = "Bool";
            return;
        }
        if (methodName == "indexOf" || methodName == "contains") {
            if (arguments.size() != 1) {
                semanticError("la méthode String." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "String") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String." + methodName + " attend un argument de type String");
            }
            resolvedType = methodName == "contains" ? "Bool" : "Int";
            return;
        }
        if (methodName == "toInt") {
            if (!arguments.empty()) {
                semanticError("la méthode String.toInt n'accepte aucun argument");
            }
            resolvedType = "Int";
            return;
        }
        if (methodName == "toCharArray") {
            if (!arguments.empty()) {
                semanticError("la méthode String.toCharArray n'accepte aucun argument");
            }
            resolvedType = "ArrayObject[Char]";
            return;
        }
        if (methodName == "split") {
            if (arguments.size() != 1) {
                semanticError("la méthode String.split attend un argument");
            }
            if (arguments[0]->getType() != "String") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String.split attend un séparateur de type String");
            }
            resolvedType = "ArrayObject[String]";
            return;
        }
        if (methodName == "substring") {
            if (arguments.size() != 2) {
                semanticError("la méthode String.substring attend deux arguments");
            }
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String.substring attend un index de début de type Int");
            }
            if (arguments[1]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[1]->getLocation(),
                    "la méthode String.substring attend un index de fin de type Int");
            }
            resolvedType = "String";
            return;
        }
        if (methodName == "repeat") {
            if (arguments.size() != 1) {
                semanticError("la méthode String.repeat attend un argument");
            }
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String.repeat attend un nombre de répétitions de type Int");
            }
            resolvedType = "String";
            return;
        }
        if (methodName == "trim") {
            if (!arguments.empty()) {
                semanticError("la méthode String.trim n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
        if (methodName == "length") {
            if (!arguments.empty()) semanticError("la méthode String.length n'accepte aucun argument");
            resolvedType = "Int";
            return;
        }
        if (methodName == "charAt") {
            if (arguments.size() != 1) {
                semanticError("la méthode String.charAt attend un argument");
            }
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode String.charAt attend un index de type Int");
            }
            resolvedType = "Char";
            return;
        }
        semanticError("méthode inconnue: String." + methodName);
    }
    if (receiverType == "Char") {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                semanticError("la méthode Char.toString n'accepte aucun argument");
            }
            resolvedType = "String";
            return;
        }
        if (isBoolBinaryMethod(methodName)) {
            if (arguments.size() != 1) {
                semanticError("la méthode Char." + methodName + " attend un argument");
            }
            if (arguments[0]->getType() != "Char") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode Char." + methodName + " attend un argument de type Char");
            }
            resolvedType = "Bool";
            return;
        }
        semanticError("méthode inconnue: Char." + methodName);
    }
    if (isNativeArrayType(receiverType)) {
        if (methodName == "length") {
            if (!arguments.empty()) semanticError("la méthode " + receiverType + ".length n'accepte aucun argument");
            resolvedType = "Int";
            return;
        }
        if (methodName == "get") {
            if (arguments.size() != 1) semanticError("la méthode " + receiverType + ".get attend un argument");
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode " + receiverType + ".get attend un index de type Int");
            }
            resolvedType = nativeArrayElementType(receiverType);
            return;
        }
        if (methodName == "set") {
            if (arguments.size() != 2) semanticError("la méthode " + receiverType + ".set attend deux arguments");
            if (arguments[0]->getType() != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[0]->getLocation(),
                    "la méthode " + receiverType + ".set attend un index de type Int");
            }
            const std::string elementType = nativeArrayElementType(receiverType);
            if (arguments[1]->getType() != elementType) {
                throw CompilerError(
                    ErrorKind::Semantic, arguments[1]->getLocation(),
                    "la méthode " + receiverType + ".set attend une valeur de type " + elementType);
            }
            resolvedType = "Unit";
            return;
        }
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    if (receiverType == "ArrayObject[String]" && methodName == "mkString") {
        if (arguments.size() != 1) {
            semanticError("la méthode Array[String].mkString attend un argument");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "la méthode Array[String].mkString attend un séparateur de type String");
        }
        resolvedType = "String";
        return;
    }
    if (isBoolBinaryMethod(methodName)) {
        if (arguments.size() != 1) {
            semanticError("la méthode " + receiverType + "." + methodName + " attend un argument");
        }
        resolvedType = "Bool";
        resolvedParameterTypes = {"Any"};
        return;
    }
    if (auto signature = stdlibTypeAliasMethodSignature(receiverType, methodName)) {
        if (!typeArguments.empty()) {
            semanticError("la méthode '" + receiverType + "." + methodName + "' n'accepte pas d'arguments de type");
        }
        validateArguments(context, receiverType + "." + methodName, arguments, signature->parameters, location);
        resolvedType = signature->returnType;
        resolvedParameterTypes = parameterTypes(signature->parameters);
        resolvedParameters = signature->parameters;
        resolvedTypeArguments.clear();
        resolvedOwnerType.clear();
        return;
    }

    const std::string classLookupName = genericBaseName(receiverType);
    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        semanticError("type receveur inconnu pour l'appel de méthode: " + receiverType);
    }
    if (!genericSubstitutionFor(context, receiverType) && !classIt->second.typeParameters.empty()) {
        semanticError(
            "classe générique '" + classLookupName + "' utilisée sans arguments de type");
    }

    const auto methodCandidates = collectClassMethodLookupCandidates(context, receiverType, methodName);
    if (methodCandidates.empty()) {
        semanticError("méthode inconnue: " + receiverType + "." + methodName);
    }
    std::vector<std::string> actualArgumentTypes;
    for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());
    auto resolvedMethodLookup = methodCandidates.size() == 1
        ? std::optional<ClassMethodLookupResult>(methodCandidates.front())
        : resolveExactClassMethodOverload(context, receiverType, methodName, actualArgumentTypes, typeArguments);
    if (!resolvedMethodLookup) {
        const auto matches =
            exactClassMethodOverloadMatches(context, receiverType, methodName, actualArgumentTypes, typeArguments);
        if (matches.concreteMatches.size() > 1 ||
            (matches.concreteMatches.empty() && matches.genericMatches.size() > 1)) {
            const auto& conflictingMatches = matches.concreteMatches.empty()
                ? matches.genericMatches
                : matches.concreteMatches;
            std::set<std::string> providers;
            for (const auto& candidate : conflictingMatches) {
                providers.insert(substituteType(candidate.ownerClassName, candidate.classSubstitution));
            }
            if (providers.size() > 1) {
                semanticError(
                    "conflit d'héritage pour la méthode '" + methodName + "' dans la classe '" +
                    genericBaseName(receiverType) + "': plusieurs définitions dans [" +
                    [&providers]() {
                        std::string message;
                        bool first = true;
                        for (const auto& provider : providers) {
                            if (!first) message += ", ";
                            message += provider;
                            first = false;
                        }
                        return message;
                    }() + "]");
            }
            semanticError(
                "appel de méthode surchargée ambigu pour '" +
                formatFunctionCallShape(receiverType + "." + methodName, actualArgumentTypes) + "'" +
                "\ncandidats:" + formatMethodOverloadCandidates(context, receiverType, methodName));
        }
        semanticError(formatNoMatchingMethodOverloadMessage(context, receiverType, methodName, actualArgumentTypes));
    }
    const auto& methodLookup = *resolvedMethodLookup;
    const auto& methodSignature = *methodLookup.signature;
    resolvedMethodName = methodLookup.methodName;

    std::map<std::string, std::string> substitution = methodLookup.classSubstitution;
    if (methodSignature.typeParameters.empty()) {
        if (!typeArguments.empty()) {
            semanticError("la méthode '" + receiverType + "." + methodName + "' n'accepte pas d'arguments de type");
        }
        resolvedTypeArguments.clear();
    } else {
        auto methodSubstitution = genericFunctionSubstitutionFor(methodSignature, typeArguments);
        if (!methodSubstitution && typeArguments.empty()) {
            CompilerContext::FunctionSignature substitutedSignature = methodSignature;
            for (auto& parameter : substitutedSignature.parameters) {
                parameter.type = substituteType(parameter.type, substitution);
                if (parameter.isRepeated) {
                    parameter.repeatedElementType = substituteType(parameter.repeatedElementType, substitution);
                }
            }
            substitutedSignature.returnType = substituteType(substitutedSignature.returnType, substitution);
            methodSubstitution =
                inferGenericFunctionSubstitutionFromArguments(substitutedSignature, arguments);
        }
        if (!methodSubstitution) {
            if (typeArguments.empty()) {
                semanticError(
                    "impossible d'inférer les arguments de type pour la méthode générique '" +
                    receiverType + "." + methodName + "'");
            }
            semanticError(
                "la méthode générique '" + receiverType + "." + methodName + "' attend " +
                std::to_string(methodSignature.typeParameters.size()) + " argument(s) de type");
        }
        substitution.insert(methodSubstitution->begin(), methodSubstitution->end());
        resolvedTypeArguments = orderedTypeArguments(methodSignature.typeParameters, *methodSubstitution);
    }
    auto parameters = substituteParameters(methodSignature.parameters, substitution);
    validateArguments(context, receiverType + "." + methodName, arguments, parameters, location);
    resolvedType = substituteType(methodSignature.returnType, substitution);
    resolvedParameterTypes = parameterTypes(parameters);
    resolvedParameters = parameters;
    resolvedOwnerType = methodLookup.ownerClassName;
}

std::string MethodCallNode::lowerToIR(IRBuilder& builder) const {
    const std::string receiverType = receiver->getType();
    const std::string activeReceiverType = builder.substituteActiveType(receiverType);
    if (receiverIsTypeParameter) {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + ".toString");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            if (activeReceiverType == "String") {
                return loweredReceiver;
            }
            std::string stringMethodOwner = "Any";
            if (!builder.isTypeParameter(activeReceiverType)) {
                stringMethodOwner = activeReceiverType;
            }
            return builder.emitMethodCall(stringMethodOwner, "toString", loweredReceiver, {}, "String");
        }
        if (methodName == "hashCode") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + ".hashCode");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            if (activeReceiverType == "String") {
                return builder.emitMethodCall("String", "hashCode", loweredReceiver, {}, "Int");
            }
            return builder.emitMethodCall("Any", "hashCode", loweredReceiver, {}, "Int");
        }
        if (methodName == "equals") {
            if (arguments.size() != 1) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + ".equals");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            if (activeReceiverType == "String" &&
                builder.substituteActiveType(arguments[0]->getType()) == "String") {
                return builder.emitMethodCall("String", "==", loweredReceiver, {loweredArgument}, "Bool");
            }
            loweredArgument = boxValueForParameter(
                builder, loweredArgument, arguments[0]->getType(), "Any");
            return builder.emitMethodCall("Any", "equals", loweredReceiver, {loweredArgument}, "Bool");
        }
        if (methodName == "==" || methodName == "!=") {
            if (arguments.size() != 1) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + "." + methodName);
            }
            const std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            if (activeReceiverType == "String" &&
                builder.substituteActiveType(arguments[0]->getType()) == "String") {
                std::string equalsResult =
                    builder.emitMethodCall("String", "==", loweredReceiver, {loweredArgument}, "Bool");
                if (methodName == "==") return equalsResult;
                std::string falseValue = emitBoolConstant(builder, false);
                return builder.emitBinary("==", equalsResult, falseValue, "Bool");
            }
            loweredArgument = boxValueForParameter(
                builder, loweredArgument, arguments[0]->getType(), "Any");
            std::string equalsResult =
                builder.emitMethodCall("Any", "equals", loweredReceiver, {loweredArgument}, "Bool");
            if (methodName == "==") return equalsResult;
            std::string falseValue = emitBoolConstant(builder, false);
            return builder.emitBinary("==", equalsResult, falseValue, "Bool");
        }
    }
    if (activeReceiverType == "String" && methodName == "toString" && arguments.empty()) {
        return receiver->lowerToIR(builder);
    }
    if (isNumericType(receiverType)) {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode " + receiverType + ".toString");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall(receiverType, "toString", loweredReceiver, {}, "String");
        }
        if (receiverType == "Int" && methodName == "toLong") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode Int.toLong");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("Int", "toLong", loweredReceiver, {}, "Long");
        }
        const std::vector<std::string> supported = {
            "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">="
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
    if (receiverType == "Bool" && methodName == "toString") {
        if (!arguments.empty()) {
            builder.unsupported(location, "l'appel de méthode Bool.toString");
        }
        std::string loweredReceiver = receiver->lowerToIR(builder);
        return builder.emitMethodCall("Bool", "toString", loweredReceiver, {}, "String");
    }
    if (receiverType == "Bool") {
        if (methodName == "==" || methodName == "!=") {
            if (arguments.size() != 1) {
                builder.unsupported(location, "l'appel de méthode Bool." + methodName);
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            return builder.emitBinary(methodName, loweredReceiver, loweredArgument, "Bool");
        }
    }
    if (receiverType == "Char" && methodName == "toString") {
        if (!arguments.empty()) {
            builder.unsupported(location, "l'appel de méthode Char.toString");
        }
        std::string loweredReceiver = receiver->lowerToIR(builder);
        return builder.emitMethodCall("Char", "toString", loweredReceiver, {}, "String");
    }
    if (receiverType == "Char") {
        if (methodName == "==" || methodName == "!=") {
            if (arguments.size() != 1) {
                builder.unsupported(location, "l'appel de méthode Char." + methodName);
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            return builder.emitBinary(methodName, loweredReceiver, loweredArgument, "Bool");
        }
    }
    if (receiverType == "String") {
        if (methodName == "toString" && arguments.empty()) {
            return receiver->lowerToIR(builder);
        }
        if (methodName == "+" && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall("String", "+", loweredReceiver, {loweredArgument}, "String");
        }
        if (isBoolBinaryMethod(methodName) && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredArgument = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall("String", methodName, loweredReceiver, {loweredArgument}, "Bool");
        }
        if ((methodName == "isEmpty" || methodName == "nonEmpty") && arguments.empty()) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("String", methodName, loweredReceiver, {}, "Bool");
        }
        if ((methodName == "startsWith" || methodName == "endsWith") && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredNeedle = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall("String", methodName, loweredReceiver, {loweredNeedle}, "Bool");
        }
        if ((methodName == "indexOf" || methodName == "contains") && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredNeedle = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall(
                "String", methodName, loweredReceiver, {loweredNeedle},
                methodName == "contains" ? "Bool" : "Int");
        }
        if (methodName == "toInt" && arguments.empty()) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("String", "toInt", loweredReceiver, {}, "Int");
        }
        if (methodName == "toCharArray" && arguments.empty()) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("String", "toCharArray", loweredReceiver, {}, "ArrayObject[Char]");
        }
        if (methodName == "split" && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredSeparator = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall(
                "String", "split", loweredReceiver, {loweredSeparator}, "ArrayObject[String]");
        }
        if (methodName == "substring" && arguments.size() == 2) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredFrom = arguments[0]->lowerToIR(builder);
            std::string loweredUntil = arguments[1]->lowerToIR(builder);
            return builder.emitMethodCall(
                "String", "substring", loweredReceiver, {loweredFrom, loweredUntil}, "String");
        }
        if (methodName == "repeat" && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredCount = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall("String", "repeat", loweredReceiver, {loweredCount}, "String");
        }
        if (methodName == "trim" && arguments.empty()) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("String", "trim", loweredReceiver, {}, "String");
        }
        if (methodName == "length" && arguments.empty()) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("String", "length", loweredReceiver, {}, "Int");
        }
        if (methodName == "charAt" && arguments.size() == 1) {
            std::string loweredReceiver = receiver->lowerToIR(builder);
            std::string loweredIndex = arguments[0]->lowerToIR(builder);
            return builder.emitMethodCall("String", "charAt", loweredReceiver, {loweredIndex}, "Char");
        }
        {
            builder.unsupported(location, "la méthode String." + methodName);
        }
    }
    if (receiverType == "Char") {
        if (methodName == "toString") {
            if (!arguments.empty()) {
                builder.unsupported(location, "l'appel de méthode Char.toString");
            }
            std::string loweredReceiver = receiver->lowerToIR(builder);
            return builder.emitMethodCall("Char", "toString", loweredReceiver, {}, "String");
        }
        if (!isBoolBinaryMethod(methodName) || arguments.size() != 1) {
            builder.unsupported(location, "l'appel de méthode Char." + methodName);
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right, "Bool");
    }
    if (receiverType == "Bool") {
        if (!isBoolBinaryMethod(methodName) || arguments.size() != 1) {
            builder.unsupported(location, "la méthode Bool." + methodName);
        }
        std::string left = receiver->lowerToIR(builder);
        std::string right = arguments[0]->lowerToIR(builder);
        return builder.emitBinary(methodName, left, right, "Bool");
    }
    if (isBoolBinaryMethod(methodName) && arguments.size() == 1) {
        std::string loweredReceiver = receiver->lowerToIR(builder);
        std::string loweredArgument = arguments[0]->lowerToIR(builder);
        loweredArgument = boxValueForParameter(
            builder, loweredArgument, arguments[0]->getType(), "Any");
        std::string equalsResult =
            builder.emitMethodCall("Any", "equals", loweredReceiver, {loweredArgument}, "Bool");
        if (methodName == "==") return equalsResult;
        std::string falseValue = emitBoolConstant(builder, false);
        return builder.emitBinary("==", equalsResult, falseValue, "Bool");
    }
    if (isNativeArrayType(receiverType)) {
        std::string loweredReceiver = receiver->lowerToIR(builder);
        if (methodName == "length" && arguments.empty()) {
            if (isObjectArrayType(receiverType)) return builder.emitObjectArrayLength(loweredReceiver);
            if (receiverType == "BoolArray") return builder.emitBoolArrayLength(loweredReceiver);
            if (receiverType == "DoubleArray") return builder.emitDoubleArrayLength(loweredReceiver);
            if (receiverType == "FloatArray") return builder.emitFloatArrayLength(loweredReceiver);
            if (receiverType == "LongArray") return builder.emitLongArrayLength(loweredReceiver);
            return builder.emitIntArrayLength(loweredReceiver);
        }
        if (methodName == "get" && arguments.size() == 1) {
            if (isObjectArrayType(receiverType)) {
                return builder.emitObjectArrayGet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), nativeArrayElementType(receiverType));
            }
            if (receiverType == "BoolArray") {
                return builder.emitBoolArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
            }
            if (receiverType == "DoubleArray") {
                return builder.emitDoubleArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
            }
            if (receiverType == "FloatArray") {
                return builder.emitFloatArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
            }
            if (receiverType == "LongArray") {
                return builder.emitLongArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
            }
            return builder.emitIntArrayGet(loweredReceiver, arguments[0]->lowerToIR(builder));
        }
        if (methodName == "set" && arguments.size() == 2) {
            if (isObjectArrayType(receiverType)) {
                return builder.emitObjectArraySet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
            }
            if (receiverType == "BoolArray") {
                return builder.emitBoolArraySet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
            }
            if (receiverType == "DoubleArray") {
                return builder.emitDoubleArraySet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
            }
            if (receiverType == "FloatArray") {
                return builder.emitFloatArraySet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
            }
            if (receiverType == "LongArray") {
                return builder.emitLongArraySet(
                    loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
            }
            return builder.emitIntArraySet(
                loweredReceiver, arguments[0]->lowerToIR(builder), arguments[1]->lowerToIR(builder));
        }
        builder.unsupported(location, "la méthode " + receiverType + "." + methodName);
    }
    if (receiverType == "ArrayObject[String]" && methodName == "mkString" && arguments.size() == 1) {
        std::string loweredReceiver = receiver->lowerToIR(builder);
        std::string loweredSeparator = arguments[0]->lowerToIR(builder);
        builder.registerFunctionSpecialization("objectStringArrayMkString", {"String"}, "String");
        return builder.emitCall(
            "objectStringArrayMkString[String]", {loweredReceiver, loweredSeparator}, "String");
    }
    if (activeReceiverType != receiverType && stdlibTypeAliasMethodSignature(receiverType, methodName)) {
        std::string loweredReceiver = receiver->lowerToIR(builder);
        std::vector<std::string> loweredArguments = resolvedParameters.empty()
            ? std::vector<std::string>{}
            : lowerCallArgumentsForParameters(builder, arguments, resolvedParameters);
        if (resolvedParameters.empty()) {
            for (size_t i = 0; i < arguments.size(); ++i) {
                std::string loweredArgument = arguments[i]->lowerToIR(builder);
                if (i < resolvedParameterTypes.size()) {
                    loweredArgument = boxValueForParameter(
                        builder, loweredArgument, arguments[i]->getType(), resolvedParameterTypes[i]);
                }
                loweredArguments.push_back(loweredArgument);
            }
        }
        std::vector<std::string> concreteTypeArguments;
        for (const auto& typeArgument : resolvedTypeArguments) {
            concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
        }
        const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
        const std::string activeReceiverBase = genericBaseName(activeReceiverType);
        if (concreteTypeArguments.empty() && activeReceiverBase == "ArrayObject" && methodName == "map") {
            auto parameterizedReturnType = parameterizedTypeFromName(concreteReturnType);
            if (parameterizedReturnType && parameterizedReturnType->first == "ArrayObject" &&
                parameterizedReturnType->second.size() == 1) {
                concreteTypeArguments.push_back(parameterizedReturnType->second[0]);
            }
        }
        const std::string loweredMethodName = concreteTypeArguments.empty()
            ? methodName
            : formatParameterizedType(methodName, concreteTypeArguments);
        if (activeReceiverBase != activeReceiverType) {
            std::vector<std::string> argumentTypes;
            for (const auto& argument : arguments) {
                argumentTypes.push_back(builder.substituteActiveType(argument->getType()));
            }
            builder.registerMethodSpecialization(
                activeReceiverType, activeReceiverBase, methodName, concreteTypeArguments,
                argumentTypes, concreteReturnType);
        }
        return builder.emitMethodCall(
            activeReceiverType, loweredMethodName, loweredReceiver, loweredArguments,
            concreteReturnType);
    }

    std::string loweredReceiver = receiver->lowerToIR(builder);
    std::vector<std::string> loweredArguments = resolvedParameters.empty()
        ? std::vector<std::string>{}
        : lowerCallArgumentsForParameters(builder, arguments, resolvedParameters);
    if (resolvedParameters.empty()) {
        for (size_t i = 0; i < arguments.size(); ++i) {
            std::string loweredArgument = arguments[i]->lowerToIR(builder);
            if (i < resolvedParameterTypes.size()) {
                loweredArgument = boxValueForParameter(
                    builder, loweredArgument, arguments[i]->getType(), resolvedParameterTypes[i]);
            }
            loweredArguments.push_back(loweredArgument);
        }
    }
    std::vector<std::string> argumentTypes;
    for (const auto& argument : arguments) {
        argumentTypes.push_back(builder.substituteActiveType(argument->getType()));
    }
    const std::string concreteReceiverType = builder.substituteActiveType(receiverType);
    std::vector<std::string> concreteTypeArguments;
    for (const auto& typeArgument : resolvedTypeArguments) {
        concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
    }
    const std::string loweredSourceMethodName =
        resolvedMethodName.empty() ? methodName : resolvedMethodName;
    const std::string concreteMethodName = concreteTypeArguments.empty()
        ? loweredSourceMethodName
        : formatParameterizedType(loweredSourceMethodName, concreteTypeArguments);
    const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
    const bool receiverBaseChanged = !resolvedOwnerType.empty() &&
        genericBaseName(concreteReceiverType) != genericBaseName(resolvedOwnerType);
    const bool shouldRegisterMethodSpecialization =
        !resolvedOwnerType.empty() && ( !concreteTypeArguments.empty() || concreteReceiverType != resolvedOwnerType );
    const bool shouldSpecializeAsConcreteClass =
        shouldRegisterMethodSpecialization && !receiverBaseChanged;
    if (shouldRegisterMethodSpecialization &&
        !isAbstractTraitMethod(builder.getContext(), resolvedOwnerType, loweredSourceMethodName)) {
        const std::string concreteClassName =
            shouldSpecializeAsConcreteClass ? concreteReceiverType : resolvedOwnerType;
        builder.registerMethodSpecialization(
            concreteClassName, resolvedOwnerType, loweredSourceMethodName, concreteTypeArguments,
            argumentTypes, concreteReturnType);
    }
    const std::string methodOwnerType =
        shouldSpecializeAsConcreteClass || resolvedOwnerType.empty()
            ? concreteReceiverType
            : resolvedOwnerType;
    const bool isSuperCall = dynamic_cast<const SuperNode*>(receiver.get()) != nullptr;
    if (isSuperCall) {
        return builder.emitStaticMethodCall(
            methodOwnerType,
            concreteMethodName, loweredReceiver, loweredArguments, concreteReturnType);
    }
    return builder.emitMethodCall(
        methodOwnerType,
        concreteMethodName, loweredReceiver, loweredArguments, concreteReturnType);
}

FunctionCallNode::FunctionCallNode(
    std::string functionName, std::vector<std::unique_ptr<ASTNode>> args,
    std::vector<std::string> genericTypeArguments, std::string initialResolvedType,
    std::string userFacingName)
    : name(std::move(functionName)), resolvedFunctionName(name), diagnosticName(std::move(userFacingName)),
      arguments(std::move(args)), typeArguments(std::move(genericTypeArguments)),
      resolvedType(std::move(initialResolvedType)) {}

std::string FunctionCallNode::getType() {
    if (name == "print") return "Unit";
    if (name == "readLine") return "String";
    if (name == "readFile") return "String";
    if (name == "writeFile") return "Int";
    if (name == "appendFile") return "Int";
    if (name == "fileExists") return "Bool";
    if (name == "deleteFile") return "Bool";
    if (name == "renameFile") return "Bool";
    if (name == "createDir") return "Bool";
    if (name == "parseInt") return "Int";
    if (name == "timeSeed") return "Int";
    return resolvedType;
}

void FunctionCallNode::validateSemantics(CompilerContext& context) {
    for (const auto& argument : arguments) argument->validateSemantics(context);
    if (name == "print") {
        if (arguments.size() != 1) {
            semanticError("print: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        resolvedType = "Unit";
        resolvedParameterTypes = {"Any"};
        return;
    }
    if (name == "readLine") {
        if (!arguments.empty()) {
            semanticError("readLine: 0 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        resolvedType = "String";
        return;
    }
    if (name == "readFile") {
        if (arguments.size() != 1) {
            semanticError("readFile: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "readFile, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "String";
        return;
    }
    if (name == "writeFile") {
        if (arguments.size() != 2) {
            semanticError("writeFile: 2 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "writeFile, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        if (arguments[1]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[1]->getLocation(),
                "writeFile, paramètre 'content': type 'String' attendu, '" +
                arguments[1]->getType() + "' reçu");
        }
        resolvedType = "Int";
        return;
    }
    if (name == "appendFile") {
        if (arguments.size() != 2) {
            semanticError("appendFile: 2 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "appendFile, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        if (arguments[1]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[1]->getLocation(),
                "appendFile, paramètre 'content': type 'String' attendu, '" +
                arguments[1]->getType() + "' reçu");
        }
        resolvedType = "Int";
        return;
    }
    if (name == "fileExists") {
        if (arguments.size() != 1) {
            semanticError("fileExists: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "fileExists, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "Bool";
        return;
    }
    if (name == "deleteFile") {
        if (arguments.size() != 1) {
            semanticError("deleteFile: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "deleteFile, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "Bool";
        return;
    }
    if (name == "renameFile") {
        if (arguments.size() != 2) {
            semanticError("renameFile: 2 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "renameFile, paramètre 'from': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        if (arguments[1]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[1]->getLocation(),
                "renameFile, paramètre 'to': type 'String' attendu, '" +
                arguments[1]->getType() + "' reçu");
        }
        resolvedType = "Bool";
        return;
    }
    if (name == "createDir") {
        if (arguments.size() != 1) {
            semanticError("createDir: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "createDir, paramètre 'path': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "Bool";
        return;
    }
    if (name == "parseInt") {
        if (arguments.size() != 1) {
            semanticError("parseInt: 1 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        if (arguments[0]->getType() != "String") {
            throw CompilerError(
                ErrorKind::Semantic, arguments[0]->getLocation(),
                "parseInt, paramètre 'value': type 'String' attendu, '" +
                arguments[0]->getType() + "' reçu");
        }
        resolvedType = "Int";
        return;
    }
    if (name == "timeSeed") {
        if (!arguments.empty()) {
            semanticError("timeSeed: 0 argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
        }
        resolvedType = "Int";
        return;
    }
    std::vector<std::string> actualArgumentTypes;
    for (const auto& argument : arguments) actualArgumentTypes.push_back(argument->getType());

    auto overloads = functionOverloadNames(context, name);
    if (overloads.empty()) {
        const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
        semanticError("fonction inconnue: " + displayName + recommendedStdlibFunctionSuffix(displayName));
    }
    std::optional<std::string> overloadName;
    if (overloads.size() == 1) {
        overloadName = overloads[0];
    } else {
        overloadName = resolveExactFunctionOverload(context, name, actualArgumentTypes, typeArguments);
    }
    if (!overloadName) {
        const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
        const auto matches = exactFunctionOverloadMatches(context, name, actualArgumentTypes, typeArguments);
        if (matches.concreteMatches.size() > 1 ||
            (matches.concreteMatches.empty() && matches.genericMatches.size() > 1)) {
            semanticError(
                "appel de fonction surchargée ambigu pour '" +
                formatFunctionCallShape(displayName, actualArgumentTypes) + "'" +
                "\ncandidats:" + formatFunctionOverloadCandidates(context, displayName) +
                recommendedStdlibFunctionSuffix(displayName));
        }
        semanticError(
            formatNoMatchingFunctionOverloadMessage(context, displayName, actualArgumentTypes) +
            recommendedStdlibFunctionSuffix(displayName));
    }
    resolvedFunctionName = *overloadName;
    auto function = context.functions.find(resolvedFunctionName);
    if (function == context.functions.end()) {
        const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
        semanticError("fonction inconnue: " + displayName + recommendedStdlibFunctionSuffix(displayName));
    }
    if (function->second.typeParameters.empty()) {
        if (!typeArguments.empty()) {
            const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
            semanticError(
                "la fonction '" + displayName + "' n'accepte pas d'arguments de type" +
                recommendedStdlibFunctionSuffix(displayName));
        }
        validateArguments(
            context, diagnosticName.empty() ? name : diagnosticName,
            arguments, function->second.parameters, location);
        resolvedType = function->second.returnType;
        resolvedParameterTypes = parameterTypes(function->second.parameters);
        resolvedParameters = function->second.parameters;
        resolvedTypeArguments.clear();
        return;
    }
    std::optional<std::map<std::string, std::string>> substitution;
    if (typeArguments.empty()) {
        substitution = inferGenericFunctionSubstitutionFromArguments(function->second, arguments);
    } else {
        substitution = genericFunctionSubstitutionFor(function->second, typeArguments);
    }
    if (!substitution) {
        if (typeArguments.empty()) {
            const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
            semanticError(
                "impossible d'inférer les arguments de type pour la fonction générique '" +
                displayName + "'" + recommendedStdlibFunctionSuffix(displayName));
        }
        const std::string displayName = diagnosticName.empty() ? name : diagnosticName;
        semanticError(
            "la fonction générique '" + displayName + "' attend " +
            std::to_string(function->second.typeParameters.size()) + " argument(s) de type" +
            recommendedStdlibFunctionSuffix(displayName));
    }
    auto parameters = substituteParameters(function->second.parameters, *substitution);
    validateArguments(
        context, diagnosticName.empty() ? name : diagnosticName,
        arguments, parameters, location);
    resolvedType = substituteType(function->second.returnType, *substitution);
    resolvedParameterTypes = parameterTypes(parameters);
    resolvedParameters = parameters;
    resolvedTypeArguments = orderedTypeArguments(function->second.typeParameters, *substitution);
}

std::string FunctionCallNode::lowerToIR(IRBuilder& builder) const {
    if (name == "print") {
        if (arguments.size() != 1) {
            builder.unsupported(location, "l'appel de print");
        }
        std::string loweredArgument = arguments[0]->lowerToIR(builder);
        const std::string concreteArgumentType = builder.substituteActiveType(arguments[0]->getType());
        loweredArgument = boxValueForParameter(
            builder, loweredArgument, concreteArgumentType, "Any");
        std::string loweredString = builder.emitMethodCall(
            "Any", "toString", loweredArgument, {}, "String");
        return builder.emitCall("print", {loweredString}, "Unit");
    }

    std::vector<std::string> loweredArguments = resolvedParameters.empty()
        ? std::vector<std::string>{}
        : lowerCallArgumentsForParameters(builder, arguments, resolvedParameters);
    if (resolvedParameters.empty()) {
        for (size_t i = 0; i < arguments.size(); ++i) {
            std::string loweredArgument = arguments[i]->lowerToIR(builder);
            if (i < resolvedParameterTypes.size()) {
                loweredArgument = boxValueForParameter(
                    builder, loweredArgument, arguments[i]->getType(), resolvedParameterTypes[i]);
            }
            loweredArguments.push_back(loweredArgument);
        }
    }
    std::vector<std::string> concreteTypeArguments;
    for (const auto& typeArgument : resolvedTypeArguments) {
        concreteTypeArguments.push_back(builder.substituteActiveType(typeArgument));
    }
    if (resolvedFunctionName == "Set.apply" && concreteTypeArguments.size() == 1 &&
        loweredArguments.size() == 1) {
        const std::string elementType = concreteTypeArguments[0];
        const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
        if (elementType == "Int") {
            return builder.emitCall("setFromArrayInt", loweredArguments, concreteReturnType);
        }
        if (elementType == "Long") {
            return builder.emitCall("setFromArrayLong", loweredArguments, concreteReturnType);
        }
        if (elementType == "Float") {
            return builder.emitCall("setFromArrayFloat", loweredArguments, concreteReturnType);
        }
        if (elementType == "Double") {
            return builder.emitCall("setFromArrayDouble", loweredArguments, concreteReturnType);
        }
        if (elementType == "Bool") {
            return builder.emitCall("setFromArrayBool", loweredArguments, concreteReturnType);
        }
        builder.registerFunctionSpecialization("setFromArray", {elementType}, concreteReturnType);
        return builder.emitCall(
            formatParameterizedType("setFromArray", {elementType}),
            loweredArguments, concreteReturnType);
    }
    if (!concreteTypeArguments.empty()) {
        const std::string concreteReturnType = builder.substituteActiveType(resolvedType);
        builder.registerFunctionSpecialization(resolvedFunctionName, concreteTypeArguments, concreteReturnType);
        return builder.emitCall(
            formatParameterizedType(resolvedFunctionName, concreteTypeArguments),
            loweredArguments, concreteReturnType);
    }
    return builder.emitCall(resolvedFunctionName, loweredArguments, resolvedType);
}

FunctionReferenceNode::FunctionReferenceNode(
    std::string functionName, std::string functionType,
    std::vector<std::string> genericTypeArguments, std::vector<Capture> capturedValues,
    std::string resolvedName)
    : name(std::move(functionName)), resolvedFunctionName(resolvedName.empty() ? name : std::move(resolvedName)),
      resolvedType(std::move(functionType)),
      typeArguments(std::move(genericTypeArguments)),
      captures(std::move(capturedValues)) {}

std::string FunctionReferenceNode::getType() {
    return resolvedType;
}

void FunctionReferenceNode::validateSemantics(CompilerContext& context) {
    auto function = context.functions.find(resolvedFunctionName);
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
        builder.registerFunctionSpecialization(resolvedFunctionName, concreteTypeArguments, concreteReturnType);
        return builder.emitFunctionReference(
            formatParameterizedType(resolvedFunctionName, concreteTypeArguments), loweredCaptures);
    }
    return builder.emitFunctionReference(resolvedFunctionName, loweredCaptures);
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
    calleeType = symbol->second;
    auto functionType = functionTypeFromName(calleeType);
    if (!functionType) {
        semanticError("la valeur '" + name + "' n'est pas appelable");
    }
    const size_t expectedArgumentCount = functionType->parameterTypes.size();
    if (arguments.size() != expectedArgumentCount) {
        semanticError(
            calleeType + ": " + std::to_string(expectedArgumentCount) +
            " argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
    }
    for (const auto& argument : arguments) {
        if (dynamic_cast<SplatNode*>(argument.get())) {
            throw CompilerError(
                ErrorKind::Semantic, argument->getLocation(),
                calleeType + ": ': _*' est uniquement autorisé pour un paramètre répété");
        }
    }
    resolvedType = functionType->returnType;
    resolvedParameterTypes = functionType->parameterTypes;
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (!isTypeAssignable(context, arguments[i]->getType(), functionType->parameterTypes[i])) {
            throw CompilerError(
                ErrorKind::Semantic, arguments[i]->getLocation(),
                calleeType + ", paramètre: type '" + functionType->parameterTypes[i] +
                "' attendu, '" + arguments[i]->getType() + "' reçu");
        }
    }
}

std::string FunctionValueCallNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredFunction = builder.emitLoad(symbolName, calleeType);
    std::vector<std::string> loweredArguments;
    for (size_t i = 0; i < arguments.size(); ++i) {
        std::string loweredArgument = arguments[i]->lowerToIR(builder);
        if (i < resolvedParameterTypes.size()) {
            loweredArgument = boxValueForParameter(
                builder, loweredArgument, arguments[i]->getType(), resolvedParameterTypes[i]);
        }
        loweredArguments.push_back(loweredArgument);
    }
    return builder.emitIndirectCall(loweredFunction, loweredArguments, resolvedType);
}

FunctionExpressionCallNode::FunctionExpressionCallNode(
    std::string functionName, std::unique_ptr<ASTNode> functionExpression,
    std::vector<std::unique_ptr<ASTNode>> args, std::string initialResolvedType)
    : name(std::move(functionName)), callee(std::move(functionExpression)), arguments(std::move(args)),
      resolvedType(std::move(initialResolvedType)) {}

std::string FunctionExpressionCallNode::getType() {
    return resolvedType;
}

void FunctionExpressionCallNode::validateSemantics(CompilerContext& context) {
    callee->validateSemantics(context);
    for (const auto& argument : arguments) argument->validateSemantics(context);
    auto functionType = functionTypeFromName(callee->getType());
    if (!functionType) {
        semanticError("la valeur '" + name + "' n'est pas appelable");
    }
    const size_t expectedArgumentCount = functionType->parameterTypes.size();
    if (arguments.size() != expectedArgumentCount) {
        semanticError(
            callee->getType() + ": " + std::to_string(expectedArgumentCount) +
            " argument(s) attendu(s), " + std::to_string(arguments.size()) + " reçu(s)");
    }
    for (const auto& argument : arguments) {
        if (dynamic_cast<SplatNode*>(argument.get())) {
            throw CompilerError(
                ErrorKind::Semantic, argument->getLocation(),
                callee->getType() + ": ': _*' est uniquement autorisé pour un paramètre répété");
        }
    }
    resolvedType = functionType->returnType;
    resolvedParameterTypes = functionType->parameterTypes;
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (!isTypeAssignable(context, arguments[i]->getType(), functionType->parameterTypes[i])) {
            throw CompilerError(
                ErrorKind::Semantic, arguments[i]->getLocation(),
                callee->getType() + ", paramètre: type '" + functionType->parameterTypes[i] +
                "' attendu, '" + arguments[i]->getType() + "' reçu");
        }
    }
}

std::string FunctionExpressionCallNode::lowerToIR(IRBuilder& builder) const {
    std::string loweredFunction = callee->lowerToIR(builder);
    std::vector<std::string> loweredArguments;
    for (size_t i = 0; i < arguments.size(); ++i) {
        std::string loweredArgument = arguments[i]->lowerToIR(builder);
        if (i < resolvedParameterTypes.size()) {
            loweredArgument = boxValueForParameter(
                builder, loweredArgument, arguments[i]->getType(), resolvedParameterTypes[i]);
        }
        loweredArguments.push_back(loweredArgument);
    }
    return builder.emitIndirectCall(loweredFunction, loweredArguments, resolvedType);
}

SplatNode::SplatNode(std::unique_ptr<ASTNode> expr)
    : expression(std::move(expr)) {}

std::string SplatNode::getType() {
    return expression->getType();
}

void SplatNode::validateSemantics(CompilerContext& context) {
    expression->validateSemantics(context);
}

std::string SplatNode::lowerToIR(IRBuilder& builder) const {
    return expression->lowerToIR(builder);
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
    if (thenBranch->getType() == elseBranch->getType()) {
        resolvedType = thenBranch->getType();
    } else {
        resolvedType = "Unit";
    }
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
    if (resolvedType == "Unit") {
        return builder.emitConstant("0", "Unit");
    }
    return builder.emitPhi(thenValue, elseValue, resolvedType);
}

MatchNode::MatchNode(std::unique_ptr<ASTNode> value, std::vector<Branch> matchBranches)
    : scrutinee(std::move(value)), branches(std::move(matchBranches)) {}

std::string MatchNode::getType() {
    return resolvedType;
}

void MatchNode::validateSemantics(CompilerContext& context) {
    scrutinee->validateSemantics(context);
    scrutineeType = scrutinee->getType();
    if (branches.empty()) {
        semanticError("match sans branches");
    }
    if (!branches.back().isWildcard) {
        semanticError("branche '_' finale attendue dans un match");
    }
    if (branches.back().guard) {
        semanticError("la branche '_' finale ne peut pas avoir de garde");
    }
    bool sawWildcard = false;
    for (const auto& branch : branches) {
        if (sawWildcard) {
            throw CompilerError(
                ErrorKind::Semantic, branch.location,
                "aucune branche n'est autorisée après '_' dans un match");
        }
        if (branch.isWildcard) {
            sawWildcard = true;
        } else {
            if (!branch.isNamedPattern) {
                branch.pattern->validateSemantics(context);
                if (branch.pattern->getType() != scrutineeType) {
                    throw CompilerError(
                        ErrorKind::Semantic, branch.pattern->getLocation(),
                        "motif de match: type '" + scrutineeType + "' attendu, '" +
                        branch.pattern->getType() + "' reçu");
                }
            }
        }
        bool hadSymbol = false;
        std::string savedType;
        if (branch.isNamedPattern) {
            auto existing = context.semanticSymbolTypes.find(branch.boundSymbol);
            if (existing != context.semanticSymbolTypes.end()) {
                hadSymbol = true;
                savedType = existing->second;
            }
            context.semanticSymbolTypes[branch.boundSymbol] = scrutineeType;
        }
        if (branch.guard) {
            branch.guard->validateSemantics(context);
            if (branch.guard->getType() != "Bool") {
                throw CompilerError(
                    ErrorKind::Semantic, branch.guard->getLocation(),
                    "la garde d'une branche de match doit être de type Bool");
            }
        }
        branch.body->validateSemantics(context);
        if (branch.isNamedPattern) {
            if (hadSymbol) {
                context.semanticSymbolTypes[branch.boundSymbol] = savedType;
            } else {
                context.semanticSymbolTypes.erase(branch.boundSymbol);
            }
        }
    }
    if (!sawWildcard) {
        semanticError("branche '_' finale attendue dans un match");
    }
    const std::string branchType = branches.front().body->getType();
    for (const auto& branch : branches) {
        if (branch.body->getType() != branchType) {
            throw CompilerError(
                ErrorKind::Semantic, branch.body->getLocation(),
                "les branches d'un match doivent avoir le même type");
        }
    }
    resolvedType = branchType;
}

std::string MatchNode::lowerToIR(IRBuilder& builder) const {
    std::string endLabel = builder.makeLabel("match.end");
    std::string scrutineeValue = scrutinee->lowerToIR(builder);
    std::vector<std::string> branchValues;

    for (size_t i = 0; i < branches.size(); ++i) {
        const auto& branch = branches[i];
        const bool hasNextBranch = (i + 1) < branches.size();
        std::string nextLabel = hasNextBranch ? builder.makeLabel("match.next") : endLabel;
        if (branch.isNamedPattern) {
            builder.emitStore(branch.boundSymbol, scrutineeValue, scrutineeType);
        } else if (!branch.isWildcard) {
            std::string patternValue = branch.pattern->lowerToIR(builder);
            std::string condition = scrutineeType == "String"
                ? builder.emitMethodCall("String", "==", scrutineeValue, {patternValue}, "Bool")
                : builder.emitBinary("==", scrutineeValue, patternValue, "Bool");
            builder.emitBranchIfFalse(condition, nextLabel);
        }
        if (branch.guard) {
            std::string guardValue = branch.guard->lowerToIR(builder);
            builder.emitBranchIfFalse(guardValue, nextLabel);
        }
        branchValues.push_back(branch.body->lowerToIR(builder));
        builder.emitJump(endLabel);
        if (hasNextBranch) {
            builder.emitLabel(nextLabel);
        }
    }

    builder.emitLabel(endLabel);
    return builder.emitPhi(branchValues, resolvedType);
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
    if (body && !isTypeAssignable(context, body->getType(), returnType)) {
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
    const std::string functionName = className.empty() ? name : "method." + className + "." + name;
    builder.beginFunction(functionName, irParameters, returnType);
    if (!className.empty()) {
        builder.bindThis();
        builder.bindParameter("this", "this");
    } else if (!captures.empty()) {
        builder.bindClosure();
        for (size_t i = 0; i < captures.size(); ++i) {
            builder.bindCapture(captures[i].symbolName, static_cast<int>(i), captures[i].type);
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
    if (ownerTypeParameters.empty()) {
        if (concreteClassName != className) {
            builder.unsupported(location, "la spécialisation de méthode " + concreteClassName + "." + name);
        }
    } else {
        if (!parameterizedType || parameterizedType->first != className ||
            parameterizedType->second.size() != ownerTypeParameters.size()) {
            builder.unsupported(location, "la spécialisation de méthode " + concreteClassName + "." + name);
        }
    }
    if (concreteMethodTypeArguments.size() != typeParameters.size()) {
        builder.unsupported(location, "la spécialisation de méthode " + concreteClassName + "." + name);
    }

    std::map<std::string, std::string> substitution;
    substitution[className] = concreteClassName;
    if (parameterizedType) {
        for (size_t i = 0; i < ownerTypeParameters.size(); ++i) {
            substitution[ownerTypeParameters[i]] = parameterizedType->second[i];
        }
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
    builder.beginFunction("method." + concreteClassName + "." + concreteMethodName, irParameters, returnType);
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

VarDeclNode::VarDeclNode(
    std::string n, std::string symbol, std::unique_ptr<ASTNode> init, bool mut,
    std::string annotatedType)
    : name(std::move(n)), symbolName(std::move(symbol)), initializer(std::move(init)),
      isMutable(mut), declaredType(std::move(annotatedType)) {}

std::string VarDeclNode::getType() {
    if (!declaredType.empty()) return declaredType;
    return initializer ? initializer->getType() : "Unit";
}

void VarDeclNode::validateSemantics(CompilerContext& context) {
    initializer->validateSemantics(context);
    if (!declaredType.empty() && !isTypeAssignable(context, initializer->getType(), declaredType)) {
        semanticError(
            "Déclaration invalide pour '" + name + "': type '" + declaredType +
            "' attendu, '" + initializer->getType() + "' reçu");
    }
    context.semanticSymbolTypes[symbolName] = getType();
}

std::string VarDeclNode::lowerToIR(IRBuilder& builder) const {
    std::string value = initializer->lowerToIR(builder);
    const std::string storageType =
        declaredType.empty() ? const_cast<ASTNode*>(initializer.get())->getType() : declaredType;
    builder.emitStore(symbolName, value, storageType);
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
    if (!isTypeAssignable(context, value->getType(), targetType)) {
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
