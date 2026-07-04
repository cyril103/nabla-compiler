#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
mkdir -p "$BUILD_DIR"
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_active"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_reclaims_temporaries.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = {
        value
    }
}

def churn(value: Int): Int = {
    val cell = new Cell(value)
    cell.get()
}

def main(): Int = {
    var i = 0
    var total = 0
    while i < 800 {
        total = total + churn(i)
        i = i + 1
    }
    if total > 0 {
        0
    } else {
        1
    }
}
NABLA

cat >"$TMP_DIR/gc_keeps_stack_root.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = {
        value
    }
}

class Box(cell: Cell) {
    def get(): Int = {
        cell.get()
    }
}

def makeBox(): Box = {
    new Box(new Cell(123))
}

def churn(value: Int): Int = {
    val cell = new Cell(value)
    cell.get()
}

def main(): Int = {
    val kept = makeBox()
    var i = 0
    while i < 800 {
        churn(i)
        i = i + 1
    }
    if kept.get() == 123 {
        0
    } else {
        2
    }
}
NABLA

cat >"$TMP_DIR/gc_coalesces_free_run.nabla" <<'NABLA'
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
    while i < 90 {
        total = total + churn(i)
        i = i + 1
    }
    total
}

def main(): Int = {
    makeGarbage()
    val xs = new IntArray(250)
    xs.set(249, 42)
    if xs.get(249) == 42 {
        0
    } else {
        3
    }
}
NABLA

compile_and_run() {
    local source_file=$1
    local label=$2

    PATH=/opt/data/local/usr/bin:$PATH \
    NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 --keep-temp "$source_file" \
        >"$TMP_DIR/${label}.compile.out" 2>"$TMP_DIR/${label}.compile.err"

    local executable="$CUSTOM_BUILD_DIR/$(basename "$source_file" .nabla)"
    "$executable" >"$TMP_DIR/${label}.run.out" 2>"$TMP_DIR/${label}.run.err"

    if [ -s "$TMP_DIR/${label}.run.out" ] || [ -s "$TMP_DIR/${label}.run.err" ]; then
        echo "FAIL: $label ne doit rien écrire" >&2
        cat "$TMP_DIR/${label}.run.out" >&2
        cat "$TMP_DIR/${label}.run.err" >&2
        exit 1
    fi
}

compile_and_run "$TMP_DIR/gc_reclaims_temporaries.nabla" gc_reclaims_temporaries
compile_and_run "$TMP_DIR/gc_keeps_stack_root.nabla" gc_keeps_stack_root
compile_and_run "$TMP_DIR/gc_coalesces_free_run.nabla" gc_coalesces_free_run

ASM="$CUSTOM_BUILD_DIR/gc_reclaims_temporaries_tmp.asm"

python3 - "$ASM" <<'PY'
from pathlib import Path
import sys

asm = Path(sys.argv[1]).read_text(encoding="utf-8")

for needle in [
    "Runtime_gc:",
    "Runtime_gc_mark_candidate:",
    "heap_free_list: dq 0",
    "gc_stack_top: dq 0",
    "gc_mark_changed: dq 0",
    "gc_mark_flag: dq 0x8000000000000000",
    "gc_free_flag: dq 0x4000000000000000",
    "gc_size_mask: dq 0x3fffffffffffffff",
    "add rdx, 16",
    "push rcx",
    "pop rcx",
    "mov [r9], r11",
    ".L_gc_sweep_coalesce_next:",
    ".L_alloc_after_gc_from_free_zero_payload:",
    "mov [heap_free_list], rbx",
]:
    if needle not in asm:
        raise SystemExit(f"missing GC runtime assembly marker: {needle}")

alloc = asm.index("Runtime_alloc:")
gc_call = asm.index("call Runtime_gc", alloc)
overflow = asm.index("Runtime_heap_overflow:")
if not alloc < gc_call < overflow:
    raise SystemExit("Runtime_alloc should call Runtime_gc before Runtime_heap_overflow")
PY

echo "PASS: GC actif conservateur récupère des temporaires et conserve les racines pile"
