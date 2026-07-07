#include "semantic_analyzer.hpp"

#include <algorithm>

namespace {

using MethodOwnerMap = std::map<std::string, std::set<std::string>>;
using ClassFieldOwnerMap = std::map<std::string, std::set<std::string>>;
using ParentMethodProviders = std::map<std::string, std::set<std::string>>;

std::string methodConflictKey(
    const std::string& sourceName,
    const CompilerContext::FunctionSignature& signature,
    const std::map<std::string, std::string>& substitution) {
    std::string key = sourceName + "(";
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (i > 0) key += ",";
        key += substituteType(signature.parameters[i].type, substitution);
        if (signature.parameters[i].isRepeated) key += "*";
    }
    key += ")";
    if (!signature.typeParameters.empty()) {
        key += "[";
        for (size_t i = 0; i < signature.typeParameters.size(); ++i) {
            if (i > 0) key += ",";
            key += signature.typeParameters[i];
        }
        key += "]";
    }
    return key;
}

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

std::string sourceNameFromMethodConflictKey(const std::string& methodKey) {
    const auto paren = methodKey.find('(');
    if (paren == std::string::npos) return methodKey;
    return methodKey.substr(0, paren);
}

std::string formatMemberProviders(
    const std::set<std::string>& fieldProviders,
    const std::set<std::string>& methodProviders) {
    std::set<std::string> providers;
    for (const auto& provider : fieldProviders) {
        providers.insert(provider + ".<field>");
    }
    for (const auto& provider : methodProviders) {
        providers.insert(provider + ".<method>");
    }
    return formatMethodProviders(providers);
}

bool hasMethodProviderDistinctFromFieldProviders(
    const std::set<std::string>& fieldProviders,
    const std::set<std::string>& methodProviders) {
    for (const auto& methodProvider : methodProviders) {
        if (!fieldProviders.count(methodProvider)) return true;
    }
    return false;
}

std::string formatMethodSignature(
    const std::string& ownerClassName,
    const std::string& sourceName,
    const CompilerContext::FunctionSignature& signature,
    const std::map<std::string, std::string>& classSubstitution) {
    std::string message = ownerClassName + "." + sourceName;
    if (!signature.typeParameters.empty()) {
        message += "[";
        for (size_t i = 0; i < signature.typeParameters.size(); ++i) {
            if (i > 0) message += ", ";
            message += signature.typeParameters[i];
        }
        message += "]";
    }
    message += "(";
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (i > 0) message += ", ";
        message += substituteType(signature.parameters[i].type, classSubstitution);
        if (signature.parameters[i].isRepeated) message += "*";
    }
    message += "): ";
    message += substituteType(signature.returnType, classSubstitution);
    return message;
}

std::string formatMethodLookupCandidates(
    const std::vector<ClassMethodLookupResult>& candidates,
    const std::string& sourceName) {
    std::string message;
    bool first = true;
    for (const auto& candidate : candidates) {
        if (!candidate.signature) continue;
        if (!first) message += ", ";
        message += formatMethodSignature(
            candidate.ownerClassName, sourceName, *candidate.signature, candidate.classSubstitution);
        first = false;
    }
    return message;
}

void collectInheritedMethodSummaries(
    const CompilerContext& context,
    const std::string& receiverType,
    std::set<std::string>& visiting,
    std::set<std::string>& summaries) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visiting.count(classLookupName)) return;
    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) return;

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        classSubstitution = *genericSubstitution;
    }

    if (classLookupName != "Any") {
        for (const auto& [sourceName, overloadNames] : classIt->second.methodOverloads) {
            for (const auto& overloadName : overloadNames) {
                auto methodIt = classIt->second.methods.find(overloadName);
                if (methodIt == classIt->second.methods.end()) continue;
                summaries.insert(formatMethodSignature(
                    classLookupName, sourceName, methodIt->second, classSubstitution));
            }
        }
    }

    visiting.insert(classLookupName);
    for (const auto& parentType : classIt->second.parentTypes) {
        collectInheritedMethodSummaries(
            context, substituteType(parentType, classSubstitution), visiting, summaries);
    }
    visiting.erase(classLookupName);
}

