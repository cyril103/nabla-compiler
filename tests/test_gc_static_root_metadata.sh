#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/gc_static_roots.nabla" <<'NABLA'
trait Named {
    def name(): String
}

object Current with Named {
    override def name(): String = {
        "gc"
    }
}

def describe(value: Named): String = {
    value.name() + "-root"
}

def main(): Int = {
    val text = describe(Current)
    val escaped = "line\nroot"
    if text == "gc-root" && Current.name() == "gc" && escaped == "line\nroot" {
        42
    } else {
        1
    }
}
NABLA

(
    cd "$TMP_DIR"
    PATH=/opt/data/local/usr/bin:$PATH "$ROOT_DIR/build/nablac" --keep-asm gc_static_roots.nabla >/dev/null
)

ASM="$TMP_DIR/gc_static_roots_tmp.asm"
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

require_asm "nabla_gc_static_roots: dq "
require_asm "nabla_sym_singleton_2eCurrent"
require_asm "; gc static root runtime singleton object nabla_sym_singleton_2eCurrent source Current"
require_asm "; gc static root static string literal object"
require_asm " source gc"
require_asm " source -root"
require_asm " source gc-root"
require_asm " source line\\nroot"

python3 - "$ASM" <<'PY'
import re
import sys
asm = open(sys.argv[1], encoding="utf-8").read()
match = re.search(r"^nabla_gc_static_roots: dq (\d+)([^\n]*)", asm, re.MULTILINE)
if not match:
    raise SystemExit("missing nabla_gc_static_roots descriptor")
count = int(match.group(1))
labels = [label.strip() for label in match.group(2).split(",") if label.strip()]
if count != len(labels):
    raise SystemExit(f"descriptor count {count} does not match labels {len(labels)}")
comments = re.findall(r"^    ; gc static root ", asm, re.MULTILINE)
if count != len(comments):
    raise SystemExit(f"descriptor count {count} does not match comments {len(comments)}")
if not any("nabla_sym_singleton_2eCurrent" in label for label in labels):
    raise SystemExit("singleton label missing from descriptor")
string_labels = [label for label in labels if label.startswith("nabla_string_") and label.endswith("_obj")]
if len(string_labels) < 5:
    raise SystemExit(f"expected several static string roots, got {len(string_labels)}")
PY

if grep -Eq 'Runtime_(gc|alloc).*nabla_gc_static_roots|nabla_gc_static_roots.*Runtime_(gc|alloc)' "$ASM"; then
    echo "static root metadata must remain runtime-inert" >&2
    exit 1
fi

set +e
"$TMP_DIR/gc_static_roots" >/dev/null
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "expected executable exit status 42, got $status" >&2
    exit 1
fi
