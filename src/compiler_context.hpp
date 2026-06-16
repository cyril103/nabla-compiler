#pragma once

#include "compiler_error.hpp"
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <filesystem>
#include <vector>

struct CompilerContext {
    struct FunctionType {
        std::vector<std::string> parameterTypes;
        std::string returnType;
    };

    struct ParameterInfo {
        std::string name;
        std::string type;
        SourceLocation location;
    };

    struct FunctionSignature {
        std::vector<ParameterInfo> parameters;
        std::string returnType;
        std::vector<std::string> typeParameters;
        SourceLocation location;
        SourceLocation returnTypeLocation;
    };

    struct FieldInfo {
        std::string name;
        std::string type;
        SourceLocation location;
    };

    struct ClassInfo {
        std::vector<FieldInfo> fields;
        std::map<std::string, FunctionSignature> methods;
        std::vector<std::string> parentTypes;
        bool hasExplicitParent = false;
        std::vector<std::string> typeParameters;
        SourceLocation location;
    };

    std::map<std::string, std::map<std::string, int>> classLayouts;
    std::map<std::string, ClassInfo> classes;
    std::map<std::string, FunctionSignature> functions;
    std::set<std::string> parsedFiles;
    std::filesystem::path rootDir;
    std::filesystem::path stdlibDir;
    std::filesystem::path currentFile;
    std::map<std::string, std::string> semanticSymbolTypes;
    std::vector<std::string> semanticTypeParameters;
    int nextLambdaId = 0;
};

struct StdlibTypeAlias {
    std::string baseName;
    std::vector<std::string> arguments;
    std::string resolvedName;
};

struct StdlibFunctionAlias {
    std::string name;
    std::vector<std::string> typeArguments;
    std::string resolvedName;
};

inline const std::vector<StdlibTypeAlias>& stdlibTypeAliases() {
    static const std::vector<StdlibTypeAlias> aliases = {
        {"Array", {"Int"}, "ArrayInt"},
        {"Array", {"Long"}, "ArrayLong"},
        {"Array", {"Float"}, "ArrayFloat"},
        {"Array", {"Double"}, "ArrayDouble"},
        {"Array", {"Bool"}, "ArrayBool"},
    };
    return aliases;
}

inline const std::vector<StdlibFunctionAlias>& stdlibFunctionAliases() {
    static const std::vector<StdlibFunctionAlias> aliases = {
        {"arrayFill", {"Int"}, "arrayIntFill"},
        {"arrayFill", {"Long"}, "arrayLongFill"},
        {"arrayFill", {"Float"}, "arrayFloatFill"},
        {"arrayFill", {"Double"}, "arrayDoubleFill"},
        {"arrayFill", {"Bool"}, "arrayBoolFill"},
        {"arrayMap", {"Int"}, "arrayIntMap"},
        {"arrayMap", {"Long"}, "arrayLongMap"},
        {"arrayMap", {"Float"}, "arrayFloatMap"},
        {"arrayMap", {"Double"}, "arrayDoubleMap"},
        {"arrayMap", {"Bool"}, "arrayBoolMap"},
        {"arrayFilter", {"Int"}, "arrayIntFilter"},
        {"arrayFilter", {"Long"}, "arrayLongFilter"},
        {"arrayFilter", {"Float"}, "arrayFloatFilter"},
        {"arrayFilter", {"Double"}, "arrayDoubleFilter"},
        {"arrayFilter", {"Bool"}, "arrayBoolFilter"},
        {"arrayMkString", {"Int"}, "arrayIntMkString"},
        {"arrayMkString", {"Long"}, "arrayLongMkString"},
        {"arrayMkString", {"Bool"}, "arrayBoolMkString"},
        {"arrayFold", {"Int"}, "arrayIntFold"},
        {"arrayFold", {"Long"}, "arrayLongFold"},
        {"arrayFold", {"Float"}, "arrayFloatFold"},
        {"arrayFold", {"Double"}, "arrayDoubleFold"},
        {"arrayFold", {"Bool"}, "arrayBoolFold"},
        {"arrayFlatMap", {"Int"}, "arrayIntFlatMap"},
        {"arrayFlatMap", {"Long"}, "arrayLongFlatMap"},
        {"arrayFlatMap", {"Float"}, "arrayFloatFlatMap"},
        {"arrayFlatMap", {"Double"}, "arrayDoubleFlatMap"},
        {"arrayFlatMap", {"Bool"}, "arrayBoolFlatMap"},
        {"arrayForeach", {"Int"}, "arrayIntForeach"},
        {"arrayForeach", {"Long"}, "arrayLongForeach"},
        {"arrayForeach", {"Float"}, "arrayFloatForeach"},
        {"arrayForeach", {"Double"}, "arrayDoubleForeach"},
        {"arrayForeach", {"Bool"}, "arrayBoolForeach"},
    };
    return aliases;
}

