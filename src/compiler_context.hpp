#pragma once

#include "compiler_error.hpp"
#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <sstream>
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
        bool isRepeated = false;
        std::string repeatedElementType = "";
        bool isByName = false;
    };

    struct TypeParameterInfo {
        std::string name;
        int arity = 0;
    };

    struct FunctionSignature {
        std::vector<ParameterInfo> parameters;
        std::string returnType;
        std::vector<std::string> typeParameters;
        SourceLocation location;
        SourceLocation returnTypeLocation;
        bool isOverride = false;
        bool isAbstract = false;
        bool isSyntheticAccessor = false;
        std::vector<bool> returnFunctionByNameParameters;
        std::vector<TypeParameterInfo> typeParameterInfos;
        bool hasClosureParameter = false;
    };

    struct FieldInfo {
        std::string name;
        std::string type;
        SourceLocation location;
        bool hasAccessor = false;
        bool isMutable = false;
    };

    struct ClassInfo {
        std::vector<FieldInfo> fields;
        std::map<std::string, FunctionSignature> methods;
        std::map<std::string, std::vector<std::string>> methodOverloads;
        std::vector<std::string> parentTypes;
        std::vector<std::string> parentConstructorArguments;
        std::vector<FieldInfo> inheritedConstructorSignature;
        bool hasExplicitParent = false;
        bool isTrait = false;
        std::vector<std::string> typeParameters;
        std::vector<TypeParameterInfo> typeParameterInfos;
        SourceLocation location;
    };

    struct TopLevelSymbolInfo {
        std::string sourceName;
        std::string internalName;
        std::string moduleName;
        SourceLocation location;
    };

    std::map<std::string, std::map<std::string, int>> classLayouts;
    std::map<std::string, ClassInfo> classes;
    std::map<std::string, FunctionSignature> functions;
    std::map<std::string, std::vector<std::string>> functionOverloads;
    std::map<std::string, std::vector<TopLevelSymbolInfo>> topLevelFunctionsBySourceName;
    std::map<std::string, TopLevelSymbolInfo> topLevelFunctionsByInternalName;
    std::map<std::string, std::vector<TopLevelSymbolInfo>> topLevelTypesBySourceName;
    std::map<std::string, TopLevelSymbolInfo> topLevelTypesByInternalName;
    std::set<std::string> objects;
    std::set<std::string> runtimeObjects;
    std::set<std::string> parsedFiles;
    std::filesystem::path rootDir;
    std::filesystem::path stdlibDir;
    std::filesystem::path currentFile;
    std::string currentModuleName;
    std::string currentPackageName;
    std::map<std::string, std::string> semanticSymbolTypes;
    std::vector<std::string> semanticTypeParameters;
    std::vector<TypeParameterInfo> semanticTypeParameterInfos;
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
        {"Array", {"Int"}, "collections.int_array.ArrayInt"},
        {"Array", {"Long"}, "collections.long_array.ArrayLong"},
        {"Array", {"Float"}, "collections.float_array.ArrayFloat"},
        {"Array", {"Double"}, "collections.double_array.ArrayDouble"},
        {"Array", {"Bool"}, "collections.bool_array.ArrayBool"},
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
        {"ArrayFill", {"Int"}, "arrayIntFill"},
        {"ArrayFill", {"Long"}, "arrayLongFill"},
        {"ArrayFill", {"Float"}, "arrayFloatFill"},
        {"ArrayFill", {"Double"}, "arrayDoubleFill"},
        {"ArrayFill", {"Bool"}, "arrayBoolFill"},
        {"Array.fill", {"Int"}, "arrayIntFill"},
        {"Array.fill", {"Long"}, "arrayLongFill"},
        {"Array.fill", {"Float"}, "arrayFloatFill"},
        {"Array.fill", {"Double"}, "arrayDoubleFill"},
        {"Array.fill", {"Bool"}, "arrayBoolFill"},
        {"Array.empty", {"Int"}, "arrayIntEmpty"},
        {"Array.empty", {"Long"}, "arrayLongEmpty"},
        {"Array.empty", {"Float"}, "arrayFloatEmpty"},
        {"Array.empty", {"Double"}, "arrayDoubleEmpty"},
        {"Array.empty", {"Bool"}, "arrayBoolEmpty"},
        {"Array.tabulate", {"Int"}, "arrayIntTabulate"},
        {"Array.tabulate", {"Long"}, "arrayLongTabulate"},
        {"Array.tabulate", {"Float"}, "arrayFloatTabulate"},
        {"Array.tabulate", {"Double"}, "arrayDoubleTabulate"},
        {"Array.tabulate", {"Bool"}, "arrayBoolTabulate"},
        {"Set.fromArray", {"Int"}, "setFromArrayInt"},
        {"Set.fromArray", {"Long"}, "setFromArrayLong"},
        {"Set.fromArray", {"Float"}, "setFromArrayFloat"},
        {"Set.fromArray", {"Double"}, "setFromArrayDouble"},
        {"Set.fromArray", {"Bool"}, "setFromArrayBool"},
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

inline std::optional<std::pair<std::string, std::vector<std::string>>> parameterizedTypeFromName(
    const std::string& type);
inline bool isFunctionTypeName(const std::string& type);

inline bool isPrimitiveArrayElementType(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" ||
           type == "Double" || type == "Bool";
}

inline bool isBuiltinValueType(const std::string& type) {
    return type == "Int" || type == "Long" || type == "Float" ||
           type == "Double" || type == "Bool" || type == "Char" ||
           type == "Unit";
}

