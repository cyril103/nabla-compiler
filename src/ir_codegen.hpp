#pragma once

#include "compiler_context.hpp"
#include "ir.hpp"
#include <iosfwd>
#include <cstdint>

class IRCodeGenerator {
public:
    explicit IRCodeGenerator(std::uint64_t heapCapacityBytes = 8388608ULL);
    void generateASM(const IRProgram& program, const CompilerContext& context, std::ostream& out) const;

private:
    std::uint64_t heapCapacityBytes;
};
