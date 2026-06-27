# Remove List defaultValue compatibility overloads

## Context

`collections.list` now has the final-shape empty-list primitive for the current language: runtime singleton `Nil: List[Nothing]` plus the narrow `List[Nothing] -> List[T]` assignability rule. The earlier V0 overloads that accepted an ignored `defaultValue` are no longer needed and keep the public surface noisier than the Scala-like API.

## Scope

- Remove ignored `defaultValue` overloads from `List.empty`, `List.map`, `List.filter`, `List.fromArray`, and private helper aliases.
- Migrate existing positive tests/examples/docs to the no-default signatures.
- Add negative diagnostic coverage proving old call shapes are rejected rather than silently accepted.
- Regenerate stdlib HTML docs and update `docs/stdlib-api.md` and `AGENTS.md`.

## Verification

- Targeted List tests including old-signature diagnostics.
- `make stdlib-docs` with generated diff committed.
- `make all-tests`, `make examples`, `make tooling-tests`, strict C++ build, `git diff --check`.
- Independent review for public API consistency and accidental breakage.