inline bool isBuiltinReferenceType(const std::string& type) {
    if (type == "String" || type == "IntArray" || type == "LongArray" ||
        type == "FloatArray" || type == "DoubleArray" || type == "BoolArray" ||
        isFunctionTypeName(type)) {
        return true;
    }
    auto parameterizedType = parameterizedTypeFromName(type);
    return parameterizedType && parameterizedType->first == "ObjectArray" &&
           parameterizedType->second.size() == 1;
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
    const auto stdlibAliasName = [](const std::string& value) {
        const std::array<std::pair<std::string, std::string>, 9> dottedAliases = {{
            {"collections.array.Array.fill", "Array.fill"},
            {"collections.array.Array.empty", "Array.empty"},
            {"collections.array.Array.tabulate", "Array.tabulate"},
            {"collections.set.Set.fromArray", "Set.fromArray"},
            {"collections.map.Map.fromArray", "Map.fromArray"},
            {"collections.map.Map.empty", "Map.empty"},
            {"collections.list.List.fromArray", "List.fromArray"},
            {"collections.list.List.empty", "List.empty"},
            {"core.option.Option.some", "Option.some"}
        }};
        const std::array<std::string, 9> shortAliases = {
            "Array.fill", "Array.empty", "Array.tabulate", "Set.fromArray", "Map.fromArray",
            "Map.empty", "List.fromArray", "List.empty", "Option.some"
        };
        for (const auto& alias : shortAliases) {
            if (value == alias) return value;
        }
        for (const auto& [qualifiedAlias, sourceAlias] : dottedAliases) {
            if (value == qualifiedAlias) return sourceAlias;
        }
        return value;
    };
    const std::string aliasName = stdlibAliasName(name);
    for (const auto& alias : stdlibFunctionAliases()) {
        if (alias.name == aliasName && alias.typeArguments == typeArguments) return alias.resolvedName;
    }
    if (typeArguments.size() == 1 && !isPrimitiveArrayElementType(typeArguments[0])) {
        if (aliasName == "arrayMkString" && typeArguments[0] == "String") return "objectStringArrayMkString";
        if (aliasName == "arrayFill" || aliasName == "ArrayFill" || aliasName == "Array.fill") return "objectArrayFill";
        if (aliasName == "Array.empty") return "objectArrayEmpty";
        if (aliasName == "Array.tabulate") return "objectArrayTabulate";
        if (aliasName == "Set.fromArray") return "setFromArray";
        if (aliasName == "arrayMap") return "objectArrayMapSame";
        if (aliasName == "arrayFilter") return "objectArrayFilter";
        if (aliasName == "arrayForeach") return "objectArrayForeach";
    }
    if (typeArguments.size() == 2 && !isPrimitiveArrayElementType(typeArguments[0])) {
        if (aliasName == "arrayMap") return "objectArrayMap";
        if (aliasName == "arrayFold") return "objectArrayFold";
        if (aliasName == "arrayFlatMap") return "objectArrayFlatMap";
    }
    if (aliasName == "arrayMap" && typeArguments.size() == 2) {
        if (auto alias = resolvePrimitiveArrayMapAlias(typeArguments)) return alias;
    }
    if (aliasName == "arrayFlatMap" && typeArguments.size() == 2) {
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

inline std::optional<std::string> recommendedStdlibFunctionName(const std::string& name) {
    if (name == "ArrayFill" || name == "arrayFill" ||
        name == "arrayIntFill" || name == "arrayLongFill" ||
        name == "arrayFloatFill" || name == "arrayDoubleFill" ||
        name == "arrayBoolFill" || name == "objectArrayFill") {
        return "Array.fill";
    }
    if (name == "ArrayRange" || name == "arrayIntRange") return "Array.range";
    if (name == "ArrayRangeUntil" || name == "arrayIntRangeUntil") return "Array.range";
    if (name == "ArrayEmpty" || name == "arrayEmpty" ||
        name == "arrayIntEmpty" || name == "arrayLongEmpty" ||
        name == "arrayFloatEmpty" || name == "arrayDoubleEmpty" ||
        name == "arrayBoolEmpty" || name == "objectArrayEmpty") {
        return "Array.empty";
    }
    if (name == "ArrayTabulate" || name == "arrayTabulate" ||
        name == "arrayIntTabulate" || name == "arrayLongTabulate" ||
        name == "arrayFloatTabulate" || name == "arrayDoubleTabulate" ||
        name == "arrayBoolTabulate" || name == "objectArrayTabulate") {
        return "Array.tabulate";
    }
    if (name == "SetEmpty" || name == "setEmpty") return "Set.empty";
    if (name == "SetFromArray" || name == "setFromArray" ||
        name == "setFromArrayInt" || name == "setFromArrayLong" ||
        name == "setFromArrayFloat" || name == "setFromArrayDouble" ||
        name == "setFromArrayBool") {
        return "Set.fromArray";
    }
    if (name == "MapEmpty" || name == "mapEmpty") return "Map.empty";
    if (name == "MapFromArray" || name == "mapFromArray") return "Map.fromArray";
    if (name == "ListEmpty" || name == "listEmpty") return "List.empty";
    if (name == "ListCons" || name == "listCons") return "List.cons";
    if (name == "ListFromArray" || name == "listFromArrayFrom") return "List.fromArray";
    if (name == "OptionSome" || name == "optionSome") return "Option.some";
    if (name == "OptionNone" || name == "optionNone") return "Option.none";
    return std::nullopt;
}

inline std::string recommendedStdlibFunctionSuffix(const std::string& name) {
    if (auto recommended = recommendedStdlibFunctionName(name)) {
        return ". Nom recommande: '" + *recommended + "'";
    }
    return "";
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
    if (type.rfind(prefix, 0) != 0) return std::nullopt;

    int parenDepth = 0;
    int bracketDepth = 0;
    size_t arrowPosition = std::string::npos;
    for (size_t i = prefix.size(); i + 2 < type.size(); ++i) {
        const char c = type[i];
        if (c == '(') ++parenDepth;
        else if (c == ')') {
            if (parenDepth == 0 && bracketDepth == 0 && type.compare(i, 3, ")->") == 0) {
                arrowPosition = i;
                break;
            }
            --parenDepth;
        } else if (c == '[') ++bracketDepth;
        else if (c == ']') --bracketDepth;
        if (parenDepth < 0 || bracketDepth < 0) return std::nullopt;
    }
    if (arrowPosition == std::string::npos) return std::nullopt;

    CompilerContext::FunctionType functionType;
    std::string parameters = type.substr(prefix.size(), arrowPosition - prefix.size());
    functionType.parameterTypes = splitTopLevelTypes(parameters);
    if (!parameters.empty() && functionType.parameterTypes.empty()) return std::nullopt;
    functionType.returnType = type.substr(arrowPosition + 3);
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
    if (methodName == "sorted") {
        signature.parameters.push_back({
            "lessThan", formatFunctionType({{elementType, elementType}, "Bool"}), {}});
        signature.returnType = receiverType;
        return signature;
    }
    return std::nullopt;
}

inline std::string unqualifiedSourceName(const std::string& name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

inline bool looksLikeTypeVariableName(const std::string& name) {
    if (name.empty() || name.find('.') != std::string::npos || name.find('[') != std::string::npos) return false;
    return std::isupper(static_cast<unsigned char>(name.front())) != 0;
}

inline std::string resolveStdlibTypeAlias(const std::string& type) {
    auto parameterizedType = parameterizedTypeFromName(type);
    if (!parameterizedType) return type;
    const auto& [baseName, arguments] = *parameterizedType;
    const std::string sourceBaseName = unqualifiedSourceName(baseName);
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.baseName == sourceBaseName && alias.arguments == arguments) return alias.resolvedName;
    }
    if (sourceBaseName == "Array" && arguments.size() == 1) {
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
        return formatParameterizedType("collections.object_array.ArrayObject", arguments);
    }
    return type;
}

inline bool isStdlibTypeAliasFamily(const std::string& baseName, size_t arity) {
    const std::string sourceBaseName = unqualifiedSourceName(baseName);
    if (sourceBaseName == "Array" && arity == 1) return true;
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.baseName == sourceBaseName && alias.arguments.size() == arity) return true;
    }
    return false;
}

