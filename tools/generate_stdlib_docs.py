#!/usr/bin/env python3
"""Generate HTML API docs from Nabla stdlib doc comments.

Public documentation is opt-in: only declarations preceded by `///` comments
are emitted. This keeps low-level helpers available to the compiler and stdlib
without exposing them in the user-facing reference.
"""

from __future__ import annotations

import html
import re
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STDLIB_DIR = ROOT / "stdlib"
OUTPUT_DIR = ROOT / "docs" / "stdlib"

DECL_RE = re.compile(r"^\s*(?:override\s+)?(def|class|trait|object)\s+(.+?)\s*(?:=\s*\{|\{|$)")
SLUG_RE = re.compile(r"[^A-Za-z0-9_.-]+")


@dataclass
class Example:
    title: str | None
    lines: list[str]


@dataclass
class Entry:
    kind: str
    name: str
    signature: str
    status: str | None
    description: list[str]
    examples: list[Example] = field(default_factory=list)


@dataclass
class ModuleDoc:
    path: Path
    import_name: str
    title: str
    description: list[str]
    examples: list[Example]
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
    if text.startswith(" "):
        text = text[1:]
    return text.rstrip()


def symbol_name(kind: str, signature_tail: str) -> str:
    if kind in {"class", "trait", "object"}:
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


def extract_examples(lines: list[str]) -> tuple[list[str], list[Example]]:
    kept: list[str] = []
    examples: list[Example] = []
    current_title: str | None = None
    current_lines: list[str] | None = None

    for line in lines:
        if line.startswith("@example"):
            if current_lines is not None:
                examples.append(Example(current_title, current_lines))
            raw_title = line[len("@example") :].strip()
            current_title = raw_title or None
            current_lines = []
            continue
        if line == "@end":
            if current_lines is not None:
                examples.append(Example(current_title, current_lines))
                current_title = None
                current_lines = None
            else:
                kept.append(line)
            continue
        if current_lines is not None:
            current_lines.append(line)
        else:
            kept.append(line)

    if current_lines is not None:
        examples.append(Example(current_title, current_lines))
    return kept, examples


def extract_metadata(lines: list[str], fallback: str) -> tuple[str, str | None, list[str], list[Example]]:
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
    kept, examples = extract_examples(kept)
    return signature, status, kept, examples


def entry_from_signature(signature: str, description: list[str]) -> Entry:
    declaration = DECL_RE.match(signature)
    if not declaration:
        raise ValueError(f"Invalid documented symbol signature: {signature}")
    kind = declaration.group(1)
    signature, status, description, examples = extract_metadata(description, signature)
    return Entry(
        kind=kind,
        name=symbol_name(kind, declaration.group(2).strip()),
        signature=signature,
        status=status,
        description=description,
        examples=examples,
    )


def parse_module(path: Path) -> ModuleDoc:
    import_name = module_name(path)
    pending_doc: list[str] = []
    module_title: str | None = None
    module_description: list[str] = []
    module_examples: list[Example] = []
    entries: list[Entry] = []

    def finish_module_doc() -> None:
        nonlocal module_title, module_description, module_examples, pending_doc
        if pending_doc and pending_doc[0].startswith("@module "):
            title, module_doc_lines, remaining_doc_lines = split_summary(pending_doc)
            module_doc_lines, examples = extract_examples(module_doc_lines + remaining_doc_lines)
            module_title = title
            module_description = module_doc_lines
            module_examples = examples
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
            signature, status, doc_lines, examples = extract_metadata(doc_lines, signature)

            if title is not None:
                module_doc_lines, module_examples = extract_examples(module_doc_lines + doc_lines)
                doc_lines = []
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
                        examples=examples,
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
        examples=module_examples,
        entries=entries,
    )


def slug(value: str) -> str:
    cleaned = SLUG_RE.sub("-", value.strip()).strip("-")
    return cleaned or "entry"


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


def render_examples(examples: list[Example]) -> str:
    if not examples:
        return ""
    rendered: list[str] = []
    for index, example in enumerate(examples, start=1):
        title = example.title or ("Exemple" if len(examples) == 1 else f"Exemple {index}")
        code = "\n".join(example.lines).strip("\n")
        rendered.append(
            f"""      <figure class="example">
        <figcaption>{html.escape(title)}</figcaption>
        <pre><code>{html.escape(code)}</code></pre>
      </figure>"""
        )
    return "\n".join(rendered)


