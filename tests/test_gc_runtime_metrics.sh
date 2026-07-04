#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_metrics"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_runtime_metrics.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = {
        value
    }
}

def churn(value: Int): Int = {
    val cell = new Cell(value)
    cell.get()
}

def makeGarbage(): Int = {
    var i = 0
    var total = 0
    while i < 130 {
        total = total + churn(i)
        i = i + 1
    }
    total
}

def main(): Int = {
    val beforeCollections = gcCollections()
    val beforeFree = heapFreeBytes()
    val beforeLargest = heapLargestFreeBlock()
    val beforeLastFreed = gcLastFreedBytes()
    val beforeLastLargest = gcLastLargestFreeBlock()
    makeGarbage()
    val afterCollections = gcCollections()
    val afterFree = heapFreeBytes()
    val afterLargest = heapLargestFreeBlock()
    val afterLastFreed = gcLastFreedBytes()
    val afterLastLargest = gcLastLargestFreeBlock()
    if beforeCollections == 0 && beforeFree == 0 && beforeLargest == 0 &&
       beforeLastFreed == 0 && beforeLastLargest == 0 &&
       afterCollections > beforeCollections && afterFree >= 0 && afterLargest >= 0 &&
       afterLastFreed > 0 && afterLastLargest > 0 {
        0
    } else {
        7
    }
}
NABLA

PATH=/opt/data/local/usr/bin:$PATH NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" \
    "$ROOT_DIR/build/nablac" --heap-size 4096 --keep-asm "$TMP_DIR/gc_runtime_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_runtime_metrics" >/dev/null

ASM="$CUSTOM_BUILD_DIR/gc_runtime_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "gc_collections: dq 0",
    "gc_last_freed_bytes: dq 0",
    "gc_last_largest_free_block: dq 0",
    "inc qword [gc_collections]",
    "add qword [gc_last_freed_bytes], r12",
    ".L_gc_sweep_existing_free_block:",
    ".L_gc_sweep_coalesce_existing_free:",
    "Runtime_gcCollections:",
    "Runtime_gcLastFreedBytes:",
    "Runtime_gcLastLargestFreeBlock:",
    "Runtime_heapFreeBytes:",
    "Runtime_heapLargestFreeBlock:",
    ".L_heap_free_bytes_loop:",
    ".L_heap_largest_free_block_loop:",
    "call Runtime_gcCollections",
    "call Runtime_gcLastFreedBytes",
    "call Runtime_gcLastLargestFreeBlock",
    "call Runtime_heapFreeBytes",
    "call Runtime_heapLargestFreeBlock",
]
for needle in required:
    if needle not in asm:
        raise SystemExit(f"missing GC metric marker in assembly: {needle}")
PY

for primitive in \
    gcCollections \
    gcLastFreedBytes \
    gcLastLargestFreeBlock \
    heapFreeBytes \
    heapLargestFreeBlock; do
    cat >"$TMP_DIR/${primitive}_reserved.nabla" <<NABLA
def ${primitive}(): Int = {
    0
}
NABLA
    if PATH=/opt/data/local/usr/bin:$PATH NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" \
        "$ROOT_DIR/build/nablac" "$TMP_DIR/${primitive}_reserved.nabla" \
        >"$TMP_DIR/${primitive}_reserved.out" 2>&1; then
        echo "reserved GC metric primitive declaration should fail: ${primitive}" >&2
        exit 1
    fi
    if ! grep -Fq "nom de fonction réservé pour une primitive runtime: ${primitive}" \
        "$TMP_DIR/${primitive}_reserved.out"; then
        echo "reserved GC metric primitive diagnostic missing: ${primitive}" >&2
        cat "$TMP_DIR/${primitive}_reserved.out" >&2
        exit 1
    fi
done

echo "PASS: GC runtime metrics primitives"
