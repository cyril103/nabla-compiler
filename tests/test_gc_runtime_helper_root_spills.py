#!/usr/bin/env python3
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NABLAC = ROOT / "build" / "nablac"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def require_ordered(body: str, needles: list[str], context: str) -> None:
    offset = 0
    for needle in needles:
        index = body.find(needle, offset)
        require(index >= 0, f"missing or out-of-order {needle!r} in {context}")
        offset = index + len(needle)


source = "import collections.array\n\ndef main(args: Array[String]): Int = { args.size() }\n"

tmp_parent = ROOT / "build"
tmp_parent.mkdir(exist_ok=True)

with tempfile.TemporaryDirectory(
    prefix="nabla_gc_runtime_helper_root_spills_",
    dir=tmp_parent,
) as tmp:
    tmp_path = Path(tmp)
    source_path = tmp_path / "gc_runtime_helper_root_spills.nabla"
    source_path.write_text(source, encoding="utf-8")

    env = os.environ.copy()
    env["PATH"] = f"/opt/data/local/usr/bin:{env.get('PATH', '')}"
    env["NABLA_BUILD_DIR"] = str(tmp_path)
    result = subprocess.run(
        [str(NABLAC), "--keep-asm", str(source_path)],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(
        result.returncode == 0,
        "nablac --keep-asm failed while generating Runtime_buildArgsArray\n"
        f"stdout={result.stdout}\nstderr={result.stderr}",
    )

    asm_path = tmp_path / "gc_runtime_helper_root_spills_tmp.asm"
    require(asm_path.exists(), f"expected generated asm file missing: {asm_path}")
    asm = asm_path.read_text(encoding="utf-8")

required_wording = [
    "not consumed by Runtime_alloc",
    "do not enable collection",
]
for wording in required_wording:
    require(wording in asm, f"generated asm should keep inert GC wording: {wording}")

block_match = re.search(
    r"^Runtime_buildArgsArray:\n(.*?)(?=^Runtime_alloc:)",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(block_match is not None, "missing Runtime_buildArgsArray block in generated ASM")
build_args = block_match.group(1)

require_ordered(
    build_args,
    [
        "; gc runtime helper root spill begin: preserve r15 raw args array across Runtime_cStringToString",
        "push r15",
        "call Runtime_cStringToString",
        "pop r15",
        "; gc runtime helper root spill end: restore r15 raw args array after Runtime_cStringToString",
    ],
    "Runtime_cStringToString spill",
)

require_ordered(
    build_args,
    [
        "; gc runtime helper root spill begin: preserve r15 raw args array across final Runtime_alloc",
        "push r15",
        "call Runtime_alloc",
        "pop r15",
        "; gc runtime helper root spill end: restore r15 raw args array after final Runtime_alloc",
    ],
    "final Runtime_alloc spill",
)

map_match = re.search(
    r"^\s*nabla_gc_runtime_helper_alloc_Runtime_buildArgsArray_1: dq 1"
    r"(.*?)(?=^\s*nabla_gc_runtime_helper_allocs_|^section )",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(
    map_match is not None,
    "missing Runtime_buildArgsArray alloc 1 helper root map",
)
require(
    '"native_stack+8", 0' in map_match.group(1),
    "Runtime_buildArgsArray alloc 1 should describe spilled r15 as native_stack+8",
)

block_match = re.search(
    r"^Runtime_stringToCharArray:\n(.*?)(?=^Runtime_stringSubstring:)",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(block_match is not None, "missing Runtime_stringToCharArray block in generated ASM")
string_to_char_array = block_match.group(1)

require_ordered(
    string_to_char_array,
    [
        "mov r11, [rdi + 8]",
        "mov r10, [rdi + 16]",
        "; gc runtime helper root spill begin: preserve source String owner across raw char array Runtime_alloc",
        "push rdi",
        "lea rdi, [r11 * 8 + 16]",
        "call Runtime_alloc",
        "pop rdi",
        "; gc runtime helper root spill end: restore source String owner after raw char array Runtime_alloc",
    ],
    "Runtime_stringToCharArray raw char array allocation spill",
)

require_ordered(
    string_to_char_array,
    [
        ".L_string_to_char_array_wrap:",
        "mov rdi, 16",
        "; gc runtime helper root spill begin: preserve rbx raw ObjectArray[Char] across facade Runtime_alloc",
        "push rbx",
        "call Runtime_alloc",
        "pop rbx",
        "; gc runtime helper root spill end: restore rbx raw ObjectArray[Char] after facade Runtime_alloc",
    ],
    "Runtime_stringToCharArray facade allocation spill",
)

for index in (0, 1):
    map_match = re.search(
        rf"^\s*nabla_gc_runtime_helper_alloc_Runtime_stringToCharArray_{index}: dq 1"
        r"(.*?)(?=^\s*nabla_gc_runtime_helper_alloc_|^\s*nabla_gc_runtime_helper_allocs_|^section )",
        asm,
        re.MULTILINE | re.DOTALL,
    )
    require(
        map_match is not None,
        f"missing Runtime_stringToCharArray alloc {index} helper root map",
    )
    require(
        '"native_stack+8", 0' in map_match.group(1),
        f"Runtime_stringToCharArray alloc {index} should describe native_stack+8",
    )

block_match = re.search(
    r"^Runtime_stringSplit:\n(.*?)(?=^Runtime_stringSplitMakeSegment:)",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(block_match is not None, "missing Runtime_stringSplit block in generated ASM")
string_split = block_match.group(1)

require_ordered(
    string_split,
    [
        ".L_string_split_allocate:",
        "lea rdi, [r8 * 8 + 16]",
        "; gc runtime helper root spill begin: preserve source/separator String owners across raw split array Runtime_alloc",
        "push r10",
        "push r11",
        "call Runtime_alloc",
        "pop r11",
        "pop r10",
        "; gc runtime helper root spill end: restore source/separator String owners after raw split array Runtime_alloc",
    ],
    "Runtime_stringSplit non-empty separator raw array allocation spill",
)

require_ordered(
    string_split,
    [
        ".L_string_split_empty_separator:",
        "mov r8, r12",
        "lea rdi, [r8 * 8 + 16]",
        "; gc runtime helper root spill begin: preserve source/separator String owners across empty split array Runtime_alloc",
        "push r10",
        "push r11",
        "call Runtime_alloc",
        "pop r11",
        "pop r10",
        "; gc runtime helper root spill end: restore source/separator String owners after empty split array Runtime_alloc",
    ],
    "Runtime_stringSplit empty separator raw array allocation spill",
)

require_ordered(
    string_split,
    [
        ".L_string_split_wrap:",
        "mov rdi, 16",
        "; gc runtime helper root spill begin: preserve rbx raw ObjectArray[String] across split facade Runtime_alloc",
        "push rbx",
        "call Runtime_alloc",
        "pop rbx",
        "; gc runtime helper root spill end: restore rbx raw ObjectArray[String] after split facade Runtime_alloc",
    ],
    "Runtime_stringSplit facade allocation spill",
)

split_expected = {
    0: ['"native_stack+8", 0', '"native_stack+16", 0', '"interior:r14", 0', '"interior:r15", 0'],
    1: ['"native_stack+8", 0', '"native_stack+16", 0', '"interior:r14", 0'],
    2: ['"native_stack+8", 0'],
}
for index, descriptors in split_expected.items():
    map_match = re.search(
        rf"^\s*nabla_gc_runtime_helper_alloc_Runtime_stringSplit_{index}: dq {len(descriptors)}"
        r"(.*?)(?=^\s*nabla_gc_runtime_helper_alloc_|^\s*nabla_gc_runtime_helper_allocs_|^section )",
        asm,
        re.MULTILINE | re.DOTALL,
    )
    require(map_match is not None, f"missing Runtime_stringSplit alloc {index} helper root map")
    for descriptor in descriptors:
        require(
            descriptor in map_match.group(1),
            f"Runtime_stringSplit alloc {index} should describe {descriptor}",
        )

block_match = re.search(
    r"^Runtime_stringSplitMakeSegment:\n(.*?)(?=^Int_method_toString:)",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(block_match is not None, "missing Runtime_stringSplitMakeSegment block in generated ASM")
string_split_make_segment = block_match.group(1)

require_ordered(
    string_split_make_segment,
    [
        "push r8",
        "push r9",
        "push rdx",
        "; gc runtime helper root spill begin: preserve split destination/source owners across segment Runtime_alloc",
        "push rbx",
        "push r10",
        "call Runtime_alloc",
        "pop r10",
        "pop rbx",
        "; gc runtime helper root spill end: restore split destination/source owners after segment Runtime_alloc",
        "pop rdx",
        "pop r9",
        "pop r8",
    ],
    "Runtime_stringSplitMakeSegment segment allocation spill",
)

map_match = re.search(
    r"^\s*nabla_gc_runtime_helper_alloc_Runtime_stringSplitMakeSegment_0: dq 3"
    r"(.*?)(?=^\s*nabla_gc_runtime_helper_alloc_|^\s*nabla_gc_runtime_helper_allocs_|^section )",
    asm,
    re.MULTILINE | re.DOTALL,
)
require(map_match is not None, "missing Runtime_stringSplitMakeSegment alloc 0 helper root map")
for descriptor in ['"native_stack+8", 0', '"native_stack+16", 0', '"interior:r14", 0']:
    require(
        descriptor in map_match.group(1),
        f"Runtime_stringSplitMakeSegment alloc 0 should describe {descriptor}",
    )

print("PASS: runtime helper root spills preserve native owners around allocing helper calls")