inline bool isPrimitiveArrayElementType(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" ||
           type == "Double" || type == "Bool";
}

inline std::optional<std::string> primitiveArrayAliasPrefix(const std::string& type) {
    if (type == "Int") return "arrayInt";
    if (type == "Long") return "arrayLong";
    if (type == "Float") return "arrayFloat";
    if (type == "Double") return "arrayDouble";
    if (type == "Bool") return "arrayBool";
    return std::nullopt;
}

inline std::optional<std::string> resolvePrimitiveArrayMapAlias(
    const std::vector<std::string>& typeArguments) {
    if (typeArguments.size() != 2) return std::nullopt;
    auto prefix = primitiveArrayAliasPrefix(typeArguments[0]);
    if (!prefix) return std::nullopt;
    if (typeArguments[0] == typeArguments[1]) return *prefix + "Map";
    return *prefix + "MapObject";
}

inline std::optional<std::string> resolvePrimitiveArrayFlatMapAlias(
    const std::vector<std::string>& typeArguments) {
    if (typeArguments.size() != 2) return std::nullopt;
    auto prefix = primitiveArrayAliasPrefix(typeArguments[0]);
    if (!prefix) return std::nullopt;
    return *prefix + "FlatMapObject";
}

inline std::optional<std::string> resolveStdlibFunctionAlias(
    const std::string& name, const std::vector<std::string>& typeArguments) {
    for (const auto& alias : stdlibFunctionAliases()) {
        if (alias.name == name && alias.typeArguments == typeArguments) return alias.resolvedName;
    }
    if (typeArguments.size() == 1 && !isPrimitiveArrayElementType(typeArguments[0])) {
        if (name == "arrayMkString" && typeArguments[0] == "String") return "objectStringArrayMkString";
        if (name == "arrayFill") return "objectArrayFill";
        if (name == "arrayMap") return "objectArrayMapSame";
        if (name == "arrayFilter") return "objectArrayFilter";
        if (name == "arrayForeach") return "objectArrayForeach";
    }
    if (typeArguments.size() == 2 && !isPrimitiveArrayElementType(typeArguments[0])) {
        if (name == "arrayMap") return "objectArrayMap";
        if (name == "arrayFold") return "objectArrayFold";
        if (name == "arrayFlatMap") return "objectArrayFlatMap";
    }
    if (name == "arrayMap" && typeArguments.size() == 2) {
        if (auto alias = resolvePrimitiveArrayMapAlias(typeArguments)) return alias;
    }
    if (name == "arrayFlatMap" && typeArguments.size() == 2) {
        if (auto alias = resolvePrimitiveArrayFlatMapAlias(typeArguments)) return alias;
    }
    return std::nullopt;
}

inline bool isStdlibFunctionAliasName(const std::string& name) {
    for (const auto& alias : stdlibFunctionAliases()) {
        if (alias.name == name) return true;
    }
    return false;
}

inline std::string formatFunctionType(const CompilerContext::FunctionType& functionType) {
    std::string formatted = "Fn(";
    for (size_t i = 0; i < functionType.parameterTypes.size(); ++i) {
        if (i > 0) formatted += ",";
        formatted += functionType.parameterTypes[i];
    }
    formatted += ")->";
    formatted += functionType.returnType;
    return formatted;
}

