#pragma once

#include <iosfwd>
#include <cstdint>

namespace RuntimeASM {
void emit(std::ostream& out, std::uint64_t heapCapacityBytes = 8388608ULL);
}
