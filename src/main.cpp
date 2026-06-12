#include "lexer.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include <iostream>

std::map<std::string, std::map<std::string, int>> class_layouts;
std::set<std::string> parsed_files;
std::string current_parsing_class = "";

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <fichier.nabla>\n"; return 1; }
    std::string mainFile = argv[1];
    std::string exeName = mainFile.substr(0, mainFile.find_last_of('.'));
    std::string asmFilename = exeName + "_tmp.asm";
    std::string objFilename = exeName + "_tmp.o";

    std::ifstream file(mainFile);
    if (!file.is_open()) { std::cerr << "Erreur: Fichier introuvable '" << mainFile << "'\n"; return 1; }
    std::stringstream buffer; buffer << file.rdbuf();

    try {
        parsed_files.insert(mainFile);
        Lexer lexer(buffer.str());
        Parser parser(lexer.tokenize());
        auto globalAST = parser.parseProgram();

        std::ofstream asmFile(asmFilename);
        globalAST->generateASM(asmFile);
        asmFile.close();

        std::string nasmCmd = "nasm -f elf64 " + asmFilename + " -o " + objFilename;
        std::string ldCmd = "ld " + objFilename + " -o " + exeName;
        if (std::system(nasmCmd.c_str()) != 0 || std::system(ldCmd.c_str()) != 0) return 1;

        std::remove(asmFilename.c_str());
        std::remove(objFilename.c_str());
        std::cout << "\033[1;32m[Nabla] Exécutable compilé avec succès : ./" << exeName << "\033[0m\n";
    } catch (const std::exception& e) {
        std::cerr << "Erreur: Parser Error " << e.what() << "\n";
        return 1;
    }
    return 0;
}