inline std::vector<std::string> splitTopLevelTypes(const std::string& types) {
    std::vector<std::string> parts;
    size_t start = 0;
    int parenDepth = 0;
    int bracketDepth = 0;
    for (size_t i = 0; i < types.size(); ++i) {
        char c = types[i];
        if (c == '(') ++parenDepth;
        else if (c == ')') --parenDepth;
        else if (c == '[') ++bracketDepth;
        else if (c == ']') --bracketDepth;
        else if (c == ',' && parenDepth == 0 && bracketDepth == 0) {
            if (i == start) return {};
            parts.push_back(types.substr(start, i - start));
            start = i + 1;
        }
        if (parenDepth < 0 || bracketDepth < 0) return {};
    }
    if (parenDepth != 0 || bracketDepth != 0) return {};
    if (start < types.size()) parts.push_back(types.substr(start));
    else if (!types.empty()) return {};
    return parts;
}

inline std::optional<CompilerContext::FunctionType> functionTypeFromName(const std::string& type) {
    const std::string prefix = "Fn(";
    const std::string arrow = ")->";
    if (type.rfind(prefix, 0) != 0) return std::nullopt;
    size_t arrowPosition = type.find(arrow, prefix.size());
    if (arrowPosition == std::string::npos) return std::nullopt;

    CompilerContext::FunctionType functionType;
    std::string parameters = type.substr(prefix.size(), arrowPosition - prefix.size());
    functionType.parameterTypes = splitTopLevelTypes(parameters);
    if (!parameters.empty() && functionType.parameterTypes.empty()) return std::nullopt;
    functionType.returnType = type.substr(arrowPosition + arrow.size());
    if (functionType.returnType.empty()) return std::nullopt;
    return functionType;
}

inline std::string formatParameterizedType(
    const std::string& baseName, const std::vector<std::string>& arguments) {
    std::string formatted = baseName + "[";
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) formatted += ",";
        formatted += arguments[i];
    }
    formatted += "]";
    return formatted;
}

inline std::optional<std::pair<std::string, std::vector<std::string>>> parameterizedTypeFromName(
    const std::string& type) {
    size_t bracket = type.find('[');
    if (bracket == std::string::npos || type.empty() || type.back() != ']') return std::nullopt;
    std::string baseName = type.substr(0, bracket);
    if (baseName.empty()) return std::nullopt;
    std::string argumentsText = type.substr(bracket + 1, type.size() - bracket - 2);
    auto arguments = splitTopLevelTypes(argumentsText);
    if (arguments.empty()) return std::nullopt;
    return std::make_pair(baseName, arguments);
}

inline std::optional<CompilerContext::FunctionSignature> stdlibTypeAliasMethodSignature(
    const std::string& receiverType, const std::string& methodName) {
    auto parameterizedType = parameterizedTypeFromName(receiverType);
    if (!parameterizedType) return std::nullopt;
    const auto& [baseName, arguments] = *parameterizedType;
    if (baseName != "Array" || arguments.size() != 1) return std::nullopt;

    const std::string& elementType = arguments[0];
    CompilerContext::FunctionSignature signature;
    if (methodName == "length" || methodName == "size") {
        signature.returnType = "Int";
        return signature;
    }
    if (methodName == "isEmpty" || methodName == "nonEmpty") {
        signature.returnType = "Bool";
        return signature;
    }
    if (methodName == "get") {
        signature.parameters.push_back({"index", "Int", {}});
        signature.returnType = elementType;
        return signature;
    }
    if (methodName == "set") {
        signature.parameters.push_back({"index", "Int", {}});
        signature.parameters.push_back({"value", elementType, {}});
        signature.returnType = "Unit";
        return signature;
    }
    if (methodName == "map") {
        signature.parameters.push_back({
            "f", formatFunctionType({{elementType}, elementType}), {}});
        signature.returnType = receiverType;
        return signature;
    }
    if (methodName == "foreach") {
        signature.parameters.push_back({
            "f", formatFunctionType({{elementType}, "Unit"}), {}});
        signature.returnType = "Unit";
        return signature;
    }
    return std::nullopt;
}

