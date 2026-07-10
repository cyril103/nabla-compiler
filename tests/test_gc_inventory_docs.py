#!/usr/bin/env python3
"""Regression guard for the GC heap-family and backend-root inventory docs."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTERNALS = (ROOT / "docs" / "internals.md").read_text(encoding="utf-8")
LANGUAGE = (ROOT / "docs" / "language.md").read_text(encoding="utf-8")
PLAN = (ROOT / "docs" / "plans" / "runtime-memory-management.md").read_text(encoding="utf-8")
PLAN_README = (ROOT / "docs" / "plans" / "README.md").read_text(encoding="utf-8")
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
    "nabla_gc_alloc_safepoints_<fonction>",
    "nabla_gc_alloc_safepoint_tables",
    "nabla_gc_alloc_call_<fonction>_<index>",
    "nabla_gc_alloc_return_<fonction>_<index>",
    "return_pc, map",
    "gc alloc call",
    "gc alloc safepoint map",
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
    "gc_last_alloc_safepoint_map_found",
    "gc_last_alloc_safepoint_map_missed",
    "gc_last_alloc_safepoint_root_slots",
    "gc_last_alloc_safepoint_root_bytes",
    "gc_last_alloc_safepoint_map",
    "`gcLastAllocSafepointMapFound()`",
    "`gcLastAllocSafepointMapMissed()`",
    "`gcLastAllocSafepointRootSlots()`",
    "`gcLastAllocSafepointRootBytes()`",
]

for term in required_inventory_terms:
    require(term in INTERNALS, f"docs/internals.md should document GC heap inventory term: {term}")

language_flat = " ".join(LANGUAGE.split())
for term in [
    "`gcLastAllocSafepointRootSlots(): Int`",
    "`gcLastAllocSafepointRootBytes(): Int`",
    "carte exacte",
    "nombre de slots déclarés dans l'en-tête de cette carte",
    "octets correspondants (`slots * 8`)",
    "consommation des offsets `rbp - offset`",
]:
    require(
        term in language_flat,
        f"docs/language.md should document GC safepoint root metric contract: {term}",
    )

combined_docs = "\n".join([INTERNALS, LANGUAGE, PLAN, PLAN_README, ROADMAP])
for stale in [
    "sans lire cette carte pour le marquage",
    "lit seulement le premier `dq` de la carte",
    "Observational only: read the map header count, not root offsets.",
    "gc alloc safepoint map ... non-consumed",
]:
    require(
        stale not in combined_docs,
        f"GC allocation safepoint docs should not keep stale non-consuming wording: {stale}",
    )

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
    "nabla_gc_alloc_safepoints_<fonction>",
    "nabla_gc_alloc_safepoint_tables",
    "nabla_gc_alloc_call_<fonction>_<index>",
    "nabla_gc_alloc_return_<fonction>_<index>",
    "gc alloc safepoint map",
    "Inventaire des allocations internes aux helpers runtime couvert",
    "tests/test_gc_runtime_helper_alloc_inventory.py",
    "Cartes candidates de racines internes aux helpers runtime couvertes",
    "nabla_gc_runtime_helper_allocs_<helper>",
    "nabla_gc_runtime_helper_alloc_<helper>_<index>",
    "tests/test_gc_runtime_helper_root_maps.py",
    "Lookup runtime des cartes d'allocation couvert",
    "tests/test_gc_alloc_safepoint_lookup_metrics.sh",
    "gcLastAllocSafepointMapFound() > 0",
    "gcLastAllocSafepointMapMissed() == 0",
    "gcLastAllocSafepointRootSlots() > 0",
    "gcLastAllocSafepointRootBytes() == slots * 8",
]:
    require(term in PLAN, f"runtime memory plan should track the inventory state: {term}")

for term in [
    "nabla_gc_alloc_return_<fonction>_<index>",
    "nabla_gc_alloc_safepoints_<fonction>",
    "nabla_gc_alloc_safepoint_tables",
    "return PC",
    "consomme les offsets",
    "gcLastAllocSafepointMapFound()",
    "gcLastAllocSafepointMapMissed()",
    "gcLastAllocSafepointRootSlots()",
    "gcLastAllocSafepointRootBytes()",
]:
    require(term in PLAN_README, f"plans README should track inert allocation return-PC metadata: {term}")

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
    "gc alloc safepoint map",
    "nabla_gc_alloc_return_<fonction>_<index>",
    "nabla_gc_alloc_safepoints_<fonction>",
    "nabla_gc_alloc_safepoint_tables",
    "l'inventaire outillé des allocations internes aux helpers",
    "runtime",
    "cartes candidates",
    "gcLastAllocSafepointMapFound()",
    "gcLastAllocSafepointMapMissed()",
    "gcLastAllocSafepointRootSlots()",
    "gcLastAllocSafepointRootBytes()",
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
    "emitGcAllocationSafepointComment",
    "emitGcAllocationReturnLabel",
    "emitGcAllocationSafepointTableIndex",
    "allocationCallKind",
    "collectConcreteClassesToEmit",
    "isGcReferenceCapableType",
    "nabla_gc_frame_roots_",
    "nabla_gc_object_layout_",
    "nabla_gc_closure_layout_",
    "nabla_gc_static_roots",
    "nabla_gc_alloc_calls_",
    "nabla_gc_alloc_call_",
    "nabla_gc_alloc_safepoints_",
    "nabla_gc_alloc_safepoint_tables",
    "nabla_gc_alloc_return_",
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
    "gc_last_alloc_safepoint_map_found",
    "gc_last_alloc_safepoint_map_missed",
    "gc_last_alloc_safepoint_root_slots",
    "gc_last_alloc_safepoint_root_bytes",
    "Runtime_gcLastAllocSafepointMapFound",
    "Runtime_gcLastAllocSafepointMapMissed",
    "Runtime_gcLastAllocSafepointRootSlots",
    "Runtime_gcLastAllocSafepointRootBytes",
    "Consume exact user-frame root offsets from the found allocation map",
    ".L_gc_exact_root_scan",
]:
    require(term in RUNTIME_ASM, f"expected runtime helper missing: {term}")

print("PASS: GC heap/root inventory docs are present and implementation-anchored")
