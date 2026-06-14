#pragma once

#include "ast.hpp"
#include "compiler_context.hpp"

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(CompilerContext& context);
    void analyze(ProgramNode& program);

private:
    CompilerContext& context;

    void validateDeclaredTypes() const;
    bool isKnownType(const std::string& type) const;
    bool isKnownTypeInScope(
        const std::string& type, const std::vector<std::string>& typeParameters) const;
};
