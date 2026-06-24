#!/usr/bin/env python3
"""Generate HTML API docs from Nabla stdlib doc comments.

Public documentation is opt-in: only declarations preceded by `///` comments
are emitted. This keeps low-level helpers available to the compiler and stdlib
without exposing them in the user-facing reference.
"""

from __future__ import annotations

import html
import re
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STDLIB_DIR = ROOT / "stdlib"
OUTPUT_DIR = ROOT / "docs" / "stdlib"


DECL_RE = re.compile(r"^\s*(?:override\s+)?(def|class)\s+(.+?)\s*(?:=\s*\{|$)")


@dataclass
class Entry:
    kind: str
    name: str
    signature: str
    status: str | None
    description: list[str]


@dataclass
class ModuleDoc:
    path: Path
    import_name: str
    title: str
    description: list[str]
    entries: list[Entry]


def module_name(path: Path) -> str:
    relative = path.relative_to(STDLIB_DIR).with_suffix("")
    return ".".join(relative.parts)


def title_from_module(import_name: str) -> str:
    return import_name.split(".")[-1].replace("_", " ").title()


def clean_doc_line(line: str) -> str:
    text = line.strip()
    if text.startswith("///"):
        text = text[3:]
    return text.strip()


def symbol_name(kind: str, signature_tail: str) -> str:
    if kind == "class":
        return signature_tail.split("(", 1)[0].split("[", 1)[0].split()[0]
    return signature_tail.split("(", 1)[0].split("[", 1)[0].strip()


def split_summary(lines: list[str]) -> tuple[str | None, list[str], list[str]]:
    if lines and lines[0].startswith("@module "):
        rest = lines[1:]
        if "" in rest:
            separator = rest.index("")
            return lines[0][8:].strip(), rest[:separator], rest[separator + 1 :]
        return lines[0][8:].strip(), rest, []
    return None, [], lines


def extract_metadata(lines: list[str], fallback: str) -> tuple[str, str | None, list[str]]:
    kept: list[str] = []
    signature = fallback
    status: str | None = None
    for line in lines:
        if line.startswith("@signature "):
            signature = line[11:].strip()
        elif line.startswith("@status "):
            status = line[8:].strip()
        else:
            kept.append(line)
    return signature, status, kept


def entry_from_signature(signature: str, description: list[str]) -> Entry:
    declaration = DECL_RE.match(signature)
    if not declaration:
        raise ValueError(f"Invalid documented symbol signature: {signature}")
    kind = declaration.group(1)
    signature, status, description = extract_metadata(description, signature)
    return Entry(
        kind=kind,
        name=symbol_name(kind, declaration.group(2).strip()),
        signature=signature,
        status=status,
        description=description,
    )


def parse_module(path: Path) -> ModuleDoc:
    import_name = module_name(path)
    pending_doc: list[str] = []
    module_title: str | None = None
    module_description: list[str] = []
    entries: list[Entry] = []

    def finish_module_doc() -> None:
        nonlocal module_title, module_description, pending_doc
        if pending_doc and pending_doc[0].startswith("@module "):
            title, module_doc_lines, _ = split_summary(pending_doc)
            module_title = title
            module_description = module_doc_lines
            pending_doc = []

    def finish_symbol_doc() -> None:
        nonlocal pending_doc
        if pending_doc and pending_doc[0].startswith("@symbol "):
            signature = pending_doc[0][8:].strip()
            entries.append(entry_from_signature(signature, pending_doc[1:]))
            pending_doc = []

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if stripped.startswith("///"):
            pending_doc.append(clean_doc_line(raw_line))
            continue

        if not stripped:
            finish_module_doc()
            finish_symbol_doc()
            continue

        declaration = DECL_RE.match(raw_line)
        if declaration and pending_doc:
            kind = declaration.group(1)
            signature_tail = declaration.group(2).strip()
            title, module_doc_lines, doc_lines = split_summary(pending_doc)
            signature = raw_line.strip()
            if signature.endswith("{"):
                signature = signature[:-1].rstrip()
            if signature.endswith("="):
                signature = signature[:-1].rstrip()
            signature, status, doc_lines = extract_metadata(doc_lines, signature)

            if title is not None:
                module_title = title
                module_description = module_doc_lines
            if doc_lines:
                entries.append(
                    Entry(
                        kind=kind,
                        name=symbol_name(kind, signature_tail),
                        signature=signature,
                        status=status,
                        description=doc_lines,
                    )
                )
            pending_doc = []
            continue

        if stripped and not stripped.startswith("//"):
            finish_module_doc()
            finish_symbol_doc()
            pending_doc = []

    return ModuleDoc(
        path=path,
        import_name=import_name,
        title=module_title or title_from_module(import_name),
        description=module_description,
        entries=entries,
    )


def render_paragraphs(lines: list[str]) -> str:
    paragraphs: list[str] = []
    current: list[str] = []
    for line in lines:
        if not line:
            if current:
                paragraphs.append(" ".join(current))
                current = []
            continue
        current.append(line)
    if current:
        paragraphs.append(" ".join(current))
    return "\n".join(f"<p>{html.escape(paragraph)}</p>" for paragraph in paragraphs)


def page_template(title: str, body: str, depth: int = 0) -> str:
    prefix = "../" * depth
    return f"""<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{html.escape(title)} - Nabla stdlib</title>
  <link rel="stylesheet" href="{prefix}style.css">
</head>
<body>
  <header>
    <a class="brand" href="{prefix}index.html">Nabla stdlib</a>
    <span>Reference API publique</span>
  </header>
  <main>
{body}
  </main>
</body>
</html>
"""


