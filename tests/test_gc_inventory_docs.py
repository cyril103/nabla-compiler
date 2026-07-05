#!/usr/bin/env python3
"""Regression guard for the GC heap-family and backend-root inventory docs."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTERNALS = (ROOT / "docs" / "internals.md").read_text(encoding="utf-8")
PLAN = (ROOT / "docs" / "plans" / "runtime-memory-management.md").read_text(encoding="utf-8")
ROADMAP = (ROOT / "docs" / "roadmap.md").read_text(encoding="utf-8")
CODEGEN = (ROOT / "src" / "ir_codegen.cpp").read_text(encoding="utf-8")
RUNTIME_VALUES = (ROOT / "src" / "runtime_values.hpp").read_text(encoding="utf-8")
RUNTIME_ASM = (ROOT / "src" / "runtime_asm.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


required_inventory_terms = [
    "Inventaire Des Familles Heap Pour Le Futur GC",
    "kStringTag",
    "`ObjectArray[T]` brut",
    "ArrayObject[T]",
    "IntArray",
    "LongArray",
    "FloatArray",
    "DoubleArray",
    "BoolArray",
    "champ `values`",
    "Instances utilisateur",
    "Closures",
    "Valeurs boxées",
    "Singletons runtime",
    "Valeurs immédiates",
    "kNullSlot",
    "tableau de valeurs",
    "tableau de références",
    "instance utilisateur",
    "closure",
    "boîte primitive",
    "singleton statique",
    "`Any`, `AnyRef`",
    "Inventaire Des Racines Backend Pour Le Futur GC",
    "StackFrame",
    "collectSlots()",
    "loadValue()",
    "storeRegister()",
    "Paramètres de fonction et de méthode",
    "Temporaires IR nommés",
    "Variables locales mutables",
    "Arguments et temporaires en registres",
    "Racines statiques",
    "État runtime transitoire",
    "Runtime_alloc",
    "spillant les registres généraux",
    "descripteur additif par fonction",
    "Métadonnées De Racines De Frame GC",
    "nabla_gc_frame_roots_<fonction>",
    "dq count, offset1, offset2",
    "distance positive utilisée par le frame `rbp`",
    "Métadonnées De Layout Heap GC",
    "nabla_gc_object_layout_<classe>",
    "nabla_gc_closure_layout_<fonction>_<result>",
    "gc field [Classe + offset] champ: Type",
    "captures commencent à l'offset `+16`",
    "Métadonnées De Racines Statiques GC",
    "nabla_gc_static_roots",
    "runtime singleton object",
    "static string literal object",
    "Métadonnées De Points D'Allocation GC",
    "nabla_gc_alloc_calls_<fonction>",
    "nabla_gc_alloc_call_<fonction>_<index>",
    "gc alloc call",
    "gc alloc root [rbp -",
    "Inventaire Des Allocations Internes Aux Helpers Runtime",
    "Runtime_stringSplitMakeSegment",
    "FloatDouble_method_toString",
    "Métadonnées De Racines Internes Des Helpers Runtime",
    "nabla_gc_runtime_helper_allocs_<helper>",
    "nabla_gc_runtime_helper_alloc_<helper>_<index>",
    "cartes candidates",
    "interior:*",
    "non consommées",
    "cartes racines consommables",
]

for term in required_inventory_terms:
    require(term in INTERNALS, f"docs/internals.md should document GC heap inventory term: {term}")

for term in [
    "Inventaire heap couvert",
    "racines backend",
    "métadonnées de racines de frame",
    "Inventaire des racines backend couvert",
    "descripteurs testables",
    "Métadonnées de racines de frame couvertes",
    "nabla_gc_frame_roots_<fonction>",
    "Descripteurs de champs/captures heap couverts",
    "nabla_gc_object_layout_<classe>",
    "nabla_gc_closure_layout_<fonction>_<result>",
    "Métadonnées de racines statiques couvertes",
    "nabla_gc_static_roots",
    "Cartes de points d'appel `Runtime_alloc` couvertes",
    "nabla_gc_alloc_calls_<fonction>",
    "nabla_gc_alloc_call_<fonction>_<index>",
    "Inventaire des allocations internes aux helpers runtime couvert",
    "tests/test_gc_runtime_helper_alloc_inventory.py",
    "Cartes candidates de racines internes aux helpers runtime couvertes",
    "nabla_gc_runtime_helper_allocs_<helper>",
    "nabla_gc_runtime_helper_alloc_<helper>_<index>",
    "tests/test_gc_runtime_helper_root_maps.py",
]:
    require(term in PLAN, f"runtime memory plan should track the inventory state: {term}")

require(
    "l'inventaire interne des familles heap et des racines backend" in ROADMAP,
    "roadmap should mention the GC heap/root inventories",
)
require(
    "métadonnées de racines de frame" in ROADMAP,
    "roadmap should mention the GC frame-root metadata",
)
require(
    "descripteurs champs/captures pour classes/closures" in ROADMAP.replace("\n  ", " "),
    "roadmap should mention the GC heap layout descriptors",
)
require(
    "nabla_gc_static_roots" in ROADMAP,
    "roadmap should mention the GC static root metadata",
)
for term in [
    "cartes de points d'appel `Runtime_alloc` du code",
    "l'inventaire outillé des allocations internes aux helpers",
    "runtime",
    "cartes candidates",
]:
    require(term in ROADMAP, f"roadmap should mention the GC runtime helper allocation inventory: {term}")
require(
    "cartes candidates de racines internes aux helpers runtime" in ROADMAP,
    "roadmap should mention the GC runtime helper root maps",
)

# Keep the docs anchored to the implementation families that currently allocate
# or identify heap objects.
for term in [
    "emitFunctionReference",
    "emitNewObject",
    "emitNewNativeArray",
    "emitBoxedValue",
    "context.runtimeObjects",
    "class StackFrame",
    "collectSlots",
    "loadValue",
    "storeRegister",
    "emitGcFrameMap",
    "emitGcClosureMaps",
    "emitGcObjectLayoutMaps",
    "collectGcStaticRoots",
    "emitGcStaticRootMap",
    "emitGcAllocationCallMaps",
    "allocationCallKind",
    "collectConcreteClassesToEmit",
    "isGcReferenceCapableType",
    "nabla_gc_frame_roots_",
    "nabla_gc_object_layout_",
    "nabla_gc_closure_layout_",
    "nabla_gc_static_roots",
    "nabla_gc_alloc_calls_",
    "nabla_gc_alloc_call_",
]:
    require(term in CODEGEN, f"expected implementation hook missing: {term}")

for term in [
    "kStringTag",
    "kBoxedIntTag",
    "kBoxedLongTag",
    "kBoxedFloatTag",
    "kBoxedDoubleTag",
    "kBoxedBoolTag",
    "kBoxedCharTag",
    "kBoxedUnitTag",
    "kNullSlot",
]:
    require(term in RUNTIME_VALUES, f"expected runtime tag missing: {term}")

for term in [
    "Runtime_alloc",
    "Runtime_stringConcat",
    "Runtime_stringSplit",
    "Runtime_buildArgsArray",
    "emitRuntimeHelperAllocMetadata",
    "nabla_gc_runtime_helper_allocs_",
    "nabla_gc_runtime_helper_alloc_",
]:
    require(term in RUNTIME_ASM, f"expected runtime helper missing: {term}")

print("PASS: GC heap/root inventory docs are present and implementation-anchored")
