#include "semantic_analyzer.hpp"
SemanticAnalyzer::SemanticAnalyzer(CompilerContext& context) : context(context) {}

void SemanticAnalyzer::analyze(ProgramNode& program) {
    validateDeclaredTypes();
    program.validateSemantics(context);
}

void SemanticAnalyzer::validateDeclaredTypes() const {
    for (const auto& [className, classInfo] : context.classes) {
        for (const auto& field : classInfo.fields) {
            if (!isKnownTypeInScope(field.type, classInfo.typeParameters)) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "type inconnu '" + field.type + "' pour le champ '" + className + "." + field.name + "'");
            }
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            auto methodTypeParameters = classInfo.typeParameters;
            methodTypeParameters.insert(
                methodTypeParameters.end(), signature.typeParameters.begin(), signature.typeParameters.end());
            if (signature.parameters.size() > 5) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "la méthode '" + className + "." + methodName + "' dépasse la limite de 5 paramètres");
            }
            for (const auto& parameter : signature.parameters) {
                if (!isKnownTypeInScope(parameter.type, methodTypeParameters)) {
                    throw CompilerError(
                        ErrorKind::Semantic, parameter.location,
                        "type inconnu '" + parameter.type + "' pour le paramètre '" +
                        className + "." + methodName + "." + parameter.name + "'");
                }
            }
            if (!isKnownTypeInScope(signature.returnType, methodTypeParameters)) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.returnTypeLocation,
                    "type de retour inconnu '" + signature.returnType + "' pour la méthode '" +
                    className + "." + methodName + "'");
            }
        }
    }
    for (const auto& [functionName, signature] : context.functions) {
        if (signature.parameters.size() > 6) {
            throw CompilerError(
                ErrorKind::Semantic, signature.location,
                "la fonction '" + functionName + "' dépasse la limite de 6 paramètres");
        }
        if (functionName == "main" && !signature.parameters.empty()) {
            throw CompilerError(
                ErrorKind::Semantic, signature.location,
                "la fonction 'main' ne peut pas accepter de paramètres");
        }
        for (const auto& parameter : signature.parameters) {
            if (!isKnownTypeInScope(parameter.type, signature.typeParameters)) {
                throw CompilerError(
                    ErrorKind::Semantic, parameter.location,
                    "type inconnu '" + parameter.type + "' pour le paramètre '" +
                    functionName + "." + parameter.name + "'");
            }
        }
        if (!isKnownTypeInScope(signature.returnType, signature.typeParameters)) {
            throw CompilerError(
                ErrorKind::Semantic, signature.returnTypeLocation,
                "type de retour inconnu '" + signature.returnType + "' pour la fonction '" +
                functionName + "'");
        }
    }
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    if (type == "Int" || type == "Long" || type == "Float" || type == "Double" || type == "Bool" ||
        type == "String" || type == "Unit" || type == "IntArray" || type == "LongArray" ||
        isFunctionTypeName(type)) {
        return true;
    }
    auto classIt = context.classes.find(type);
    if (classIt != context.classes.end()) return classIt->second.typeParameters.empty();
    auto substitution = genericSubstitutionFor(context, type);
    if (!substitution) return false;
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return false;
    for (const auto& argument : parameterizedType->second) {
        if (!isKnownType(argument)) return false;
    }
    return true;
}

bool SemanticAnalyzer::isKnownTypeInScope(
    const std::string& type, const std::vector<std::string>& typeParameters) const {
    if (isTypeParameterName(type, typeParameters)) return true;
    auto functionType = functionTypeFromName(type);
    if (functionType) {
        for (const auto& parameterType : functionType->parameterTypes) {
            if (!isKnownTypeInScope(parameterType, typeParameters)) return false;
        }
        return isKnownTypeInScope(functionType->returnType, typeParameters);
    }
    auto parameterizedType = parameterizedTypeFromName(type);
    if (parameterizedType) {
        const auto& [baseName, arguments] = *parameterizedType;
        auto classIt = context.classes.find(baseName);
        if (classIt == context.classes.end()) return false;
        if (classIt->second.typeParameters.size() != arguments.size()) return false;
        for (const auto& argument : arguments) {
            if (!isKnownTypeInScope(argument, typeParameters)) return false;
        }
        return true;
    }
    return isKnownType(type);
}
