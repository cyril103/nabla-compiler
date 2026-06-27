# Nil singleton extending List[Nothing]

## Context

`Nothing` and runtime singleton objects are now available, so `collections.list` can move from the temporary `Nil[T](defaultValue)` class representation toward the Scala-like shape:

```nabla
object Nil with List[Nothing] { ... }
```

Nabla still has invariant generic syntax and no symbolic `::`/`:::` operators, so this plan keeps the existing user-facing method names while allowing `Nil` to flow into `List[T]` through bottom-type assignability.

## TDD plan

1. Add RED tests for:
   - assigning `Nil` to `List[Int]` and using it via `List.cons`;
   - `List.empty[T]()` without a default value;
   - `List.map`, `List.filter`, and `List.fromArray` overloads that no longer require default values;
   - direct runtime-object behavior for `Nil` (`isEmpty`, `headOption`, `mkString`, `AnyRef` equality).
2. Implement the minimal compiler assignability support needed for `List[Nothing]` to satisfy `List[T]` in call/assignment positions.
3. Replace the stdlib `class Nil[T](defaultValue)` with runtime singleton `object Nil with List[Nothing]` and keep compatibility overloads where cheap.
4. Update docs, generated stdlib HTML, and AGENTS milestone notes.
5. Run targeted tests, full matrix, and independent reviews before PR.
