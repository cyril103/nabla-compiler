#pragma once

#include "compiler_error.hpp"
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
};

inline std::optional<CompilerContext::FunctionType> functionTypeFromAlias(const std::string& type) {
    if (type == "IntUnaryFn") return CompilerContext::FunctionType{{"Int"}, "Int"};
    if (type == "IntConsumerFn") return CompilerContext::FunctionType{{"Int"}, "Unit"};
    if (type == "IntBinaryFn") return CompilerContext::FunctionType{{"Int", "Int"}, "Int"};
    return std::nullopt;
}

inline bool isFunctionTypeAlias(const std::string& type) {
    return functionTypeFromAlias(type).has_value();
}

inline std::optional<std::string> functionAliasForType(
    const CompilerContext::FunctionType& functionType) {
    if (functionType.parameterTypes == std::vector<std::string>{"Int"} &&
        functionType.returnType == "Int") {
        return "IntUnaryFn";
    }
    if (functionType.parameterTypes == std::vector<std::string>{"Int"} &&
        functionType.returnType == "Unit") {
        return "IntConsumerFn";
    }
    if (functionType.parameterTypes == std::vector<std::string>{"Int", "Int"} &&
        functionType.returnType == "Int") {
        return "IntBinaryFn";
    }
    return std::nullopt;
}

inline std::optional<std::string> functionAliasForSignature(
    const CompilerContext::FunctionSignature& signature) {
    CompilerContext::FunctionType functionType;
    for (const auto& parameter : signature.parameters) {
        functionType.parameterTypes.push_back(parameter.type);
    }
    functionType.returnType = signature.returnType;
    return functionAliasForType(functionType);
}

inline bool functionAliasMatchesSignature(
    const std::string& alias, const CompilerContext::FunctionSignature& signature) {
    auto expectedType = functionTypeFromAlias(alias);
    if (!expectedType) return false;
    if (expectedType->parameterTypes.size() != signature.parameters.size()) return false;
    for (size_t i = 0; i < expectedType->parameterTypes.size(); ++i) {
        if (expectedType->parameterTypes[i] != signature.parameters[i].type) return false;
    }
    return expectedType->returnType == signature.returnType;
}
