#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/gc_frame_roots.nabla" <<'NABLA'
class Box(value: String) {
    def text(): String = { value }
}

def keep(name: String, count: Int): Int = {
    val xs = new IntArray(count)
    val box = new Box(name)
    xs.set(0, count)
    box.text().length() + xs.get(0)
}

def main(): Int = {
    keep("gc", 40)
}
NABLA

(
    cd "$TMP_DIR"
    PATH=/opt/data/local/usr/bin:$PATH "$ROOT_DIR/build/nablac" --keep-asm gc_frame_roots.nabla >/dev/null
)

ASM="$TMP_DIR/gc_frame_roots_tmp.asm"
if [ ! -f "$ASM" ]; then
    echo "expected kept assembly file: $ASM" >&2
    exit 1
fi

require_asm() {
    local needle=$1
    if ! grep -Fq "$needle" "$ASM"; then
        echo "expected assembly to contain: $needle" >&2
        exit 1
    fi
}

require_asm "nabla_gc_frame_roots_nabla_sym_keep: dq"
require_asm "nabla_gc_frame_roots_nabla_sym_method_2eBox_2etext: dq"
require_asm "nabla_gc_frame_roots_main: dq"
require_asm "%name: String"
require_asm ": IntArray"
require_asm ": Box"
if grep -Eq '; gc root \[rbp - [0-9]+\] %count: Int' "$ASM"; then
    echo "assembly should not mark %count: Int as a GC root" >&2
    exit 1
fi

set +e
"$TMP_DIR/gc_frame_roots" >/dev/null
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "expected executable exit status 42, got $status" >&2
    exit 1
fi