inline std::optional<StdlibTypeAlias> stdlibTypeAliasForResolvedName(const std::string& resolvedName) {
    for (const auto& alias : stdlibTypeAliases()) {
        if (alias.resolvedName == resolvedName) return alias;
    }
    auto parameterizedType = parameterizedTypeFromName(resolvedName);
    if (parameterizedType && unqualifiedSourceName(parameterizedType->first) == "ArrayObject" &&
        parameterizedType->second.size() == 1) {
        return StdlibTypeAlias{"Array", parameterizedType->second, resolvedName};
    }
    return std::nullopt;
}

inline std::string publicTypeNameForDiagnostics(const std::string& type) {
    if (auto alias = stdlibTypeAliasForResolvedName(type)) {
        return formatParameterizedType(alias->baseName, alias->arguments);
    }
    return type;
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

inline std::string classLookupNameFor(const CompilerContext& context, const std::string& type) {
    const std::string baseName = genericBaseName(type);
    if (context.classes.count(baseName)) return baseName;
    auto sourceIt = context.topLevelTypesBySourceName.find(baseName);
    if (sourceIt != context.topLevelTypesBySourceName.end() && sourceIt->second.size() == 1 &&
        context.classes.count(sourceIt->second.front().internalName)) {
        return sourceIt->second.front().internalName;
    }
    const std::string shortBaseName = unqualifiedSourceName(baseName);
    if (shortBaseName != baseName) {
        auto shortSourceIt = context.topLevelTypesBySourceName.find(shortBaseName);
        if (shortSourceIt != context.topLevelTypesBySourceName.end()) {
            for (const auto& candidate : shortSourceIt->second) {
                if (candidate.internalName == baseName && context.classes.count(candidate.internalName)) {
                    return candidate.internalName;
                }
            }
        }
    }
    return baseName;
}

struct ClassMethodLookupResult {
    const CompilerContext::FunctionSignature* signature = nullptr;
    std::string methodName;
    std::string ownerClassName;
    std::map<std::string, std::string> classSubstitution;
};

struct ClassFieldLookupResult {
    std::string ownerClassName;
    std::string type;
    bool isMutable = false;
};

inline std::optional<std::map<std::string, std::string>> genericSubstitutionFor(
    const CompilerContext& context, const std::string& concreteType);
inline std::string substituteType(
    const std::string& type, const std::map<std::string, std::string>& substitution);
inline std::optional<std::map<std::string, std::string>> inferGenericFunctionSubstitution(
    const CompilerContext::FunctionSignature& signature,
    const std::vector<std::string>& actualArgumentTypes);

inline void collectVisibleFieldsInHierarchy(
    const CompilerContext& context, const std::string& receiverType,
    std::set<std::string>& visiting,
    std::map<std::string, std::set<std::string>>& fieldOwners,
    std::map<std::string, std::string>& fieldTypes,
    std::map<std::string, bool>* fieldMutability = nullptr) {
    const std::string classLookupName = classLookupNameFor(context, receiverType);
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
        if (fieldMutability && !fieldMutability->count(field.name)) {
            (*fieldMutability)[field.name] = field.isMutable;
        }
    }

    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, classSubstitution);
        collectVisibleFieldsInHierarchy(context, concreteParentType, visiting, fieldOwners, fieldTypes, fieldMutability);
    }

    visiting.erase(classLookupName);
}

