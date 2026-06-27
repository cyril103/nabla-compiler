# Scala-inspired List methods

## Context

Cyril pointed to Scala 2.13 `immutable.List` as inspiration. Nabla already has an experimental `collections.list` V0, with `Nil[T](defaultValue)` still acting as a temporary stand-in until `object Nil`, variance, and a fully polymorphic empty list are available.

## Scope

Add a pragmatic subset of Scala-like immutable list operations that fits current Nabla syntax and runtime:

- `prepended(value)` as a Scala-name alias for `prepend(value)`.
- `concat(suffix)` as a Nabla spelling for Scala `:::`.
- `reverse()` and `reverseConcat(suffix)` as a Nabla spelling for Scala `reverse_:::`.
- `appended(value)` for single-element append.
- `take(n)`, `drop(n)`, and `slice(from, until)`.

Operator names (`::`, `:::`) and `object Nil extends List[Nothing]` remain out of scope until parser/operator, variance, and singleton-empty-list design are ready.

## TDD

- Add runtime tests for concat/reverse/appended and take/drop/slice using `mkString` and `size` oracles.
- Keep old List tests passing.

## Implementation

- Add default trait methods on `List[T]` when they can be expressed in terms of generic helpers.
- Introduce an internal `emptyDefault(): T` hook so methods that return empty `List[T]` can preserve the temporary V0 `Nil[T](defaultValue)` representation without exposing a new documented public constructor.
- Keep recursion simple and immutable.

## Verification

- Targeted new List tests.
- `make all-tests`, `make examples`, `make tooling-tests`, `make stdlib-docs` diff/idempotence check, strict C++ build, `git diff --check`.
