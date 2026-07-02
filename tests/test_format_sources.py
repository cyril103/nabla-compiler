#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "format_sources.py"


def run_format(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        dirty = Path(tmp) / "dirty.nabla"
        dirty.write_text("def main(): Int = { 42 }   \n", encoding="utf-8")

        check_dirty = run_format("--check", str(dirty))
        if check_dirty.returncode == 0:
            print("FAIL: --check should reject trailing whitespace", file=sys.stderr)
            return 1
        if "dirty.nabla" not in check_dirty.stdout + check_dirty.stderr:
            print("FAIL: --check should name the dirty file", file=sys.stderr)
            return 1

        fix_dirty = run_format(str(dirty))
        if fix_dirty.returncode != 0:
            print(f"FAIL: formatter failed: {fix_dirty.stderr}", file=sys.stderr)
            return 1
        if dirty.read_text(encoding="utf-8") != "def main(): Int = { 42 }\n":
            print("FAIL: formatter did not remove trailing whitespace", file=sys.stderr)
            return 1

        check_clean = run_format("--check", str(dirty))
        if check_clean.returncode != 0:
            print(f"FAIL: clean file rejected: {check_clean.stderr}", file=sys.stderr)
            return 1

        missing_newline = Path(tmp) / "missing_newline.md"
        missing_newline.write_text("# Title", encoding="utf-8")
        fix_missing_newline = run_format(str(missing_newline))
        if fix_missing_newline.returncode != 0:
            print(
                f"FAIL: formatter rejected missing newline: {fix_missing_newline.stderr}",
                file=sys.stderr,
            )
            return 1
        if missing_newline.read_text(encoding="utf-8") != "# Title\n":
            print("FAIL: formatter did not add final newline", file=sys.stderr)
            return 1

    print("PASS: format_sources normalizes whitespace and supports --check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
