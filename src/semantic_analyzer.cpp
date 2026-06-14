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
        for (const auto& [methodName, returnType] : classInfo.methods) {
            if (!isKnownType(returnType)) {
                throw std::runtime_error(
                    "Type de retour inconnu '" + returnType + "' pour la méthode '" +
                    className + "." + methodName + "'");
            }
        }
    }
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    return type == "Int" || type == "String" || type == "Unit" || context.classes.count(type) != 0;
}