inline std::string resolveStdlibTypeAlias(const std::string& type) {
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return type;
    const auto& [baseName, arguments] = *parameterizedType;
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.baseName == baseName && alias.arguments == arguments) return alias.resolvedName;
    }
    if (baseName == "Array" && arguments.size() == 1) {
        const bool concreteArgument =
            arguments[0] == "Int" || arguments[0] == "Long" || arguments[0] == "Float" ||
            arguments[0] == "Double" || arguments[0] == "Bool" || arguments[0] == "Char" ||
            arguments[0] == "String" ||
            arguments[0] == "Unit" || arguments[0] == "IntArray" || arguments[0] == "LongArray" ||
            arguments[0] == "FloatArray" || arguments[0] == "DoubleArray" ||
            arguments[0] == "BoolArray" || arguments[0] == "ArrayInt" ||
            arguments[0] == "ArrayLong" || arguments[0] == "ArrayFloat" ||
            arguments[0] == "ArrayDouble" || arguments[0] == "ArrayBool" ||
            functionTypeFromName(arguments[0]).has_value() ||
            parameterizedTypeFromName(arguments[0]).has_value();
        if (!concreteArgument) return type;
        return formatParameterizedType("ArrayObject", arguments);
    }
    return type;
}

inline bool isStdlibTypeAliasFamily(const std::string& baseName, size_t arity) {
    if (baseName == "Array" && arity == 1) return true;
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.baseName == baseName && alias.arguments.size() == arity) return true;
    }
    return false;
}

inline std::optional<StdlibTypeAlias> stdlibTypeAliasForResolvedName(const std::string& resolvedName) {
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.resolvedName == resolvedName) return alias;
    }
    auto parameterizedType = parameterizedTypeFromName(resolvedName);
    if (parameterizedType && parameterizedType->first == "ArrayObject" &&
        parameterizedType->second.size() == 1) {
        return StdlibTypeAlias{"Array", parameterizedType->second, resolvedName};
    }
    return std::nullopt;
}

inline std::string canonicalTypeName(const std::string& type) {
    auto functionType = functionTypeFromName(type);
    if (functionType) {
        for (auto& parameterType : functionType->parameterTypes) {
            parameterType = canonicalTypeName(parameterType);
        }
        functionType->returnType = canonicalTypeName(functionType->returnType);
        return formatFunctionType(*functionType);
    }
    auto parameterizedType = parameterizedTypeFromName(type);
    if (parameterizedType) {
        auto [baseName, arguments] = *parameterizedType;
        for (auto& argument : arguments) {
            argument = canonicalTypeName(argument);
        }
        return resolveStdlibTypeAlias(formatParameterizedType(baseName, arguments));
    }
    return type;
}

inline std::string genericBaseName(const std::string& type) {
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return type;
    return parameterizedType->first;
}

struct ClassMethodLookupResult {
    const CompilerContext::FunctionSignature* signature = nullptr;
    std::string ownerClassName;
    std::map<std::string, std::string> classSubstitution;
};

struct ClassFieldLookupResult {
    std::string ownerClassName;
    std::string type;
};

inline std::optional<std::map<std::string, std::string>> genericSubstitutionFor(
    const CompilerContext& context, const std::string& concreteType);
inline std::string substituteType(
    const std::string& type, const std::map<std::string, std::string>& substitution);

inline void collectVisibleFieldsInHierarchy(
    const CompilerContext& context, const std::string& receiverType,
    std::set<std::string>& visiting,
    std::map<std::string, std::set<std::string>>& fieldOwners,
    std::map<std::string, std::string>& fieldTypes) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visiting.count(classLookupName)) return;
    visiting.insert(classLookupName);

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        visiting.erase(classLookupName);
        return;
    }

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        classSubstitution = *genericSubstitution;
    }

    const std::string providerType = substituteType(receiverType, classSubstitution);
    for (const auto& field : classIt->second.fields) {
        fieldOwners[field.name].insert(providerType);
        if (!fieldTypes.count(field.name)) {
            fieldTypes[field.name] = substituteType(field.type, classSubstitution);
        }
    }

    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, classSubstitution);
        collectVisibleFieldsInHierarchy(context, concreteParentType, visiting, fieldOwners, fieldTypes);
    }

    visiting.erase(classLookupName);
}

