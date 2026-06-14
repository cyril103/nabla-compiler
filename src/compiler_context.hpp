#pragma once

#include "compiler_error.hpp"
#include <map>
#include <set>
#include <string>
#include <filesystem>
#include <vector>

struct CompilerContext {
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