std::string formatInheritedMethodsForParents(
    const CompilerContext& context,
    const CompilerContext::ClassInfo& classInfo,
    const std::map<std::string, std::string>& classTypeSubstitution) {
    std::set<std::string> summaries;
    std::set<std::string> visiting;
    for (const auto& parentType : classInfo.parentTypes) {
        collectInheritedMethodSummaries(
            context, substituteType(parentType, classTypeSubstitution), visiting, summaries);
    }
    std::string message;
    bool first = true;
    for (const auto& summary : summaries) {
        if (!first) message += ", ";
        message += summary;
        first = false;
    }
    return message;
}

std::string methodTypeWithSubstitution(
    const std::string& type,
    const std::map<std::string, std::string>& classSubstitution,
    const std::map<std::string, std::string>& methodSubstitution) {
    return substituteType(substituteType(type, methodSubstitution), classSubstitution);
}

bool overrideSignatureMatches(
    const CompilerContext& context,
    const CompilerContext::FunctionSignature& overridingSignature,
    const CompilerContext::FunctionSignature& inheritedSignature,
    const std::map<std::string, std::string>& inheritedClassSubstitution) {
    if (overridingSignature.typeParameters.size() != inheritedSignature.typeParameters.size()) {
        return false;
    }
    if (overridingSignature.parameters.size() != inheritedSignature.parameters.size()) {
        return false;
    }

    std::map<std::string, std::string> overridingMethodSubstitution;
    for (size_t i = 0; i < overridingSignature.typeParameters.size(); ++i) {
        overridingMethodSubstitution[overridingSignature.typeParameters[i]] =
            inheritedSignature.typeParameters[i];
    }

    for (size_t i = 0; i < overridingSignature.parameters.size(); ++i) {
        const std::string overridingParameterType = methodTypeWithSubstitution(
            overridingSignature.parameters[i].type, {}, overridingMethodSubstitution);
        const std::string inheritedParameterType = methodTypeWithSubstitution(
            inheritedSignature.parameters[i].type, inheritedClassSubstitution, {});
        if (overridingParameterType != inheritedParameterType) {
            return false;
        }
    }

    const std::string overridingReturnType = methodTypeWithSubstitution(
        overridingSignature.returnType, {}, overridingMethodSubstitution);
    const std::string inheritedReturnType = methodTypeWithSubstitution(
        inheritedSignature.returnType, inheritedClassSubstitution, {});
    return isTypeAssignable(context, overridingReturnType, inheritedReturnType);
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
    for (const auto& [sourceName, overloadNames] : classIt->second.methodOverloads) {
        for (const auto& overloadName : overloadNames) {
            auto methodIt = classIt->second.methods.find(overloadName);
            if (methodIt == classIt->second.methods.end() || methodIt->second.isAbstract) continue;
            const std::string methodKey = methodConflictKey(sourceName, methodIt->second, substitution);
            if (visibleMethods[methodKey].empty()) {
                visibleMethods[methodKey].insert(providerType);
            }
        }
    }

    visiting.insert(classLookupName);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, substitution);
        collectVisibleMethodsInHierarchy(context, concreteParentType, substitution, visiting, visibleMethods);
    }
    visiting.erase(classLookupName);
}

