# Remove Option/Map default compatibility overloads

## Context

The stdlib now has public no-default constructors/lookups for absence:
`Option.none[T]()` and `Map.getOption(key)`. The older compatibility shapes
`Option.none[T](default)`, `optionNone[T](default)`, and
`Map.getOption(default, key)` keep a fake internal value visible in the API and
match the cleanup direction already applied to `List`.

## Scope

- Remove public compatibility overloads for `Option.none[T](default)`,
  `optionNone[T](default)`, and `Map.getOption(default, key)`.
- Remove private map helper overloads that only exist to feed the old default
  lookup path.
- Migrate positive tests/examples/docs to `Option.none[T]()` and
  `Map.getOption(key)`.
- Add expected-error diagnostics proving the old arities/names are rejected.
- Regenerate stdlib HTML docs and update `docs/stdlib-api.md`, `docs/language.md`,
  `docs/roadmap.md`, and `AGENTS.md` where they mention these compatibility
  forms.

## Verification

- Targeted RED diagnostics for each removed call shape before implementation.
- `make stdlib-docs` with generated docs committed.
- `make all-tests`, `make examples`, `make tooling-tests`.
- Strict `g++ -std=c++17 -Wall -Wextra -Werror ...` build.
- `git diff --check` and independent review.