inline void collectClassFieldsInHierarchyForLayout(
    const CompilerContext& context, const std::string& receiverType,
    std::set<std::string>& visiting,
    std::vector<std::pair<std::string, std::string>>& fields) {
    const std::string classLookupName = classLookupNameFor(context, receiverType);
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

inline bool isDeclaredTypeParameterName(const CompilerContext& context, const std::string& type) {
    if (type.empty() || type.find('.') != std::string::npos || type.find('[') != std::string::npos) {
        return false;
    }
    for (const auto& [className, classInfo] : context.classes) {
        (void)className;
        if (isTypeParameterName(type, classInfo.typeParameters)) return true;
        for (const auto& [methodName, signature] : classInfo.methods) {
            (void)methodName;
            if (isTypeParameterName(type, signature.typeParameters)) return true;
        }
    }
    for (const auto& [functionName, signature] : context.functions) {
        (void)functionName;
        if (isTypeParameterName(type, signature.typeParameters)) return true;
    }
    return false;
}

inline std::vector<CompilerContext::TypeParameterInfo> ordinaryTypeParameterInfos(
    const std::vector<std::string>& typeParameters) {
    std::vector<CompilerContext::TypeParameterInfo> infos;
    for (const auto& typeParameter : typeParameters) {
        infos.push_back({typeParameter, 0});
    }
    return infos;
}

inline int typeParameterArity(
    const std::string& type, const std::vector<CompilerContext::TypeParameterInfo>& typeParameters) {
    for (const auto& typeParameter : typeParameters) {
        if (typeParameter.name == type) return typeParameter.arity;
    }
    return -1;
}

inline std::optional<std::map<std::string, std::string>> genericSubstitutionFor(
    const CompilerContext& context, const std::string& concreteType) {
    auto parameterizedType = parameterizedTypeFromName(concreteType);
    if (!parameterizedType) return std::nullopt;
    const auto& [baseName, arguments] = *parameterizedType;
    auto classIt = context.classes.find(classLookupNameFor(context, baseName));
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
        const int expectedArity = i < signature.typeParameterInfos.size()
            ? signature.typeParameterInfos[i].arity
            : 0;
        if (expectedArity == 1) {
            if (parameterizedTypeFromName(arguments[i])) return std::nullopt;
        } else if (expectedArity != 0) {
            return std::nullopt;
        }
        substitution[signature.typeParameters[i]] = arguments[i];
    }
    return substitution;
}

inline bool hasRepeatedParameter(const CompilerContext::FunctionSignature& signature) {
    return !signature.parameters.empty() && signature.parameters.back().isRepeated;
}

inline size_t minimumArgumentCount(const CompilerContext::FunctionSignature& signature) {
    return hasRepeatedParameter(signature)
        ? signature.parameters.size() - 1
        : signature.parameters.size();
}

inline bool acceptsArgumentCount(
    const CompilerContext::FunctionSignature& signature, size_t argumentCount) {
    if (hasRepeatedParameter(signature)) {
        return argumentCount >= minimumArgumentCount(signature);
    }
    return argumentCount == signature.parameters.size();
}

inline std::string expectedArgumentTypeAt(
    const CompilerContext::FunctionSignature& signature,
    size_t argumentIndex,
    const std::map<std::string, std::string>& substitution) {
    if (hasRepeatedParameter(signature) && argumentIndex >= signature.parameters.size() - 1) {
        return substituteType(signature.parameters.back().repeatedElementType, substitution);
    }
    return substituteType(signature.parameters[argumentIndex].type, substitution);
}

inline std::vector<std::string> expandedExpectedArgumentTypes(
    const CompilerContext::FunctionSignature& signature,
    size_t argumentCount,
    const std::map<std::string, std::string>& substitution = {}) {
    std::vector<std::string> expectedTypes;
    if (!acceptsArgumentCount(signature, argumentCount)) return expectedTypes;
    for (size_t i = 0; i < argumentCount; ++i) {
        expectedTypes.push_back(expectedArgumentTypeAt(signature, i, substitution));
    }
    return expectedTypes;
}

inline std::vector<std::string> orderedTypeArguments(
    const std::vector<std::string>& typeParameters,
    const std::map<std::string, std::string>& substitution) {
    std::vector<std::string> arguments;
    for (const auto& typeParameter : typeParameters) {
        auto argument = substitution.find(typeParameter);
        if (argument != substitution.end()) arguments.push_back(argument->second);
    }
    return arguments;
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
        auto baseSubstitution = substitution.find(baseName);
        bool anyArgumentChanged = false;
        for (auto& argument : arguments) {
            const std::string originalArgument = argument;
            argument = substituteType(argument, substitution);
            anyArgumentChanged = anyArgumentChanged || argument != originalArgument;
        }
        if (baseSubstitution != substitution.end() &&
            !parameterizedTypeFromName(baseSubstitution->second)) {
            return canonicalTypeName(formatParameterizedType(baseSubstitution->second, arguments));
        }
        const std::string substitutedType = canonicalTypeName(formatParameterizedType(baseName, arguments));
        if (substitutedType == formatParameterizedType(baseName, arguments) &&
            baseName == "Array" && arguments.size() == 1 && anyArgumentChanged) {
            return formatParameterizedType("collections.object_array.ArrayObject", arguments);
        }
        return substitutedType;
    }

    return type;
}