void resolveInheritedConstructorSignatures(CompilerContext& context) {
    for (auto& [className, classInfo] : context.classes) {
        if (classInfo.inheritedConstructorSignature.empty()) continue;
        if (classInfo.parentTypes.empty()) {
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "signature héritée invalide pour la classe '" + className +
                "' : aucun parent explicite");
        }
        if (!classInfo.parentConstructorArguments.empty()) {
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "signature héritée invalide pour la classe '" + className +
                "' : arguments parent explicites déjà définis");
        }

        const auto parentFields =
            collectClassFieldsInHierarchyForLayout(context, classInfo.parentTypes[0]);
        if (classInfo.inheritedConstructorSignature.size() < parentFields.size()) {
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "signature héritée invalide pour la classe '" + className + "' : " +
                std::to_string(parentFields.size()) +
                " champ(s) parent attendu(s), " +
                std::to_string(classInfo.inheritedConstructorSignature.size()) + " reçu(s)");
        }

        std::vector<std::string> parentConstructorArguments;
        for (size_t i = 0; i < parentFields.size(); ++i) {
            const auto& inheritedField = classInfo.inheritedConstructorSignature[i];
            if (inheritedField.name != parentFields[i].first ||
                inheritedField.type != parentFields[i].second) {
                throw CompilerError(
                    ErrorKind::Semantic, inheritedField.location,
                    "signature héritée invalide pour la classe '" + className +
                    "' : champ parent '" + parentFields[i].first + ": " +
                    parentFields[i].second + "' attendu");
            }
            parentConstructorArguments.push_back(inheritedField.name);
        }

        std::set<std::string> signatureFieldNames;
        for (const auto& field : classInfo.inheritedConstructorSignature) {
            signatureFieldNames.insert(field.name);
        }

        std::vector<CompilerContext::FieldInfo> parsedOwnFields = std::move(classInfo.fields);
        classInfo.fields.clear();

        std::set<std::string> seenFieldNames;
        for (const auto& parentField : parentFields) {
            seenFieldNames.insert(parentField.first);
        }
        for (const auto& field : parsedOwnFields) {
            if (signatureFieldNames.count(field.name)) continue;
            if (!seenFieldNames.insert(field.name).second) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "champ déjà déclaré dans '" + className + "': " + field.name);
            }
            classInfo.fields.push_back(field);
        }
        for (size_t i = parentFields.size(); i < classInfo.inheritedConstructorSignature.size(); ++i) {
            const auto& field = classInfo.inheritedConstructorSignature[i];
            if (!seenFieldNames.insert(field.name).second) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "champ déjà déclaré dans '" + className + "': " + field.name);
            }
            classInfo.fields.push_back(field);
        }

        classInfo.parentConstructorArguments = std::move(parentConstructorArguments);
        classInfo.inheritedConstructorSignature.clear();
    }
}

struct AbstractMethodRequirement {
    std::string methodName;
    const CompilerContext::FunctionSignature* signature;
    std::map<std::string, std::string> classSubstitution;
    std::string providerType;
};

void collectAbstractMethodRequirements(
    const CompilerContext& context, const std::string& receiverType,
    std::map<std::string, std::string> substitution,
    std::set<std::string>& visiting,
    std::vector<AbstractMethodRequirement>& requirements) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visiting.count(classLookupName)) return;

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) return;

    if (auto classGenericSubstitution = genericSubstitutionFor(context, receiverType)) {
        substitution = *classGenericSubstitution;
    }

    const std::string providerType = substituteType(receiverType, substitution);
    for (const auto& [sourceName, overloadNames] : classIt->second.methodOverloads) {
        for (const auto& overloadName : overloadNames) {
            auto methodIt = classIt->second.methods.find(overloadName);
            if (methodIt == classIt->second.methods.end() || !methodIt->second.isAbstract) continue;
            requirements.push_back({sourceName, &methodIt->second, substitution, providerType});
        }
    }

    visiting.insert(classLookupName);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, substitution);
        collectAbstractMethodRequirements(
            context, concreteParentType, substitution, visiting, requirements);
    }
    visiting.erase(classLookupName);
}

std::vector<AbstractMethodRequirement> collectAbstractMethodRequirements(
    const CompilerContext& context, const std::string& receiverType) {
    std::map<std::string, std::string> substitution;
    std::set<std::string> visiting;
    std::vector<AbstractMethodRequirement> requirements;
    collectAbstractMethodRequirements(context, receiverType, substitution, visiting, requirements);
    return requirements;
}

bool hasConcreteImplementationForRequirement(
    const CompilerContext& context, const std::string& className,
    const AbstractMethodRequirement& requirement) {
    const auto candidates = collectClassMethodLookupCandidates(context, className, requirement.methodName);
    for (const auto& candidate : candidates) {
        if (!candidate.signature || candidate.signature->isAbstract) continue;
        if (overrideSignatureMatches(
                context, *candidate.signature, *requirement.signature,
                requirement.classSubstitution)) {
            return true;
        }
    }
    return false;
}

