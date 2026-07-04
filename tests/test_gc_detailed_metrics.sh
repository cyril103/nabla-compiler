#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_detailed_metrics"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_detailed_metrics.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = {
        value
    }
}

class Holder(left: Cell, right: Cell) {
    def sum(): Int = {
        left.get() + right.get()
    }
}

def churn(value: Int): Int = {
    val holder = new Holder(new Cell(value), new Cell(value + 1))
    holder.sum()
}

def main(): Int = {
    val kept = new Holder(new Cell(20), new Cell(22))
    var i = 0
    var total = 0
    while i < 180 {
        total = total + churn(i)
        i = i + 1
    }
    if kept.sum() == 42 && total > 0 &&
       gcCollections() > 0 && gcLastFreedBytes() > 0 &&
       gcLastMarkedBlocks() > 0 && gcLastFreedBlocks() > 1 &&
       gcLastStackWords() > 0 && gcLastHeapWords() > 0 {
        0
    } else {
        17
    }
}
NABLA

PATH=/opt/data/local/usr/bin:$PATH NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" \
    "$ROOT_DIR/build/nablac" --heap-size 8192 --keep-asm "$TMP_DIR/gc_detailed_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_detailed_metrics" >/dev/null

ASM="$CUSTOM_BUILD_DIR/gc_detailed_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "gc_last_marked_blocks: dq 0",
    "gc_last_freed_blocks: dq 0",
    "gc_last_stack_words: dq 0",
    "gc_last_heap_words: dq 0",
    "inc qword [gc_last_marked_blocks]",
    "inc qword [gc_last_freed_blocks]",
    "add r12, r14\n    inc qword [gc_last_freed_blocks]",
    "inc qword [gc_last_stack_words]",
    "inc qword [gc_last_heap_words]",
    "Runtime_gcLastMarkedBlocks:",
    "Runtime_gcLastFreedBlocks:",
    "Runtime_gcLastStackWords:",
    "Runtime_gcLastHeapWords:",
    "call Runtime_gcLastMarkedBlocks",
    "call Runtime_gcLastFreedBlocks",
    "call Runtime_gcLastStackWords",
    "call Runtime_gcLastHeapWords",
]
for needle in required:
    if needle not in asm:
        raise SystemExit(f"missing detailed GC metric marker in assembly: {needle}")
PY

for primitive in \
    gcLastMarkedBlocks \
    gcLastFreedBlocks \
    gcLastStackWords \
    gcLastHeapWords; do
    cat >"$TMP_DIR/${primitive}_reserved.nabla" <<NABLA
def ${primitive}(): Int = {
    0
}
NABLA
    if PATH=/opt/data/local/usr/bin:$PATH NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" \
        "$ROOT_DIR/build/nablac" "$TMP_DIR/${primitive}_reserved.nabla" \
        >"$TMP_DIR/${primitive}_reserved.out" 2>&1; then
        echo "reserved detailed GC metric primitive declaration should fail: ${primitive}" >&2
        exit 1
    fi
    if ! grep -Fq "nom de fonction réservé pour une primitive runtime: ${primitive}" \
        "$TMP_DIR/${primitive}_reserved.out"; then
        echo "reserved detailed GC metric primitive diagnostic missing: ${primitive}" >&2
        cat "$TMP_DIR/${primitive}_reserved.out" >&2
        exit 1
    fi
done

echo "PASS: detailed GC runtime metrics primitives"