inline bool isTypeAssignable(
    const CompilerContext& context,
    const std::string& actualType,
    const std::string& expectedType,
    std::set<std::string>& visiting) {
    if (actualType == expectedType) return true;
    if (actualType == "Nothing") return true;
    auto actualParameterized = parameterizedTypeFromName(actualType);
    auto expectedParameterized = parameterizedTypeFromName(expectedType);
    if (actualParameterized && expectedParameterized &&
        unqualifiedSourceName(actualParameterized->first) == "List" &&
        unqualifiedSourceName(expectedParameterized->first) == "List" &&
        actualParameterized->second.size() == 1 && expectedParameterized->second.size() == 1 &&
        actualParameterized->second[0] == "Nothing") {
        return true;
    }
    if (actualParameterized && expectedParameterized &&
        actualParameterized->second == expectedParameterized->second &&
        unqualifiedSourceName(actualParameterized->first) == unqualifiedSourceName(expectedParameterized->first)) {
        return true;
    }
    if (actualParameterized && expectedParameterized &&
        actualParameterized->second == expectedParameterized->second &&
        ((unqualifiedSourceName(actualParameterized->first) == "Array" &&
          unqualifiedSourceName(expectedParameterized->first) == "ArrayObject") ||
         (unqualifiedSourceName(actualParameterized->first) == "ArrayObject" &&
          unqualifiedSourceName(expectedParameterized->first) == "Array"))) {
        return true;
    }
    if (expectedParameterized && unqualifiedSourceName(expectedParameterized->first) == "Array") {
        auto actualAlias = stdlibTypeAliasForResolvedName(actualType);
        if (actualAlias && actualAlias->baseName == "Array" &&
            actualAlias->arguments == expectedParameterized->second) {
            return true;
        }
    }
    if (expectedType == "Any") return true;
    if (expectedType == "AnyVal") return isBuiltinValueType(actualType);
    if (expectedType == "AnyRef" && isBuiltinReferenceType(actualType)) return true;

    const std::string actualBaseName = classLookupNameFor(context, actualType);
    if (visiting.count(actualBaseName)) return false;

    auto classIt = context.classes.find(actualBaseName);
    if (classIt == context.classes.end()) return false;

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, actualType)) {
        classSubstitution = *genericSubstitution;
    }

    visiting.insert(actualBaseName);
    for (const auto& parentType : classIt->second.parentTypes) {
        const std::string concreteParentType = substituteType(parentType, classSubstitution);
        if (isTypeAssignable(context, concreteParentType, expectedType, visiting)) {
            visiting.erase(actualBaseName);
            return true;
        }
    }
    visiting.erase(actualBaseName);
    return false;
}

inline bool isTypeAssignable(
    const CompilerContext& context,
    const std::string& actualType,
    const std::string& expectedType) {
    std::set<std::string> visiting;
    return isTypeAssignable(context, actualType, expectedType, visiting);
}

inline int numericWideningRank(const std::string& type) {
    if (type == "Char") return 0;
    if (type == "Int") return 1;
    if (type == "Long") return 2;
    if (type == "Float") return 3;
    if (type == "Double") return 4;
    return -1;
}

inline bool isNumericWideningAssignable(const std::string& actualType, const std::string& expectedType) {
    const int actualRank = numericWideningRank(actualType);
    const int expectedRank = numericWideningRank(expectedType);
    return actualRank >= 0 && expectedRank >= 0 && actualRank < expectedRank;
}

inline bool isValueAssignable(
    const CompilerContext& context,
    const std::string& actualType,
    const std::string& expectedType) {
    return isTypeAssignable(context, actualType, expectedType) ||
           isNumericWideningAssignable(actualType, expectedType);
}

inline bool exactOrBottomListAssignable(
    const CompilerContext& context,
    const std::string& actualType,
    const std::string& expectedType) {
    if (actualType == expectedType) return true;
    const auto expectedParameterized = parameterizedTypeFromName(expectedType);
    if (!expectedParameterized || expectedParameterized->first != "List" ||
        expectedParameterized->second.size() != 1) {
        return false;
    }
    return isTypeAssignable(context, actualType, expectedType);
}

