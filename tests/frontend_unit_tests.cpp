#include "../src/lexer.hpp"
#include "../src/parser.hpp"
#include "../src/semantic_analyzer.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& testName, const std::string& message) {
    ++failures;
    std::cerr << "FAIL " << testName << ": " << message << "\n";
}

void pass(const std::string& testName) {
    std::cout << "PASS " << testName << "\n";
}

void expectTrue(const std::string& testName, bool condition, const std::string& message) {
    if (!condition) fail(testName, message);
}

void run(const std::string& testName, const std::function<void(const std::string&)>& body) {
    int before = failures;
    try {
        body(testName);
    } catch (const std::exception& ex) {
        fail(testName, std::string("exception inattendue: ") + ex.what());
    }
    if (failures == before) pass(testName);
}

CompilerContext makeContext(const std::filesystem::path& file) {
    CompilerContext context;
    context.rootDir = std::filesystem::current_path();
    context.stdlibDir = context.rootDir / "stdlib";
    context.currentFile = file;
    return context;
}

std::unique_ptr<ProgramNode> parseSource(
    const std::string& source,
    const std::string& fileName,
    CompilerContext& context) {
    std::filesystem::path file = fileName;
    Lexer lexer(source, fileName);
    Parser parser(lexer.tokenize(), context, file);
    return parser.parseProgram();
}

void analyzeSource(const std::string& source, const std::string& fileName) {
    CompilerContext context = makeContext(fileName);
    auto program = parseSource(source, fileName, context);
    SemanticAnalyzer analyzer(context);
    analyzer.analyze(*program);
}

CompilerError expectCompilerError(
    const std::string& source,
    const std::string& fileName,
    ErrorKind expectedKind) {
    try {
        analyzeSource(source, fileName);
    } catch (const CompilerError& error) {
        if (error.kind != expectedKind) {
            throw CompilerError(
                error.kind,
                error.location,
                std::string("mauvais type d'erreur: ") + error.what());
        }
        return error;
    }
    throw std::runtime_error("aucune erreur compilateur levée");
}

void testLexerTracksTokensAndPositions(const std::string& testName) {
    Lexer lexer("\n  def main(): Int = 42\n  if true { 1 }\n", "unit_lexer.nabla");
    auto tokens = lexer.tokenize();

    expectTrue(testName, tokens.size() >= 12, "nombre de tokens inattendu");
    if (tokens.size() < 12) return;
    expectTrue(testName, tokens[0].type == TokenType::KW_DEF, "premier token != def");
    expectTrue(testName, tokens[0].location.file == "unit_lexer.nabla", "fichier du token def incorrect");
    expectTrue(testName, tokens[0].location.line == 2, "ligne du token def incorrecte");
    expectTrue(testName, tokens[0].location.column == 3, "colonne du token def incorrecte");
    expectTrue(testName, tokens[1].type == TokenType::IDENTIFIER, "second token != identifiant");
    expectTrue(testName, tokens[1].value == "main", "valeur de l'identifiant incorrecte");

    bool sawIf = false;
    for (const auto& token : tokens) {
        if (token.type == TokenType::KW_IF) {
            sawIf = true;
            expectTrue(testName, token.location.line == 3, "ligne du token if incorrecte");
            expectTrue(testName, token.location.column == 3, "colonne du token if incorrecte");
        }
    }
    expectTrue(testName, sawIf, "token if absent");
    expectTrue(testName, tokens.back().type == TokenType::EOF_TOKEN, "token EOF absent");
}

void testParserAcceptsGenericFunctionShape(const std::string& testName) {
    const std::string source =
        "def identity[T](value: T): T = {\n"
        "    value\n"
        "}\n";
    CompilerContext context = makeContext("unit_parser.nabla");
    auto program = parseSource(source, "unit_parser.nabla", context);

    expectTrue(testName, program->elements.size() == 1, "le programme devrait contenir une fonction");
    if (program->elements.empty()) return;
    auto* function = dynamic_cast<FunctionDefNode*>(program->elements[0].get());
    expectTrue(testName, function != nullptr, "le noeud parsé n'est pas une FunctionDefNode");
    if (function != nullptr) {
        expectTrue(testName, function->getName() == "identity", "nom de fonction incorrect");
    }
}

void testParserRejectsTopLevelStatement(const std::string& testName) {
    try {
        CompilerContext context = makeContext("unit_parser_error.nabla");
        (void)parseSource("if true { 1 }", "unit_parser_error.nabla", context);
        fail(testName, "le parser a accepté une instruction top-level interdite");
    } catch (const CompilerError& error) {
        expectTrue(testName, error.kind == ErrorKind::Parser, "l'erreur n'est pas une erreur parser");
        expectTrue(testName, error.location.line == 1, "ligne du diagnostic parser incorrecte");
        expectTrue(testName, error.location.column == 1, "colonne du diagnostic parser incorrecte");
        expectTrue(
            testName,
            std::string(error.what()).find("instruction inattendue") != std::string::npos,
            "message parser inattendu");
    }
}

void testSemanticIfConditionDiagnosticLocation(const std::string& testName) {
    const std::string source =
        "def main(): Int = {\n"
        "    if 1 {\n"
        "        10\n"
        "    } else {\n"
        "        20\n"
        "    }\n"
        "}\n";
    auto error = expectCompilerError(source, "unit_if_condition.nabla", ErrorKind::Semantic);
    expectTrue(testName, error.location.line == 2, "ligne de condition if incorrecte");
    expectTrue(testName, error.location.column == 8, "colonne de condition if incorrecte");
    expectTrue(
        testName,
        std::string(error.what()) == "la condition d'un 'if' doit être de type Bool",
        "message de condition if incorrect");
}

