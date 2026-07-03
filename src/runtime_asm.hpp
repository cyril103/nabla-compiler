#pragma once

#include <iosfwd>
#include <cstdint>

namespace RuntimeASM {
void emitData(std::ostream& out, std::uint64_t heapCapacityBytes = 8388608ULL);
void emitText(std::ostream& out, bool mainAcceptsArguments = false);
void emit(
    std::ostream& out,
    std::uint64_t heapCapacityBytes = 8388608ULL,
    bool mainAcceptsArguments = false);
}
