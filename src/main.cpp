#include "compiler_context.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include "semantic_analyzer.hpp"
#include "ir.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::filesystem::path findProjectRoot(const std::filesystem::path& start) {
    auto current = start;
    if (!current.has_filename()) current = current.parent_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "Makefile")) {
            return current;
        }
        auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return start.parent_path();
}

static void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [--keep-asm|--keep-temp|--emit-ir] <fichier.nabla>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    bool keepAsm = false;
    bool keepTemp = false;
    bool emitIR = false;
    std::string sourcePath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--keep-asm") {
            keepAsm = true;
        } else if (arg == "--keep-temp") {
            keepAsm = true;
            keepTemp = true;
        } else if (arg == "--emit-ir") {
            emitIR = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (sourcePath.empty()) {
            sourcePath = arg;
        } else {
            std::cerr << "Argument inattendu : " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    if (sourcePath.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    std::filesystem::path mainPath = sourcePath;
    if (!mainPath.is_absolute()) mainPath = std::filesystem::absolute(mainPath);
    std::string exeName = mainPath.stem().string();
    const char* buildDirEnv = std::getenv("NABLA_BUILD_DIR");
    std::filesystem::path buildDir = buildDirEnv ? std::filesystem::path(buildDirEnv) : std::filesystem::path(".");
    if (!std::filesystem::exists(buildDir)) std::filesystem::create_directories(buildDir);
    std::string asmFilename = (buildDir / (exeName + "_tmp.asm")).string();
    std::string objFilename = (buildDir / (exeName + "_tmp.o")).string();
    std::string exePath = (buildDir / exeName).string();

    std::ifstream file(mainPath);
    if (!file.is_open()) { std::cerr << "Erreur: Fichier introuvable '" << mainPath << "'\n"; return 1; }
    std::stringstream buffer; buffer << file.rdbuf();

    CompilerContext context;
    context.rootDir = findProjectRoot(mainPath);
    context.currentFile = mainPath;

    try {
        std::string canonicalMain = std::filesystem::canonical(mainPath).string();
        context.parsedFiles.insert(canonicalMain);

        Lexer lexer(buffer.str(), mainPath.string());
        Parser parser(lexer.tokenize(), context, mainPath);
        auto globalAST = parser.parseProgram();
        SemanticAnalyzer semanticAnalyzer(context);
        semanticAnalyzer.analyze(*globalAST);
        if (emitIR) {
            IRBuilder irBuilder;
            std::cout << irBuilder.build(*globalAST).format();
            return 0;
        }

        std::ofstream asmFile(asmFilename);
        globalAST->generateASM(asmFile, context);
        asmFile.close();

        std::string nasmCmd = "nasm -f elf64 " + asmFilename + " -o " + objFilename;
        std::string ldCmd = "ld " + objFilename + " -o " + exePath;
        if (std::system(nasmCmd.c_str()) != 0 || std::system(ldCmd.c_str()) != 0) return 1;

        if (!keepTemp) {
            std::remove(objFilename.c_str());
        }
        if (keepAsm) {
            std::cout << "\033[1;33m[Nabla] Fichier assembleur conservé : " << asmFilename << "\033[0m\n";
        } else {
            std::remove(asmFilename.c_str());
        }
        if (keepTemp) {
            std::cout << "\033[1;33m[Nabla] Fichiers temporaires conservés : " << asmFilename << ", " << objFilename << "\033[0m\n";
        }
        std::cout << "\033[1;32m[Nabla] Exécutable compilé avec succès : ./" << exePath << "\033[0m\n";
    } catch (const CompilerError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