void testSemanticForCountDiagnosticLocation(const std::string& testName) {
    const std::string source =
        "def main(): Int = {\n"
        "    for true {\n"
        "        1\n"
        "    }\n"
        "    0\n"
        "}\n";
    auto error = expectCompilerError(source, "unit_for_count.nabla", ErrorKind::Semantic);
    expectTrue(testName, error.location.line == 2, "ligne de compteur for incorrecte");
    expectTrue(testName, error.location.column == 9, "colonne de compteur for incorrecte");
    expectTrue(
        testName,
        std::string(error.what()) == "le compteur d'un 'for' doit être de type Int",
        "message de compteur for incorrect");
}

void testSemanticBoolAndLeftDiagnosticLocation(const std::string& testName) {
    const std::string source =
        "def main(): Int = {\n"
        "    if 1 && true {\n"
        "        1\n"
        "    } else {\n"
        "        0\n"
        "    }\n"
        "}\n";
    auto error = expectCompilerError(source, "unit_bool_and.nabla", ErrorKind::Semantic);
    expectTrue(testName, error.location.line == 2, "ligne de l'opérande gauche incorrecte");
    expectTrue(testName, error.location.column == 8, "colonne de l'opérande gauche incorrecte");
    expectTrue(
        testName,
        std::string(error.what()) == "l'opérateur '&&' attend une expression gauche de type Bool",
        "message de l'opérande gauche incorrect");
}

void testSemanticGlobalOverloadResolvesByArgumentType(const std::string& testName) {
    (void)testName;
    const std::string source =
        "def pick(value: Int): Int = {\n"
        "    value + 1\n"
        "}\n"
        "\n"
        "def pick(value: String): Int = {\n"
        "    value.length()\n"
        "}\n"
        "\n"
        "def main(): Int = {\n"
        "    pick(41) + pick(\"abc\")\n"
        "}\n";
    analyzeSource(source, "unit_global_overload.nabla");
}

void testSemanticFunctionOverloadAmbiguityDiagnostic(const std::string& testName) {
    const std::string source =
        "def choose[T](value: T): Int = {\n"
        "    1\n"
        "}\n"
        "\n"
        "def choose[U](value: U): Int = {\n"
        "    2\n"
        "}\n"
        "\n"
        "def main(): Int = {\n"
        "    choose(42)\n"
        "}\n";
    auto error = expectCompilerError(source, "unit_global_overload_ambiguous.nabla", ErrorKind::Semantic);
    expectTrue(testName, error.location.line == 10, "ligne de l'appel ambigu incorrecte");
    expectTrue(testName, error.location.column == 5, "colonne de l'appel ambigu incorrecte");
    expectTrue(
        testName,
        std::string(error.what()).find("appel de fonction surchargée ambigu pour 'choose(Int)'") != std::string::npos,
        "message d'ambiguïté de surcharge globale incorrect");
}

void testSemanticGenericFunctionInferenceWithLambda(const std::string& testName) {
    (void)testName;
    const std::string source =
        "def applyOnce[T](value: T, f: (T) => T): T = {\n"
        "    f(value)\n"
        "}\n"
        "\n"
        "def main(): Int = {\n"
        "    applyOnce(41, value => value + 1)\n"
        "}\n";
    analyzeSource(source, "unit_generic_lambda_inference.nabla");
}

void testSemanticMethodOverloadResolvesByArgumentType(const std::string& testName) {
    (void)testName;
    const std::string source =
        "class Box() {\n"
        "    def pick(value: Int): Int = {\n"
        "        value + 1\n"
        "    }\n"
        "\n"
        "    def pick(value: String): Int = {\n"
        "        value.length()\n"
        "    }\n"
        "}\n"
        "\n"
        "def main(): Int = {\n"
        "    val box = new Box()\n"
        "    box.pick(41) + box.pick(\"abc\")\n"
        "}\n";
    analyzeSource(source, "unit_method_overload.nabla");
}

} // namespace

int main() {
    run("lexer_tracks_tokens_and_positions", testLexerTracksTokensAndPositions);
    run("parser_accepts_generic_function_shape", testParserAcceptsGenericFunctionShape);
    run("parser_rejects_top_level_statement", testParserRejectsTopLevelStatement);
    run("semantic_if_condition_diagnostic_location", testSemanticIfConditionDiagnosticLocation);
    run("semantic_for_count_diagnostic_location", testSemanticForCountDiagnosticLocation);
    run("semantic_bool_and_left_diagnostic_location", testSemanticBoolAndLeftDiagnosticLocation);
    run("semantic_global_overload_resolves_by_argument_type", testSemanticGlobalOverloadResolvesByArgumentType);
    run("semantic_function_overload_ambiguity_diagnostic", testSemanticFunctionOverloadAmbiguityDiagnostic);
    run("semantic_generic_function_inference_with_lambda", testSemanticGenericFunctionInferenceWithLambda);
    run("semantic_method_overload_resolves_by_argument_type", testSemanticMethodOverloadResolvesByArgumentType);

    if (failures != 0) {
        std::cerr << failures << " échec(s) de tests unitaires front-end\n";
        return 1;
    }
    std::cout << "Tous les tests unitaires front-end sont OK\n";
    return 0;
}