inline void collectClassMethodLookupCandidates(
    const CompilerContext& context, const std::string& receiverType,
    const std::string& methodName, std::set<std::string>& visited,
    std::set<std::string>& providerTypes,
    std::vector<ClassMethodLookupResult>& candidates) {
    const std::string classLookupName = classLookupNameFor(context, receiverType);
    if (visited.count(classLookupName)) return;

    auto classIt = context.classes.find(classLookupName);
    if (classIt == context.classes.end()) return;

    std::map<std::string, std::string> classSubstitution;
    if (auto genericSubstitution = genericSubstitutionFor(context, receiverType)) {
        classSubstitution = *genericSubstitution;
    }

    std::vector<std::string> methodNames;
    auto overloadsIt = classIt->second.methodOverloads.find(methodName);
    if (overloadsIt != classIt->second.methodOverloads.end()) {
        methodNames = overloadsIt->second;
    } else if (classIt->second.methods.count(methodName)) {
        methodNames = {methodName};
    }

    if (!methodNames.empty()) {
        const std::string providerType = substituteType(receiverType, classSubstitution);
        if (providerTypes.insert(providerType).second) {
            for (const auto& resolvedMethodName : methodNames) {
                auto methodIt = classIt->second.methods.find(resolvedMethodName);
                if (methodIt == classIt->second.methods.end()) continue;
                candidates.push_back(ClassMethodLookupResult{
                    &methodIt->second, resolvedMethodName, classLookupName, classSubstitution});
            }
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

struct ClassMethodOverloadMatches {
    std::vector<ClassMethodLookupResult> concreteMatches;
    std::vector<ClassMethodLookupResult> genericMatches;
};

inline ClassMethodOverloadMatches exactClassMethodOverloadMatches(
    const CompilerContext& context,
    const std::string& receiverType,
    const std::string& methodName,
    const std::vector<std::string>& actualArgumentTypes,
    const std::vector<std::string>& typeArguments = {}) {
    ClassMethodOverloadMatches matches;
    for (const auto& candidate : collectClassMethodLookupCandidates(context, receiverType, methodName)) {
        if (!candidate.signature) continue;
        const auto& signature = *candidate.signature;
        if (!acceptsArgumentCount(signature, actualArgumentTypes.size())) continue;

        std::map<std::string, std::string> substitution = candidate.classSubstitution;
        if (!typeArguments.empty()) {
            auto genericSubstitution = genericFunctionSubstitutionFor(signature, typeArguments);
            if (!genericSubstitution) continue;
            substitution.insert(genericSubstitution->begin(), genericSubstitution->end());
        } else if (!signature.typeParameters.empty()) {
            CompilerContext::FunctionSignature substitutedSignature = signature;
            for (auto& parameter : substitutedSignature.parameters) {
                parameter.type = substituteType(parameter.type, substitution);
            }
            substitutedSignature.returnType = substituteType(substitutedSignature.returnType, substitution);
            auto inferredSubstitution =
                inferGenericFunctionSubstitution(substitutedSignature, actualArgumentTypes);
            if (!inferredSubstitution) continue;
            substitution.insert(inferredSubstitution->begin(), inferredSubstitution->end());
        }

        bool compatible = true;
        for (size_t i = 0; i < actualArgumentTypes.size(); ++i) {
            const std::string expectedType = expectedArgumentTypeAt(signature, i, substitution);
            if (!exactOrBottomListAssignable(context, actualArgumentTypes[i], expectedType)) {
                compatible = false;
                break;
            }
        }
        if (compatible) {
            if (signature.typeParameters.empty()) {
                matches.concreteMatches.push_back(candidate);
            } else {
                matches.genericMatches.push_back(candidate);
            }
        }
    }
    return matches;
}

inline std::optional<ClassMethodLookupResult> resolveExactClassMethodOverload(
    const CompilerContext& context,
    const std::string& receiverType,
    const std::string& methodName,
    const std::vector<std::string>& actualArgumentTypes,
    const std::vector<std::string>& typeArguments = {}) {
    const auto matches =
        exactClassMethodOverloadMatches(context, receiverType, methodName, actualArgumentTypes, typeArguments);
    if (matches.concreteMatches.size() == 1) return matches.concreteMatches[0];
    if (matches.concreteMatches.empty() && matches.genericMatches.size() == 1) {
        return matches.genericMatches[0];
    }
    return std::nullopt;
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
    if (!acceptsArgumentCount(signature, actualArgumentTypes.size())) return std::nullopt;
    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < actualArgumentTypes.size(); ++i) {
        const std::string expectedPattern = hasRepeatedParameter(signature) &&
                i >= signature.parameters.size() - 1
            ? signature.parameters.back().repeatedElementType
            : signature.parameters[i].type;
        if (!inferTypeArgumentsFromTypes(
                expectedPattern, actualArgumentTypes[i],
                signature.typeParameters, substitution)) {
            return std::nullopt;
        }
    }
    for (const auto& typeParameter : signature.typeParameters) {
        if (substitution.count(typeParameter) == 0) return std::nullopt;
    }
    return substitution;
}

inline std::optional<std::map<std::string, std::string>> inferGenericFunctionSubstitutionForFunctionType(
    const CompilerContext::FunctionSignature& signature, const std::string& expectedType) {
    if (signature.typeParameters.empty()) return std::map<std::string, std::string>{};
    auto expectedFunction = functionTypeFromName(expectedType);
    if (!expectedFunction) return std::nullopt;
    if (signature.parameters.size() != expectedFunction->parameterTypes.size()) return std::nullopt;

    std::map<std::string, std::string> substitution;
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (!inferTypeArgumentsFromTypes(
                signature.parameters[i].type, expectedFunction->parameterTypes[i],
                signature.typeParameters, substitution)) {
            return std::nullopt;
        }
    }
    if (!inferTypeArgumentsFromTypes(
            signature.returnType, expectedFunction->returnType,
            signature.typeParameters, substitution)) {
        return std::nullopt;
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

inline std::string mangleFunctionTypePart(const std::string& type) {
    std::string result;
    for (char c : type) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            result.push_back(c);
        } else {
            result.push_back('_');
        }
    }
    return result;
}

inline std::string overloadedFunctionName(
    const std::string& sourceName, const CompilerContext::FunctionSignature& signature) {
    std::string result = sourceName;
    result += "$";
    if (signature.parameters.empty()) {
        result += "Unit";
    } else {
        for (size_t i = 0; i < signature.parameters.size(); ++i) {
            if (i > 0) result += "$";
            result += mangleFunctionTypePart(signature.parameters[i].type);
        }
    }
    return result;
}

inline std::string topLevelInternalPrefix(const std::string& moduleName) {
    return moduleName.empty() ? std::string{} : moduleName + ".";
}

inline std::string topLevelInternalName(
    const std::string& moduleName, const std::string& sourceName) {
    return topLevelInternalPrefix(moduleName) + sourceName;
}

inline std::set<std::string> topLevelFunctionModules(
    const CompilerContext& context, const std::string& sourceName) {
    std::set<std::string> modules;
    auto it = context.topLevelFunctionsBySourceName.find(sourceName);
    if (it == context.topLevelFunctionsBySourceName.end()) return modules;
    for (const auto& symbol : it->second) {
        modules.insert(symbol.moduleName);
    }
    return modules;
}

inline std::string formatImportedModules(const std::set<std::string>& modules) {
    std::string result;
    bool first = true;
    for (const auto& module : modules) {
        if (!first) result += ", ";
        result += module.empty() ? "<programme courant>" : module;
        first = false;
    }
    return result;
}

inline std::string ambiguousImportedNameMessage(
    const std::string& sourceName, const std::set<std::string>& modules) {
    return "nom importé ambigu '" + sourceName + "': exposé par " +
           formatImportedModules(modules);
}

inline bool functionSignatureParametersMatch(
    const CompilerContext::FunctionSignature& left,
    const CompilerContext::FunctionSignature& right) {
    if (left.parameters.size() != right.parameters.size()) return false;
    if (left.typeParameters.size() != right.typeParameters.size()) return false;

    std::map<std::string, std::string> rightToLeftTypeParameters;
    for (size_t i = 0; i < left.typeParameters.size(); ++i) {
        rightToLeftTypeParameters[right.typeParameters[i]] = left.typeParameters[i];
    }

    for (size_t i = 0; i < left.parameters.size(); ++i) {
        const auto& leftParameter = left.parameters[i];
        const auto& rightParameter = right.parameters[i];
        if (leftParameter.type != substituteType(rightParameter.type, rightToLeftTypeParameters)) return false;
        if (leftParameter.isRepeated != rightParameter.isRepeated) return false;
        if (leftParameter.repeatedElementType !=
            substituteType(rightParameter.repeatedElementType, rightToLeftTypeParameters)) {
            return false;
        }
    }
    return true;
}

inline std::string overloadedMethodName(
    const std::string& sourceName, const CompilerContext::FunctionSignature& signature) {
    return overloadedFunctionName(sourceName, signature);
}

inline const CompilerContext::FunctionSignature* findFunctionSignature(
    const CompilerContext& context, const std::string& functionName) {
    auto function = context.functions.find(functionName);
    if (function == context.functions.end()) return nullptr;
    return &function->second;
}

inline std::vector<std::string> functionOverloadNames(
    const CompilerContext& context, const std::string& sourceName) {
    auto overloads = context.functionOverloads.find(sourceName);
    if (overloads != context.functionOverloads.end()) return overloads->second;
    const std::array<std::pair<std::string, std::string>, 9> stdlibQualifiedFunctionAliases = {{
        {"collections.array.Array.fill", "Array.fill"},
        {"collections.array.Array.empty", "Array.empty"},
        {"collections.array.Array.tabulate", "Array.tabulate"},
        {"collections.set.Set.fromArray", "Set.fromArray"},
        {"collections.map.Map.fromArray", "Map.fromArray"},
        {"collections.map.Map.empty", "Map.empty"},
        {"collections.list.List.fromArray", "List.fromArray"},
        {"collections.list.List.empty", "List.empty"},
        {"core.option.Option.some", "Option.some"}
    }};
    for (const auto& [qualifiedName, shortName] : stdlibQualifiedFunctionAliases) {
        if (sourceName == qualifiedName) {
            auto shortOverloads = context.functionOverloads.find(shortName);
            if (shortOverloads != context.functionOverloads.end()) return shortOverloads->second;
            if (context.functions.count(shortName)) return {shortName};
            break;
        }
    }
    if (context.functions.count(sourceName)) return {sourceName};
    return {};
}

inline std::optional<std::string> uniqueFunctionInternalNameForSource(
    const CompilerContext& context, const std::string& sourceName) {
    auto overloads = functionOverloadNames(context, sourceName);
    if (overloads.size() != 1) return std::nullopt;
    return overloads[0];
}

inline const CompilerContext::FunctionSignature* uniqueFunctionSignatureForName(
    const CompilerContext& context, const std::string& sourceName) {
    auto internalName = uniqueFunctionInternalNameForSource(context, sourceName);
    if (!internalName) return nullptr;
    return findFunctionSignature(context, *internalName);
}

inline std::optional<std::string> resolveExactFunctionOverload(
    const CompilerContext& context,
    const std::string& sourceName,
    const std::vector<std::string>& actualArgumentTypes,
    const std::vector<std::string>& typeArguments = {});

struct FunctionOverloadMatches {
    std::vector<std::string> concreteMatches;
    std::vector<std::string> genericMatches;
};

inline FunctionOverloadMatches exactFunctionOverloadMatches(
    const CompilerContext& context,
    const std::string& sourceName,
    const std::vector<std::string>& actualArgumentTypes,
    const std::vector<std::string>& typeArguments = {}) {
    FunctionOverloadMatches matches;
    for (const auto& overloadName : functionOverloadNames(context, sourceName)) {
        const auto* signature = findFunctionSignature(context, overloadName);
        if (!signature) continue;
        if (!acceptsArgumentCount(*signature, actualArgumentTypes.size())) continue;
        if (!typeArguments.empty()) {
            if (!genericFunctionSubstitutionFor(*signature, typeArguments)) continue;
        } else if (!signature->typeParameters.empty()) {
            if (!inferGenericFunctionSubstitution(*signature, actualArgumentTypes)) continue;
        }

        std::map<std::string, std::string> substitution;
        if (!typeArguments.empty()) {
            auto genericSubstitution = genericFunctionSubstitutionFor(*signature, typeArguments);
            if (!genericSubstitution) continue;
            substitution = *genericSubstitution;
        } else if (!signature->typeParameters.empty()) {
            auto inferredSubstitution = inferGenericFunctionSubstitution(*signature, actualArgumentTypes);
            if (!inferredSubstitution) continue;
            substitution = *inferredSubstitution;
        }

        bool compatible = true;
        for (size_t i = 0; i < actualArgumentTypes.size(); ++i) {
            const std::string expectedType = expectedArgumentTypeAt(*signature, i, substitution);
            if (!exactOrBottomListAssignable(context, actualArgumentTypes[i], expectedType)) {
                compatible = false;
                break;
            }
        }
        if (compatible) {
            if (signature->typeParameters.empty()) {
                matches.concreteMatches.push_back(overloadName);
            } else {
                matches.genericMatches.push_back(overloadName);
            }
        }
    }
    return matches;
}

inline std::optional<std::string> resolveExactFunctionOverload(
    const CompilerContext& context,
    const std::string& sourceName,
    const std::vector<std::string>& actualArgumentTypes,
    const std::vector<std::string>& typeArguments) {
    const auto matches =
        exactFunctionOverloadMatches(context, sourceName, actualArgumentTypes, typeArguments);
    if (matches.concreteMatches.size() == 1) return matches.concreteMatches[0];
    if (matches.concreteMatches.empty() && matches.genericMatches.size() == 1) {
        return matches.genericMatches[0];
    }
    return std::nullopt;
}

inline std::string formatTypeList(const std::vector<std::string>& types) {
    std::string result;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) result += ", ";
        result += types[i];
    }
    return result;
}

