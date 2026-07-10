#include "ir_codegen.hpp"
#include "compiler_error.hpp"
#include "runtime_asm.hpp"
#include "runtime_values.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::vector<std::string> GLOBAL_ARGUMENT_REGISTERS = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
const std::vector<std::string> METHOD_ARGUMENT_REGISTERS = {"rsi", "rdx", "rcx", "r8", "r9"};

[[noreturn]] void codegenError(const std::string& message) {
    throw CompilerError(ErrorKind::Codegen, {"<ir>", 1, 1}, message);
}

std::string asmSymbolName(const std::string& name) {
    std::string result;
    result.reserve(name.size() + 10);
    result = "nabla_sym_";
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            result += static_cast<char>(c);
        } else {
            result += "_";
            result += hex[(c >> 4) & 0x0f];
            result += hex[c & 0x0f];
        }
    }
    return result;
}

std::string asmFunctionName(const std::string& name) {
    if (name == "main") return "main";
    return asmSymbolName(name);
}

bool mainAcceptsCommandLineArguments(const CompilerContext& context) {
    auto it = context.functions.find("main");
    if (it == context.functions.end()) return false;
    return it->second.parameters.size() == 1 &&
           (it->second.parameters[0].type == formatParameterizedType("ArrayObject", {"String"}) ||
            it->second.parameters[0].type ==
                formatParameterizedType("collections.object_array.ArrayObject", {"String"}));
}

std::string singletonObjectLabel(const std::string& className) {
    return asmSymbolName("singleton." + className);
}

bool isGcReferenceCapableType(const CompilerContext& context, const std::string& type) {
    if (type == "Any" || type == "AnyVal" || type == "AnyRef") return true;
    if (isBuiltinReferenceType(type)) return true;
    if (isBuiltinValueType(type)) return false;
    if (auto parameterized = parameterizedTypeFromName(type)) {
        if (parameterized->first == "ArrayObject" || parameterized->first == "ObjectArray") return true;
        return context.classes.count(parameterized->first) > 0;
    }
    return context.classes.count(genericBaseName(type)) > 0;
}

long long classIdForContext(const CompilerContext& context, const std::string& className) {
    const std::string classBaseName = genericBaseName(className);
    long long nextClassId = 1000;
    for (const auto& [candidateName, _] : context.classes) {
        if (candidateName == classBaseName) return nextClassId;
        ++nextClassId;
    }
    return 0;
}

std::string asmLabelName(const std::string& functionName, const std::string& label) {
    return "L_ir_" + asmFunctionName(functionName) + "_" + asmFunctionName(label);
}

std::pair<std::string, std::string> splitQualifiedMember(const std::string& name) {
    int bracketDepth = 0;
    for (size_t index = name.size(); index > 0; --index) {
        const char c = name[index - 1];
        if (c == ']') {
            ++bracketDepth;
        } else if (c == '[') {
            if (bracketDepth > 0) --bracketDepth;
        } else if (c == '.' && bracketDepth == 0) {
            return {name.substr(0, index - 1), name.substr(index)};
        }
    }
    codegenError("membre IR non qualifie: " + name);
}

std::string methodFunctionName(const std::string& className, const std::string& methodName) {
    return "method." + className + "." + methodName;
}

std::string methodFunctionName(const std::string& qualifiedMember) {
    auto [className, methodName] = splitQualifiedMember(qualifiedMember);
    return methodFunctionName(className, methodName);
}

std::string methodSourceName(const std::string& resolvedMethodName) {
    size_t generic = resolvedMethodName.find('[');
    size_t overload = resolvedMethodName.find('$');
    size_t end = std::min(
        generic == std::string::npos ? resolvedMethodName.size() : generic,
        overload == std::string::npos ? resolvedMethodName.size() : overload);
    return resolvedMethodName.substr(0, end);
}

std::string methodSpecializationBaseName(const std::string& resolvedMethodName) {
    size_t generic = resolvedMethodName.find('[');
    if (generic == std::string::npos) return resolvedMethodName;
    return resolvedMethodName.substr(0, generic);
}

bool vtableTypeEquivalent(const std::string& left, const std::string& right) {
    if (left == right) return true;
    auto leftFn = functionTypeFromName(left);
    auto rightFn = functionTypeFromName(right);
    if (leftFn && rightFn && leftFn->parameterTypes.size() == rightFn->parameterTypes.size()) {
        for (size_t i = 0; i < leftFn->parameterTypes.size(); ++i) {
            if (!vtableTypeEquivalent(leftFn->parameterTypes[i], rightFn->parameterTypes[i])) return false;
        }
        return vtableTypeEquivalent(leftFn->returnType, rightFn->returnType);
    }
    auto leftParam = parameterizedTypeFromName(left);
    auto rightParam = parameterizedTypeFromName(right);
    if (leftParam && rightParam && leftParam->second.size() == rightParam->second.size()) {
        const std::string leftBase = unqualifiedSourceName(leftParam->first);
        const std::string rightBase = unqualifiedSourceName(rightParam->first);
        const bool compatibleBase =
            leftBase == rightBase ||
            (leftBase == "Array" && rightBase == "ArrayObject") ||
            (leftBase == "ArrayObject" && rightBase == "Array");
        if (!compatibleBase) return false;
        for (size_t i = 0; i < leftParam->second.size(); ++i) {
            if (!vtableTypeEquivalent(leftParam->second[i], rightParam->second[i])) return false;
        }
        return true;
    }
    return false;
}

bool vtableSlotParametersMatch(
    const CompilerContext::FunctionSignature& candidate,
    const CompilerContext::FunctionSignature& slot) {
    if (candidate.parameters.size() != slot.parameters.size()) return false;
    std::set<std::string> candidateTypeParameters(
        candidate.typeParameters.begin(), candidate.typeParameters.end());
    std::set<std::string> slotTypeParameters(slot.typeParameters.begin(), slot.typeParameters.end());
    for (size_t i = 0; i < candidate.parameters.size(); ++i) {
        const std::string& candidateType = candidate.parameters[i].type;
        const std::string& slotType = slot.parameters[i].type;
        if (vtableTypeEquivalent(candidateType, slotType)) continue;
        if (candidateTypeParameters.count(candidateType) && slotTypeParameters.count(slotType)) continue;
        return false;
    }
    return true;
}

long long boxedInt(const std::string& value) {
    long long parsed = 0;
    try {
        parsed = std::stoll(value);
    } catch (...) {
        codegenError("entier constant invalide ou trop grand: " + value);
    }

    if (parsed > RuntimeValues::kMaxTaggedIntPayload || parsed < RuntimeValues::kMinTaggedIntPayload) {
        codegenError("entier constant hors de la plage Int: " + value);
    }
    return RuntimeValues::encodeTaggedInteger(parsed);
}

std::string asmDataBytes(const std::string& value) {
    if (value.empty()) return "";
    std::ostringstream out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) out << ", ";
        out << static_cast<int>(static_cast<unsigned char>(value[i]));
    }
    return out.str();
}

std::string asmCommentText(const std::string& value) {
    std::ostringstream out;
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (std::isprint(c)) {
                    out << static_cast<char>(c);
                } else {
                    out << "\\x" << hex[(c >> 4) & 0xf] << hex[c & 0xf];
                }
                break;
        }
    }
    return out.str();
}

std::string normalizeFloatConstant(const std::string& value) {
    if (value.find('.') != std::string::npos) return value;
    if (value.find('e') != std::string::npos || value.find('E') != std::string::npos) return value;
    return value + ".0";
}

std::string asmSymbolPart(const std::string& value) {
    return asmSymbolName(value);
}

struct PhiInfo {
    std::string result;
    std::string type;
    std::vector<std::string> operands;
};

struct CallingConvention {
    std::vector<std::string> globalArgumentRegisters = GLOBAL_ARGUMENT_REGISTERS;
    std::vector<std::string> methodArgumentRegisters = METHOD_ARGUMENT_REGISTERS;
};

struct DynamicDispatchTarget {
    std::string runtimeClassName;
    std::string targetOwnerName;
};

class StackFrame {
public:
    void addSlot(const std::string& name) {
        if (slotOffsets.count(name)) return;
        slotOrder.push_back(name);
        slotOffsets[name] = static_cast<int>(slotOrder.size()) * 8;
    }

    bool empty() const {
        return slotOffsets.empty();
    }

    size_t byteSize() const {
        return slotOffsets.size() * 8;
    }

    int offsetFor(const std::string& name) const {
        auto slot = slotOffsets.find(name);
        if (slot == slotOffsets.end()) codegenError("valeur IR inconnue: " + name);
        return slot->second;
    }

    const std::vector<std::string>& slotsInFrameOrder() const {
        return slotOrder;
    }

private:
    std::map<std::string, int> slotOffsets;
    std::vector<std::string> slotOrder;
};

class FunctionEmitter {
public:
    FunctionEmitter(
        const IRFunction& function, const CompilerContext& context, std::ostream& out,
        const std::map<std::string, int>& methodOffsets)
        : function(function), context(context), methodOffsets(methodOffsets), out(out) {
        collectPhiLabels();
        collectSlots();
    }

