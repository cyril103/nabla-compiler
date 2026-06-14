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

inline std::optional<CompilerContext::FunctionType> functionTypeFromName(const std::string& type) {
    const std::string prefix = "Fn(";
    const std::string arrow = ")->";
    if (type.rfind(prefix, 0) != 0) return std::nullopt;
    size_t arrowPosition = type.find(arrow, prefix.size());
    if (arrowPosition == std::string::npos) return std::nullopt;

    CompilerContext::FunctionType functionType;
    std::string parameters = type.substr(prefix.size(), arrowPosition - prefix.size());
    size_t start = 0;
    while (start < parameters.size()) {
        size_t comma = parameters.find(',', start);
        if (comma == std::string::npos) comma = parameters.size();
        if (comma == start) return std::nullopt;
        functionType.parameterTypes.push_back(parameters.substr(start, comma - start));
        start = comma + 1;
    }
    functionType.returnType = type.substr(arrowPosition + arrow.size());
    if (functionType.returnType.empty()) return std::nullopt;
    return functionType;
}

inline std::string canonicalTypeName(const std::string& type) {
    auto functionType = functionTypeFromName(type);
    if (!functionType) return type;
    return formatFunctionType(*functionType);
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
