#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_interior_candidate_scan_metrics"
TMP_DIR=$(mktemp -d "$ROOT_DIR/.tmp_gc_interior_candidate_scan_metrics.XXXXXX")
export PATH=/opt/data/local/usr/bin:$PATH
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_interior_candidate_scan_metrics.nabla" <<'NABLA'
import collections.array

def churn(count: Int): Int = {
    var i = 0
    var total = 0
    while i < count {
        val text = "left," + i.toString() + ",right"
        val parts = text.split(",")
        total = total + 1
        i = i + 1
    }
    total
}

def main(): Int = {
    val kept = "root,kept,value"
    val parts = kept.split(",")
    val total = churn(260)
    if total > 0 &&
       gcCollections() > 0 &&
       gcLastStackCandidateWords() >= gcLastStackInteriorCandidateWords() &&
       gcLastHeapCandidateWords() >= gcLastHeapInteriorCandidateWords() &&
       gcLastStackInteriorCandidateWords() > 0 &&
       gcLastHeapInteriorCandidateWords() > 0 {
        0
    } else {
        1
    }
}
NABLA

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 16384 --keep-asm "$TMP_DIR/gc_interior_candidate_scan_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_interior_candidate_scan_metrics"

ASM="$CUSTOM_BUILD_DIR/gc_interior_candidate_scan_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "Runtime_gcLastStackInteriorCandidateWords:",
    "Runtime_gcLastHeapInteriorCandidateWords:",
    "call Runtime_gcLastStackInteriorCandidateWords",
    "call Runtime_gcLastHeapInteriorCandidateWords",
    "gc_last_stack_interior_candidate_words: dq 0",
    "gc_last_heap_interior_candidate_words: dq 0",
    "inc qword [gc_last_stack_interior_candidate_words]",
    "inc qword [gc_last_heap_interior_candidate_words]",
]
missing = [item for item in required if item not in asm]
if missing:
    raise SystemExit("missing expected GC interior-candidate metric ASM markers: " + ", ".join(missing))
PY

for primitive in gcLastStackInteriorCandidateWords gcLastHeapInteriorCandidateWords; do
    cat >"$TMP_DIR/${primitive}_reserved.nabla" <<NABLA
def ${primitive}(): Int = { 0 }
def main(): Int = { ${primitive}() }
NABLA
    set +e
    NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR/${primitive}" "$BUILD_DIR/nablac" "$TMP_DIR/${primitive}_reserved.nabla" >"$TMP_DIR/${primitive}.out" 2>"$TMP_DIR/${primitive}.err"
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        echo "expected ${primitive} reserved-name diagnostic" >&2
        exit 1
    fi
    if ! grep -Fq "nom de fonction réservé pour une primitive runtime: ${primitive}" "$TMP_DIR/${primitive}.err"; then
        echo "expected reserved runtime-name diagnostic for ${primitive}" >&2
        cat "$TMP_DIR/${primitive}.err" >&2
        exit 1
    fi
done

echo "PASS: GC conservative interior-candidate scan metrics primitives"