bool isKnownTypeConstructorArgument(
    const CompilerContext& context, const std::string& type, int expectedArity) {
    if (expectedArity != 1) return false;
    if (parameterizedTypeFromName(type)) return false;
    auto classIt = context.classes.find(type);
    if (classIt != context.classes.end()) {
        return classIt->second.typeParameters.size() == 1 &&
               typeParameterArity(type, classIt->second.typeParameterInfos) < 0;
    }
    return isStdlibTypeAliasFamily(type, 1);
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
    resolveInheritedConstructorSignatures(context);

    for (const auto& [className, classInfo] : context.classes) {
        for (const auto& field : classInfo.fields) {
            if (!isKnownTypeInScope(field.type, classInfo.typeParameterInfos)) {
                throw CompilerError(
                    ErrorKind::Semantic, field.location,
                    "type inconnu '" + field.type + "' pour le champ '" +
                    className + "." + field.name + "'");
            }
        }
        for (const auto& [methodName, signature] : classInfo.methods) {
            auto methodTypeParameters = classInfo.typeParameterInfos;
            methodTypeParameters.insert(
                methodTypeParameters.end(), signature.typeParameterInfos.begin(), signature.typeParameterInfos.end());
            if (signature.parameters.size() > 5) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "la méthode '" + className + "." + methodName +
                    "' dépasse la limite de 5 paramètres");
            }
            if (signature.isAbstract && !classInfo.isTrait) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "méthode abstraite non autorisée dans la classe '" + className +
                    "." + methodName + "'");
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

        if (!classInfo.parentConstructorArguments.empty()) {
            if (classInfo.parentTypes.empty()) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "constructeur de parent explicite invalide pour la classe '" + className + "'");
            }

            const auto& directParentType = classInfo.parentTypes[0];
            auto parentFields = collectClassFieldsInHierarchyForLayout(context, directParentType);
            if (parentFields.size() != classInfo.parentConstructorArguments.size()) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "constructeur de la classe '" + className + "': " +
                    std::to_string(parentFields.size()) +
                    " argument(s) attendu(s) pour le parent explicite, " +
                    std::to_string(classInfo.parentConstructorArguments.size()) + " reçu(s)");
            }

            std::set<std::string> parentFieldNames;
            for (const auto& parentField : parentFields) {
                parentFieldNames.insert(parentField.first);
            }
            std::set<std::string> seenParentArguments;
            for (const auto& parentArgument : classInfo.parentConstructorArguments) {
                if (!parentFieldNames.count(parentArgument)) {
                    throw CompilerError(
                        ErrorKind::Semantic, classInfo.location,
                        "constructeur de parent explicite invalide dans la classe '" + className +
                        "' : argument inconnu '" + parentArgument + "'");
                }
                if (!seenParentArguments.insert(parentArgument).second) {
                    throw CompilerError(
                        ErrorKind::Semantic, classInfo.location,
                        "constructeur de parent explicite invalide dans la classe '" + className +
                        "' : argument dupliqué '" + parentArgument + "'");
                }
            }
        }
    }
    for (const auto& [functionName, signature] : context.functions) {
        if (signature.parameters.size() > 6) {
            throw CompilerError(
                ErrorKind::Semantic, signature.location,
                "la fonction '" + functionName + "' dépasse la limite de 6 paramètres");
        }
        if (functionName == "main" && !signature.typeParameters.empty()) {
            throw CompilerError(
                ErrorKind::Semantic, signature.location,
                "la fonction 'main' ne peut pas être générique");
        }
        if (functionName == "main") {
            const bool acceptsNoArguments = signature.parameters.empty();
            const bool acceptsCommandLineArguments =
                signature.parameters.size() == 1 &&
                signature.parameters[0].type == formatParameterizedType("ArrayObject", {"String"});
            if (!acceptsNoArguments && !acceptsCommandLineArguments) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "signature main invalide: utiliser 'def main(): Int' ou 'def main(args: Array[String]): Int'");
            }
            if (signature.returnType != "Int") {
                throw CompilerError(
                    ErrorKind::Semantic, signature.returnTypeLocation,
                    "signature main invalide: utiliser 'def main(): Int' ou 'def main(args: Array[String]): Int'");
            }
        }
        for (const auto& parameter : signature.parameters) {
            if (!isKnownTypeInScope(parameter.type, signature.typeParameterInfos)) {
                throw CompilerError(
                    ErrorKind::Semantic, parameter.location,
                    "type inconnu '" + parameter.type + "' pour le paramètre '" +
                    functionName + "." + parameter.name + "'");
            }
        }
        if (!isKnownTypeInScope(signature.returnType, signature.typeParameterInfos)) {
            throw CompilerError(
                ErrorKind::Semantic, signature.returnTypeLocation,
                    "type de retour inconnu '" + signature.returnType + "' pour la fonction '" +
                    functionName + "'");
        }
    }
    if (auto mainOverloads = context.functionOverloads.find("main");
        mainOverloads != context.functionOverloads.end()) {
        if (mainOverloads->second.size() > 1) {
            const auto function = context.functions.find(mainOverloads->second[1]);
            SourceLocation location{};
            if (function != context.functions.end()) location = function->second.location;
            throw CompilerError(
                ErrorKind::Semantic, location,
                "la fonction 'main' ne peut pas être surchargée; utiliser 'def main(): Int' ou 'def main(args: Array[String]): Int'");
        }
    }

    for (const auto& [className, classInfo] : context.classes) {
        if (className == "Any") continue;

        ParentMethodProviders inheritedProviders;
        ParentMethodProviders visibleMethodMemberProviders;
        ClassFieldOwnerMap inheritedFieldProviders;
        std::set<std::string> ownMethods;
        std::set<std::string> ownMethodSourceNames;
        const auto classTypeSubstitution = classTypeTemplateSubstitution(context, className);
        for (const auto& [sourceName, overloadNames] : classInfo.methodOverloads) {
            for (const auto& overloadName : overloadNames) {
                auto methodIt = classInfo.methods.find(overloadName);
                if (methodIt == classInfo.methods.end() || methodIt->second.isAbstract) continue;
                ownMethods.insert(methodConflictKey(sourceName, methodIt->second, classTypeSubstitution));
                ownMethodSourceNames.insert(sourceName);
                visibleMethodMemberProviders[sourceName].insert(className);
            }
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
            for (const auto& [methodKey, ownerClasses] : visibleMethods) {
                const std::string sourceName = sourceNameFromMethodConflictKey(methodKey);
                if (ownMethodSourceNames.count(sourceName)) continue;
                visibleMethodMemberProviders[sourceName].insert(ownerClasses.begin(), ownerClasses.end());
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
        for (const auto& [sourceName, methodProviders] : visibleMethodMemberProviders) {
            auto fieldProviders = inheritedFieldProviders.find(sourceName);
            if (fieldProviders == inheritedFieldProviders.end()) continue;
            if (fieldProviders->second.size() > 1) continue;
            if (!hasMethodProviderDistinctFromFieldProviders(fieldProviders->second, methodProviders)) {
                continue;
            }
            throw CompilerError(
                ErrorKind::Semantic, classInfo.location,
                "conflit d'héritage pour le membre '" + sourceName +
                "' dans la classe '" + className +
                "': champ et méthode visibles dans [" +
                formatMemberProviders(fieldProviders->second, methodProviders) + "]");
        }
        for (const auto& [registeredMethodName, signature] : classInfo.methods) {
            std::string methodName = registeredMethodName;
            for (const auto& [sourceName, overloadNames] : classInfo.methodOverloads) {
                if (std::find(overloadNames.begin(), overloadNames.end(), registeredMethodName) != overloadNames.end()) {
                    methodName = sourceName;
                    break;
                }
            }
            bool hasInheritedMethod = false;
            bool hasCompatibleInheritedMethod = false;
            bool hasCompatibleConcreteInheritedMethod = false;
            std::vector<ClassMethodLookupResult> inheritedCandidatesForName;
            for (const auto& parentType : classInfo.parentTypes) {
                const std::string concreteParentType = substituteType(parentType, classTypeSubstitution);
                const auto inheritedCandidates = collectClassMethodLookupCandidates(
                    context, concreteParentType, methodName);
                inheritedCandidatesForName.insert(
                    inheritedCandidatesForName.end(), inheritedCandidates.begin(), inheritedCandidates.end());
                if (!inheritedCandidates.empty()) {
                    hasInheritedMethod = true;
                    for (const auto& inheritedCandidate : inheritedCandidates) {
                        if (overrideSignatureMatches(
                                context, signature, *inheritedCandidate.signature,
                                inheritedCandidate.classSubstitution)) {
                            hasCompatibleInheritedMethod = true;
                            if (!inheritedCandidate.signature->isAbstract) {
                                hasCompatibleConcreteInheritedMethod = true;
                            }
                        }
                    }
                }
            }
            if (signature.isOverride && !hasInheritedMethod) {
                std::string message =
                    "override invalide pour '" + className + "." + methodName +
                    "' : aucune méthode héritée nommée '" + methodName + "'";
                const std::string inheritedMethods =
                    formatInheritedMethodsForParents(context, classInfo, classTypeSubstitution);
                if (!inheritedMethods.empty()) {
                    message += "\nméthodes héritées disponibles: " + inheritedMethods;
                }
                throw CompilerError(ErrorKind::Semantic, signature.location, message);
            }
            if (signature.isOverride && hasInheritedMethod && !hasCompatibleInheritedMethod) {
                std::string message =
                    "override invalide pour '" + className + "." + methodName +
                    "' : signature incompatible avec la méthode héritée";
                const std::string candidates = formatMethodLookupCandidates(
                    inheritedCandidatesForName, methodName);
                if (!candidates.empty()) {
                    message += "\nsignatures héritées candidates: " + candidates;
                }
                throw CompilerError(ErrorKind::Semantic, signature.location, message);
            }
            if (!signature.isOverride && hasCompatibleInheritedMethod &&
                (!signature.isSyntheticAccessor || hasCompatibleConcreteInheritedMethod)) {
                throw CompilerError(
                    ErrorKind::Semantic, signature.location,
                    "override obligatoire pour '" + className + "." + methodName +
                    "' : utilisez 'override' pour redéfinir une méthode héritée");
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
        if (!classInfo.isTrait) {
            const auto abstractRequirements =
                collectAbstractMethodRequirements(context, className);
            for (const auto& requirement : abstractRequirements) {
                if (hasConcreteImplementationForRequirement(context, className, requirement)) continue;
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "méthode abstraite héritée non implémentée dans la classe '" + className +
                    "': " + requirement.providerType + "." + requirement.methodName);
            }
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
    auto builtInLocation = SourceLocation{"<built-in>", 1, 1};
    if (context.classes.count("Any") == 0) {
        context.classes["Any"] = {};
    }
    context.classes["Any"].location = builtInLocation;

    if (context.classes.count("AnyVal") == 0) {
        context.classes["AnyVal"] = {};
    }
    auto& anyValClass = context.classes["AnyVal"];
    anyValClass.location = builtInLocation;
    anyValClass.parentTypes = {"Any"};

    if (context.classes.count("AnyRef") == 0) {
        context.classes["AnyRef"] = {};
    }
    auto& anyRefClass = context.classes["AnyRef"];
    anyRefClass.location = builtInLocation;
    anyRefClass.parentTypes = {"Any"};

    auto& anyClass = context.classes["Any"];

    if (!anyClass.methods.count("toString")) {
        anyClass.methods["toString"] = {
            {},
            "String",
            {},
            builtInLocation,
            builtInLocation,
            false,
            false,
            false,
            {},
            {}
        };
    }
    anyClass.methodOverloads["toString"] = {"toString"};
    if (!anyClass.methods.count("hashCode")) {
        anyClass.methods["hashCode"] = {
            {},
            "Int",
            {},
            builtInLocation,
            builtInLocation,
            false,
            false,
            false,
            {},
            {}
        };
    }
    anyClass.methodOverloads["hashCode"] = {"hashCode"};
    if (!anyClass.methods.count("equals")) {
        anyClass.methods["equals"] = {
            {{"other", "Any", builtInLocation}},
            "Bool",
            {},
            builtInLocation,
            builtInLocation,
            false,
            false,
            false,
            {},
            {}
        };
    }
    anyClass.methodOverloads["equals"] = {"equals"};
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
            if (context.runtimeObjects.count(parentName)) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "objet runtime '" + parentName +
                    "' non utilisable comme parent de classe; utilisez sa valeur singleton '" + parentName + "'");
            }
            if (classInfo.isTrait && !parentIt->second.isTrait) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "le trait '" + className + "' ne peut composer que des traits avec 'with'");
            }
            if (context.runtimeObjects.count(className) && parentName != "AnyRef" && !parentIt->second.isTrait) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "l'objet runtime '" + className + "' ne peut composer que des traits avec 'with'");
            }
            if (!classInfo.isTrait && classInfo.hasExplicitParent &&
                !classInfo.parentTypes.empty() && parentType == classInfo.parentTypes[0] &&
                parentIt->second.isTrait) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "le trait '" + parentName + "' doit être composé avec 'with'");
            }
            const std::size_t expectedArguments = parentIt->second.typeParameters.size();
            const std::size_t actualArguments = parentParameterization ? parentParameterization->second.size() : 0;
            if (expectedArguments != actualArguments) {
                throw CompilerError(
                    ErrorKind::Semantic, classInfo.location,
                    "la classe parente '" + parentName + "' de la classe '" + className +
                        "' attend " + std::to_string(expectedArguments) + " argument(s) de type");
            }
            if (parentParameterization) {
                for (std::size_t i = 0; i < parentParameterization->second.size(); ++i) {
                    const int expectedArity = i < parentIt->second.typeParameterInfos.size()
                        ? parentIt->second.typeParameterInfos[i].arity
                        : 0;
                    const std::string& actualArgument = parentParameterization->second[i];
                    if (expectedArity == 1) {
                        if (typeParameterArity(actualArgument, classInfo.typeParameterInfos) == expectedArity) {
                            continue;
                        }
                        if (parameterizedTypeFromName(actualArgument)) {
                            throw CompilerError(
                                ErrorKind::Semantic, classInfo.location,
                                "le paramètre de type '" + parentIt->second.typeParameters[i] +
                                    "[_]' de '" + parentName +
                                    "' attend un constructeur de type d'arité 1, pas le type appliqué '" +
                                    actualArgument + "'");
                        }
                        if (!isKnownTypeConstructorArgument(context, actualArgument, expectedArity)) {
                            throw CompilerError(
                                ErrorKind::Semantic, classInfo.location,
                                "le paramètre de type '" + parentIt->second.typeParameters[i] +
                                    "[_]' de '" + parentName +
                                    "' attend un constructeur de type d'arité 1");
                        }
                    }
                }
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
        type == "Any" || type == "AnyVal" || type == "AnyRef" || type == "Nothing" ||
        type == "IntArray" || type == "LongArray" || type == "FloatArray" ||
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
    return isKnownTypeInScope(type, ordinaryTypeParameterInfos(typeParameters));
}

