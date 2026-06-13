#pragma once

#include <map>
#include <set>
#include <string>
#include <filesystem>

struct CompilerContext {
    std::map<std::string, std::map<std::string, int>> classLayouts;
    std::set<std::string> parsedFiles;
    std::filesystem::path rootDir;
    std::filesystem::path currentFile;
    int nextLabelId = 0;
};