def render_status(status: str | None) -> str:
    if not status:
        return ""
    normalized = status.lower()
    if "compat" in normalized:
        css_class = "compat"
    elif "experiment" in normalized:
        css_class = "experimental"
    elif "interne" in normalized:
        css_class = "internal"
    else:
        css_class = "recommended"
    return f' <span class="status {css_class}">{html.escape(status)}</span>'


def entry_group(entry: Entry) -> str:
    if entry.kind in {"class", "trait", "object"}:
        return "Types"
    if entry.kind == "def" and re.match(r"def\s+[A-Z][A-Za-z0-9_]*\.", entry.signature):
        return "Constructeurs et fabriques"
    return "Methodes"


def output_depth(output: Path) -> int:
    return len(output.relative_to(OUTPUT_DIR).parents) - 1


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
  <a class="skip-link" href="#contenu">Aller au contenu</a>
  <header class="topbar">
    <a class="brand" href="{prefix}index.html">Nabla stdlib</a>
    <nav class="topnav" aria-label="Navigation principale">
      <a href="{prefix}index.html">Modules</a>
      <a href="https://github.com/cyril103/nabla-compiler">GitHub</a>
    </nav>
  </header>
  <main id="contenu">
{body}
  </main>
</body>
</html>
"""


def render_entry_link(entry: Entry, entry_id: str) -> str:
    signature = html.escape(entry.signature)
    return f"""        <li><a href="#{html.escape(entry_id)}" title="{signature}"><span>{html.escape(entry.name)}</span><code>{signature}</code></a></li>"""


def render_module(module: ModuleDoc, depth: int) -> str:
    seen_entry_ids: dict[str, int] = {}
    entry_ids: list[str] = []
    rendered_entries: list[str] = []
    grouped_links: dict[str, list[str]] = {"Types": [], "Constructeurs et fabriques": [], "Methodes": []}

    for entry in module.entries:
        seen_count = seen_entry_ids.get(entry.name, 0) + 1
        seen_entry_ids[entry.name] = seen_count
        base_id = slug(entry.name)
        entry_id = base_id if seen_count == 1 else f"{base_id}-{seen_count}"
        entry_ids.append(entry_id)
        grouped_links.setdefault(entry_group(entry), []).append(render_entry_link(entry, entry_id))
        rendered_entries.append(
            f"""        <article class="entry" id="{html.escape(entry_id)}">
          <div class="entry-meta">
            <span class="kind">{html.escape(entry.kind)}</span>{render_status(entry.status)}
          </div>
          <h2>{html.escape(entry.name)}</h2>
          <pre class="signature"><code>{html.escape(entry.signature)}</code></pre>
          {render_paragraphs(entry.description)}
{render_examples(entry.examples)}
        </article>"""
        )

    toc_groups = []
    for group, links in grouped_links.items():
        if links:
            toc_groups.append(
                f"""      <section class="toc-group">
        <h3>{html.escape(group)}</h3>
        <ul>
{chr(10).join(links)}
        </ul>
      </section>"""
            )
    toc = "\n".join(toc_groups) or "      <p class=\"empty\">Aucun symbole public documente.</p>"
    entries = "\n".join(rendered_entries) or "        <p class=\"empty\">Aucun symbole public documente.</p>"

    body = f"""    <nav class="breadcrumb" aria-label="Fil d'Ariane"><a href="{'../' * depth}index.html">Modules</a> / {html.escape(module.import_name)}</nav>
    <section class="module-hero">
      <p class="eyebrow">Module</p>
      <h1>{html.escape(module.title)}</h1>
      <p class="import"><code>import {html.escape(module.import_name)}</code></p>
      {render_paragraphs(module.description)}
{render_examples(module.examples)}
    </section>
    <div class="doc-shell">
      <aside class="sidebar" aria-label="Sommaire des symboles publics">
        <h2>API publique</h2>
{toc}
      </aside>
      <section class="content entries" aria-label="Reference detaillee">
{entries}
      </section>
    </div>"""
    return page_template(module.title, body, depth=depth)


def render_index(modules: list[ModuleDoc]) -> str:
    visible_modules = [module for module in modules if module.entries or module.description]
    cards = "\n".join(
        f"""      <a class="module-card" href="{html.escape(module.import_name.replace('.', '/'))}.html">
        <span>{html.escape(module.import_name)}</span>
        <strong>{html.escape(module.title)}</strong>
        <small>{len(module.entries)} symbole(s) public(s)</small>
      </a>"""
        for module in visible_modules
    )
    body = f"""    <section class="module-hero index-hero">
      <p class="eyebrow">Reference API publique</p>
      <h1>Bibliotheque standard Nabla</h1>
      <p>Documentation generee depuis les commentaires <code>///</code> de la stdlib. Comme dans une reference Scala, chaque module liste les types, fabriques et methodes utilisables avec leurs signatures et exemples.</p>
    </section>
    <section class="module-grid">
{cards}
    </section>"""
    return page_template("Bibliotheque standard", body)


STYLE = """
:root {
  color-scheme: light;
  --text: #172026;
  --muted: #5d6875;
  --line: #dce3ea;
  --surface: #f7f9fb;
  --surface-strong: #eef4f8;
  --panel: #ffffff;
  --accent: #0f6c80;
  --accent-strong: #0b4f63;
  --code: #101820;
  --shadow: 0 18px 45px rgba(22, 35, 48, 0.08);
}

* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0;
  color: var(--text);
  font: 16px/1.55 Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  background: linear-gradient(180deg, #f9fbfd 0, #fff 280px);
}

a { color: var(--accent); text-decoration: none; }
a:hover { color: var(--accent-strong); text-decoration: underline; }
a:focus-visible,
.module-card:focus-visible,
.toc-group a:focus-visible {
  outline: 3px solid rgba(15, 108, 128, 0.35);
  outline-offset: 3px;
  border-radius: 10px;
}
code, pre { font-family: "JetBrains Mono", ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }

.skip-link {
  position: absolute;
  left: 16px;
  top: 10px;
  z-index: 100;
  transform: translateY(-140%);
  background: var(--accent-strong);
  color: white;
  border-radius: 999px;
  padding: 8px 12px;
  font-weight: 800;
}
.skip-link:focus-visible { transform: translateY(0); }

.topbar {
  position: sticky;
  top: 0;
  z-index: 10;
  border-bottom: 1px solid rgba(220, 227, 234, 0.9);
  display: flex;
  justify-content: space-between;
  gap: 16px;
  align-items: center;
  padding: 14px clamp(18px, 4vw, 48px);
  background: rgba(255, 255, 255, 0.88);
  backdrop-filter: blur(14px);
}
.brand { font-weight: 800; color: var(--text); letter-spacing: -0.02em; }
.topnav { display: flex; gap: 18px; color: var(--muted); font-size: 0.95rem; }
.topnav a { color: var(--muted); }

main {
  max-width: 1320px;
  margin: 0 auto;
  padding: 34px clamp(18px, 4vw, 48px) 72px;
}

.breadcrumb { margin-bottom: 22px; color: var(--muted); font-size: 0.95rem; }
.module-hero {
  border: 1px solid var(--line);
  border-radius: 24px;
  background: var(--panel);
  box-shadow: var(--shadow);
  padding: clamp(24px, 4vw, 44px);
  margin-bottom: 28px;
}
.index-hero { max-width: 920px; }
.eyebrow {
  color: var(--accent);
  font-weight: 750;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  font-size: 0.78rem;
  margin: 0 0 10px;
}
.module-hero h1 {
  font-size: clamp(2.2rem, 5vw, 4.2rem);
  line-height: 0.98;
  letter-spacing: -0.055em;
  margin: 0 0 18px;
}
.import {
  color: var(--muted);
  margin: 0 0 18px;
}
.import code {
  color: var(--code);
  background: var(--surface-strong);
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 5px 10px;
}
.module-hero p { max-width: 78ch; }

