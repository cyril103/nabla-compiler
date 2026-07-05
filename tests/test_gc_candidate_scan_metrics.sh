#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_candidate_scan_metrics"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_candidate_scan_metrics.nabla" <<'NABLA'
class Blob(a: String, b: String, c: String, d: String, value: Int) {
    def get(): Int = {
        value
    }
    def text(): String = {
        a + b + c + d
    }
}

def churn(count: Int): Int = {
    var i = 0
    var total = 0
    while i < count {
        val text = "a" + i.toString()
        val temp = new Blob(text, text + "b", text + "c", text + "d", i)
        total = total + temp.get()
        i = i + 1
    }
    total
}

def main(): Int = {
    val kept = new Blob("keep", "-", "root", "!", 7)
    val total = churn(420)
    if kept.get() == 7 && kept.text() == "keep-root!" && total >= 0 &&
       gcCollections() > 0 && gcLastStackWords() > 0 && gcLastHeapWords() > 0 &&
       gcLastStackCandidateWords() > 0 && gcLastHeapCandidateWords() > 0 {
        0
    } else {
        1
    }
}
NABLA

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 16384 --keep-asm "$TMP_DIR/gc_candidate_scan_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_candidate_scan_metrics"

ASM="$CUSTOM_BUILD_DIR/gc_candidate_scan_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "Runtime_gcLastStackCandidateWords:",
    "Runtime_gcLastHeapCandidateWords:",
    "call Runtime_gcLastStackCandidateWords",
    "call Runtime_gcLastHeapCandidateWords",
    "gc_last_stack_candidate_words: dq 0",
    "gc_last_heap_candidate_words: dq 0",
]
missing = [item for item in required if item not in asm]
if missing:
    raise SystemExit("missing expected GC candidate metric ASM markers: " + ", ".join(missing))
PY

for primitive in gcLastStackCandidateWords gcLastHeapCandidateWords; do
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
    if ! grep -q "nom de fonction réservé pour une primitive runtime" "$TMP_DIR/${primitive}.err"; then
        echo "expected reserved runtime-name diagnostic for ${primitive}" >&2
        cat "$TMP_DIR/${primitive}.err" >&2
        exit 1
    fi
done

echo "PASS: GC conservative candidate scan metrics primitives"
