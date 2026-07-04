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


source = "def main(): Int = { 0 }\n"

with tempfile.TemporaryDirectory(prefix="nabla_gc_runtime_helper_root_maps_") as tmp:
    tmp_path = Path(tmp)
    source_path = tmp_path / "gc_runtime_helper_root_maps.nabla"
    source_path.write_text(source, encoding="utf-8")

    env = os.environ.copy()
    env["PATH"] = f"/opt/data/local/usr/bin:{env.get('PATH', '')}"
    result = subprocess.run(
        [str(NABLAC), "--keep-asm", source_path.name],
        cwd=tmp_path,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(
        result.returncode == 0,
        "nablac --keep-asm failed while generating runtime helper GC metadata\n"
        f"stdout={result.stdout}\nstderr={result.stderr}",
    )

    asm_path = tmp_path / "gc_runtime_helper_root_maps_tmp.asm"
    require(asm_path.exists(), f"expected generated asm file missing: {asm_path}")
    asm = asm_path.read_text(encoding="utf-8")

required_wording = [
    "GC runtime-helper allocation maps are additive, conservative candidates",
    "not consumed by Runtime_alloc",
    "do not enable collection",
]
for wording in required_wording:
    require(wording in asm, f"generated asm should document inert runtime helper maps: {wording}")

expected_roots = {
    "Runtime_buildArgsArray": [[], ["native_stack+8"]],
    "Runtime_stringToCharArray": [["native_stack+8"], ["native_stack+8"]],
    "Runtime_stringSplit": [
        ["r10", "r11", "interior:r14", "interior:r15"],
        ["r10", "r11", "interior:r14"],
        ["rbx"],
    ],
    "Runtime_stringSplitMakeSegment": [["rbx", "r10", "interior:r14"]],
    "FloatDouble_method_toString": [["native_stack+16"], ["native_stack+16"]],
}

for helper, alloc_roots in expected_roots.items():
    index_match = re.search(
        rf"^\s*nabla_gc_runtime_helper_allocs_{re.escape(helper)}: dq ([^\n]+)",
        asm,
        re.MULTILINE,
    )
    require(index_match is not None, f"missing runtime helper allocation index for {helper}")
    index_parts = [part.strip() for part in index_match.group(1).split(",")]
    require(index_parts, f"empty runtime helper allocation index for {helper}")
    require(
        index_parts[0] == str(len(alloc_roots)),
        f"runtime helper allocation count drifted for {helper}: {index_parts[0]}",
    )
    expected_labels = [
        f"nabla_gc_runtime_helper_alloc_{helper}_{index}"
        for index in range(len(alloc_roots))
    ]
    require(
        index_parts[1:] == expected_labels,
        f"runtime helper allocation index labels drifted for {helper}: {index_parts[1:]}",
    )

    for index, roots in enumerate(alloc_roots):
        label = f"nabla_gc_runtime_helper_alloc_{helper}_{index}"
        next_label = (
            f"nabla_gc_runtime_helper_alloc_{helper}_{index + 1}"
            if index + 1 < len(alloc_roots)
            else f"nabla_gc_runtime_helper_allocs_"
        )
        body_match = re.search(
            rf"^\s*{re.escape(label)}: dq ([0-9]+)(.*?)(?=^\s*{re.escape(next_label)}|^\s*nabla_gc_runtime_helper_allocs_|^section )",
            asm,
            re.MULTILINE | re.DOTALL,
        )
        if body_match is None:
            raise SystemExit(f"missing runtime helper allocation map {label}")
        require(
            body_match.group(1) == str(len(roots)),
            f"runtime helper root count drifted for {label}: {body_match.group(1)}",
        )
        body = body_match.group(2)
        for root in roots:
            descriptor = root
            comment = root.replace("+", " +")
            require(
                f'"{descriptor}", 0' in body or f"gc runtime helper root {comment}" in body,
                f"missing root descriptor/comment for {label}: {root}",
            )

print("PASS: generated ASM contains inert GC runtime helper allocation root maps")