inline void collectClassFieldsInHierarchyForLayout(
    const CompilerContext& context, const std::string& receiverType,
    std::set<std::string>& visiting,
    std::vector<std::pair<std::string, std::string>>& fields) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visiting.count(classLookupName)) return;
    visiting.insert(classLookupName);

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) {
        visiting.erase(classLookupName);
        return;
    }

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        classSubstitution = *genericSubstitution;
    }

    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, classSubstitution);
        collectClassFieldsInHierarchyForLayout(context, concreteParentType, visiting, fields);
    }

    for (const auto& field : classIt->second.fields) {
        fields.emplace_back(field.name, substituteType(field.type, classSubstitution));
    }

    visiting.erase(classLookupName);
}

inline std::vector<std::pair<std::string, std::string>> collectClassFieldsInHierarchyForLayout(
    const CompilerContext& context, const std::string& receiverType) {
    std::vector<std::pair<std::string, std::string>> fields;
    std::set<std::string> visiting;
    collectClassFieldsInHierarchyForLayout(context, receiverType, visiting, fields);
    return fields;
}

inline std::map<std::string, std::string> classTypeTemplateSubstitution(
    const CompilerContext& context, const std::string& classType) {
    auto parameterizedType = parameterizedTypeFromName(classType);
    std::string className = classType;
    std::vector<std::string> arguments;
    if (parameterizedType) {
        className = parameterizedType->first;
        arguments = parameterizedType->second;
    }
    auto classIt = context.classes.find(className);
    if (classIt == context.classes.end()) return {};

    const auto& typeParameters = classIt->second.typeParameters;
    if (typeParameters.empty()) return {};

    if (arguments.empty()) {
        std::map<std::string, std::string> substitution;
        for (const auto& typeParameter : typeParameters) {
            substitution[typeParameter] = typeParameter;
        }
        return substitution;
    }
    if (typeParameters.size() != arguments.size()) {
        return {};
    }

    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < typeParameters.size(); ++i) {
        substitution[typeParameters[i]] = arguments[i];
    }
    return substitution;
}

inline bool isTypeParameterName(const std::string& type, const std::vector<std::string>& typeParameters) {
    for (const auto& typeParameter : typeParameters) {
        if (type == typeParameter) return true;
    }
    return false;
}

inline std::optional<std::map<std::string, std::string>> genericSubstitutionFor(
    const CompilerContext& context, const std::string& concreteType) {
    auto parameterizedType = parameterizedTypeFromName(concreteType);
    if (!parameterizedType) return std::nullopt;
    const auto& [baseName, arguments] = *parameterizedType;
    auto classIt = context.classes.find(baseName);
    if (classIt == context.classes.end()) return std::nullopt;
    const auto& typeParameters = classIt->second.typeParameters;
    if (typeParameters.size() != arguments.size()) return std::nullopt;
    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < typeParameters.size(); ++i) {
        substitution[typeParameters[i]] = arguments[i];
    }
    return substitution;
}

inline std::optional<std::map<std::string, std::string>> genericFunctionSubstitutionFor(
    const CompilerContext::FunctionSignature& signature, const std::vector<std::string>& arguments) {
    if (signature.typeParameters.size() != arguments.size()) return std::nullopt;
    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < signature.typeParameters.size(); ++i) {
        substitution[signature.typeParameters[i]] = arguments[i];
    }
    return substitution;
}

