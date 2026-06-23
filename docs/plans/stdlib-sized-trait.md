# Stdlib Sized Trait Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Introduce a minimal public `Sized` trait in the Nabla stdlib and prove real generic stdlib classes can implement it.

**Architecture:** Add `core.sized` as a small non-generic trait with `size(): Int` abstract plus default `isEmpty()` and `nonEmpty()`. Apply it narrowly to selected existing classes that already expose compatible methods (`ArrayObject[T]`, `Set[T]`, `Map[K, V]`) and avoid a broad array/internal-helper cleanup in this PR.

**Tech Stack:** Nabla stdlib source, compiler language tests, generated stdlib docs.

---

## Scope

### Include
- New `stdlib/core/sized.nabla` module.
- `ArrayObject[T]`, `Set[T]`, and `Map[K, V]` implement `Sized`.
- Tests proving trait-typed calls dispatch to real stdlib classes and default methods work.
- Docs and generated stdlib reference update.

### Exclude
- Generic traits (`trait Iterable[T]`) remain out of scope.
- `Showable` remains out of scope.
- No full cleanup of internal array facades/helpers.
- No backend/runtime feature beyond using existing trait dispatch.

## Task 1: Add failing tests

**Objective:** Capture desired public behavior before stdlib changes.

**Files:**
- Create: `tests/test_stdlib_sized_trait_collections.nabla`
- Create: `tests/test_stdlib_sized_trait_collections.expected`
- Create: `tests/test_stdlib_sized_trait_array_object.nabla`
- Create: `tests/test_stdlib_sized_trait_array_object.expected`

**Test behaviors:**
1. `Set[T]` can be assigned to `Sized`; `size()`, `isEmpty()`, `nonEmpty()` work through the trait type.
2. `Map[K,V]` can be assigned to `Sized`; default trait methods work through dispatch.
3. `ArrayObject[T]` from `Array.fill[T]` can be assigned to `Sized` and reports its size.

**RED command:**

```bash
NABLA_BUILD_DIR=build ./build/nablac tests/test_stdlib_sized_trait_collections.nabla
NABLA_BUILD_DIR=build ./build/nablac tests/test_stdlib_sized_trait_array_object.nabla
```

Expected: fail because `core.sized` / `Sized` does not exist or classes do not implement it.

## Task 2: Add `core.sized`

**Objective:** Define the public trait.

**Files:**
- Create: `stdlib/core/sized.nabla`

**Implementation:**

```nabla
/// @module Sized
/// Trait pour les types ayant une taille entiere.

/// Type ayant une taille entiere.
/// @signature trait Sized
/// @status Recommandee
trait Sized {
    /// Retourne le nombre d'elements logiques.
    /// @signature def size(): Int
    /// @status Recommandee
    def size(): Int

    /// Indique si la collection est vide.
    /// @signature def isEmpty(): Bool
    /// @status Recommandee
    def isEmpty(): Bool = {
        this.size() == 0
    }

    /// Indique si la collection n'est pas vide.
    /// @signature def nonEmpty(): Bool
    /// @status Recommandee
    def nonEmpty(): Bool = {
        this.size() != 0
    }
}
```

## Task 3: Apply `Sized` narrowly

**Objective:** Make selected existing stdlib classes implement `Sized` without broad refactoring.

**Files:**
- Modify: `stdlib/collections/object_array.nabla`
- Modify: `stdlib/collections/set.nabla`
- Modify: `stdlib/collections/map.nabla`

**Changes:**
- Add `import core.sized`.
- Change class headers:
  - `class ArrayObject[T](values: ObjectArray[T]) with Sized {`
  - `class Set[T](values: ArrayObject[T]) with Sized {`
  - `class Map[K, V](entries: ArrayObject[Tuple2[K, V]]) with Sized {`
- Mark implemented methods with `override`:
  - `override def size(): Int`
  - `override def isEmpty(): Bool` where kept as class-specific body
  - `override def nonEmpty(): Bool` where kept as class-specific body

Keep existing bodies to minimize behavior changes.

## Task 4: Docs and generated reference

**Objective:** Document the new public trait and keep generated docs synchronized.

**Files:**
- Modify: `docs/stdlib-api.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md` if project notes list stdlib feature state.
- Generated: `docs/stdlib/...` via `make stdlib-docs`.

**Commands:**

```bash
make stdlib-docs
git diff -- docs/stdlib
```

## Task 5: Verification

**Objective:** Prove no regressions.

**Commands:**

```bash
export PATH=/opt/data/local/usr/bin:$PATH
make nablac
NABLA_BUILD_DIR=build ./build/nablac tests/test_stdlib_sized_trait_collections.nabla && ./build/test_stdlib_sized_trait_collections
NABLA_BUILD_DIR=build ./build/nablac tests/test_stdlib_sized_trait_array_object.nabla && ./build/test_stdlib_sized_trait_array_object
make all-tests
make examples
make tooling-tests
git diff --check
```

Expected: all pass; new executables exit `42`.