.module-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
  gap: 16px;
}
.module-card {
  border: 1px solid var(--line);
  border-radius: 18px;
  padding: 20px;
  color: var(--text);
  background: var(--panel);
  box-shadow: 0 10px 28px rgba(22, 35, 48, 0.05);
  transition: transform 140ms ease, border-color 140ms ease, box-shadow 140ms ease;
}
.module-card:hover {
  transform: translateY(-2px);
  border-color: #b7cad7;
  box-shadow: var(--shadow);
  text-decoration: none;
}
.module-card span,
.module-card small,
.kind,
.empty { color: var(--muted); }
.module-card strong {
  display: block;
  font-size: 1.28rem;
  margin: 5px 0;
}

.doc-shell {
  display: grid;
  grid-template-columns: minmax(240px, 310px) minmax(0, 1fr);
  align-items: start;
  gap: 28px;
}
.sidebar {
  position: sticky;
  top: 82px;
  max-height: calc(100vh - 112px);
  overflow: auto;
  border: 1px solid var(--line);
  border-radius: 20px;
  background: var(--panel);
  padding: 18px;
}
.sidebar h2 {
  margin: 0 0 14px;
  font-size: 1rem;
}
.toc-group + .toc-group { margin-top: 18px; }
.toc-group h3 {
  margin: 0 0 8px;
  color: var(--muted);
  font-size: 0.78rem;
  letter-spacing: 0.07em;
  text-transform: uppercase;
}
.toc-group ul { list-style: none; padding: 0; margin: 0; display: grid; gap: 6px; }
.toc-group a {
  display: grid;
  gap: 2px;
  border-radius: 12px;
  padding: 8px 10px;
  color: var(--text);
}
.toc-group a:hover { background: var(--surface); text-decoration: none; }
.toc-group span { font-weight: 700; }
.toc-group code {
  color: var(--muted);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 0.78rem;
}

.entries { display: grid; gap: 18px; }
.entry {
  border: 1px solid var(--line);
  border-radius: 20px;
  background: var(--panel);
  padding: clamp(20px, 3vw, 30px);
  scroll-margin-top: 92px;
}
.entry-meta {
  display: flex;
  gap: 10px;
  align-items: center;
  flex-wrap: wrap;
  margin-bottom: 8px;
}
.kind {
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 2px 8px;
  font-size: 0.78rem;
  font-weight: 700;
  text-transform: uppercase;
}
.status {
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 2px 8px;
  font-size: 0.78rem;
  font-weight: 700;
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
.status.experimental {
  color: #155f7a;
  background: #e8f6fb;
  border-color: #a9d9e9;
}
.status.internal {
  color: #6b425b;
  background: #f8edf4;
  border-color: #e5bdd6;
}
.entry h2 {
  margin: 0 0 12px;
  font-size: clamp(1.45rem, 3vw, 2rem);
  letter-spacing: -0.025em;
}
.signature,
.example pre {
  overflow-x: auto;
  background: #0f1720;
  color: #edf7ff;
  border: 1px solid #1f2c39;
  border-radius: 14px;
  padding: 14px 16px;
}
.signature code,
.example code { color: inherit; }
.example {
  margin: 18px 0 0;
}
.example figcaption {
  color: var(--muted);
  font-size: 0.9rem;
  font-weight: 750;
  margin-bottom: 8px;
}

@media (max-width: 900px) {
  .topbar { position: static; }
  .doc-shell { grid-template-columns: 1fr; }
  .sidebar { position: static; max-height: none; }
}

@media (prefers-reduced-motion: reduce) {
  html { scroll-behavior: auto; }
  *, *::before, *::after {
    transition-duration: 0.01ms !important;
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
  }
  .module-card:hover { transform: none; }
}
"""


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> None:
    modules = [parse_module(path) for path in sorted(STDLIB_DIR.rglob("*.nabla"))]
    write_file(OUTPUT_DIR / "style.css", STYLE.strip() + "\n")
    write_file(OUTPUT_DIR / "index.html", render_index(modules))
    for module in modules:
        if not module.entries and not module.description:
            continue
        output = OUTPUT_DIR / (module.import_name.replace(".", "/") + ".html")
        write_file(output, render_module(module, output_depth(output)))
    print(f"Generated stdlib docs in {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