bool SemanticAnalyzer::isKnownTypeInScope(
    const std::string& type,
    const std::vector<CompilerContext::TypeParameterInfo>& typeParameters) const {
    const int directArity = typeParameterArity(type, typeParameters);
    if (directArity >= 0) return directArity == 0;
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
        const int constructorParameterArity = typeParameterArity(baseName, typeParameters);
        if (constructorParameterArity >= 0) {
            if (constructorParameterArity != 1 || arguments.size() != 1) return false;
            return isKnownTypeInScope(arguments[0], typeParameters);
        }
        if (baseName == "ObjectArray" && arguments.size() == 1) {
            return isKnownTypeInScope(arguments[0], typeParameters);
        }
        auto classIt = context.classes.find(baseName);
        if (classIt == context.classes.end()) {
            if (!isStdlibTypeAliasFamily(baseName, arguments.size())) return false;
            for (const auto& argument : arguments) {
                if (!isKnownTypeInScope(argument, typeParameters)) return false;
            }
            return true;
        }
        if (classIt->second.typeParameters.size() != arguments.size()) return false;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            const int expectedArity = i < classIt->second.typeParameterInfos.size()
                ? classIt->second.typeParameterInfos[i].arity
                : 0;
            if (expectedArity == 0) {
                if (!isKnownTypeInScope(arguments[i], typeParameters)) return false;
            } else if (expectedArity == 1) {
                if (typeParameterArity(arguments[i], typeParameters) == 1) continue;
                if (!isKnownTypeConstructorArgument(context, arguments[i], expectedArity)) return false;
            } else {
                return false;
            }
        }
        return true;
    }
    return isKnownType(type);
}
