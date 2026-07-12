# Higher-Kinded Collection Ops Implementation Plan

> **For Hermes:** Use subagent-driven-development/Codex only task-by-task; keep each slice independently testable.

**Goal:** Introduce the minimal higher-kinded type-constructor support needed to model Scala-like collection builders/factories and eventually `IterableOps[A, CC[_], C]`.

**Architecture:** Start with arity-1 type-constructor parameters (`CC[_]`) in class/trait/function type-parameter lists, then support applying such parameters in type positions (`CC[A]`) and substituting them with nominal generic constructors such as `List`, `Array`, or `Set`. Defer variance, higher arities, wildcard existential types, and full Scala `IterableOps` parity until the arity-1 vertical slice is stable.

**Tech Stack:** Nabla compiler frontend (`src/parser.cpp`, `src/semantic_analyzer.cpp`, `src/compiler_context.hpp`), stdlib (`stdlib/core`, `stdlib/collections`), `.nabla` regression tests, generated stdlib docs.

---

## Non-goals for the first PR

- No variance syntax (`+A`, `-A`).
- No general wildcard type values; `_` is only accepted in type-parameter declarations such as `CC[_]`.
- No higher-kinded values or runtime representation changes.
- No multi-arity constructors (`F[_, _]`) until arity-1 proves useful.
- No full `IterableOps` method surface in the first compiler slice.

## Slice 1: type-constructor parameter declarations and substitution

### Task 1: Add RED syntax/semantic tests

**Objective:** Prove the desired minimal surface before implementation.

**Files:**
- Create: `tests/test_higher_kinded_type_constructor_param.nabla`
- Create: `tests/test_higher_kinded_type_constructor_param.expected`
- Create: `tests/test_error_higher_kinded_wrong_arity.nabla`
- Create: `tests/test_error_higher_kinded_wrong_arity.diagnostic`

**Positive test shape:**

```nabla
import collections.list

trait WrapFactory[CC[_]] {
    def single[T](value: T): CC[T]
}

class ListWrapFactory() with WrapFactory[List] {
    override def single[T](value: T): List[T] = {
        List.cons[T](value, Nil)
    }
}

def useFactory(factory: WrapFactory[List]): Int = {
    val xs = factory.single[Int](42)
    xs.head()
}

def main(): Int = {
    useFactory(new ListWrapFactory())
}
```

Expected exit: `42`.

**Negative test shape:** pass a concrete applied type where a constructor is required:

```nabla
trait WrapFactory[CC[_]] {
    def single[T](value: T): CC[T]
}
class Bad() with WrapFactory[List[Int]] {
    override def single[T](value: T): List[Int] = {
        List.cons[Int](1, Nil)
    }
}
def main(): Int = { 0 }
```

Expected diagnostic should clearly mention that `CC[_]` expects a type constructor of arity 1, not an already-applied type.

### Task 2: Represent type-parameter kinds

**Objective:** Preserve both ordinary type parameters (`T`) and constructor parameters (`CC[_]`).

**Files:**
- Modify: `src/compiler_context.hpp`
- Modify: `src/ast.hpp` / `src/ast.cpp` only if codegen needs structured data; otherwise keep ordered string compatibility and add parallel kind metadata.
- Modify: `src/parser.hpp` / `src/parser.cpp`

**Implementation notes:**
- Add `TypeParameterInfo { name, arity }`, with `arity=0` for ordinary types and `arity=1` for `CC[_]`.
- Keep existing `std::vector<std::string> typeParameters` for compatibility, but add `typeParameterArities` or `typeParameterInfos` to `ClassInfo` and `FunctionSignature`.
- Provide helpers:
  - `typeParameterArity(name, infos)`
  - `isTypeConstructorParameterName(name, infos)`
  - `validateTypeConstructorArgument(context, actual, expectedArity, location)`

### Task 3: Parse `CC[_]` in type-parameter lists only

**Objective:** Accept syntax such as `trait IterableOps[A, CC[_], C]`.

**Files:**
- Modify: `src/parser.cpp`
- Tests: run the RED tests from Task 1.

