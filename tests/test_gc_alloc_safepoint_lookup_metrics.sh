#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${NABLA_BUILD_DIR:-$ROOT_DIR/build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_alloc_safepoint_lookup_metrics"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
export PATH=/opt/data/local/usr/bin:$PATH

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/gc_alloc_safepoint_lookup_metrics.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = { value }
}

def churn(anchor: Cell, count: Int): Int = {
    var i = 0
    var total = 0
    while i < count {
        val dead = new Cell(i)
        total = total + dead.get()
        i = i + 1
    }
    total + anchor.get()
}

def main(): Int = {
    val kept = new Cell(7)
    val total = churn(kept, 220)
    if kept.get() != 7 {
        10
    } else if total <= 0 {
        11
    } else if gcCollections() <= 0 {
        12
    } else if gcLastAllocSafepointMapFound() <= 0 {
        13
    } else if gcLastAllocSafepointMapMissed() != 0 {
        14
    } else if gcLastAllocSafepointRootSlots() != 1 {
        15
    } else if gcLastAllocSafepointRootBytes() != gcLastAllocSafepointRootSlots() * 8 {
        16
    } else {
        0
    }
}
NABLA

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 --keep-asm "$TMP_DIR/gc_alloc_safepoint_lookup_metrics.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/gc_alloc_safepoint_lookup_metrics"

cat >"$TMP_DIR/gc_alloc_safepoint_lookup_miss_reset.nabla" <<'NABLA'
def main(): Int = {
    var i = 0
    var s = "seed"
    while i < 180 {
        s = s + "x"
        i = i + 1
    }
    if s.length() <= 0 {
        20
    } else if gcCollections() <= 0 {
        21
    } else if gcLastAllocSafepointMapFound() != 0 {
        22
    } else if gcLastAllocSafepointMapMissed() <= 0 {
        23
    } else if gcLastAllocSafepointRootSlots() != 0 {
        24
    } else if gcLastAllocSafepointRootBytes() != 0 {
        25
    } else {
        0
    }
}
NABLA

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR/miss" "$BUILD_DIR/nablac" --heap-size 4096 "$TMP_DIR/gc_alloc_safepoint_lookup_miss_reset.nabla" >/dev/null
"$CUSTOM_BUILD_DIR/miss/gc_alloc_safepoint_lookup_miss_reset"

ASM="$CUSTOM_BUILD_DIR/gc_alloc_safepoint_lookup_metrics_tmp.asm"
python3 - "$ASM" <<'PY'
import sys
asm = open(sys.argv[1], encoding='utf-8').read()
required = [
    "gc_last_alloc_safepoint_map_found: dq 0",
    "gc_last_alloc_safepoint_map_missed: dq 0",
    "gc_last_alloc_safepoint_root_slots: dq 0",
    "gc_last_alloc_safepoint_root_bytes: dq 0",
    "gc_last_alloc_safepoint_map: dq 0",
    "mov rax, [rsp + 112]",
    "8-byte Runtime_gc return + 8-byte requested-size spill + 12 saved regs",
    "lea r8, [nabla_gc_alloc_safepoint_tables]",
    ".L_gc_alloc_safepoint_table_loop:",
    ".L_gc_alloc_safepoint_entry_loop:",
    ".L_gc_alloc_safepoint_lookup_found:",
    ".L_gc_alloc_safepoint_lookup_missed:",
    "cmp rax, [r11]",
    "add r11, 16",
    "mov r13, [r11 + 8]",
    "mov [gc_last_alloc_safepoint_map], r13",
    "Consume exact user-frame root offsets from the found allocation map",
    "then keep the conservative scan active as the fallback/propagation root set.",
    "mov r14, [r13]",
    "mov [gc_last_alloc_safepoint_root_slots], r14",
    "mov r15, r14",
    "shl r15, 3",
    "mov [gc_last_alloc_safepoint_root_bytes], r15",
    ".L_gc_exact_root_scan:",
    "sub rdi, [r15]",
    "call Runtime_gc_mark_candidate",
    "Runtime_gcLastAllocSafepointMapFound:",
    "Runtime_gcLastAllocSafepointMapMissed:",
    "Runtime_gcLastAllocSafepointRootSlots:",
    "Runtime_gcLastAllocSafepointRootBytes:",
    "call Runtime_gcLastAllocSafepointMapFound",
    "call Runtime_gcLastAllocSafepointMapMissed",
    "call Runtime_gcLastAllocSafepointRootSlots",
    "call Runtime_gcLastAllocSafepointRootBytes",
    "nabla_gc_alloc_safepoint_tables:",
    "nabla_gc_alloc_safepoints_main:",
    "nabla_gc_alloc_call_nabla_sym_churn_0: dq 1, 8",
]
missing = [item for item in required if item not in asm]
if missing:
    raise SystemExit("missing expected allocation safepoint lookup ASM markers: " + ", ".join(missing))
forbidden = [
    "Observational only: read the map header count, not root offsets.",
    "without consuming root offsets",
    "sans lire cette carte pour le marquage",
]
present_forbidden = [item for item in forbidden if item in asm]
if present_forbidden:
    raise SystemExit("allocation safepoint ASM still contains stale non-consuming wording: " + ", ".join(present_forbidden))
found_start = asm.index(".L_gc_alloc_safepoint_lookup_found:")
found_end = asm.index(".L_gc_alloc_safepoint_lookup_done:", found_start)
found_block = asm[found_start:found_end]
if "Runtime_gc_mark_candidate" not in found_block or "sub rdi, [r15]" not in found_block:
    raise SystemExit("allocation safepoint lookup should consume exact root offsets before fallback scan")
PY

for primitive in \
    gcLastAllocSafepointMapFound \
    gcLastAllocSafepointMapMissed \
    gcLastAllocSafepointRootSlots \
    gcLastAllocSafepointRootBytes; do
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

echo "PASS: GC allocation safepoint lookup metrics"
