#pragma once

#include "compiler_context.hpp"
#include "ir.hpp"
#include <iosfwd>

class IRCodeGenerator {
public:
    void generateASM(const IRProgram& program, const CompilerContext& context, std::ostream& out) const;
};
