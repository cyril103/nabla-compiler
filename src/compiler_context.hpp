#pragma once

#include <map>
#include <set>
#include <string>
#include <filesystem>
#include <vector>

struct CompilerContext {
    struct ClassInfo {
        std::vector<std::pair<std::string, std::string>> fields;
        std::map<std::string, std::string> methods;
    };

    std::map<std::string, std::map<std::string, int>> classLayouts;
    std::map<std::string, ClassInfo> classes;
    std::set<std::string> parsedFiles;
    std::filesystem::path rootDir;
    std::filesystem::path currentFile;
    int nextLabelId = 0;
    struct SymbolInfo {
        int offsetFromRbp;
        bool isMutable;
        std::string type;
    };
    std::map<std::string, SymbolInfo> localVars;
    std::map<std::string, std::string> semanticSymbolTypes;
};
