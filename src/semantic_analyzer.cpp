#include "semantic_analyzer.hpp"

namespace {

using MethodOwnerMap = std::map<std::string, std::set<std::string>>;
using ClassFieldOwnerMap = std::map<std::string, std::set<std::string>>;
using ParentMethodProviders = std::map<std::string, std::set<std::string>>;

std::string formatMethodProviders(const std::set<std::string>& providers) {
    std::string message;
    bool first = true;
    for (const auto& provider : providers) {
        if (!first) message += ", ";
        message += provider;
        first = false;
    }
    return message;
}

void collectVisibleMethodsInHierarchy(
    const CompilerContext& context, const std::string& receiverType,
    std::map<std::string, std::string> substitution,
    std::set<std::string>& visiting,
    MethodOwnerMap& visibleMethods) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visiting.count(classLookupName)) return;

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) return;

    if (auto classGenericSubstitution = genericSubstitutionFor(context, receiverType)) {
        substitution = *classGenericSubstitution;
    }

    const std::string providerType = substituteType(receiverType, substitution);
    for (const auto& [methodName, _] : classIt->second.methods) {
        if (visibleMethods[methodName].empty()) {
            visibleMethods[methodName].insert(providerType);
        }
    }

    visiting.insert(classLookupName);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, substitution);
        collectVisibleMethodsInHierarchy(context, concreteParentType, substitution, visiting, visibleMethods);
    }
    visiting.erase(classLookupName);
}

}

SemanticAnalyzer::SemanticAnalyzer(CompilerContext& context) : context(context) {}

void SemanticAnalyzer::analyze(ProgramNode& program) {
    validateDeclaredTypes();
    program.validateSemantics(context);
}

void SemanticAnalyzer::validateDeclaredTypes() {
    ensureAnyRootType();
    validateParentTypes();
    validateInheritanceCycles();

    for (const auto& [className, classInfo] : context.classes) {
        for (const auto& field : classInfo.fields) {
            if (!isKnownTypeInScope(field.type, classInfo.typeParameters)) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "type inconnu '" + field.type + "' pour le champ '" +
                    className + "." + field.name + "'");
            }
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            auto methodTypeParameters = classInfo.typeParameters;
            methodTypeParameters.insert(
                methodTypeParameters.end(), signature.typeParameters.begin(), signature.typeParameters.end());
            if (signature.parameters.size() > 5) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "la méthode '" + className + "." + methodName +
                    "' dépasse la limite de 5 paramètres");
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

    for (const auto& [className, classInfo] : context.classes) {
        if (className == "Any") continue;

        ParentMethodProviders inheritedProviders;
        ClassFieldOwnerMap inheritedFieldProviders;
        std::set<std::string> ownMethods;
        const auto classTypeSubstitution = classTypeTemplateSubstitution(context, className);
        for (const auto& methodEntry : classInfo.methods) {
            ownMethods.insert(methodEntry.first);
        }
        for (const auto& field : classInfo.fields) {
            inheritedFieldProviders[field.name].insert(className);
        }
        for (const auto& parentType : classInfo.parentTypes) {
            MethodOwnerMap visibleMethods;
            std::map<std::string, std::string> substitution = classTypeSubstitution;
            std::set<std::string> visiting;
            const std::string concreteParentType = substituteType(parentType, classTypeSubstitution);
            collectVisibleMethodsInHierarchy(
                context, concreteParentType, substitution, visiting, visibleMethods);
            for (const auto& [methodName, ownerClasses] : visibleMethods) {
                if (ownMethods.count(methodName)) continue;
                inheritedProviders[methodName].insert(ownerClasses.begin(), ownerClasses.end());
            }
            ClassFieldOwnerMap visibleFields;
            std::map<std::string, std::string> fieldTypes;
            collectVisibleFieldsInHierarchy(
                context, concreteParentType, visiting, visibleFields, fieldTypes);
            for (const auto& [fieldName, ownerClasses] : visibleFields) {
                inheritedFieldProviders[fieldName].insert(ownerClasses.begin(), ownerClasses.end());
            }
        }
        for (const auto& [methodName, providers] : inheritedProviders) {
            if (providers.size() <= 1) continue;
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "conflit d'héritage pour la méthode '" + methodName +
                "' dans la classe '" + className + "': plusieurs définitions dans [" +
                formatMethodProviders(providers) + "]");
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            if (!signature.isOverride) continue;
            bool hasInheritedMethod = false;
            for (const auto& parentType : classInfo.parentTypes) {
                const std::string concreteParentType = substituteType(parentType, classTypeSubstitution);
                const auto inheritedCandidates = collectClassMethodLookupCandidates(
                    context, concreteParentType, methodName);
                if (!inheritedCandidates.empty()) {
                    hasInheritedMethod = true;
                    break;
                }
            }
            if (!hasInheritedMethod) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "override invalide pour '" + className + "." + methodName +
                    "' : aucune méthode héritée correspondante");
            }
        }
        for (const auto& [fieldName, providers] : inheritedFieldProviders) {
            if (providers.size() <= 1) continue;
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "conflit d'héritage pour le champ '" + fieldName +
                "' dans la classe '" + className + "': plusieurs définitions dans [" +
                formatMethodProviders(providers) + "]");
        }
    }

    for (const auto& [className, _] : context.classes) {
        if (className == "Any") continue;
        std::map<std::string, int> classLayout;
        int offset = 8;
        for (const auto& [fieldName, fieldType] : collectClassFieldsInHierarchyForLayout(context, className)) {
            (void) fieldType;
            classLayout[fieldName] = offset;
            offset += 8;
        }
        context.classLayouts[className] = std::move(classLayout);
    }
}

