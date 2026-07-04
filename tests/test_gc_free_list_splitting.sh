#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_free_list_splitting"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

SOURCE="$TMP_DIR/gc_free_list_splits_oversized_block.nabla"
cat >"$SOURCE" <<'NABLA'
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
    while i < 40 {
        total = total + churn(i)
        i = i + 1
    }
    total
}

def main(): Int = {
    makeGarbage()
NABLA

for i in $(seq 0 95); do
    printf '    val small%s = new Cell(%s)\n' "$i" "$i" >>"$SOURCE"
done

cat >>"$SOURCE" <<'NABLA'
    val xs = new IntArray(20)
    xs.set(19, 7)

    if small0.get() == 0 && small95.get() == 95 && xs.get(19) == 7 && gcCollections() > 0 && heapFreeBytes() > 0 {
        0
    } else {
        8
    }
}
NABLA

if ! PATH=/opt/data/local/usr/bin:$PATH \
    NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 --keep-asm \
    "$SOURCE" \
    >"$TMP_DIR/gc_free_list_splits.compile.out" \
    2>"$TMP_DIR/gc_free_list_splits.compile.err"; then
    echo "FAIL: compilation gc_free_list_splits_oversized_block" >&2
    cat "$TMP_DIR/gc_free_list_splits.compile.out" >&2
    cat "$TMP_DIR/gc_free_list_splits.compile.err" >&2
    exit 1
fi

if "$CUSTOM_BUILD_DIR/gc_free_list_splits_oversized_block" \
    >"$TMP_DIR/gc_free_list_splits.run.out" \
    2>"$TMP_DIR/gc_free_list_splits.run.err"; then
    :
else
    code=$?
    echo "FAIL: gc_free_list_splits_oversized_block a terminé avec le code $code" >&2
    cat "$TMP_DIR/gc_free_list_splits.run.out" >&2
    cat "$TMP_DIR/gc_free_list_splits.run.err" >&2
    exit 1
fi

if [ -s "$TMP_DIR/gc_free_list_splits.run.out" ] || [ -s "$TMP_DIR/gc_free_list_splits.run.err" ]; then
    echo "FAIL: gc_free_list_splits_oversized_block ne doit rien écrire" >&2
    cat "$TMP_DIR/gc_free_list_splits.run.out" >&2
    cat "$TMP_DIR/gc_free_list_splits.run.err" >&2
    exit 1
fi

ASM="$CUSTOM_BUILD_DIR/gc_free_list_splits_oversized_block_tmp.asm"
python3 - "$ASM" <<'PY'
from pathlib import Path
import sys

asm = Path(sys.argv[1]).read_text(encoding="utf-8")
for needle in [
    ".L_alloc_from_free_split_check:",
    ".L_alloc_from_free_split_block:",
    ".L_alloc_from_free_use_entire_block:",
    ".L_alloc_after_gc_from_free_split_check:",
    ".L_alloc_after_gc_from_free_split_block:",
    ".L_alloc_after_gc_from_free_use_entire_block:",
]:
    if needle not in asm:
        raise SystemExit(f"missing free-list splitting marker: {needle}")
PY

echo "PASS: GC free-list splitting preserves oversized free-block tails"
