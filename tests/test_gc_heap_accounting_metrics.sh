#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_heap_accounting_metrics"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_heap_accounting_metrics.nabla" <<'NABLA'
class Box(value: Int) {
    def get(): Int = { value }
}

def churn(n: Int): Int = {
    var i = 0
    var total = 0
    while i < n {
        val dead = new Box(i)
        total = total + dead.get()
        i = i + 1
    }
    total
}

def main(): Int = {
    val beforeAllocated = heapAllocatedBytes()
    val beforeBlocks = heapFreeBlockCount()
    val kept = new Box(7)
    val sum = churn(420)
    val probe = new Box(sum)
    val afterAllocated = heapAllocatedBytes()
    val freeBlocks = heapFreeBlockCount()
    if kept.get() == 7 && probe.get() == sum &&
       gcCollections() > 0 && gcLastFreedBytes() > 0 &&
       heapFreeBytes() > 0 && heapLargestFreeBlock() > 0 &&
       afterAllocated > beforeAllocated && freeBlocks > beforeBlocks {
        0
    } else {
        1
    }
}
NABLA

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 12288 --keep-asm "$TMP_DIR/gc_heap_accounting_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_heap_accounting_metrics"

ASM="$CUSTOM_BUILD_DIR/gc_heap_accounting_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "Runtime_heapAllocatedBytes:",
    "Runtime_heapFreeBlockCount:",
    "call Runtime_heapAllocatedBytes",
    "call Runtime_heapFreeBlockCount",
]
missing = [item for item in required if item not in asm]
if missing:
    print("missing ASM markers: " + ", ".join(missing), file=sys.stderr)
    sys.exit(1)
PY

for primitive in heapAllocatedBytes heapFreeBlockCount; do
    cat >"$TMP_DIR/${primitive}_reserved.nabla" <<NABLA
def ${primitive}(): Int = { 0 }
def main(): Int = { ${primitive}() }
NABLA
    set +e
    NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR/${primitive}" "$BUILD_DIR/nablac" "$TMP_DIR/${primitive}_reserved.nabla" >"$TMP_DIR/${primitive}.out" 2>"$TMP_DIR/${primitive}.err"
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        echo "expected reserved-name diagnostic for ${primitive}" >&2
        exit 1
    fi
    if ! grep -Fq "nom de fonction réservé pour une primitive runtime: ${primitive}" "$TMP_DIR/${primitive}.err"; then
        echo "missing reserved-name diagnostic for ${primitive}" >&2
        cat "$TMP_DIR/${primitive}.err" >&2
        exit 1
    fi
done

echo "PASS: GC heap accounting metrics primitives"
