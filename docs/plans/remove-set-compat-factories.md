# Remove Set compatibility factories

## Context

`Set.empty[T]()` and `Set.fromArray[T](values)` are now the public factories.
The older `SetEmpty[T]()` and `SetFromArray[T](values)` aliases keep legacy
factory names visible in the user-facing stdlib docs even though ordinary tests
and examples already use the companion API.

## Scope

- Remove public `SetEmpty` and `SetFromArray` alias definitions.
- Keep internal `setEmpty`, `setFromArray`, and primitive specialization helpers
  as implementation details for `Set.empty` / `Set.fromArray`.
- Remove compiler alias routing for `SetFromArray` while preserving
  `Set.fromArray` specialization for primitive `Array[T]` facades.
- Add negative tests for the removed public aliases.
- Regenerate stdlib HTML docs and update `docs/stdlib-api.md`, `docs/language.md`,
  and `AGENTS.md`.

## Validation

- RED: old alias calls should compile before the removal, so new negative tests
  fail until production code changes.
- GREEN: targeted negative tests pass with diagnostics that recommend
  `Set.empty` / `Set.fromArray`.
- Full suite: `make stdlib-docs`, generated docs cleanly updated,
  `make all-tests`, `make examples`, `make tooling-tests`, strict C++ build,
  `git diff --check`.
