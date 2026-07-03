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
    "carte d'appel minimale",
    "descripteur additif par fonction",
    "Métadonnées De Racines De Frame GC",
    "nabla_gc_frame_roots_<fonction>",
    "dq count, offset1, offset2",
    "distance positive utilisée par le frame `rbp`",
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
    "descripteurs champs/captures",
]:
    require(term in PLAN, f"runtime memory plan should track the inventory state: {term}")

require(
    "l'inventaire interne des familles heap et des racines backend" in ROADMAP,
    "roadmap should mention the GC heap/root inventories",
)
require(
    "premières métadonnées de racines de frame" in ROADMAP,
    "roadmap should mention the GC frame-root metadata",
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
    "isGcReferenceCapableType",
    "nabla_gc_frame_roots_",
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
]:
    require(term in RUNTIME_ASM, f"expected runtime helper missing: {term}")

print("PASS: GC heap/root inventory docs are present and implementation-anchored")