def render_module(module: ModuleDoc) -> str:
    seen_entry_ids: dict[str, int] = {}
    rendered_entries: list[str] = []
    for entry in module.entries:
        seen_count = seen_entry_ids.get(entry.name, 0) + 1
        seen_entry_ids[entry.name] = seen_count
        entry_id = entry.name if seen_count == 1 else f"{entry.name}-{seen_count}"
        rendered_entries.append(
            f"""    <article class="entry" id="{html.escape(entry_id)}">
      <div class="entry-meta">
        <span class="kind">{html.escape(entry.kind)}</span>{render_status(entry.status)}
      </div>
      <h2>{html.escape(entry.name)}</h2>
      <pre><code>{html.escape(entry.signature)}</code></pre>
      {render_paragraphs(entry.description)}
    </article>"""
        )
    entries = "\n".join(rendered_entries)
    if not entries:
        entries = "    <p class=\"empty\">Aucun symbole public documente.</p>"

    body = f"""    <nav class="breadcrumb"><a href="../index.html">Modules</a> / {html.escape(module.import_name)}</nav>
    <section class="module-hero">
      <p class="import">import {html.escape(module.import_name)}</p>
      <h1>{html.escape(module.title)}</h1>
      {render_paragraphs(module.description)}
    </section>
    <section class="entries">
{entries}
    </section>"""
    return page_template(module.title, body, depth=1)


def render_status(status: str | None) -> str:
    if not status:
        return ""
    normalized = status.lower()
    if "compat" in normalized:
        css_class = "compat"
    elif "interne" in normalized:
        css_class = "internal"
    else:
        css_class = "recommended"
    return f' <span class="status {css_class}">{html.escape(status)}</span>'


def render_index(modules: list[ModuleDoc]) -> str:
    cards = "\n".join(
        f"""      <a class="module-card" href="{html.escape(module.import_name.replace('.', '/'))}.html">
        <span>{html.escape(module.import_name)}</span>
        <strong>{html.escape(module.title)}</strong>
        <small>{len(module.entries)} symbole(s) public(s)</small>
      </a>"""
        for module in modules
        if module.entries or module.description
    )
    body = f"""    <section class="module-hero">
      <h1>Bibliotheque standard Nabla</h1>
      <p>Reference generee depuis les commentaires <code>///</code> de la stdlib. Les helpers internes non documentes restent hors de cette surface publique.</p>
    </section>
    <section class="module-grid">
{cards}
    </section>"""
    return page_template("Bibliotheque standard", body)


STYLE = """
:root {
  color-scheme: light;
  --text: #172026;
  --muted: #5a6872;
  --line: #d7dde2;
  --surface: #f6f8fa;
  --accent: #176d79;
}

* { box-sizing: border-box; }
body {
  margin: 0;
  color: var(--text);
  font: 16px/1.5 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  background: white;
}

header {
  border-bottom: 1px solid var(--line);
  display: flex;
  gap: 16px;
  align-items: center;
  padding: 14px clamp(18px, 4vw, 48px);
  color: var(--muted);
}

a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
.brand { font-weight: 700; color: var(--text); }

main {
  max-width: 1080px;
  margin: 0 auto;
  padding: 32px clamp(18px, 4vw, 48px) 64px;
}

.breadcrumb { margin-bottom: 24px; color: var(--muted); }
.module-hero {
  border-bottom: 1px solid var(--line);
  padding-bottom: 28px;
  margin-bottom: 28px;
}
.module-hero h1 {
  font-size: clamp(2rem, 4vw, 3.4rem);
  line-height: 1.05;
  margin: 0 0 14px;
}
.import {
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  color: var(--muted);
  margin: 0 0 10px;
}
.module-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 14px;
}
.module-card {
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 16px;
  color: var(--text);
}
.module-card span,
.module-card small,
.kind {
  color: var(--muted);
}
.entry-meta {
  display: flex;
  gap: 10px;
  align-items: center;
  flex-wrap: wrap;
}
.status {
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 2px 8px;
  font-size: 0.78rem;
  font-weight: 650;
}
.status.recommended {
  color: #0f5d3f;
  background: #e9f7ef;
  border-color: #b8e2c9;
}
.status.compat {
  color: #7a4d00;
  background: #fff5dc;
  border-color: #efd18b;
}
.status.internal {
  color: #6b425b;
  background: #f8edf4;
  border-color: #e5bdd6;
}
.module-card strong {
  display: block;
  font-size: 1.25rem;
  margin: 4px 0;
}
.entry {
  border-bottom: 1px solid var(--line);
  padding: 22px 0;
}
.entry h2 {
  margin: 0 0 10px;
  font-size: 1.5rem;
}
pre {
  overflow-x: auto;
  background: var(--surface);
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 12px;
}
code {
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
}
.empty { color: var(--muted); }
"""


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> None:
    modules = [
        parse_module(path)
        for path in sorted(STDLIB_DIR.rglob("*.nabla"))
    ]
    write_file(OUTPUT_DIR / "style.css", STYLE.strip() + "\n")
    write_file(OUTPUT_DIR / "index.html", render_index(modules))
    for module in modules:
        if not module.entries and not module.description:
            continue
        output = OUTPUT_DIR / (module.import_name.replace(".", "/") + ".html")
        write_file(output, render_module(module))
    print(f"Generated stdlib docs in {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