    void emit() {
        if (function.parameters.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("le backend IR limite les fonctions globales a 6 parametres");
        }

        out << "section .text\n";
        out << asmFunctionName(function.name) << ":\n";
        out << "    push rbp\n    mov rbp, rsp\n";
        if (!frame.empty()) out << "    sub rsp, " << frame.byteSize() << "\n";
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            storeRegister("%" + function.parameters[i].name, callingConvention.globalArgumentRegisters[i]);
        }
        out << tailEntryLabel() << ":\n";
        for (size_t i = 0; i < function.instructions.size();) {
            if (isTailRecursiveCallAt(i)) {
                emitTailRecursiveCall(function.instructions[i]);
                i += 2;
                continue;
            }
            emitInstruction(function.instructions[i]);
            ++i;
        }
        out << "\n";
    }

    void emitGcFrameMap() const {
        const std::vector<RootSlot> roots = gcReferenceRootSlots();

        out << "    align 8\n";
        out << "nabla_gc_frame_roots_" << asmFunctionName(function.name) << ": dq " << roots.size();
        for (const auto& root : roots) {
            out << ", " << root.offset;
        }
        out << "\n";
        for (const auto& root : roots) {
            out << "    ; gc root [rbp - " << root.offset << "] " << root.name
                << ": " << root.type << "\n";
        }
    }

    void emitGcClosureMaps() const {
        for (const auto& instruction : function.instructions) {
            if (instruction.opcode != IROpcode::FunctionReference || instruction.operands.empty()) continue;

            struct CaptureSlot {
                std::string name;
                std::string type;
                int offset;
            };
            std::vector<CaptureSlot> captures;
            for (size_t i = 0; i < instruction.operands.size(); ++i) {
                const std::string& operand = instruction.operands[i];
                auto typeIt = valueTypes.find(operand);
                if (typeIt == valueTypes.end()) continue;
                if (!isGcReferenceCapableType(context, typeIt->second)) continue;
                captures.push_back({operand, typeIt->second, static_cast<int>(16 + i * 8)});
            }

            out << "    align 8\n";
            out << "nabla_gc_closure_layout_" << asmFunctionName(function.name) << "_"
                << asmSymbolPart(instruction.result) << ": dq " << captures.size();
            for (const auto& capture : captures) {
                out << ", " << capture.offset;
            }
            out << "\n";
            for (const auto& capture : captures) {
                out << "    ; gc capture [closure + " << capture.offset << "] " << capture.name
                    << ": " << capture.type << "\n";
            }
        }
    }

    void emitGcAllocationCallMaps() const {
        std::set<std::string> availableSlots;
        for (const auto& parameter : function.parameters) {
            availableSlots.insert("%" + parameter.name);
        }

        size_t callIndex = 0;
        for (const auto& instruction : function.instructions) {
            const auto kind = allocationCallKind(instruction);
            if (!kind) {
                markGcAvailableAfterInstruction(instruction, availableSlots);
                continue;
            }

            const std::vector<RootSlot> roots = gcReferenceRootSlotsFor(availableSlots);
            out << "    align 8\n";
            out << "nabla_gc_alloc_call_" << asmFunctionName(function.name) << "_"
                << callIndex << ": dq " << roots.size();
            for (const auto& root : roots) {
                out << ", " << root.offset;
            }
            out << "\n";
            out << "    ; gc alloc call " << callIndex << " " << *kind
                << " result " << instruction.result;
            if (!instruction.operation.empty()) out << " op " << instruction.operation;
            out << "\n";
            for (const auto& root : roots) {
                out << "    ; gc alloc root [rbp - " << root.offset << "] " << root.name
                    << ": " << root.type << "\n";
            }
            ++callIndex;
            markGcAvailableAfterInstruction(instruction, availableSlots);
        }

        out << "    align 8\n";
        out << "nabla_gc_alloc_calls_" << asmFunctionName(function.name) << ": dq " << callIndex;
        for (size_t i = 0; i < callIndex; ++i) {
            out << ", nabla_gc_alloc_call_" << asmFunctionName(function.name) << "_" << i;
        }
        out << "\n";

        out << "    align 8\n";
        out << "nabla_gc_alloc_safepoints_" << asmFunctionName(function.name) << ": dq "
            << callIndex;
        for (size_t i = 0; i < callIndex; ++i) {
            out << ", nabla_gc_alloc_return_" << asmFunctionName(function.name) << "_" << i
                << ", nabla_gc_alloc_call_" << asmFunctionName(function.name) << "_" << i;
        }
        out << "\n";
        for (size_t i = 0; i < callIndex; ++i) {
            out << "    ; gc alloc safepoint " << i << " return nabla_gc_alloc_return_"
                << asmFunctionName(function.name) << "_" << i << " map nabla_gc_alloc_call_"
                << asmFunctionName(function.name) << "_" << i << " exact-frame-offsets-consumed\n";
        }
    }

