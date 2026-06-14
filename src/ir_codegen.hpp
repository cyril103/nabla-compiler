#pragma once

#include "ir.hpp"
#include <iosfwd>

class IRCodeGenerator {
public:
    void generateASM(const IRProgram& program, std::ostream& out) const;
};
