#include "semantic_analyzer.hpp"
#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer(CompilerContext& context) : context(context) {}

void SemanticAnalyzer::analyze(ProgramNode& program) {
    validateDeclaredTypes();
    program.validateSemantics(context);
}

void SemanticAnalyzer::validateDeclaredTypes() const {
    for (const auto& [className, classInfo] : context.classes) {
        for (const auto& [fieldName, fieldType] : classInfo.fields) {
            if (!isKnownType(fieldType)) {
                throw std::runtime_error(
                    "Type inconnu '" + fieldType + "' pour le champ '" + className + "." + fieldName + "'");
            }
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            if (signature.parameters.size() > 5) {
                throw std::runtime_error(
                    "La méthode '" + className + "." + methodName + "' dépasse la limite de 5 paramètres");
            }
            for (const auto& parameter : signature.parameters) {
                if (!isKnownType(parameter.type)) {
                    throw std::runtime_error(
                        "Type inconnu '" + parameter.type + "' pour le paramètre '" +
                        className + "." + methodName + "." + parameter.name + "'");
                }
            }
            if (!isKnownType(signature.returnType)) {
                throw std::runtime_error(
                    "Type de retour inconnu '" + signature.returnType + "' pour la méthode '" +
                    className + "." + methodName + "'");
            }
        }
    }
    for (const auto& [functionName, signature] : context.functions) {
        if (signature.parameters.size() > 6) {
            throw std::runtime_error(
                "La fonction '" + functionName + "' dépasse la limite de 6 paramètres");
        }
        if (functionName == "main" && !signature.parameters.empty()) {
            throw std::runtime_error("La fonction 'main' ne peut pas accepter de paramètres");
        }
        for (const auto& parameter : signature.parameters) {
            if (!isKnownType(parameter.type)) {
                throw std::runtime_error(
                    "Type inconnu '" + parameter.type + "' pour le paramètre '" +
                    functionName + "." + parameter.name + "'");
            }
        }
        if (!isKnownType(signature.returnType)) {
            throw std::runtime_error(
                "Type de retour inconnu '" + signature.returnType + "' pour la fonction '" +
                functionName + "'");
        }
    }
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    return type == "Int" || type == "String" || type == "Unit" || context.classes.count(type) != 0;
}
