#!/usr/bin/env python3
"""Validate the checked-in VS Code extension metadata."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTENSION = ROOT / "editor" / "vscode"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(path: Path) -> dict:
    if not path.exists():
        fail(f"missing {path.relative_to(ROOT)}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON in {path.relative_to(ROOT)}: {exc}")


def require_file(path: Path) -> None:
    if not path.exists():
        fail(f"missing {path.relative_to(ROOT)}")


def main() -> None:
    package = load_json(EXTENSION / "package.json")
    if package.get("name") != "nabla-vscode":
        fail("unexpected VS Code extension name")

    contributes = package.get("contributes")
    if not isinstance(contributes, dict):
        fail("package.json must declare contributes")

    languages = contributes.get("languages", [])
    if not any(
        language.get("id") == "nabla" and ".nabla" in language.get("extensions", [])
        for language in languages
    ):
        fail("package.json must register the nabla language for .nabla files")

    grammars = contributes.get("grammars", [])
    if not any(grammar.get("language") == "nabla" for grammar in grammars):
        fail("package.json must register a Nabla grammar")

    snippets = contributes.get("snippets", [])
    if not any(snippet.get("language") == "nabla" for snippet in snippets):
        fail("package.json must register Nabla snippets")

    for language in languages:
        config = language.get("configuration")
        if config:
            load_json(EXTENSION / config)

    for grammar in grammars:
        path = grammar.get("path")
        if not path:
            fail("grammar contribution is missing a path")
        grammar_json = load_json(EXTENSION / path)
        if grammar_json.get("scopeName") != "source.nabla":
            fail("Nabla grammar must use scopeName source.nabla")

    for snippet in snippets:
        path = snippet.get("path")
        if not path:
            fail("snippet contribution is missing a path")
        load_json(EXTENSION / path)

    require_file(EXTENSION / "README.md")
    print("PASS: VS Code extension metadata")


if __name__ == "__main__":
    main()
