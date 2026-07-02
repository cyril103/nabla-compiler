#!/usr/bin/env python3
"""Normalize simple source formatting for tracked Nabla repository text files.

The formatter intentionally stays dependency-free for now. It removes trailing
horizontal whitespace and ensures a final newline. That gives the repository a
stable `make format` baseline without requiring clang-format availability.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SUFFIXES = {
    ".cpp",
    ".hpp",
    ".h",
    ".nabla",
    ".md",
    ".py",
    ".sh",
    ".json",
    ".yml",
    ".yaml",
}
SKIP_PARTS = {".git", "build", "node_modules"}


def tracked_text_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    files: list[Path] = []
    for line in result.stdout.splitlines():
        path = ROOT / line
        if should_format(path):
            files.append(path)
    return files


def should_format(path: Path) -> bool:
    try:
        relative = path.relative_to(ROOT)
    except ValueError:
        relative = path
    if any(part in SKIP_PARTS for part in relative.parts):
        return False
    return path.is_file() and path.suffix in DEFAULT_SUFFIXES


def normalize(content: str) -> str:
    lines = content.splitlines()
    normalized = "\n".join(line.rstrip(" \t") for line in lines)
    return normalized + "\n"


def format_file(path: Path, check: bool) -> bool:
    try:
        original = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return False
    formatted = normalize(original)
    if formatted == original:
        return False
    if not check:
        path.write_text(formatted, encoding="utf-8")
    return True


def resolve_paths(arguments: Iterable[str]) -> list[Path]:
    paths = [Path(arg) for arg in arguments]
    if not paths:
        return tracked_text_files()

    resolved: list[Path] = []
    for path in paths:
        full_path = path if path.is_absolute() else ROOT / path
        if full_path.is_dir():
            for child in sorted(full_path.rglob("*")):
                if should_format(child):
                    resolved.append(child)
        elif should_format(full_path):
            resolved.append(full_path)
    return resolved


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Normalize tracked text files by trimming trailing whitespace and adding final newlines."
    )
    parser.add_argument("paths", nargs="*", help="Files or directories to format; defaults to tracked files.")
    parser.add_argument("--check", action="store_true", help="Report files that need formatting without modifying them.")
    args = parser.parse_args()

    changed: list[Path] = []
    for path in resolve_paths(args.paths):
        if format_file(path, args.check):
            changed.append(path)

    if args.check and changed:
        print("Files needing format:")
        for path in changed:
            print(f"  {display_path(path)}")
        return 1

    if changed:
        print("Formatted files:")
        for path in changed:
            print(f"  {display_path(path)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
