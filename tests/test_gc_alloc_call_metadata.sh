#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/gc_alloc_call_metadata.nabla" <<'NABLA'
class Box(label: String) {
    def text(): String = { label }
}

def acceptsAny(value: Any): Int = { 3 }

def allocate(prefix: String): Int = {
    val box = new Box(prefix)
    val ints = new IntArray(2)
    val refs = new ObjectArray[String](1)
    val make = (suffix: String) => { box.text() + suffix }
    val boxed = acceptsAny(42)
    make("!").length() + ints.length() + refs.length() + boxed
}

def main(): Int = {
    allocate("gc")
}
NABLA

(
    cd "$TMP_DIR"
    PATH=/opt/data/local/usr/bin:$PATH "$ROOT_DIR/build/nablac" --keep-asm gc_alloc_call_metadata.nabla >/dev/null
)

ASM="$TMP_DIR/gc_alloc_call_metadata_tmp.asm"
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

require_asm "nabla_gc_alloc_calls_nabla_sym_allocate: dq 5"
require_asm "nabla_gc_alloc_calls_main: dq 0"
require_asm "nabla_gc_alloc_call_nabla_sym_allocate_0: dq"
require_asm "; gc alloc call 0 object result"
require_asm "; gc alloc call 1 IntArray result"
require_asm "; gc alloc call 2 ObjectArray result"
require_asm "; gc alloc call 3 closure result"
require_asm "; gc alloc call 4 boxed-Int result"
require_asm "; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_0 kind object result"
require_asm "; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_1 kind IntArray result"
require_asm "; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_2 kind ObjectArray result"
require_asm "; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_3 kind closure result"
require_asm "; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_4 kind boxed-Int result"
require_asm " non-consumed"
require_asm "; gc alloc root [rbp -"
require_asm ": String"
require_asm ": Box"
require_asm ": IntArray"
require_asm ": ObjectArray[String]"

python3 - "$ASM" <<'PY'
import re
import sys
from pathlib import Path

asm = Path(sys.argv[1]).read_text(encoding="utf-8")
labels = re.findall(r"^nabla_gc_alloc_call_nabla_sym_allocate_(\d+): dq ([^\n]+)", asm, re.MULTILINE)
if [int(index) for index, _ in labels] != [0, 1, 2, 3, 4]:
    raise SystemExit(f"unexpected allocation call labels: {labels}")

index_match = re.search(r"^nabla_gc_alloc_calls_nabla_sym_allocate: dq ([^\n]+)", asm, re.MULTILINE)
if not index_match:
    raise SystemExit("missing allocation call descriptor index")
index_parts = [part.strip() for part in index_match.group(1).split(",")]
if int(index_parts[0]) != len(labels):
    raise SystemExit(
        f"descriptor count {index_parts[0]} does not match allocation labels {len(labels)}"
    )

blocks = {}
for index, _ in labels:
    pattern = rf"^nabla_gc_alloc_call_nabla_sym_allocate_{index}: dq [^\n]+\n(.*?)(?=^nabla_gc_alloc_call_nabla_sym_allocate_|^nabla_gc_alloc_calls_nabla_sym_allocate:)"
    match = re.search(pattern, asm, re.MULTILINE | re.DOTALL)
    if not match:
        raise SystemExit(f"missing block for allocation call {index}")
    blocks[int(index)] = match.group(1)

expected = {
    0: ["%prefix: String"],
    1: ["%prefix: String", "%0: Box", "box#2: Box"],
    2: ["%prefix: String", "%0: Box", "box#2: Box", "%2: IntArray", "ints#3: IntArray"],
    3: ["%4: ObjectArray[String]", "refs#4: ObjectArray[String]", "%5: Box"],
    4: ["make#6: Fn(String)->String"],
}
for index, needles in expected.items():
    for needle in needles:
        if needle not in blocks[index]:
            raise SystemExit(f"allocation call {index} missing root {needle}")

future_roots = {
    0: ["%0: Box", "IntArray", "ObjectArray[String]", "Fn(String)->String", ": Any"],
    1: ["ObjectArray[String]", "Fn(String)->String", ": Any"],
    2: ["Fn(String)->String", ": Any"],
    3: [": Any"],
}
for index, needles in future_roots.items():
    for needle in needles:
        if needle in blocks[index]:
            raise SystemExit(f"allocation call {index} should not include future/unavailable root {needle}")

for index, values in labels:
    parts = [part.strip() for part in values.split(",")]
    declared_count = int(parts[0])
    offsets = parts[1:]
    if declared_count != len(offsets):
        raise SystemExit(f"allocation call {index} declares {declared_count} roots but has {len(offsets)} offsets")
    if len(offsets) != len(set(offsets)):
        raise SystemExit(f"allocation call {index} contains duplicate offsets: {offsets}")

function_match = re.search(
    r"^nabla_sym_allocate:\n(.*?)(?=^main:)",
    asm,
    re.MULTILINE | re.DOTALL,
)
if not function_match:
    raise SystemExit("missing generated allocate function body")
function_body = function_match.group(1)
lines = function_body.splitlines()
runtime_alloc_calls = []
for index, line in enumerate(lines):
    if line.strip() == "call Runtime_alloc":
        previous_line = lines[index - 1].strip() if index > 0 else ""
        runtime_alloc_calls.append((index + 1, previous_line))
metadata_comments = {}
metadata_re = re.compile(
    r"; gc alloc call (?P<index>\d+) (?P<kind>\S+) result (?P<result>\S+)(?: op (?P<op>\S+))?"
)
for index, block in blocks.items():
    match = metadata_re.search(block)
    if not match:
        raise SystemExit(f"allocation call {index} missing metadata comment")
    metadata_comments[index] = match.groupdict()

if len(runtime_alloc_calls) != len(metadata_comments):
    raise SystemExit(
        f"expected {len(metadata_comments)} direct user Runtime_alloc calls, got {len(runtime_alloc_calls)}"
    )

safepoint_re = re.compile(
    r"; gc alloc safepoint map nabla_gc_alloc_call_nabla_sym_allocate_(?P<index>\d+) "
    r"kind (?P<kind>\S+) result (?P<result>\S+)(?: op (?P<op>\S+))? non-consumed$"
)
for line_number, previous_line in runtime_alloc_calls:
    match = safepoint_re.search(previous_line)
    if not match:
        raise SystemExit(
            f"Runtime_alloc at allocate line {line_number} missing immediate safepoint comment; "
            f"previous line was {previous_line!r}"
        )
    index = int(match.group("index"))
    expected = metadata_comments.get(index)
    if expected is None:
        raise SystemExit(f"safepoint comment references unknown metadata index {index}")
    for field in ["kind", "result", "op"]:
        if (match.group(field) or None) != (expected.get(field) or None):
            raise SystemExit(
                f"safepoint comment field mismatch for {index} field {field}: "
                f"{match.group(field)!r} != {expected.get(field)!r}"
            )
PY

if grep -Fq "; gc alloc root" "$ASM" && ! grep -Fq "nabla_gc_frame_roots_nabla_sym_allocate" "$ASM"; then
    echo "allocation call roots should be paired with a frame root descriptor" >&2
    exit 1
fi

set +e
"$TMP_DIR/gc_alloc_call_metadata" >/dev/null
status=$?
set -e
if [ "$status" -ne 9 ]; then
    echo "expected executable exit status 9, got $status" >&2
    exit 1
fi