inline std::string substituteType(
    const std::string& type, const std::map<std::string, std::string>& substitution) {
    auto direct = substitution.find(type);
    if (direct != substitution.end()) return direct->second;

    auto functionType = functionTypeFromName(type);
    if (functionType) {
        for (auto& parameterType : functionType->parameterTypes) {
            parameterType = substituteType(parameterType, substitution);
        }
        functionType->returnType = substituteType(functionType->returnType, substitution);
        return formatFunctionType(*functionType);
    }

    auto parameterizedType = parameterizedTypeFromName(type);
    if (parameterizedType) {
        auto [baseName, arguments] = *parameterizedType;
        bool anyArgumentChanged = false;
        for (auto& argument : arguments) {
            const std::string originalArgument = argument;
            argument = substituteType(argument, substitution);
            anyArgumentChanged = anyArgumentChanged || argument != originalArgument;
        }
        const std::string substitutedType = canonicalTypeName(formatParameterizedType(baseName, arguments));
        if (substitutedType == formatParameterizedType(baseName, arguments) &&
            baseName == "Array" && arguments.size() == 1 && anyArgumentChanged) {
            return formatParameterizedType("ArrayObject", arguments);
        }
        return substitutedType;
    }

    return type;
}

inline void collectClassMethodLookupCandidates(
    const CompilerContext& context, const std::string& receiverType,
    const std::string& methodName, std::set<std::string>& visited,
    std::set<std::string>& providerTypes,
    std::vector<ClassMethodLookupResult>& candidates) {
    const std::string classLookupName = genericBaseName(receiverType);
    if (visited.count(classLookupName)) return;

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) return;

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        classSubstitution = *genericSubstitution;
    }

    auto methodIt = classIt->second.methods.find(methodName);
    if (methodIt != classIt->second.methods.end()) {
        const std::string providerType = substituteType(receiverType, classSubstitution);
        if (providerTypes.insert(providerType).second) {
            candidates.push_back(ClassMethodLookupResult{
                &methodIt->second, classLookupName, classSubstitution});
        }
        return;
    }

    visited.insert(classLookupName);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, classSubstitution);
        collectClassMethodLookupCandidates(
            context, concreteParentType, methodName, visited, providerTypes, candidates);
    }
    visited.erase(classLookupName);
}

inline std::vector<ClassMethodLookupResult> collectClassMethodLookupCandidates(
    const CompilerContext& context, const std::string& receiverType,
    const std::string& methodName) {
    std::set<std::string> visited;
    std::set<std::string> providerTypes;
    std::vector<ClassMethodLookupResult> candidates;
    collectClassMethodLookupCandidates(context, receiverType, methodName, visited, providerTypes, candidates);
    return candidates;
}

inline std::optional<ClassMethodLookupResult> resolveClassMethodInHierarchy(
    const CompilerContext& context, const std::string& receiverType,
    const std::string& methodName) {
    const auto candidates = collectClassMethodLookupCandidates(context, receiverType, methodName);
    if (candidates.empty()) return std::nullopt;
    return candidates.front();
}

inline std::optional<std::string> resolveActiveStdlibTypeAlias(
    const std::string& type, const std::map<std::string, std::string>& substitution) {
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return std::nullopt;
    auto [baseName, arguments] = *parameterizedType;
    for (auto& argument : arguments) {
        argument = substituteType(argument, substitution);
    }
    const std::string resolvedType = resolveStdlibTypeAlias(formatParameterizedType(baseName, arguments));
    if (resolvedType == type) return std::nullopt;
    return resolvedType;
}

