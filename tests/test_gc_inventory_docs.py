#!/usr/bin/env python3
"""Regression guard for the GC heap-family inventory documentation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTERNALS = (ROOT / "docs" / "internals.md").read_text(encoding="utf-8")
PLAN = (ROOT / "docs" / "plans" / "runtime-memory-management.md").read_text(encoding="utf-8")
ROADMAP = (ROOT / "docs" / "roadmap.md").read_text(encoding="utf-8")
CODEGEN = (ROOT / "src" / "ir_codegen.cpp").read_text(encoding="utf-8")
RUNTIME_VALUES = (ROOT / "src" / "runtime_values.hpp").read_text(encoding="utf-8")


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
]

for term in required_inventory_terms:
    require(term in INTERNALS, f"docs/internals.md should document GC heap inventory term: {term}")

for term in [
    "Inventaire heap couvert",
    "racines backend",
    "métadonnées nécessaires",
]:
    require(term in PLAN, f"runtime memory plan should track the inventory state: {term}")

require("l'inventaire interne des familles heap" in ROADMAP, "roadmap should mention the GC inventory")

# Keep the docs anchored to the implementation families that currently allocate
# or identify heap objects.
for term in [
    "emitFunctionReference",
    "emitNewObject",
    "emitNewNativeArray",
    "emitBoxedValue",
    "context.runtimeObjects",
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

print("PASS: GC heap-family inventory docs are present and implementation-anchored")
