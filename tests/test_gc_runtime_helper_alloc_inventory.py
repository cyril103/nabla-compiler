#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = (ROOT / "src/runtime_asm.cpp").read_text(encoding="utf-8")
INTERNALS = (ROOT / "docs/internals.md").read_text(encoding="utf-8")
PLAN = (ROOT / "docs/plans/runtime-memory-management.md").read_text(encoding="utf-8")

LABEL_RE = re.compile(r'<< "([A-Za-z_][A-Za-z0-9_]*):\\n"')
ALLOC_RE = re.compile(r'call Runtime_alloc')

expected_counts = {
    "Runtime_cStringToString": 1,
    "Runtime_buildArgsArray": 2,
    "Runtime_readLine": 2,
    "Runtime_copyPathToCString": 1,
    "Runtime_emptyString": 1,
    "Runtime_readFile": 1,
    "Runtime_stringConcat": 1,
    "Runtime_stringToCharArray": 2,
    "Runtime_stringSubstring": 1,
    "Runtime_stringRepeat": 1,
    "Runtime_stringTrim": 1,
    "Runtime_stringSplit": 3,
    "Runtime_stringSplitMakeSegment": 1,
    "Int_method_toString": 1,
    "Bool_method_toString": 2,
    "Char_method_toString": 1,
    "FloatDouble_method_toString": 2,
    "Any_toString": 1,
}

current = None
counts = {}
for line in RUNTIME.splitlines():
    label_match = LABEL_RE.search(line)
    if label_match:
        label = label_match.group(1)
        if not label.startswith("L_"):
            current = label
    if ALLOC_RE.search(line):
        if current is None:
            raise SystemExit(f"Runtime_alloc call without helper label: {line}")
        counts[current] = counts.get(current, 0) + 1

if counts != expected_counts:
    raise SystemExit(f"runtime helper allocation inventory drifted:\nactual={counts!r}\nexpected={expected_counts!r}")

for helper, count in expected_counts.items():
    needle = f"`{helper}` ({count})"
    if needle not in INTERNALS:
        raise SystemExit(f"docs/internals.md missing runtime allocation helper entry {needle}")

for needle in [
    "Inventaire Des Allocations Internes Aux Helpers Runtime",
    "Runtime_stringSplitMakeSegment",
    "FloatDouble_method_toString",
    "Any_toString",
    "non encore couverts par des cartes racines consommables",
]:
    if needle not in INTERNALS:
        raise SystemExit(f"docs/internals.md missing GC helper allocation wording: {needle}")

for needle in [
    "inventaire des allocations internes aux helpers runtime",
    "cartes racines internes aux helpers runtime",
]:
    if needle not in PLAN:
        raise SystemExit(f"runtime memory plan missing helper allocation wording: {needle}")

print("PASS: GC runtime helper allocation inventory is documented and implementation-anchored")
