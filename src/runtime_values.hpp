#pragma once

#include <string>

namespace RuntimeValues {

constexpr long long kTaggedShift = 1;
constexpr long long kTaggedMask = 1;
constexpr long long kTaggedZero = 1;
constexpr long long kTaggedFalse = kTaggedZero;
constexpr long long kTaggedTrue = 3;
constexpr long long kNullSlot = 0;
constexpr long long kBoxedIntTag = 11;
constexpr long long kBoxedLongTag = 12;
constexpr long long kBoxedFloatTag = 13;
constexpr long long kBoxedDoubleTag = 14;
constexpr long long kBoxedBoolTag = 15;
constexpr long long kBoxedCharTag = 16;
constexpr long long kBoxedUnitTag = 17;
constexpr long long kObjectHeaderSlots = 1;
constexpr long long kSlotSizeBytes = 8;

constexpr long long kMaxTaggedIntPayload = (1LL << 62) - 1;
constexpr long long kMinTaggedIntPayload = -(1LL << 62);

constexpr long long encodeTaggedInteger(long long value) {
    return (value << kTaggedShift) | kTaggedMask;
}

inline std::string asmEncodeTaggedInteger(const std::string& reg) {
    return "    shl " + reg + ", " + std::to_string(kTaggedShift) + "\n"
        + "    or " + reg + ", " + std::to_string(kTaggedMask) + "\n";
}

inline std::string asmDecodeTaggedInteger(const std::string& reg) {
    return "    sar " + reg + ", " + std::to_string(kTaggedShift) + "\n";
}

inline std::string asmMoveTaggedFalse(const std::string& reg) {
    return "    mov " + reg + ", " + std::to_string(kTaggedFalse) + "\n";
}

inline std::string asmMoveTaggedTrue(const std::string& reg) {
    return "    mov " + reg + ", " + std::to_string(kTaggedTrue) + "\n";
}

inline std::string asmCompareTaggedFalse(const std::string& reg) {
    return "    cmp " + reg + ", " + std::to_string(kTaggedFalse) + "\n";
}

} // namespace RuntimeValues