void SemanticAnalyzer::ensureAnyRootType() {
    if (context.classes.count("Any") == 0) {
        context.classes["Any"] = {};
        context.classes["Any"].location = {"<built-in>", 1, 1};
    }

    auto& anyClass = context.classes["Any"];
    auto builtInLocation = SourceLocation{"<built-in>", 1, 1};

    if (!anyClass.methods.count("toString")) {
        anyClass.methods["toString"] = {
            {},
            "String",
            {},
            builtInLocation,
            builtInLocation
        };
    }
    if (!anyClass.methods.count("hashCode")) {
        anyClass.methods["hashCode"] = {
            {},
            "Int",
            {},
            builtInLocation,
            builtInLocation
        };
    }
}

void SemanticAnalyzer::validateParentTypes() const {
    for (const auto& [className, classInfo] : context.classes) {
        if (className == "Any") continue;
        std::set<std::string> seenParents;
        for (const auto& parentType : classInfo.parentTypes) {
            auto parentParameterization = parameterizedTypeFromName(parentType);
            const std::string parentName =
                parentParameterization ? parentParameterization->first : parentType;
            auto parentIt = context.classes.find(parentName);
            if (parentIt == context.classes.end()) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "classe parente inconnue '" + parentName + "' pour la classe '" + className + "'");
            }
            const std::size_t expectedArguments = parentIt->second.typeParameters.size();
            const std::size_t actualArguments = parentParameterization ? parentParameterization->second.size() : 0;
            if (expectedArguments != actualArguments) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "la classe parente '" + parentName + "' de la classe '" + className +
                        "' attend " + std::to_string(expectedArguments) + " argument(s) de type");
            }
            if (!seenParents.insert(parentType).second) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "la classe '" + className + "' déclare le parent '" + parentType + "' en doublon");
            }
        }
    }
}

bool SemanticAnalyzer::hasInheritanceCycle(
    const std::string& className, std::set<std::string>& visiting,
    std::set<std::string>& done) const {
    if (done.count(className)) return false;
    if (visiting.count(className)) return true;

    auto classIt = context.classes.find(className);
    if (classIt == context.classes.end()) return false;

    visiting.insert(className);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string parentBaseName = genericBaseName(parentType);
        if (hasInheritanceCycle(parentBaseName, visiting, done)) return true;
    }
    visiting.erase(className);
    done.insert(className);
    return false;
}

void SemanticAnalyzer::validateInheritanceCycles() const {
    std::set<std::string> visiting;
    std::set<std::string> done;
    for (const auto& [className, classInfo] : context.classes) {
        if (className == "Any") continue;
        if (hasInheritanceCycle(className, visiting, done)) {
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "cycle d'héritage détecté impliquant la classe '" + className + "'");
        }
    }
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    if (type == "Int" || type == "Long" || type == "Float" || type == "Double" ||
        type == "Bool" || type == "Char" || type == "String" || type == "Unit" ||
        type == "Any" || type == "IntArray" || type == "LongArray" || type == "FloatArray" ||
        type == "DoubleArray" || type == "BoolArray" || isFunctionTypeName(type)) {
        return true;
    }
    auto parameterizedType = parameterizedTypeFromName(type);
    if (parameterizedType && parameterizedType->first == "ObjectArray" &&
        parameterizedType->second.size() == 1) {
        return isKnownType(parameterizedType->second[0]);
    }
    auto classIt = context.classes.find(type);
    if (classIt != context.classes.end()) return classIt->second.typeParameters.empty();
    auto substitution = genericSubstitutionFor(context, type);
    if (!substitution) return false;
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
        if (baseName == "ObjectArray" && arguments.size() == 1) {
            return isKnownTypeInScope(arguments[0], typeParameters);
        }
        auto classIt = context.classes.find(baseName);
        if (classIt == context.classes.end()) {
            if (!isStdlibTypeAliasFamily(baseName, arguments.size())) return false;
            for (const auto& argument : arguments) {
                if (!isTypeParameterName(argument, typeParameters)) return false;
            }
            return true;
        }
        if (classIt->second.typeParameters.size() != arguments.size()) return false;
        for (const auto& argument : arguments) {
            if (!isKnownTypeInScope(argument, typeParameters)) return false;
        }
        return true;
    }
    return isKnownType(type);
}
