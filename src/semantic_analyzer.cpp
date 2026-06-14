#include "semantic_analyzer.hpp"
SemanticAnalyzer::SemanticAnalyzer(CompilerContext& context) : context(context) {}

void SemanticAnalyzer::analyze(ProgramNode& program) {
    validateDeclaredTypes();
    program.validateSemantics(context);
}

void SemanticAnalyzer::validateDeclaredTypes() const {
    for (const auto& [className, classInfo] : context.classes) {
        for (const auto& field : classInfo.fields) {
            if (!isKnownType(field.type)) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "type inconnu '" + field.type + "' pour le champ '" + className + "." + field.name + "'");
            }
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            if (signature.parameters.size() > 5) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "la méthode '" + className + "." + methodName + "' dépasse la limite de 5 paramètres");
            }
            for (const auto& parameter : signature.parameters) {
                if (!isKnownType(parameter.type)) {
                    throw CompilerError(
                        ErrorKind::Semantic, parameter.location,
                        "type inconnu '" + parameter.type + "' pour le paramètre '" +
                        className + "." + methodName + "." + parameter.name + "'");
                }
            }
            if (!isKnownType(signature.returnType)) {
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
            if (!isKnownType(parameter.type)) {
                throw CompilerError(
                    ErrorKind::Semantic, parameter.location,
                    "type inconnu '" + parameter.type + "' pour le paramètre '" +
                    functionName + "." + parameter.name + "'");
            }
        }
        if (!isKnownType(signature.returnType)) {
            throw CompilerError(
                ErrorKind::Semantic, signature.returnTypeLocation,
                "type de retour inconnu '" + signature.returnType + "' pour la fonction '" +
                functionName + "'");
        }
    }
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    return type == "Int" || type == "String" || type == "Unit" || type == "IntArray" ||
           isFunctionTypeName(type) || context.classes.count(type) != 0;
}
