#pragma once

#include "ast.hpp"
#include "compiler_context.hpp"

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(CompilerContext& context);
    void analyze(ProgramNode& program);

private:
    CompilerContext& context;

    void validateDeclaredTypes();
    void ensureAnyRootType();
    void validateParentTypes() const;
    bool hasInheritanceCycle(const std::string& className, std::set<std::string>& visiting, std::set<std::string>& done) const;
    void validateInheritanceCycles() const;
    bool isKnownType(const std::string& type) const;
    bool isKnownTypeInScope(
        const std::string& type, const std::vector<std::string>& typeParameters) const;
    bool isKnownTypeInScope(
        const std::string& type,
        const std::vector<CompilerContext::TypeParameterInfo>& typeParameters) const;
};
