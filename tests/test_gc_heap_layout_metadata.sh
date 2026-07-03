#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/gc_heap_layout_metadata.nabla" <<'NABLA'
class Box(label: String, count: Int) {
    def text(): String = { label }
}

class PairBox[T](first: T, second: Int) {
    def get(): T = { first }
}

def closureValue(prefix: String, count: Int): Int = {
    val box = new Box(prefix, count)
    val tagger = (suffix: String) => {
        if count > 0 {
            box.text() + suffix
        } else {
            suffix
        }
    }
    tagger("!").length() + count
}

def main(): Int = {
    val pair = new PairBox[String]("gc", 7)
    closureValue(pair.get(), 33) + 2
}
NABLA

(
    cd "$TMP_DIR"
    PATH=/opt/data/local/usr/bin:$PATH "$ROOT_DIR/build/nablac" --keep-asm gc_heap_layout_metadata.nabla >/dev/null
)

ASM="$TMP_DIR/gc_heap_layout_metadata_tmp.asm"
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

require_asm "nabla_gc_object_layout_nabla_sym_Box: dq 1, 8"
require_asm "; gc field [Box + 8] label: String"
require_asm "nabla_gc_object_layout_nabla_sym_PairBox_5bString_5d: dq 1, 8"
require_asm "; gc field [PairBox[String] + 8] first: String"
require_asm "nabla_gc_closure_layout_nabla_sym_closureValue_"
if ! grep -Eq '; gc capture \[closure \+ [0-9]+\] .*: Box' "$ASM"; then
    echo "expected assembly to mark captured Box as a GC capture" >&2
    exit 1
fi

if grep -Eq '; gc field \[[^]]+\] count: Int' "$ASM"; then
    echo "assembly should not mark Box.count: Int as a GC field" >&2
    exit 1
fi
if grep -Eq '; gc field \[[^]]+\] second: Int' "$ASM"; then
    echo "assembly should not mark PairBox.second: Int as a GC field" >&2
    exit 1
fi
if grep -Eq '; gc capture \[closure \+ [0-9]+\] %count: Int' "$ASM"; then
    echo "assembly should not mark captured Int slots as GC captures" >&2
    exit 1
fi

set +e
"$TMP_DIR/gc_heap_layout_metadata" >/dev/null
status=$?
set -e
if [ "$status" -ne 38 ]; then
    echo "expected executable exit status 38, got $status" >&2
    exit 1
fi
