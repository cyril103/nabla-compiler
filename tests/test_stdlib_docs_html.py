#!/usr/bin/env python3
"""Validate generated stdlib HTML navigation and Scala-style API sections."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "stdlib"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def assert_exists(path: Path, label: str) -> None:
    if not path.exists():
        fail(f"{label} missing: {path}")


def main() -> None:
    assert_exists(DOCS / "index.html", "stdlib docs index")
    assert_exists(DOCS / "style.css", "stdlib docs stylesheet")

    html_pages = sorted(DOCS.rglob("*.html"))
    if not html_pages:
        fail("no generated stdlib HTML pages found")

    for page in html_pages:
        content = page.read_text(encoding="utf-8")
        hrefs = re.findall(r'href="([^"]+)"', content)
        for href in hrefs:
            if href.startswith(("http://", "https://", "#", "mailto:")):
                continue
            target = (page.parent / href).resolve()
            if not target.exists():
                fail(f"broken local link in {page.relative_to(DOCS)}: {href}")

        ids = re.findall(r'id="([^"]+)"', content)
        duplicate_ids = sorted({entry_id for entry_id in ids if ids.count(entry_id) > 1})
        if duplicate_ids:
            fail(f"duplicate ids in {page.relative_to(DOCS)}: {', '.join(duplicate_ids)}")

        if page.name != "index.html":
            if 'class="doc-shell"' not in content:
                fail(f"missing doc-shell layout in {page.relative_to(DOCS)}")
            if 'aria-label="Fil d' not in content:
                fail(f"missing accessible breadcrumb label in {page.relative_to(DOCS)}")
            if 'class="skip-link"' not in content:
                fail(f"missing skip link in {page.relative_to(DOCS)}")
            if "API publique" not in content:
                fail(f"missing public API sidebar in {page.relative_to(DOCS)}")
            if 'class="signature"' not in content:
                fail(f"missing method signatures in {page.relative_to(DOCS)}")

    for relative in [
        "collections/array.html",
        "collections/map.html",
        "collections/set.html",
        "core/option.html",
        "math.html",
    ]:
        page = DOCS / relative
        assert_exists(page, f"documented example page {relative}")
        content = page.read_text(encoding="utf-8")
        if 'class="example"' not in content:
            fail(f"missing example block in {relative}")

    option = (DOCS / "core" / "option.html").read_text(encoding="utf-8")
    for method in ["isDefined", "isEmpty", "nonEmpty", "map", "flatMap", "foreach", "filter", "orElse", "getOrElse"]:
        if f">{method}<" not in option:
            fail(f"Option method missing from generated docs: {method}")

    print("PASS: stdlib HTML docs links, layout, signatures and examples")


if __name__ == "__main__":
    main()
