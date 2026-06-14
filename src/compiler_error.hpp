#pragma once

#include <stdexcept>
#include <string>
#include <utility>

struct SourceLocation {
    std::string file;
    int line = 1;
    int column = 1;

    std::string format() const {
        return file + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

enum class ErrorKind {
    Lexer,
    Parser,
    Semantic,
    Codegen
};

class CompilerError : public std::runtime_error {
public:
    CompilerError(ErrorKind kind, SourceLocation location, const std::string& message)
        : std::runtime_error(message), kind(kind), location(std::move(location)) {}

    ErrorKind kind;
    SourceLocation location;

    std::string format() const {
        return location.format() + ": " + kindName() + " error: " + what();
    }

private:
    std::string kindName() const {
        switch (kind) {
            case ErrorKind::Lexer: return "lexer";
            case ErrorKind::Parser: return "parser";
            case ErrorKind::Semantic: return "semantic";
            case ErrorKind::Codegen: return "codegen";
        }
        return "compiler";
    }
};
