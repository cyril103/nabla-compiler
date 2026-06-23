# Generic Trait Implementors Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Allow generic classes such as `Box[T]` to compose non-generic traits with `with`, while preserving trait V1 semantics and avoiding generic trait/typeclass expansion.

**Architecture:** Remove the temporary semantic ban on generic classes with trait ancestors, then validate the existing inheritance lookup, override checks, abstract method obligations, and IR dispatch against parameterized implementor names. Keep the scope narrow: non-generic traits may be implemented by generic classes; generic traits and trait type parameters remain out of scope unless already parsed safely.

**Tech Stack:** Nabla compiler C++17, existing `.nabla` test harness, `make nablac`, `make all-tests`.

---

## Scope

Supported after this plan:

- `class Box[T](value: T) with Sized { override def size(): Int = { ... } }`
- A variable of trait type can hold a parameterized implementor and dispatch to the class override.
- Default trait methods can call abstract methods on `this` and work with parameterized implementors.
- Existing V1 trait restrictions remain: no trait state/constructors/`super`, no `new Trait`, no unresolved abstract obligations.

Explicitly out of scope:

- Generic traits such as `trait Eq[T]` if parser/semantics do not already support them cleanly.
- Scala-style linearization or trait `super`.
- Large stdlib migration to `Sized`/`Showable`; this plan only unlocks the language/runtime behavior.

## Task 1: Add RED tests for generic classes implementing traits

**Objective:** Capture the currently forbidden behavior and expected runtime dispatch before production changes.

**Files:**
- Create: `tests/test_trait_generic_class_abstract.nabla`
- Create: `tests/test_trait_generic_class_abstract.expected`
- Create: `tests/test_trait_generic_class_default_dispatch.nabla`
- Create: `tests/test_trait_generic_class_default_dispatch.expected`
- Create: `tests/test_trait_generic_class_field_dispatch.nabla`
- Create: `tests/test_trait_generic_class_field_dispatch.expected`
- Modify or remove: `tests/test_error_trait_generic_class.*` after GREEN

**Test 1 source:**

```nabla
trait Sized {
    def size(): Int
}

class Box[T](value: T) with Sized {
    override def size(): Int = {
        21
    }
}

def doubleSize(s: Sized): Int = {
    s.size() + s.size()
}

def main(): Int = {
    doubleSize(new Box[Int](7))
}
```

Expected exit: `42`.

**Test 2 source:**

```nabla
trait Sized {
    def size(): Int

    def nonEmpty(): Bool = {
        this.size() != 0
    }
}

class Box[T](value: T) with Sized {
    override def size(): Int = {
        1
    }
}

def main(): Int = {
    val sized: Sized = new Box[String]("x")
    if sized.nonEmpty() {
        42
    } else {
        0
    }
}
```

Expected exit: `42`.

**Step 1:** Write the tests.

**Step 2:** Run RED:

```bash
make nablac
NABLA_BUILD_DIR=build ./build/nablac tests/test_trait_generic_class_abstract.nabla
NABLA_BUILD_DIR=build ./build/nablac tests/test_trait_generic_class_default_dispatch.nabla
```

Expected: both fail with the current diagnostic `la classe générique 'Box' ne peut pas composer de trait en V1`.

## Task 2: Remove the temporary semantic ban and preserve validations

**Objective:** Permit generic classes to compose trait ancestors, while keeping all existing trait constraints.

**Files:**
- Modify: `src/semantic_analyzer.cpp`, around `SemanticAnalyzer::validateParentTypes()`
- Modify: `docs/language.md`
- Modify: `docs/roadmap.md`

**Implementation notes:**

- Remove or narrow the block that throws for `!classInfo.isTrait && !classInfo.typeParameters.empty() && typeHasTraitAncestor(...)`.
- Keep validation that a class cannot `extends` a trait directly; traits remain composed with `with`.
- Keep parent type arity validation.
- Keep abstract method validation, override validation, default conflict validation.
- If generic traits are not fully supported, add/keep a diagnostic for trait declarations with type parameters only if needed; do not expand scope.

**Step 1:** Implement the smallest semantic change.

**Step 2:** Run GREEN for the two new tests. Expected both compile/run with exit `42`.

## Task 3: Convert obsolete negative test into positive coverage

**Objective:** Remove the now-obsolete expectation that generic classes with traits fail.

**Files:**
- Remove or rename: `tests/test_error_trait_generic_class.nabla`
- Remove: `tests/test_error_trait_generic_class.diagnostic`

Preferred: replace it with the positive tests from Task 1 rather than keeping contradictory tests.

**Verification:**

```bash
make nablac
for f in tests/test_trait_generic_class_*.nabla; do
  NABLA_BUILD_DIR=build ./build/nablac "$f"
  "./build/${f#tests/}" # adjust generated executable name if needed
done
```

Expected: both exit `42`.

## Task 4: Review edge cases and diagnostics

**Objective:** Ensure the unlocked behavior does not mask trait V1 errors.

**Checks:**

- Existing `tests/test_error_trait_missing_abstract.nabla` still fails.
- Existing `tests/test_error_trait_missing_override.nabla` still fails.
- Existing `tests/test_error_trait_default_conflict.nabla` still fails.
- Existing `tests/test_trait_default_overload_no_conflict.nabla` still passes.
- If a new generic-specific negative edge is found, add a narrow test before fixing.

## Task 5: Final verification and commit

Run:

```bash
make all-tests
git diff --check
```

If docs only changed manually, no stdlib docs generation is required. If stdlib/public reference changes, also run:

```bash
make stdlib-docs
make examples
make tooling-tests
```

Commit:

```bash
git add src/ docs/ tests/
git commit -m "feat: allow generic classes to implement traits"
```
