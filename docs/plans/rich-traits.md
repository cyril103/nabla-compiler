# Rich Traits Implementation Plan

> Use subagent-driven development / Codex with TDD. Keep changes incremental and verified.

## Goal

Introduce `trait` as a rich interface mechanism for Nabla, reusing the existing nominal inheritance/mixin infrastructure where possible.

## V1 Scope

Supported:

- `trait Name { ... }` declarations.
- `trait Child with ParentTrait { ... }`.
- `class Foo with SomeTrait { ... }`.
- Abstract trait methods declared without a body, e.g. `def show(): String`.
- Concrete/default trait methods declared with a body.
- Concrete classes must implement all inherited abstract trait methods.
- `override` is mandatory when a class implements or replaces a trait method.
- Default method conflicts between multiple traits are rejected unless the class provides an explicit override.
- Traits are non-instantiable.

Out of scope for V1:

- Trait constructors / primary constructors.
- Trait instance fields.
- Trait `super` calls.
- Scala-style linearization.
- Advanced variance/typeclass machinery.

## Implementation Tasks

### Task 1 — Parser/AST surface

Add parser support for `trait` declarations. Prefer extending existing class declaration representation with an `isTrait` flag if this fits the current AST and compiler pipeline.

Acceptance tests:

- Positive source with a simple trait declaration parses/compiles far enough to reach semantic validation.
- Negative source verifies unsupported trait syntax fails clearly where appropriate.

### Task 2 — Semantic model

Represent traits in `CompilerContext::ClassInfo` or equivalent metadata.

Rules:

- A trait can appear in `with` parent lists.
- A trait cannot be instantiated with `new`.
- A trait cannot declare instance fields or constructors in V1.
- Trait declarations can contain method signatures without bodies.

### Task 3 — Abstract method validation

Track abstract trait methods and require concrete classes to implement them.

Rules:

- Trait methods without a body are abstract.
- Concrete/default methods in traits are inherited like current mixin methods.
- A concrete class inheriting an abstract trait method must provide an implementation.
- If a trait extends another trait, abstract obligations are inherited.

### Task 4 — Override enforcement

When a class implements or replaces any inherited trait method, it must use `override`.

Rules:

- `def show()` implementing `Showable.show()` is rejected.
- `override def show()` is accepted.
- Existing class override rules must not regress.

### Task 5 — Default methods and conflicts

Support default trait methods by reusing current inherited/mixin method resolution.

Rules:

- A single inherited default method is callable on the concrete class.
- Two inherited defaults with the same signature conflict unless the concrete class overrides that method.
- No Scala-style linearization in V1.

### Task 6 — Stdlib traits

After language tests pass, add a minimal stdlib surface:

- `Showable` if it fits current `toString` conventions.
- `Sized` with `size`, `isEmpty`, `nonEmpty` if it fits current collection APIs.

Prefer starting with the smallest trait that compiles cleanly. Do not destabilize existing public collection APIs.

### Task 7 — Documentation and AGENTS

Update:

- `docs/language.md` with trait syntax and V1 limitations.
- `docs/roadmap.md` if roadmap status changes.
- `docs/stdlib-api.md` and generated `docs/stdlib/` if public stdlib traits are added.
- `AGENTS.md` as required by repo instructions before commit.

## Verification

Run the narrow tests introduced for traits first, then the broader project checks that are practical locally:

- targeted trait compile/run/fail tests;
- `make all-tests`;
- `make examples`;
- `make tooling-tests`;
- `make stdlib-docs` if stdlib public docs change;
- `git diff --check`.

## Notes

Keep the V1 intentionally boring: traits are named contracts with optional default methods. Avoid adding state, constructors, linearization, or `super` until the basic model is proven.