**Implementation notes:**
- Factor repeated type-parameter parsing shared by class/trait and function/method declarations.
- In a declaration list, after an identifier, if the next token is `[`, require exactly `_` then `]` for now.
- Reject `_` in ordinary type positions for this PR.
- Reject duplicate names across both ordinary and constructor parameters.

### Task 4: Apply and substitute constructor parameters

**Objective:** Make `CC[T]` a valid type when `CC` is a type-constructor parameter and substitute `CC -> List` into `List[T]`.

**Files:**
- Modify: `src/compiler_context.hpp`
- Modify: `src/semantic_analyzer.cpp`

**Implementation notes:**
- Extend `substituteType` so if a parameterized type base is present in the substitution map, it substitutes the base before formatting/canonicalizing.
- Ensure `isKnownTypeInScope("CC[T]", typeParams)` accepts constructor-parameter application only when `CC` has arity 1 and the argument is a known type in scope.
- Ensure direct raw use of `CC` as a value type remains invalid unless a future kind-polymorphic API explicitly needs it.
- When a concrete parent `WrapFactory[List]` is parsed, permit raw generic class `List` only because the corresponding type parameter is constructor-arity 1.

### Task 5: Add builder/factory skeleton stdlib types

**Objective:** Introduce names and direction without overclaiming full Scala parity.

**Files:**
- Create: `stdlib/collections/builder.nabla`
- Create or modify: `stdlib/collections/iterable_ops.nabla`
- Modify: `docs/stdlib-api.md`
- Run: `make stdlib-docs`

**Proposed minimal shape:**

```nabla
trait Builder[A, C] {
    def add(value: A): Unit
    def result(): C
}

trait IterableFactory[CC[_]] {
    def empty[A](): CC[A]
}

trait IterableOps[A, CC[_], C] with Iterable[A] {
    def iterableFactory(): IterableFactory[CC]
}
```

Do not move existing `Iterable` methods into `IterableOps` until the compiler slice is green.

### Task 6: Validation and review

**Commands:**

```bash
export PATH=/opt/data/local/usr/bin:$PATH
make nablac
./build/nablac tests/test_higher_kinded_type_constructor_param.nabla && ./test_higher_kinded_type_constructor_param
./build/nablac tests/test_error_higher_kinded_wrong_arity.nabla
make all-tests </dev/null
make examples
make tooling-tests PYTHON=python3
make stdlib-docs
git diff --check
```

**Review focus:**
- No accidental support for arbitrary wildcards.
- No broad covariance/variance changes.
- No runtime/codegen behavior changes beyond normal generic specialization naming.
- Diagnostics mention type constructor arity rather than leaking internal strings.

## Later slices

1. Extend factory coverage beyond the delivered experimental `ListFactory` /
   `ListBuilder[A]`, `ArrayFactory` / `ArrayBuilder[A]`, `SetFactory` /
   `SetBuilder[A]`, and non-`IterableFactory` `MapBuilder[K, V]` slices;
   consider whether `Map` needs an arity-2 design before it can join
   `IterableOps`.
2. `List[T]` now extends the delivered experimental
   `IterableOps[T, List, List[T]]` slice and reuses default instance
   `map`/`filter` through `ListFactory` / `ListBuilder`; keep the companion
   `List.map` / `List.filter` functions as the compatibility surface while
   this remains experimental.
3. `Set[T]` now extends the delivered experimental
   `IterableOps[T, Set, Set[T]]` slice and reuses default instance
   `map`/`filter` through `SetFactory` / `SetBuilder`; the inherited defaults
   preserve Set deduplication because reconstruction goes through `Set.add`.
4. `ArrayObject[T]`, the current generic `Array[T]` facade implementation, now
   extends the delivered experimental `IterableOps[T, Array, ArrayObject[T]]`
   slice. It keeps its direct `map`/`filter` overrides for the current efficient
   array reconstruction path, while trait upcasts can dispatch through the
   `IterableOps` family and `ArrayObjectFactory` / `ArrayObjectBuilder` validate
   the `Array` type-constructor wiring.
5. Move safe operations from `Iterable` to `IterableOps` only when their return types are representable.
6. Consider arity-2 constructors for `Map[K, V]`-like APIs.
7. Consider variance only after invariant HKT dispatch and substitution are stable.