inline bool inferTypeArgumentsFromTypes(
    const std::string& expectedType, const std::string& actualType,
    const std::vector<std::string>& typeParameters,
    std::map<std::string, std::string>& substitution) {
    if (isTypeParameterName(expectedType, typeParameters)) {
        auto existing = substitution.find(expectedType);
        if (existing != substitution.end()) return existing->second == actualType;
        substitution[expectedType] = actualType;
        return true;
    }

    auto expectedFunction = functionTypeFromName(expectedType);
    if (expectedFunction) {
        auto actualFunction = functionTypeFromName(actualType);
        if (!actualFunction) return false;
        if (expectedFunction->parameterTypes.size() != actualFunction->parameterTypes.size()) return false;
        for (size_t i = 0; i < expectedFunction->parameterTypes.size(); ++i) {
            if (!inferTypeArgumentsFromTypes(
                    expectedFunction->parameterTypes[i], actualFunction->parameterTypes[i],
                    typeParameters, substitution)) {
                return false;
            }
        }
        return inferTypeArgumentsFromTypes(
            expectedFunction->returnType, actualFunction->returnType, typeParameters, substitution);
    }

    auto expectedParameterized = parameterizedTypeFromName(expectedType);
    if (expectedParameterized) {
        auto actualParameterized = parameterizedTypeFromName(actualType);
        if (!actualParameterized) {
            auto alias = stdlibTypeAliasForResolvedName(actualType);
            if (!alias || alias->baseName != expectedParameterized->first ||
                alias->arguments.size() != expectedParameterized->second.size()) {
                return false;
            }
            for (size_t i = 0; i < expectedParameterized->second.size(); ++i) {
                if (!inferTypeArgumentsFromTypes(
                        expectedParameterized->second[i], alias->arguments[i],
                        typeParameters, substitution)) {
                    return false;
                }
            }
            return true;
        }
        if (expectedParameterized->first != actualParameterized->first) {
            auto alias = stdlibTypeAliasForResolvedName(actualType);
            if (!alias || alias->baseName != expectedParameterized->first ||
                alias->arguments.size() != expectedParameterized->second.size()) {
                return false;
            }
            for (size_t i = 0; i < expectedParameterized->second.size(); ++i) {
                if (!inferTypeArgumentsFromTypes(
                        expectedParameterized->second[i], alias->arguments[i],
                        typeParameters, substitution)) {
                    return false;
                }
            }
            return true;
        }
        if (expectedParameterized->second.size() != actualParameterized->second.size()) return false;
        for (size_t i = 0; i < expectedParameterized->second.size(); ++i) {
            if (!inferTypeArgumentsFromTypes(
                    expectedParameterized->second[i], actualParameterized->second[i],
                    typeParameters, substitution)) {
                return false;
            }
        }
        return true;
    }

    return expectedType == actualType;
}

inline std::optional<std::map<std::string, std::string>> inferGenericFunctionSubstitution(
    const CompilerContext::FunctionSignature& signature, const std::vector<std::string>& actualArgumentTypes) {
    if (signature.typeParameters.empty()) return std::map<std::string, std::string>{};
    if (signature.parameters.size() != actualArgumentTypes.size()) return std::nullopt;
    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (!inferTypeArgumentsFromTypes(
                signature.parameters[i].type, actualArgumentTypes[i],
                signature.typeParameters, substitution)) {
            return std::nullopt;
        }
    }
    for (const auto& typeParameter : signature.typeParameters) {
        if (substitution.count(typeParameter) == 0) return std::nullopt;
    }
    return substitution;
}

inline bool isFunctionTypeName(const std::string& type) {
    return functionTypeFromName(type).has_value();
}

inline std::optional<std::string> functionNameForType(
    const CompilerContext::FunctionType& functionType) {
    if (functionType.parameterTypes.empty()) return std::nullopt;
    return formatFunctionType(functionType);
}

inline std::optional<std::string> functionNameForSignature(
    const CompilerContext::FunctionSignature& signature) {
    CompilerContext::FunctionType functionType;
    for (const auto& parameter : signature.parameters) {
        functionType.parameterTypes.push_back(parameter.type);
    }
    functionType.returnType = signature.returnType;
    return functionNameForType(functionType);
}

inline bool functionTypeNameMatchesSignature(
    const std::string& typeName, const CompilerContext::FunctionSignature& signature) {
    auto expectedType = functionTypeFromName(typeName);
    if (!expectedType) return false;
    if (expectedType->parameterTypes.size() != signature.parameters.size()) return false;
    for (size_t i = 0; i < expectedType->parameterTypes.size(); ++i) {
        if (expectedType->parameterTypes[i] != signature.parameters[i].type) return false;
    }
    return expectedType->returnType == signature.returnType;
}