private:
    const IRFunction& function;
    const CompilerContext& context;
    const std::map<std::string, int>& methodOffsets;
    std::ostream& out;
    CallingConvention callingConvention;
    StackFrame frame;
    std::map<std::string, std::vector<PhiInfo>> phiLabels;
    std::map<std::string, std::string> valueTypes;
    std::map<std::string, int> emittedIncomingJumps;
    int nextIndirectCallId = 0;
    size_t nextGcAllocationCallIndex = 0;

    struct RootSlot {
        std::string name;
        std::string type;
        int offset;
    };

    std::optional<RootSlot> gcReferenceRootSlot(const std::string& slotName) const {
        auto typeIt = valueTypes.find(slotName);
        if (typeIt == valueTypes.end()) return std::nullopt;
        if (!isGcReferenceCapableType(context, typeIt->second)) return std::nullopt;
        return RootSlot{slotName, typeIt->second, frame.offsetFor(slotName)};
    }

    std::vector<RootSlot> gcReferenceRootSlots() const {
        std::vector<RootSlot> roots;
        for (const auto& slotName : frame.slotsInFrameOrder()) {
            auto root = gcReferenceRootSlot(slotName);
            if (!root) continue;
            roots.push_back(*root);
        }
        return roots;
    }

    std::vector<RootSlot> gcReferenceRootSlotsFor(const std::set<std::string>& availableSlots) const {
        std::vector<RootSlot> roots;
        for (const auto& slotName : frame.slotsInFrameOrder()) {
            if (!availableSlots.count(slotName)) continue;
            auto root = gcReferenceRootSlot(slotName);
            if (!root) continue;
            roots.push_back(*root);
        }
        return roots;
    }

    void markGcAvailableAfterInstruction(
        const IRInstruction& instruction, std::set<std::string>& availableSlots) const {
        if (!instruction.result.empty() && gcReferenceRootSlot(instruction.result)) {
            availableSlots.insert(instruction.result);
        }
        if (instruction.opcode == IROpcode::Store && gcReferenceRootSlot(instruction.operands[0])) {
            availableSlots.insert(instruction.operands[0]);
        }
    }

    static std::optional<std::string> allocationCallKind(const IRInstruction& instruction) {
        switch (instruction.opcode) {
            case IROpcode::FunctionReference:
                return std::string("closure");
            case IROpcode::NewObject:
                return std::string("object");
            case IROpcode::NewIntArray:
                return std::string("IntArray");
            case IROpcode::NewLongArray:
                return std::string("LongArray");
            case IROpcode::NewFloatArray:
                return std::string("FloatArray");
            case IROpcode::NewDoubleArray:
                return std::string("DoubleArray");
            case IROpcode::NewBoolArray:
                return std::string("BoolArray");
            case IROpcode::NewObjectArray:
                return std::string("ObjectArray");
            case IROpcode::MethodCall: {
                auto [className, methodName] = splitQualifiedMember(instruction.operation);
                if (methodName == "box") return std::string("boxed-" + className);
                return std::nullopt;
            }
            default:
                return std::nullopt;
        }
    }

    size_t emitGcAllocationSafepointComment(const IRInstruction& instruction) {
        const auto kind = allocationCallKind(instruction);
        if (!kind) codegenError("commentaire safepoint GC demande pour une instruction non allocante");

        const size_t callIndex = nextGcAllocationCallIndex;
        out << "    ; gc alloc safepoint map nabla_gc_alloc_call_"
            << asmFunctionName(function.name) << "_" << callIndex
            << " kind " << *kind << " result " << instruction.result;
        if (!instruction.operation.empty()) out << " op " << instruction.operation;
        out << " exact-frame-offsets-consumed\n";
        ++nextGcAllocationCallIndex;
        return callIndex;
    }

    void emitGcAllocationReturnLabel(size_t callIndex) {
        out << "nabla_gc_alloc_return_" << asmFunctionName(function.name) << "_"
            << callIndex << ":\n";
    }

    std::string tailEntryLabel() const {
        return "L_tail_entry_" + asmSymbolPart(function.name);
    }

    void collectPhiLabels() {
        for (size_t i = 0; i < function.instructions.size(); ++i) {
            const auto& instruction = function.instructions[i];
            if (instruction.opcode != IROpcode::Label) continue;

            std::vector<PhiInfo> phis;
            size_t next = i + 1;
            while (next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Phi) {
                const auto& phi = function.instructions[next];
                phis.push_back({phi.result, phi.type, phi.operands});
                ++next;
            }
            if (!phis.empty()) phiLabels[instruction.operands[0]] = phis;
        }
    }

    void collectSlots() {
        for (const auto& parameter : function.parameters) {
            const std::string name = "%" + parameter.name;
            frame.addSlot(name);
            valueTypes[name] = parameter.type;
        }
        for (const auto& instruction : function.instructions) {
            if (!instruction.result.empty()) {
                frame.addSlot(instruction.result);
                valueTypes[instruction.result] = instruction.type;
            }
            if (instruction.opcode == IROpcode::Store) {
                frame.addSlot(instruction.operands[0]);
                valueTypes[instruction.operands[0]] = instruction.type;
            }
        }
    }

    void loadValue(const std::string& name, const std::string& reg) const {
        out << "    mov " << reg << ", [rbp - " << frame.offsetFor(name) << "]\n";
    }

    void storeRegister(const std::string& name, const std::string& reg) const {
        out << "    mov [rbp - " << frame.offsetFor(name) << "], " << reg << "\n";
    }

    std::string typeOf(const std::string& name) const {
        auto type = valueTypes.find(name);
        if (type == valueTypes.end() || type->second.empty()) {
            codegenError("type IR inconnu pour la valeur '" + name + "'");
        }
        return type->second;
    }

    void loadNumericOperandToXmm(const std::string& operand, const std::string& sourceType,
                                 const std::string& targetType, const std::string& xmm) {
        loadValue(operand, "rax");
        if (targetType == "Double") {
            if (sourceType == "Double") {
                out << "    movq " << xmm << ", rax\n";
            } else if (sourceType == "Float") {
                out << "    movd " << xmm << ", eax\n";
                out << "    cvtss2sd " << xmm << ", " << xmm << "\n";
            } else {
                out << "    sar rax, 1\n";
                out << "    cvtsi2sd " << xmm << ", rax\n";
            }
            return;
        }
        if (targetType == "Float") {
            if (sourceType == "Float") {
                out << "    movd " << xmm << ", eax\n";
            } else if (sourceType == "Double") {
                out << "    movq " << xmm << ", rax\n";
                out << "    cvtsd2ss " << xmm << ", " << xmm << "\n";
            } else {
                out << "    sar rax, 1\n";
                out << "    cvtsi2ss " << xmm << ", rax\n";
            }
            return;
        }
        codegenError("conversion numérique IR non supportée vers " + targetType);
    }

    void emitInstruction(const IRInstruction& instruction) {
        switch (instruction.opcode) {
            case IROpcode::Constant:
                emitConstant(instruction);
                break;
            case IROpcode::StringLiteral:
                emitStringLiteral(instruction);
                break;
            case IROpcode::Binary:
                emitBinary(instruction);
                break;
            case IROpcode::Call:
                emitCall(instruction);
                break;
            case IROpcode::FunctionReference:
                emitFunctionReference(instruction);
                break;
            case IROpcode::ClosureLoad:
                emitClosureLoad(instruction);
                break;
            case IROpcode::IndirectCall:
                emitIndirectCall(instruction);
                break;
            case IROpcode::SingletonObjectRef:
                emitSingletonObjectRef(instruction);
                break;
            case IROpcode::MethodCall:
                emitMethodCall(instruction, true);
                break;
            case IROpcode::StaticMethodCall:
                emitMethodCall(instruction, false);
                break;
            case IROpcode::NewObject:
                emitNewObject(instruction);
                break;
            case IROpcode::NewIntArray:
                emitNewIntArray(instruction);
                break;
            case IROpcode::IntArrayLength:
                emitIntArrayLength(instruction);
                break;
            case IROpcode::IntArrayGet:
                emitIntArrayGet(instruction);
                break;
            case IROpcode::IntArraySet:
                emitIntArraySet(instruction);
                break;
            case IROpcode::NewLongArray:
                emitNewLongArray(instruction);
                break;
            case IROpcode::LongArrayLength:
                emitLongArrayLength(instruction);
                break;
            case IROpcode::LongArrayGet:
                emitLongArrayGet(instruction);
                break;
            case IROpcode::LongArraySet:
                emitLongArraySet(instruction);
                break;
            case IROpcode::NewFloatArray:
                emitNewFloatArray(instruction);
                break;
            case IROpcode::FloatArrayLength:
                emitFloatArrayLength(instruction);
                break;
            case IROpcode::FloatArrayGet:
                emitFloatArrayGet(instruction);
                break;
            case IROpcode::FloatArraySet:
                emitFloatArraySet(instruction);
                break;
            case IROpcode::NewDoubleArray:
                emitNewDoubleArray(instruction);
                break;
            case IROpcode::DoubleArrayLength:
                emitDoubleArrayLength(instruction);
                break;
            case IROpcode::DoubleArrayGet:
                emitDoubleArrayGet(instruction);
                break;
            case IROpcode::DoubleArraySet:
                emitDoubleArraySet(instruction);
                break;
            case IROpcode::NewBoolArray:
                emitNewBoolArray(instruction);
                break;
            case IROpcode::BoolArrayLength:
                emitBoolArrayLength(instruction);
                break;
            case IROpcode::BoolArrayGet:
                emitBoolArrayGet(instruction);
                break;
            case IROpcode::BoolArraySet:
                emitBoolArraySet(instruction);
                break;
            case IROpcode::NewObjectArray:
                emitNewObjectArray(instruction);
                break;
            case IROpcode::ObjectArrayLength:
                emitObjectArrayLength(instruction);
                break;
            case IROpcode::ObjectArrayGet:
                emitObjectArrayGet(instruction);
                break;
            case IROpcode::ObjectArraySet:
                emitObjectArraySet(instruction);
                break;
            case IROpcode::FieldLoad:
                emitFieldLoad(instruction);
                break;
            case IROpcode::FieldStore:
                emitFieldStore(instruction);
                break;
            case IROpcode::ClassIs:
                emitClassIs(instruction);
                break;
            case IROpcode::Load:
                loadValue(instruction.operands[0], "rax");
                storeRegister(instruction.result, "rax");
                break;
            case IROpcode::Store:
                loadValue(instruction.operands[1], "rax");
                storeRegister(instruction.operands[0], "rax");
                break;
            case IROpcode::Return:
                loadValue(instruction.operands[0], "rax");
                out << "    mov rsp, rbp\n    pop rbp\n    ret\n";
                break;
            case IROpcode::Label:
                out << asmLabelName(function.name, instruction.operands[0]) << ":\n";
                break;
            case IROpcode::BranchIfFalse:
                loadValue(instruction.operands[0], "rax");
                out << RuntimeValues::asmCompareTaggedFalse("rax");
                out << "    je " << asmLabelName(function.name, instruction.operands[1]) << "\n";
                break;
            case IROpcode::Jump:
                emitPhiTransfers(instruction.operands[0]);
                out << "    jmp " << asmLabelName(function.name, instruction.operands[0]) << "\n";
                break;
            case IROpcode::Phi:
                break;
        }
    }

    bool isTailRecursiveCallAt(size_t index) const {
        if (index + 1 >= function.instructions.size()) return false;
        const IRInstruction& call = function.instructions[index];
        if (call.opcode != IROpcode::Call || call.operation != function.name) return false;
        if (call.operands.size() != function.parameters.size()) return false;

        const IRInstruction& next = function.instructions[index + 1];
        if (next.opcode == IROpcode::Return) {
            return next.operands.size() == 1 && next.operands[0] == call.result;
        }
        if (next.opcode != IROpcode::Jump || next.operands.size() != 1) return false;
        return jumpTargetReturnsCallResult(next.operands[0], call.result);
    }

    bool jumpTargetReturnsCallResult(const std::string& targetLabel, const std::string& callResult) const {
        for (size_t i = 0; i < function.instructions.size(); ++i) {
            const IRInstruction& label = function.instructions[i];
            if (label.opcode != IROpcode::Label || label.operands.empty() ||
                label.operands[0] != targetLabel) {
                continue;
            }

            size_t next = i + 1;
            std::string returningPhi;
            while (next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Phi) {
                const IRInstruction& phi = function.instructions[next];
                if (std::find(phi.operands.begin(), phi.operands.end(), callResult) != phi.operands.end()) {
                    returningPhi = phi.result;
                }
                ++next;
            }
            return !returningPhi.empty() &&
                   next < function.instructions.size() &&
                   function.instructions[next].opcode == IROpcode::Return &&
                   function.instructions[next].operands.size() == 1 &&
                   function.instructions[next].operands[0] == returningPhi;
        }
        return false;
    }

    void emitTailRecursiveCall(const IRInstruction& instruction) {
        if (instruction.operands.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel tail-rec IR avec plus de 6 arguments");
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], callingConvention.globalArgumentRegisters[i]);
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            storeRegister("%" + function.parameters[i].name, callingConvention.globalArgumentRegisters[i]);
        }
        out << "    jmp " << tailEntryLabel() << "\n";
    }

    void emitPhiTransfers(const std::string& targetLabel) {
        auto phis = phiLabels.find(targetLabel);
        if (phis == phiLabels.end()) return;

        int incomingIndex = emittedIncomingJumps[targetLabel]++;
        for (const auto& phi : phis->second) {
            if (incomingIndex >= static_cast<int>(phi.operands.size())) {
                codegenError("phi IR sans operande pour le saut vers " + targetLabel);
            }
            loadValue(phi.operands[incomingIndex], "rax");
            storeRegister(phi.result, "rax");
        }
    }

    void emitBinary(const IRInstruction& instruction) {
        const std::string leftType = typeOf(instruction.operands[0]);
        const std::string rightType = typeOf(instruction.operands[1]);
        if (instruction.type == "Double" || leftType == "Double" || rightType == "Double") {
            emitDoubleBinary(instruction);
            return;
        }
        if (instruction.type == "Float" || leftType == "Float" || rightType == "Float") {
            emitFloatBinary(instruction);
            return;
        }
        loadValue(instruction.operands[0], "rax");
        loadValue(instruction.operands[1], "rbx");
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    add rax, rbx\n    sub rax, 1\n";
        } else if (op == "-") {
            out << "    sub rax, rbx\n    add rax, 1\n";
        } else if (op == "*") {
            out << "    sar rax, 1\n    sar rbx, 1\n    imul rax, rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "/") {
            out << "    sar rax, 1\n    sar rbx, 1\n    test rbx, rbx\n    je Runtime_division_by_zero\n    cqo\n    idiv rbx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "%") {
            out << "    sar rax, 1\n    sar rbx, 1\n    test rbx, rbx\n    je Runtime_division_by_zero\n    cqo\n    idiv rbx\n    mov rax, rdx\n    shl rax, 1\n    or rax, 1\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    sar rax, 1\n    sar rbx, 1\n    cmp rax, rbx\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setl al\n";
            else if (op == ">") out << "    setg al\n";
            else if (op == "<=") out << "    setle al\n";
            else out << "    setge al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitConstant(const IRInstruction& instruction) {
        if (instruction.type == "Double") {
            const std::string label = "nabla_double_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result);
            out << "    mov rax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        if (instruction.type == "Float") {
            const std::string label = "nabla_float_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result);
            out << "    xor rax, rax\n";
            out << "    mov eax, [" << label << "]\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        if (instruction.type == "Bool") {
            long long parsed = 0;
            try {
                parsed = std::stoll(instruction.operands[0]);
            } catch (...) {
                codegenError("constante Bool IR invalide: " + instruction.operands[0]);
            }
            if (parsed != RuntimeValues::kTaggedFalse && parsed != RuntimeValues::kTaggedTrue) {
                codegenError("constante Bool IR non taggee: " + instruction.operands[0]);
            }
            out << "    mov rax, " << parsed << "\n";
            storeRegister(instruction.result, "rax");
            return;
        }
        out << "    mov rax, " << boxedInt(instruction.operands[0]) << "\n";
        storeRegister(instruction.result, "rax");
    }

    void emitFloatBinary(const IRInstruction& instruction) {
        loadNumericOperandToXmm(instruction.operands[0], typeOf(instruction.operands[0]), "Float", "xmm0");
        loadNumericOperandToXmm(instruction.operands[1], typeOf(instruction.operands[1]), "Float", "xmm1");
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    addss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "-") {
            out << "    subss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "*") {
            out << "    mulss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "/") {
            out << "    divss xmm0, xmm1\n";
            out << "    xor rax, rax\n";
            out << "    movd eax, xmm0\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    ucomiss xmm0, xmm1\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setb al\n";
            else if (op == ">") out << "    seta al\n";
            else if (op == "<=") out << "    setbe al\n";
            else out << "    setae al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR Float inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitDoubleBinary(const IRInstruction& instruction) {
        loadNumericOperandToXmm(instruction.operands[0], typeOf(instruction.operands[0]), "Double", "xmm0");
        loadNumericOperandToXmm(instruction.operands[1], typeOf(instruction.operands[1]), "Double", "xmm1");
        const std::string& op = instruction.operation;
        if (op == "+") {
            out << "    addsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "-") {
            out << "    subsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "*") {
            out << "    mulsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "/") {
            out << "    divsd xmm0, xmm1\n";
            out << "    movq rax, xmm0\n";
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            out << "    ucomisd xmm0, xmm1\n";
            if (op == "==") out << "    sete al\n";
            else if (op == "!=") out << "    setne al\n";
            else if (op == "<") out << "    setb al\n";
            else if (op == ">") out << "    seta al\n";
            else if (op == "<=") out << "    setbe al\n";
            else out << "    setae al\n";
            out << "    movzx rax, al\n    shl rax, 1\n    or rax, 1\n";
        } else {
            codegenError("operation IR Double inconnue: " + op);
        }
        storeRegister(instruction.result, "rax");
    }

    void emitStringLiteral(const IRInstruction& instruction) {
        const std::string label = "nabla_string_" + asmSymbolPart(function.name) + "_" +
                                  asmSymbolPart(instruction.result);
        out << "    lea rax, [" << label << "_obj]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitCall(const IRInstruction& instruction) {
        if (instruction.operands.size() > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel IR avec plus de 6 arguments");
        }
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], callingConvention.globalArgumentRegisters[i]);
        }
        if (instruction.operation == "print") {
            out << "    call Runtime_print\n";
        } else if (instruction.operation == "readLine") {
            out << "    call Runtime_readLine\n";
        } else if (instruction.operation == "readFile") {
            out << "    call Runtime_readFile\n";
        } else if (instruction.operation == "writeFile") {
            out << "    call Runtime_writeFile\n";
        } else if (instruction.operation == "appendFile") {
            out << "    call Runtime_appendFile\n";
        } else if (instruction.operation == "fileExists") {
            out << "    call Runtime_fileExists\n";
        } else if (instruction.operation == "deleteFile") {
            out << "    call Runtime_deleteFile\n";
        } else if (instruction.operation == "renameFile") {
            out << "    call Runtime_renameFile\n";
        } else if (instruction.operation == "createDir") {
            out << "    call Runtime_createDir\n";
        } else if (instruction.operation == "parseInt") {
            out << "    call Runtime_stringToInt\n";
        } else if (instruction.operation == "parseFloat") {
            out << "    call Runtime_stringToFloat\n";
        } else if (instruction.operation == "parseDouble") {
            out << "    call Runtime_stringToDouble\n";
        } else if (instruction.operation == "timeSeed") {
            out << "    call Runtime_timeSeed\n";
        } else if (instruction.operation == "heapUsed") {
            out << "    call Runtime_heapUsed\n";
        } else if (instruction.operation == "heapCapacity") {
            out << "    call Runtime_heapCapacity\n";
        } else if (instruction.operation == "gcCollections") {
            out << "    call Runtime_gcCollections\n";
        } else if (instruction.operation == "gcLastFreedBytes") {
            out << "    call Runtime_gcLastFreedBytes\n";
        } else if (instruction.operation == "gcLastLargestFreeBlock") {
            out << "    call Runtime_gcLastLargestFreeBlock\n";
        } else if (instruction.operation == "gcLastMarkedBlocks") {
            out << "    call Runtime_gcLastMarkedBlocks\n";
        } else if (instruction.operation == "gcLastFreedBlocks") {
            out << "    call Runtime_gcLastFreedBlocks\n";
        } else if (instruction.operation == "gcLastStackWords") {
            out << "    call Runtime_gcLastStackWords\n";
        } else if (instruction.operation == "gcLastHeapWords") {
            out << "    call Runtime_gcLastHeapWords\n";
        } else if (instruction.operation == "gcLastStackCandidateWords") {
            out << "    call Runtime_gcLastStackCandidateWords\n";
        } else if (instruction.operation == "gcLastHeapCandidateWords") {
            out << "    call Runtime_gcLastHeapCandidateWords\n";
        } else if (instruction.operation == "gcLastStackInteriorCandidateWords") {
            out << "    call Runtime_gcLastStackInteriorCandidateWords\n";
        } else if (instruction.operation == "gcLastHeapInteriorCandidateWords") {
            out << "    call Runtime_gcLastHeapInteriorCandidateWords\n";
        } else if (instruction.operation == "gcLastAllocSafepointMapFound") {
            out << "    call Runtime_gcLastAllocSafepointMapFound\n";
        } else if (instruction.operation == "gcLastAllocSafepointMapMissed") {
            out << "    call Runtime_gcLastAllocSafepointMapMissed\n";
        } else if (instruction.operation == "gcLastAllocSafepointRootSlots") {
            out << "    call Runtime_gcLastAllocSafepointRootSlots\n";
        } else if (instruction.operation == "gcLastAllocSafepointRootBytes") {
            out << "    call Runtime_gcLastAllocSafepointRootBytes\n";
        } else if (instruction.operation == "heapAllocatedBytes") {
            out << "    call Runtime_heapAllocatedBytes\n";
        } else if (instruction.operation == "heapFreeBytes") {
            out << "    call Runtime_heapFreeBytes\n";
        } else if (instruction.operation == "heapFreeBlockCount") {
            out << "    call Runtime_heapFreeBlockCount\n";
        } else if (instruction.operation == "heapLargestFreeBlock") {
            out << "    call Runtime_heapLargestFreeBlock\n";
        } else if (instruction.operation == "panic") {
            out << "    call Runtime_panic\n";
        } else {
            out << "    call " << asmFunctionName(instruction.operation) << "\n";
        }
        storeRegister(instruction.result, "rax");
    }

    void emitSingletonObjectRef(const IRInstruction& instruction) {
        out << "    mov rax, " << singletonObjectLabel(instruction.operation) << "\n";
        storeRegister(instruction.result, "rax");
    }

    void emitFunctionReference(const IRInstruction& instruction) {
        const size_t objectSize = 16 + instruction.operands.size() * 8;
        out << "    mov rdi, " << objectSize << "\n";
        const size_t gcCallIndex = emitGcAllocationSafepointComment(instruction);
        out << "    call Runtime_alloc\n";
        emitGcAllocationReturnLabel(gcCallIndex);
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], " << asmFunctionName(instruction.operation) << "\n";
        out << "    mov qword [rbx + 8], " << (instruction.operands.empty() ? 0 : 1) << "\n";
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], "rax");
            out << "    mov [rbx + " << (16 + i * 8) << "], rax\n";
        }
        storeRegister(instruction.result, "rbx");
    }

    void emitClosureLoad(const IRInstruction& instruction) {
        if (instruction.operands.size() != 2) codegenError("lecture de closure IR invalide");
        loadValue(instruction.operands[0], "rax");
        const int captureIndex = std::stoi(instruction.operands[1]);
        out << "    mov rax, [rax + " << (16 + captureIndex * 8) << "]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitIndirectCall(const IRInstruction& instruction) {
        if (instruction.operands.empty()) codegenError("appel indirect IR sans callee");
        const size_t argumentCount = instruction.operands.size() - 1;
        if (argumentCount > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel indirect IR avec plus de 6 arguments");
        }
        if (argumentCount + 1 > callingConvention.globalArgumentRegisters.size()) {
            codegenError("appel indirect capturant IR avec plus de 5 arguments");
        }
        const int callId = nextIndirectCallId++;
        const std::string capturedLabel = ".L_closure_captured_" + asmSymbolPart(function.name) + "_" +
                                          std::to_string(callId);
        const std::string doneLabel = ".L_closure_done_" + asmSymbolPart(function.name) + "_" +
                                      std::to_string(callId);
        loadValue(instruction.operands[0], "r11");
        out << "    cmp qword [r11 + 8], 0\n";
        out << "    jne " << capturedLabel << "\n";
        for (size_t i = 0; i < argumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.globalArgumentRegisters[i]);
        }
        out << "    call qword [r11]\n";
        out << "    jmp " << doneLabel << "\n";
        out << capturedLabel << ":\n";
        out << "    mov rdi, r11\n";
        for (size_t i = 0; i < argumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.globalArgumentRegisters[i + 1]);
        }
        out << "    call qword [r11]\n";
        out << doneLabel << ":\n";
        storeRegister(instruction.result, "rax");
    }

    std::vector<int> fieldOffsetsByConstructorOrder(const std::string& className) const {
        const std::string layoutClassName = genericBaseName(className);
        auto classIt = context.classes.find(layoutClassName);
        if (classIt == context.classes.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        auto layoutIt = context.classLayouts.find(layoutClassName);
        if (layoutIt == context.classLayouts.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        std::vector<int> offsets;
        for (const auto& field : collectClassFieldsInHierarchyForLayout(context, layoutClassName)) {
            auto offsetIt = layoutIt->second.find(field.first);
            if (offsetIt == layoutIt->second.end()) {
                codegenError("offset de champ inconnu: " + layoutClassName + "." + field.first);
            }
            offsets.push_back(offsetIt->second);
        }
        return offsets;
    }

    int objectSizeFor(const std::string& className) const {
        std::vector<int> offsets = fieldOffsetsByConstructorOrder(className);
        if (offsets.empty()) return 8;
        return *std::max_element(offsets.begin(), offsets.end()) + 8;
    }

    long long classIdFor(const std::string& className) const {
        return classIdForContext(context, className);
    }

    bool canUseRuntimeClassDispatch(const std::string& className, const std::string& methodName) const {
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        const std::string classBaseName = genericBaseName(className);
        auto classIt = context.classes.find(classBaseName);
        if (classIt == context.classes.end()) return false;
        auto methodIt = classIt->second.methods.find(methodBaseName);
        if (methodIt != classIt->second.methods.end() &&
            !methodIt->second.typeParameters.empty() && !parameterizedMethod &&
            !classIt->second.isTrait) {
            return false;
        }
        return true;
    }

    bool unifyRuntimeGenericType(
        const std::string& patternType,
        const std::string& expectedType,
        const std::set<std::string>& typeParameters,
        std::map<std::string, std::string>& bindings) const {
        if (typeParameters.count(patternType)) {
            auto bindingIt = bindings.find(patternType);
            if (bindingIt == bindings.end()) {
                bindings[patternType] = expectedType;
                return true;
            }
            return bindingIt->second == expectedType;
        }

        auto patternParameterized = parameterizedTypeFromName(patternType);
        auto expectedParameterized = parameterizedTypeFromName(expectedType);
        if (!patternParameterized || !expectedParameterized) return patternType == expectedType;
        if (patternParameterized->first != expectedParameterized->first ||
            patternParameterized->second.size() != expectedParameterized->second.size()) {
            return false;
        }
        for (size_t i = 0; i < patternParameterized->second.size(); ++i) {
            if (!unifyRuntimeGenericType(
                    patternParameterized->second[i], expectedParameterized->second[i],
                    typeParameters, bindings)) {
                return false;
            }
        }
        return true;
    }

    bool runtimeGenericTypeCanReachExpected(
        const std::string& currentType,
        const std::string& expectedType,
        const std::set<std::string>& typeParameters,
        std::map<std::string, std::string> bindings,
        std::set<std::string>& visiting) const {
        auto directBindings = bindings;
        if (unifyRuntimeGenericType(currentType, expectedType, typeParameters, directBindings)) {
            return true;
        }

        const std::string currentBaseName = genericBaseName(currentType);
        if (visiting.count(currentBaseName)) return false;
        auto classIt = context.classes.find(currentBaseName);
        if (classIt == context.classes.end()) return false;

        std::map<std::string, std::string> classSubstitution;
        if (auto genericSubstitution = genericSubstitutionFor(context, currentType)) {
            classSubstitution = *genericSubstitution;
        }
        classSubstitution.insert(bindings.begin(), bindings.end());

        visiting.insert(currentBaseName);
        for (const auto& parentType : classIt->second.parentTypes) {
            const std::string symbolicParentType = substituteType(parentType, classSubstitution);
            if (runtimeGenericTypeCanReachExpected(
                    symbolicParentType, expectedType, typeParameters, bindings, visiting)) {
                visiting.erase(currentBaseName);
                return true;
            }
        }
        visiting.erase(currentBaseName);
        return false;
    }

    bool runtimeTypeCanReachBase(
        const std::string& currentType,
        const std::string& expectedBaseName,
        std::set<std::string>& visiting) const {
        const std::string currentBaseName = genericBaseName(currentType);
        if (currentBaseName == expectedBaseName) return true;
        if (visiting.count(currentBaseName)) return false;
        auto classIt = context.classes.find(currentBaseName);
        if (classIt == context.classes.end()) return false;

        std::map<std::string, std::string> classSubstitution;
        if (auto genericSubstitution = genericSubstitutionFor(context, currentType)) {
            classSubstitution = *genericSubstitution;
        }

        visiting.insert(currentBaseName);
        for (const auto& parentType : classIt->second.parentTypes) {
            const std::string parent = substituteType(parentType, classSubstitution);
            if (runtimeTypeCanReachBase(parent, expectedBaseName, visiting)) {
                visiting.erase(currentBaseName);
                return true;
            }
        }
        visiting.erase(currentBaseName);
        return false;
    }

    bool runtimeClassCanImplementStaticType(
        const std::string& runtimeClassName, const std::string& staticType) const {
        if (isTypeAssignable(context, runtimeClassName, staticType)) return true;

        auto runtimeClassIt = context.classes.find(runtimeClassName);
        if (runtimeClassIt == context.classes.end() ||
            runtimeClassIt->second.typeParameters.empty()) {
            return false;
        }

        std::set<std::string> typeParameters(
            runtimeClassIt->second.typeParameters.begin(),
            runtimeClassIt->second.typeParameters.end());
        const std::string symbolicRuntimeType =
            formatParameterizedType(runtimeClassName, runtimeClassIt->second.typeParameters);
        std::map<std::string, std::string> bindings;
        std::set<std::string> visiting;
        if (runtimeGenericTypeCanReachExpected(
                symbolicRuntimeType, staticType, typeParameters, bindings, visiting)) {
            return true;
        }

        if (parameterizedTypeFromName(staticType)) {
            return false;
        }

        auto staticClassIt = context.classes.find(genericBaseName(staticType));
        if (staticClassIt == context.classes.end() || !staticClassIt->second.isTrait) {
            return false;
        }
        visiting.clear();
        return runtimeTypeCanReachBase(symbolicRuntimeType, genericBaseName(staticType), visiting);
    }

    bool isAbstractTraitMethod(const std::string& className, const std::string& methodName) const {
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        auto classIt = context.classes.find(genericBaseName(className));
        if (classIt == context.classes.end() || !classIt->second.isTrait) return false;
        auto methodIt = classIt->second.methods.find(methodBaseName);
        return methodIt != classIt->second.methods.end() && methodIt->second.isAbstract;
    }

    void emitTraitAbstractDispatchFallback(const std::string& className, const std::string& methodName) {
        if (isAbstractTraitMethod(className, methodName)) {
            out << "    jmp Runtime_trait_dispatch_error\n";
            return;
        }
        out << "    call " << asmFunctionName(methodFunctionName(className, methodName)) << "\n";
    }

    std::vector<DynamicDispatchTarget> dynamicDispatchTargets(
        const std::string& className, const std::string& methodName) const {
        std::vector<DynamicDispatchTarget> targets;
        if (!canUseRuntimeClassDispatch(className, methodName)) return targets;

        const std::string staticClassName = genericBaseName(className);
        const auto parameterizedMethod = parameterizedTypeFromName(methodName);
        const std::string methodBaseName = parameterizedMethod ? parameterizedMethod->first : methodName;
        const bool staticOwnerIsTrait = [&]() {
            auto staticOwnerIt = context.classes.find(staticClassName);
            return staticOwnerIt != context.classes.end() && staticOwnerIt->second.isTrait;
        }();
        std::set<std::pair<std::string, std::string>> seen;
        for (const auto& [runtimeClassName, runtimeClass] : context.classes) {
            if (runtimeClassName == staticClassName) continue;
            if (runtimeClass.isTrait) continue;

            const std::string runtimeLookupType = runtimeClass.typeParameters.empty()
                ? runtimeClassName
                : formatParameterizedType(runtimeClassName, runtimeClass.typeParameters);
            auto methodLookup = resolveClassMethodInHierarchy(context, runtimeLookupType, methodBaseName);
            if (!methodLookup) continue;
            if (!runtimeClassCanImplementStaticType(runtimeClassName, className)) {
                continue;
            }
            const std::string targetOwnerName = genericBaseName(methodLookup->ownerClassName);
            if (targetOwnerName == staticClassName) continue;
            auto targetOwnerIt = context.classes.find(targetOwnerName);
            if (targetOwnerIt == context.classes.end()) continue;
            if (staticOwnerIsTrait && targetOwnerIt->second.isTrait) continue;
            auto targetMethodIt = targetOwnerIt->second.methods.find(methodBaseName);
            if (targetMethodIt == targetOwnerIt->second.methods.end()) continue;
            if (!targetMethodIt->second.typeParameters.empty() && !parameterizedMethod) continue;

            if (seen.insert({runtimeClassName, targetOwnerName}).second) {
                targets.push_back({runtimeClassName, targetOwnerName});
            }
        }
        return targets;
    }

    int fieldOffsetFor(const std::string& qualifiedField) const {
        auto [className, fieldName] = splitQualifiedMember(qualifiedField);
        const std::string layoutClassName = genericBaseName(className);
        auto layoutIt = context.classLayouts.find(layoutClassName);
        if (layoutIt == context.classLayouts.end()) {
            codegenError("layout de classe inconnu: " + className);
        }
        auto fieldIt = layoutIt->second.find(fieldName);
        if (fieldIt == layoutIt->second.end()) {
            codegenError("offset de champ inconnu: " + qualifiedField);
        }
        return fieldIt->second;
    }

    void emitNewObject(const IRInstruction& instruction) {
        std::vector<int> fieldOffsets = fieldOffsetsByConstructorOrder(instruction.operation);
        if (instruction.operands.size() != fieldOffsets.size()) {
            codegenError("nombre d'arguments invalide pour new " + instruction.operation);
        }

        out << "    mov rdi, " << objectSizeFor(instruction.operation) << "\n";
        const size_t gcCallIndex = emitGcAllocationSafepointComment(instruction);
        out << "    call Runtime_alloc\n";
        emitGcAllocationReturnLabel(gcCallIndex);
        out << "    mov rbx, rax\n";
        out << "    lea rax, [vtable_" << asmSymbolName(instruction.operation) << "]\n";
        out << "    mov [rbx], rax\n";
        for (size_t i = 0; i < instruction.operands.size(); ++i) {
            loadValue(instruction.operands[i], "rax");
            out << "    mov [rbx + " << fieldOffsets[i] << "], rax\n";
        }
        storeRegister(instruction.result, "rbx");
    }

    std::string nativeArrayDefaultValue(IROpcode opcode) const {
        switch (opcode) {
            case IROpcode::NewFloatArray:
            case IROpcode::NewDoubleArray:
            case IROpcode::NewObjectArray:
                return std::to_string(RuntimeValues::kNullSlot);
            case IROpcode::NewIntArray:
            case IROpcode::NewLongArray:
            case IROpcode::NewBoolArray:
                return std::to_string(RuntimeValues::kTaggedZero);
            default:
                codegenError("opcode IR de tableau natif inattendu");
        }
    }

    void emitNewNativeArray(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << RuntimeValues::asmCompareTaggedFalse("rax");
        out << "    jl Runtime_bounds_error\n";
        out << "    mov r8, rax\n";
        out << "    sar rax, 1\n";
        out << "    mov rcx, 2305843009213693949\n";
        out << "    cmp rax, rcx\n";
        out << "    ja Runtime_heap_overflow\n";
        out << "    mov rdi, rax\n";
        out << "    shl rdi, 3\n";
        out << "    add rdi, 16\n";
        out << "    jc Runtime_heap_overflow\n";
        const size_t gcCallIndex = emitGcAllocationSafepointComment(instruction);
        out << "    call Runtime_alloc\n";
        emitGcAllocationReturnLabel(gcCallIndex);
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], 0\n";
        out << "    mov [rbx + 8], r8\n";
        out << "    sar r8, 1\n";
        out << "    xor rcx, rcx\n";
        out << ".L_array_init_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        out << "    cmp rcx, r8\n";
        out << "    jge .L_array_init_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << "    mov qword [rbx + 16 + rcx * 8], " << nativeArrayDefaultValue(instruction.opcode) << "\n";
        out << "    inc rcx\n";
        out << "    jmp .L_array_init_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << ".L_array_init_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        storeRegister(instruction.result, "rbx");
    }

    void emitNewIntArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewLongArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewFloatArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewDoubleArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewBoolArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNewObjectArray(const IRInstruction& instruction) {
        emitNewNativeArray(instruction);
    }

    void emitNativeArrayLength(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    mov rax, [rax + 8]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitArrayBoundsCheck(const std::string& arrayReg, const std::string& indexReg) {
        out << "    sar " << indexReg << ", 1\n";
        out << "    cmp " << indexReg << ", 0\n";
        out << "    jl Runtime_bounds_error\n";
        out << "    mov rdx, [" << arrayReg << " + 8]\n";
        out << "    sar rdx, 1\n";
        out << "    cmp " << indexReg << ", rdx\n";
        out << "    jge Runtime_bounds_error\n";
    }

    void emitIntArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitLongArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitFloatArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitDoubleArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitBoolArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitObjectArrayLength(const IRInstruction& instruction) {
        emitNativeArrayLength(instruction);
    }

    void emitNativeArrayGet(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rcx");
        emitArrayBoundsCheck("rbx", "rcx");
        out << "    mov rax, [rbx + 16 + rcx * 8]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitIntArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitLongArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitFloatArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitDoubleArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitBoolArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitObjectArrayGet(const IRInstruction& instruction) {
        emitNativeArrayGet(instruction);
    }

    void emitNativeArraySet(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rcx");
        emitArrayBoundsCheck("rbx", "rcx");
        loadValue(instruction.operands[2], "rax");
        out << "    mov [rbx + 16 + rcx * 8], rax\n";
        out << RuntimeValues::asmMoveTaggedFalse("rax");
        storeRegister(instruction.result, "rax");
    }

    void emitIntArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitLongArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitFloatArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitDoubleArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitBoolArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitObjectArraySet(const IRInstruction& instruction) {
        emitNativeArraySet(instruction);
    }

    void emitFieldLoad(const IRInstruction& instruction) {
        loadValue(instruction.operands[0], "rax");
        out << "    mov rax, [rax + " << fieldOffsetFor(instruction.operation) << "]\n";
        storeRegister(instruction.result, "rax");
    }

    void emitFieldStore(const IRInstruction& instruction) {
        if (instruction.operands.size() != 2) codegenError("écriture de champ IR invalide");
        loadValue(instruction.operands[0], "rbx");
        loadValue(instruction.operands[1], "rax");
        out << "    mov [rbx + " << fieldOffsetFor(instruction.operation) << "], rax\n";
    }

    void emitClassIs(const IRInstruction& instruction) {
        if (instruction.operands.size() != 1) codegenError("test de classe IR invalide");
        loadValue(instruction.operands[0], "rax");
        out << "    cmp rax, 0\n";
        out << "    je .L_class_is_false_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << "    mov rbx, [rax]\n";
        out << "    lea r10, [vtable_" << asmSymbolName(instruction.operation) << "]\n";
        out << "    cmp rbx, r10\n";
        out << "    jne .L_class_is_false_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << RuntimeValues::asmMoveTaggedTrue("rax");
        out << "    jmp .L_class_is_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << "\n";
        out << ".L_class_is_false_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        out << RuntimeValues::asmMoveTaggedFalse("rax");
        out << ".L_class_is_done_" << asmSymbolPart(function.name) << "_" << asmSymbolPart(instruction.result) << ":\n";
        storeRegister(instruction.result, "rax");
    }

    void emitBoxedValue(const IRInstruction& instruction, long long tag) {
        if (instruction.operands.size() != 1) codegenError("boxing IR invalide");
        loadValue(instruction.operands[0], "r10");
        out << "    mov rdi, 16\n";
        const size_t gcCallIndex = emitGcAllocationSafepointComment(instruction);
        out << "    call Runtime_alloc\n";
        emitGcAllocationReturnLabel(gcCallIndex);
        out << "    mov qword [rax], " << tag << "\n";
        out << "    mov [rax + 8], r10\n";
        storeRegister(instruction.result, "rax");
    }

    void emitMethodCall(const IRInstruction& instruction, bool allowDynamicDispatch) {
        auto [className, methodName] = splitQualifiedMember(instruction.operation);
        if (instruction.operands.empty()) codegenError("appel de methode IR sans receveur");
        if (methodName == "box") {
            if (className == "Int") return emitBoxedValue(instruction, RuntimeValues::kBoxedIntTag);
            if (className == "Long") return emitBoxedValue(instruction, RuntimeValues::kBoxedLongTag);
            if (className == "Float") return emitBoxedValue(instruction, RuntimeValues::kBoxedFloatTag);
            if (className == "Double") return emitBoxedValue(instruction, RuntimeValues::kBoxedDoubleTag);
            if (className == "Bool") return emitBoxedValue(instruction, RuntimeValues::kBoxedBoolTag);
            if (className == "Char") return emitBoxedValue(instruction, RuntimeValues::kBoxedCharTag);
            if (className == "Unit") return emitBoxedValue(instruction, RuntimeValues::kBoxedUnitTag);
            codegenError("boxing non supporté pour " + className);
        }

        loadValue(instruction.operands[0], "rdi");
        const size_t methodArgumentCount = instruction.operands.size() - 1;
        if (methodArgumentCount > callingConvention.methodArgumentRegisters.size()) {
            codegenError("appel de methode IR avec plus de 5 arguments");
        }
        for (size_t i = 0; i < methodArgumentCount; ++i) {
            loadValue(instruction.operands[i + 1], callingConvention.methodArgumentRegisters[i]);
        }

        if ((className == "Int" || className == "Long") && methodName == "toString") {
            out << "    call Int_method_toString\n";
        } else if (className == "Float" && methodName == "toString") {
            out << "    call Float_method_toString\n";
        } else if (className == "Double" && methodName == "toString") {
            out << "    call Double_method_toString\n";
        } else if (className == "Bool" && methodName == "toString") {
            out << "    call Bool_method_toString\n";
        } else if (className == "Char" && methodName == "toString") {
            out << "    call Char_method_toString\n";
        } else if (className == "Any" &&
                   (methodName == "toString" || methodName == "hashCode" || methodName == "equals")) {
            const std::string fallbackMethod =
                methodName == "toString" ? "Any_toString" :
                methodName == "hashCode" ? "Any_hashCode" :
                "Any_equals";
            if (!allowDynamicDispatch) {
                out << "    call " << fallbackMethod << "\n";
            } else {
                const std::string dispatchPrefix =
                    ".L_dispatch_" + asmSymbolPart(function.name) + "_" +
                    asmSymbolPart(instruction.result);
                const std::string fallbackLabel = dispatchPrefix + "_fallback";
                const std::string doneLabel = dispatchPrefix + "_done";
                auto it = methodOffsets.find("Any." + methodName);
                if (it == methodOffsets.end()) {
                    codegenError("sélecteur de méthode inconnu pour le dispatch dynamique: " + methodName);
                }
                int offset = it->second;
                out << "    test rdi, 1\n";
                out << "    jnz " << fallbackLabel << "\n";
                out << "    mov r10, [rdi]\n";
                out << "    cmp r10, 100\n";
                out << "    jbe " << fallbackLabel << "\n";
                out << "    call [r10 + " << offset << "]\n";
                out << "    jmp " << doneLabel << "\n";
                out << fallbackLabel << ":\n";
                out << "    call " << fallbackMethod << "\n";
                out << doneLabel << ":\n";
            }
        } else if (className == "Char" && methodName == "toInt") {
            out << "    mov rax, rdi\n";
        } else if ((className == "Char" || className == "Int") && methodName == "toLong") {
            out << "    mov rax, rdi\n";
        } else if ((className == "Char" || className == "Int" || className == "Long") && methodName == "toFloat") {
            out << "    mov rax, rdi\n";
            out << "    sar rax, 1\n";
            out << "    cvtsi2ss xmm0, rax\n";
            out << "    movd eax, xmm0\n";
        } else if ((className == "Char" || className == "Int" || className == "Long") && methodName == "toDouble") {
            out << "    mov rax, rdi\n";
            out << "    sar rax, 1\n";
            out << "    cvtsi2sd xmm0, rax\n";
            out << "    movq rax, xmm0\n";
        } else if (className == "Float" && methodName == "toDouble") {
            out << "    movd xmm0, edi\n";
            out << "    cvtss2sd xmm0, xmm0\n";
            out << "    movq rax, xmm0\n";
        } else if (className == "String" && methodName == "length") {
            out << "    mov rax, [rdi + 8]\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "+") {
            out << "    call Runtime_stringConcat\n";
        } else if (className == "String" && methodName == "hashCode") {
            out << "    call Runtime_stringHashCode\n";
        } else if (className == "String" && methodName == "isEmpty") {
            out << "    cmp qword [rdi + 8], 0\n";
            out << "    sete al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "nonEmpty") {
            out << "    cmp qword [rdi + 8], 0\n";
            out << "    setne al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "charAt") {
            out << "    mov rax, rsi\n";
            out << "    sar rax, 1\n";
            out << "    cmp rax, 0\n";
            out << "    jl Runtime_bounds_error\n";
            out << "    cmp rax, [rdi + 8]\n";
            out << "    jge Runtime_bounds_error\n";
            out << "    mov rbx, [rdi + 16]\n";
            out << "    movzx rax, byte [rbx + rax]\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && (methodName == "==" || methodName == "!=")) {
            out << "    call Runtime_stringEquals\n";
            if (methodName == "!=") {
                out << "    xor rax, 2\n";
            }
        } else if (className == "String" && methodName == "startsWith") {
            out << "    call Runtime_stringStartsWith\n";
        } else if (className == "String" && methodName == "endsWith") {
            out << "    call Runtime_stringEndsWith\n";
        } else if (className == "String" && methodName == "indexOf") {
            out << "    call Runtime_stringIndexOf\n";
        } else if (className == "String" && methodName == "contains") {
            out << "    call Runtime_stringIndexOf\n";
            out << "    cmp rax, -1\n";
            out << "    setne al\n";
            out << "    movzx rax, al\n";
            out << "    shl rax, 1\n";
            out << "    or rax, 1\n";
        } else if (className == "String" && methodName == "toInt") {
            out << "    call Runtime_stringToInt\n";
        } else if (className == "String" && methodName == "toFloat") {
            out << "    call Runtime_stringToFloat\n";
        } else if (className == "String" && methodName == "toDouble") {
            out << "    call Runtime_stringToDouble\n";
        } else if (className == "String" && methodName == "toCharArray") {
            out << "    call Runtime_stringToCharArray\n";
            out << "    lea r10, [vtable_" << asmSymbolName("collections.object_array.ArrayObject[Char]") << "]\n";
            out << "    mov [rax], r10\n";
        } else if (className == "String" && methodName == "split") {
            out << "    call Runtime_stringSplit\n";
            out << "    lea r10, [vtable_" << asmSymbolName("collections.object_array.ArrayObject[String]") << "]\n";
            out << "    mov [rax], r10\n";
        } else if (className == "String" && methodName == "substring") {
            out << "    call Runtime_stringSubstring\n";
        } else if (className == "String" && methodName == "repeat") {
            out << "    call Runtime_stringRepeat\n";
        } else if (className == "String" && methodName == "trim") {
            out << "    call Runtime_stringTrim\n";
        } else if (allowDynamicDispatch) {
            auto it = methodOffsets.find(className + "." + methodName);
            if (it == methodOffsets.end()) {
                codegenError("sélecteur de méthode inconnu pour le dispatch dynamique: " + methodName);
            }
            int offset = it->second;
            out << "    mov r10, [rdi]\n";
            out << "    call [r10 + " << offset << "]\n";
        } else {
            out << "    call " << asmFunctionName(methodFunctionName(instruction.operation)) << "\n";
        }
        storeRegister(instruction.result, "rax");
    }
};

std::set<std::string> collectConcreteClassesToEmit(
    const IRProgram& program, const CompilerContext& context, bool mainUsesCommandLineArguments) {
    std::set<std::string> concreteClassesToEmit;
    if (mainUsesCommandLineArguments) {
        concreteClassesToEmit.insert(formatParameterizedType("collections.object_array.ArrayObject", {"String"}));
    }

    for (const auto& [className, classInfo] : context.classes) {
        if (classInfo.isTrait || className == "Any" || className == "AnyVal" || className == "AnyRef") {
            continue;
        }
        if (classInfo.typeParameters.empty()) {
            concreteClassesToEmit.insert(className);
        }
    }

    for (const auto& function : program.functions) {
        for (const auto& instruction : function.instructions) {
            if (instruction.opcode == IROpcode::NewObject) {
                concreteClassesToEmit.insert(instruction.operation);
            } else if (instruction.opcode == IROpcode::ClassIs) {
                concreteClassesToEmit.insert(instruction.operation);
            } else if (instruction.opcode == IROpcode::MethodCall ||
                       instruction.opcode == IROpcode::StaticMethodCall) {
                auto [methodClassName, _] = splitQualifiedMember(instruction.operation);
                const auto classIt = context.classes.find(genericBaseName(methodClassName));
                if (classIt != context.classes.end() && !classIt->second.isTrait) {
                    concreteClassesToEmit.insert(methodClassName);
                }
            }
        }
    }

    return concreteClassesToEmit;
}

void emitGcObjectLayoutMaps(
    const std::set<std::string>& concreteClassesToEmit, const CompilerContext& context, std::ostream& out) {
    for (const std::string& className : concreteClassesToEmit) {
        const std::string layoutClassName = genericBaseName(className);
        auto layoutIt = context.classLayouts.find(layoutClassName);
        if (layoutIt == context.classLayouts.end()) continue;

        struct FieldSlot {
            std::string name;
            std::string type;
            int offset;
        };
        std::vector<FieldSlot> fields;
        for (const auto& field : collectClassFieldsInHierarchyForLayout(context, className)) {
            if (!isGcReferenceCapableType(context, field.second)) continue;
            auto offsetIt = layoutIt->second.find(field.first);
            if (offsetIt == layoutIt->second.end()) continue;
            fields.push_back({field.first, field.second, offsetIt->second});
        }

        out << "    align 8\n";
        out << "nabla_gc_object_layout_" << asmSymbolName(className) << ": dq " << fields.size();
        for (const auto& field : fields) {
            out << ", " << field.offset;
        }
        out << "\n";
        for (const auto& field : fields) {
            out << "    ; gc field [" << className << " + " << field.offset << "] "
                << field.name << ": " << field.type << "\n";
        }
    }
}

struct StaticRootDescriptor {
    std::string label;
    std::string kind;
    std::string source;
};

std::vector<StaticRootDescriptor> collectGcStaticRoots(
    const IRProgram& program, const CompilerContext& context) {
    std::vector<StaticRootDescriptor> roots;
    for (const auto& objectName : context.runtimeObjects) {
        roots.push_back({singletonObjectLabel(objectName), "runtime singleton object", objectName});
    }

    for (const auto& function : program.functions) {
        for (const auto& instruction : function.instructions) {
            if (instruction.opcode != IROpcode::StringLiteral) continue;
            const std::string label = "nabla_string_" + asmSymbolPart(function.name) + "_" +
                                      asmSymbolPart(instruction.result) + "_obj";
            roots.push_back({label, "static string literal object", instruction.operands[0]});
        }
    }
    return roots;
}

void emitGcStaticRootMap(const std::vector<StaticRootDescriptor>& roots, std::ostream& out) {
    out << "    align 8\n";
    out << "nabla_gc_static_roots: dq " << roots.size();
    for (const auto& root : roots) {
        out << ", " << root.label;
    }
    out << "\n";
    for (const auto& root : roots) {
        out << "    ; gc static root " << root.kind << " " << root.label
            << " source " << asmCommentText(root.source) << "\n";
    }
}

void emitGcAllocationSafepointTableIndex(
    const std::vector<IRFunction>& functions, std::ostream& out) {
    out << "    align 8\n";
    out << "nabla_gc_alloc_safepoint_tables: dq " << functions.size();
    for (const auto& function : functions) {
        out << ", nabla_gc_alloc_safepoints_" << asmFunctionName(function.name);
    }
    out << "\n";
    for (const auto& function : functions) {
        out << "    ; gc alloc safepoint table " << asmFunctionName(function.name)
            << " -> nabla_gc_alloc_safepoints_" << asmFunctionName(function.name)
            << " exact-frame-offsets-consumed\n";
    }
}
}