inline std::string formatFunctionCallShape(
    const std::string& sourceName,
    const std::vector<std::string>& argumentTypes) {
    return sourceName + "(" + formatTypeList(argumentTypes) + ")";
}

inline std::string formatFunctionSignatureCandidate(
    const std::string& sourceName,
    const CompilerContext::FunctionSignature& signature) {
    std::vector<std::string> parameterTypes;
    for (const auto& parameter : signature.parameters) {
        parameterTypes.push_back(parameter.isRepeated
            ? parameter.repeatedElementType + "*"
            : parameter.type);
    }
    return sourceName + "(" + formatTypeList(parameterTypes) + "): " + signature.returnType;
}

inline std::string formatFunctionOverloadCandidates(
    const CompilerContext& context,
    const std::string& sourceName) {
    std::ostringstream out;
    for (const auto& overloadName : functionOverloadNames(context, sourceName)) {
        const auto* signature = findFunctionSignature(context, overloadName);
        if (!signature) continue;
        out << "\n- " << formatFunctionSignatureCandidate(sourceName, *signature);
    }
    return out.str();
}

inline std::string formatNoMatchingFunctionOverloadMessage(
    const CompilerContext& context,
    const std::string& sourceName,
    const std::vector<std::string>& argumentTypes) {
    std::string message =
        "aucune surcharge compatible pour '" +
        formatFunctionCallShape(sourceName, argumentTypes) + "'";
    std::string candidates = formatFunctionOverloadCandidates(context, sourceName);
    if (!candidates.empty()) {
        message += "\ncandidats:" + candidates;
    }
    return message;
}