IRCodeGenerator::IRCodeGenerator(std::uint64_t heapCapacityBytes)
    : heapCapacityBytes(heapCapacityBytes) {}

void IRCodeGenerator::generateASM(
    const IRProgram& program, const CompilerContext& context, std::ostream& out) const {
    out << "default rel\n";
    RuntimeASM::emitData(out, heapCapacityBytes);

    std::set<std::string> emittedFunctions;
    for (const auto& function : program.functions) {
        emittedFunctions.insert(function.name);
    }

    // Collect vtable slots by static owner and resolved method name. The owner
    // is part of the key so unrelated trait overloads with the same source name
    // do not share one slot.
    std::set<std::string> uniqueSignatures;
    uniqueSignatures.insert("Any.toString");
    uniqueSignatures.insert("Any.hashCode");
    uniqueSignatures.insert("Any.equals");
    for (const auto& function : program.functions) {
        if (function.name.rfind("method.", 0) == 0) {
            uniqueSignatures.insert(function.name.substr(7));
        }
        for (const auto& instruction : function.instructions) {
            if (instruction.opcode == IROpcode::MethodCall) {
                uniqueSignatures.insert(instruction.operation);
            }
        }
    }

    std::map<std::string, int> methodOffsets;
    int offset = 0;
    for (const auto& sig : uniqueSignatures) {
        methodOffsets[sig] = offset;
        offset += 8;
    }

    const bool mainUsesCommandLineArguments = mainAcceptsCommandLineArguments(context);

    // Generate Vtables and all constants in section .data
    std::set<std::string> concreteClassesToEmit =
        collectConcreteClassesToEmit(program, context, mainUsesCommandLineArguments);

    for (const std::string& className : concreteClassesToEmit) {
        out << "vtable_" << asmSymbolName(className) << ":\n";

        for (const auto& [slotKey, _] : methodOffsets) {
            auto [slotOwner, slotMethodName] = splitQualifiedMember(slotKey);
            const std::string sourceName = methodSourceName(slotMethodName);
            const std::string slotResolvedBaseName = methodSpecializationBaseName(slotMethodName);

            std::string targetFunc = "Runtime_trait_dispatch_error";
            if (slotOwner == "Any" && sourceName == "toString") {
                targetFunc = "Any_toString";
            } else if (slotOwner == "Any" && sourceName == "hashCode") {
                targetFunc = "Any_hashCode";
            } else if (slotOwner == "Any" && sourceName == "equals") {
                targetFunc = "Any_equals";
            }

            const auto slotOwnerIt = context.classes.find(genericBaseName(slotOwner));
            std::optional<CompilerContext::FunctionSignature> slotSignature;
            const bool slotOwnerHasUnboundClassTypeParameters =
                slotOwnerIt != context.classes.end() &&
                !slotOwnerIt->second.typeParameters.empty() &&
                !genericSubstitutionFor(context, slotOwner).has_value();
            if (slotOwnerIt != context.classes.end()) {
                auto slotMethodIt = slotOwnerIt->second.methods.find(slotResolvedBaseName);
                if (slotMethodIt == slotOwnerIt->second.methods.end()) {
                    slotMethodIt = slotOwnerIt->second.methods.find(sourceName);
                }
                if (slotMethodIt != slotOwnerIt->second.methods.end()) {
                    slotSignature = slotMethodIt->second;
                    if (auto slotSubstitution = genericSubstitutionFor(context, slotOwner)) {
                        for (auto& parameter : slotSignature->parameters) {
                            parameter.type = substituteType(parameter.type, *slotSubstitution);
                        }
                        slotSignature->returnType =
                            substituteType(slotSignature->returnType, *slotSubstitution);
                    }
                }
            }

            for (const auto& candidate :
                 collectClassMethodLookupCandidates(context, className, sourceName)) {
                if (!candidate.signature) continue;
                CompilerContext::FunctionSignature candidateSignature = *candidate.signature;
                for (auto& parameter : candidateSignature.parameters) {
                    parameter.type = substituteType(parameter.type, candidate.classSubstitution);
                }
                candidateSignature.returnType =
                    substituteType(candidateSignature.returnType, candidate.classSubstitution);
                if (slotSignature &&
                    !vtableSlotParametersMatch(candidateSignature, *slotSignature)) {
                    if (!slotOwnerHasUnboundClassTypeParameters ||
                        candidateSignature.parameters.size() != slotSignature->parameters.size()) {
                        continue;
                    }
                }

                std::string targetOwner = candidate.ownerClassName;
                auto ownerIt = context.classes.find(targetOwner);
                std::string concreteOwner = targetOwner;
                if (ownerIt != context.classes.end() && !ownerIt->second.typeParameters.empty()) {
                    std::vector<std::string> args;
                    for (const auto& tp : ownerIt->second.typeParameters) {
                        auto substIt = candidate.classSubstitution.find(tp);
                        args.push_back(substIt != candidate.classSubstitution.end() ? substIt->second : tp);
                    }
                    concreteOwner = formatParameterizedType(targetOwner, args);
                }

                const std::string targetMethodName =
                    candidateSignature.typeParameters.empty() ? candidate.methodName : slotMethodName;
                const std::string candidateFunction = methodFunctionName(concreteOwner, targetMethodName);
                if (emittedFunctions.count(candidateFunction)) {
                    targetFunc = asmFunctionName(candidateFunction);
                    break;
                }

                // Some trait methods inherited by a parameterized type do not need a
                // class-level specialization: their emitted implementation remains
                // under the generic owner (for example LocalList.iterableFactory,
                // whose body is independent from A).  Still allow the concrete
                // runtime vtable slot to point at that generic implementation when
                // the concrete-owner function was not emitted.
                const std::string genericCandidateFunction =
                    methodFunctionName(candidate.ownerClassName, targetMethodName);
                if (genericCandidateFunction != candidateFunction &&
                    emittedFunctions.count(genericCandidateFunction)) {
                    targetFunc = asmFunctionName(genericCandidateFunction);
                    break;
                }
            }
            if (targetFunc == "Runtime_trait_dispatch_error" &&
                slotSignature && !slotSignature->isAbstract) {
                const std::string fallbackFunction = methodFunctionName(slotOwner, slotMethodName);
                if (emittedFunctions.count(fallbackFunction)) {
                    targetFunc = asmFunctionName(fallbackFunction);
                }
            }
            out << "    dq " << targetFunc << "\n";
        }
    }

    if (!context.runtimeObjects.empty()) {
        for (const auto& objectName : context.runtimeObjects) {
            out << singletonObjectLabel(objectName) << ": dq vtable_" << asmSymbolName(objectName) << "\n";
        }
    }

    // Emit all double constants, float constants, and string literals in section .data
    for (const auto& function : program.functions) {
        for (const auto& instruction : function.instructions) {
            if (instruction.opcode == IROpcode::Constant) {
                if (instruction.type == "Double") {
                    const std::string label = "nabla_double_" + asmSymbolPart(function.name) + "_" +
                                              asmSymbolPart(instruction.result);
                    out << label << ": dq __float64__(" << normalizeFloatConstant(instruction.operands[0]) << ")\n";
                } else if (instruction.type == "Float") {
                    const std::string label = "nabla_float_" + asmSymbolPart(function.name) + "_" +
                                              asmSymbolPart(instruction.result);
                    out << label << ": dd __float32__(" << normalizeFloatConstant(instruction.operands[0]) << "), 0\n";
                }
            } else if (instruction.opcode == IROpcode::StringLiteral) {
                const std::string label = "nabla_string_" + asmSymbolPart(function.name) + "_" +
                                          asmSymbolPart(instruction.result);
                int size = instruction.operands[0].size();
                int padding = (8 - (size % 8)) % 8;
                if (!instruction.operands[0].empty()) {
                    out << label << "_chars: db " << asmDataBytes(instruction.operands[0]);
                    for (int p = 0; p < padding; ++p) {
                        out << ", 0";
                    }
                    out << "\n";
                } else {
                    out << label << "_chars: db 0, 0, 0, 0, 0, 0, 0, 0\n";
                }
                out << "    align 8\n";
                out << label << "_obj: dq " << RuntimeValues::kStringTag << ", " << instruction.operands[0].size()
                    << ", " << label << "_chars\n";
            }
        }
    }

    // Emit additive GC layout metadata in .data. The runtime does not consume
    // these descriptors yet; they make candidate frame slots, object fields,
    // and closure captures testable before collection is enabled.
    emitGcObjectLayoutMaps(concreteClassesToEmit, context, out);
    emitGcStaticRootMap(collectGcStaticRoots(program, context), out);
    for (const auto& function : program.functions) {
        FunctionEmitter emitter(function, context, out, methodOffsets);
        emitter.emitGcFrameMap();
        emitter.emitGcClosureMaps();
        emitter.emitGcAllocationCallMaps();
    }
    emitGcAllocationSafepointTableIndex(program.functions, out);

    // Emit all code in section .text (runtime helper functions first, then user functions)
    RuntimeASM::emitText(out, mainUsesCommandLineArguments);

    for (const auto& function : program.functions) {
        FunctionEmitter(function, context, out, methodOffsets).emit();
    }
}