inline std::string formatMethodOverloadCandidates(
    const CompilerContext& context,
    const std::string& receiverType,
    const std::string& methodName) {
    std::ostringstream out;
    for (const auto& candidate : collectClassMethodLookupCandidates(context, receiverType, methodName)) {
        if (!candidate.signature) continue;
        std::map<std::string, std::string> substitution = candidate.classSubstitution;
        CompilerContext::FunctionSignature signature = *candidate.signature;
        for (auto& parameter : signature.parameters) {
            parameter.type = substituteType(parameter.type, substitution);
        }
        signature.returnType = substituteType(signature.returnType, substitution);
        out << "\n- " << formatFunctionSignatureCandidate(receiverType + "." + methodName, signature);
    }
    return out.str();
}

inline std::string formatNoMatchingMethodOverloadMessage(
    const CompilerContext& context,
    const std::string& receiverType,
    const std::string& methodName,
    const std::vector<std::string>& argumentTypes) {
    std::string message =
        "aucune surcharge compatible pour '" +
        formatFunctionCallShape(receiverType + "." + methodName, argumentTypes) + "'";
    std::string candidates = formatMethodOverloadCandidates(context, receiverType, methodName);
    if (!candidates.empty()) {
        message += "\ncandidats:" + candidates;
    }
    return message;
}
