# Runtime Singleton Object Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Turn Nabla `object` declarations from static namespaces only into optional runtime singleton values, so an object can implement a trait and be passed through nominal dispatch.

**Architecture:** Keep current `object Foo { def bar(...) }` static companion/namespace behavior intact. Add a narrow runtime-object path only when an object declares parents with `with`, e.g. `object IntNil with IntList { ... }`; that object receives class-like metadata, one static heap object, and value-expression support for bare `IntNil`. Generic singleton objects, fields, `Nothing`, variance, and pattern matching stay out of scope.

**Tech Stack:** C++17 compiler (`src/parser.cpp`, `src/ast.*`, `src/semantic_analyzer.cpp`, `src/ir.*`, `src/ir_codegen.cpp`, `src/runtime_asm.cpp`, `src/compiler_context.hpp`), Nabla tests (`tests/*.nabla`, `.expected`, `.diagnostic`, `.stdout`), docs (`docs/language.md`, `docs/internals.md`, `docs/roadmap.md`, `AGENTS.md`).

---

## Current state

Today `object` is a static namespace:

```nabla
object Math {
    def answer(): Int = { 42 }
}

Math.answer()
```

The parser records object names in `CompilerContext::objects`, emits contained methods as static functions like `Math.answer`, and explicitly rejects `override` inside objects because objects are not runtime implementors yet.

Important current implementation points:

- `src/parser.cpp::parseObjectDefinition` accepts only `object Name { def ... }`.
- `CompilerContext` has `objects: set<string>`, but no `ObjectInfo` and no class-like metadata for objects.
- Static object calls are parsed as namespaced function calls (`Name.method(...)`).
- Bare `Name` currently remains an ordinary identifier path and must not accidentally become a value unless `Name` is a runtime object.
- Class/trait dispatch already has the machinery we want to reuse: `ClassInfo`, `parentTypes`, `methods`, object allocation headers/class IDs, method calls via trait/parent type.

---

## Target V0 behavior

### Accepted syntax

```nabla
trait IntList {
    def isEmpty(): Bool
}

object IntNil with IntList {
    override def isEmpty(): Bool = {
        true
    }
}

def main(): Int = {
    val xs: IntList = IntNil
    if xs.isEmpty() { 42 } else { 1 }
}
```

### Preserved syntax

```nabla
object Math {
    def answer(): Int = { 42 }
}

def main(): Int = {
    Math.answer()
}
```

### Explicit V0 non-goals

Do **not** implement these in this PR:

- `object Nil with List[Nothing]` as a polymorphic `Nil`.
- `Nothing` or generic bottom-type assignability.
- variance syntax like `trait List[+T]`.
- generic singleton objects (`object Empty[T]`).
- fields or constructor parameters on objects.
- structural pattern matching (`case Nil`, `case Cons(h, t)`).
- property-style access without parentheses.
- changing static companion method behavior for existing code.

---

## Design decisions

1. **Runtime object only when parents are present.**
   `object Name { ... }` remains a static namespace. `object Name with Trait { ... }`
   becomes a singleton runtime value `Name` of type `Name`. V0 rejects
   `class Name` + `object Name with Trait` companion pairs because the singleton
   type/class-id would collide with the class type; parentless static companions
   keep existing behavior.

2. **No fields in V0.**
   The singleton object has only a class/runtime header. Instance methods may return constants or call functions but cannot read object fields.

3. **Use class-like metadata.**
   Add a runtime object info path that reuses `ClassInfo`-like method signatures and parent types so semantic validation and dispatch do not fork into a parallel type system.

4. **Bare object name is a value expression only for runtime objects.**
   If `Math` is static-only, `val x = Math` remains an error. If `IntNil` has parent traits, `val x: IntList = IntNil` is valid.

5. **All methods inside runtime objects are instance methods.**
   Static-only objects keep the existing namespace lowering (`Name.method`). Runtime
   objects do not mix static namespace functions and instance methods in V0: every
   `def` inside `object Name with Trait` is an instance method, `override def`
   can satisfy inherited trait methods, and the method participates in dynamic
   dispatch.

6. **Resolve `RuntimeObject.method()` as an instance call on the singleton value.**
   `StaticObject.method()` continues to mean a static namespace call for parentless
   objects. `RuntimeObject.method()` is accepted as sugar for calling the instance
   method on the singleton value, not as a static function call. Add a regression so
   parser/function-call resolution cannot silently route runtime-object methods
   through the old `Name.method` namespace path.

---

## Task 1: Add RED parser/semantic tests for runtime object syntax

**Objective:** Lock the intended syntax and current failure mode before compiler changes.

**Files:**
- Create: `tests/test_runtime_object_trait_dispatch.nabla`
- Create: `tests/test_runtime_object_trait_dispatch.expected`
- Create: `tests/test_runtime_object_value_identity.nabla`
- Create: `tests/test_runtime_object_value_identity.expected`
- Create: `tests/test_runtime_object_direct_method_call.nabla`
- Create: `tests/test_runtime_object_direct_method_call.expected`
- Create: `tests/test_error_static_object_as_value.nabla`
- Create: `tests/test_error_static_object_as_value.diagnostic`

**Step 1: Add trait-dispatch test**

`tests/test_runtime_object_trait_dispatch.nabla`:

```nabla
trait Flag {
    def enabled(): Bool
}

object Enabled with Flag {
    override def enabled(): Bool = {
        true
    }
}

def check(flag: Flag): Int = {
    if flag.enabled() { 42 } else { 1 }
}

def main(): Int = {
    check(Enabled)
}
```

`tests/test_runtime_object_trait_dispatch.expected`:

```text
42
```

**Step 2: Add stable singleton identity test**

`tests/test_runtime_object_value_identity.nabla`:

```nabla
trait Marker {
    def id(): Int
}

object Only with Marker {
    override def id(): Int = {
        42
    }
}

def main(): Int = {
    val a: Marker = Only
    val b: Marker = Only
    if a.id() == 42 && b.id() == 42 && a == b { 42 } else { 1 }
}
```

`tests/test_runtime_object_value_identity.expected`:

```text
42
```

**Step 3: Add direct runtime-object method-call test**

`tests/test_runtime_object_direct_method_call.nabla`:

```nabla
trait Flag {
    def enabled(): Bool
}

object Enabled with Flag {
    override def enabled(): Bool = {
        true
    }
}

def main(): Int = {
    if Enabled.enabled() { 42 } else { 1 }
}
```

`tests/test_runtime_object_direct_method_call.expected`:

```text
42
```

This protects the rule that `Enabled.enabled()` is an instance call on singleton
value `Enabled`, not a legacy static namespace call to `Enabled.enabled`.

**Step 4: Add static-only object-as-value diagnostic**

`tests/test_error_static_object_as_value.nabla`:

```nabla
object Math {
    def answer(): Int = { 42 }
}

def main(): Int = {
    val value = Math
    1
}
```

Expected diagnostic should explain that static-only objects are namespaces, not values, and suggest adding `with Trait` only if a runtime singleton is intended.

**Step 5: Verify RED**

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_trait_dispatch.nabla
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_value_identity.nabla
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_direct_method_call.nabla
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_error_static_object_as_value.nabla
```

Expected:

- positive tests fail before implementation because `object Name with Trait` does not parse;
- static-only diagnostic may initially be a generic unknown variable error and should be tightened during implementation.

---

## Task 2: Extend object metadata without changing codegen

**Objective:** Represent runtime-capable objects explicitly in the compiler context.

**Files:**
- Modify: `src/compiler_context.hpp`

**Implementation sketch:**

Add a small object info struct instead of overloading `objects` too much:

```cpp
struct ObjectInfo {
    bool isRuntimeSingleton = false;
    std::vector<std::string> parentTypes;
    std::map<std::string, FunctionSignature> methods;
    std::map<std::string, std::vector<std::string>> methodOverloads;
    SourceLocation location;
};

std::map<std::string, ObjectInfo> objectInfos;
```

Keep `std::set<std::string> objects` temporarily for compatibility if many call sites only need namespace membership. The follow-up refactor can replace the set with `objectInfos.count(name)` after behavior is green.

**Verification:**

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make nablac
```

Expected: compiler still builds and all existing object namespace tests still pass.

---

## Task 3: Parse `object Name with Trait` as runtime singleton

**Objective:** Accept parent traits on objects while preserving existing static-only object syntax.

**Files:**
- Modify: `src/parser.cpp::parseObjectDefinition`
- Possibly modify: `src/parser.hpp` if helper signatures change

**Steps:**

1. After the object name, parse zero or more parent trait types introduced by `with`.
2. Reject `extends` for V0 with a clear parser diagnostic; `object` construction has no parent constructor arguments.
3. Mark `objectInfos[name].isRuntimeSingleton = true` when parent types are non-empty.
4. Allow `override def` only when `isRuntimeSingleton` is true.
5. Register runtime object instance methods into `objectInfos[name].methods` and method overload maps, not into the legacy static `Name.method` function namespace.
6. Still create `FunctionDefNode`s for runtime object method bodies through the class/method owner path (`clName = objectName`, or a dedicated equivalent helper), not through the existing `objectName` static namespace path, so IR lowering emits `method.Name.method`, accepts `override`, binds implicit `this`, and gives dispatch/direct calls a concrete target symbol.
7. Keep static-only object parsing unchanged: no parents, no `override`, only namespace `def` declarations.

**RED/GREEN command:**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_trait_dispatch.nabla
```

Expected after this task: parsing succeeds far enough to reach semantic/codegen limitations rather than failing on `with` or `override`.

---

## Task 4: Add semantic validation and assignability for runtime object values

**Objective:** Make a bare runtime object name type-check as a value and satisfy trait parent types.

**Files:**
- Modify: `src/semantic_analyzer.cpp`
- Modify: `src/parser.cpp` for `parsePostfix` runtime-object receiver disambiguation
- Modify: `src/ast.*` if identifier expression lowering needs explicit object-value metadata
- Modify: `src/compiler_context.hpp` helpers if type predicates need runtime object awareness

**Steps:**

1. Treat runtime object names as known concrete reference types.
2. Add assignability from `ObjectName` to each declared parent trait using existing nominal parent traversal.
3. Validate runtime objects like classes:
   - all inherited abstract methods are implemented;
   - `override` methods match inherited signatures;
   - inherited concrete-method conflicts require explicit override;
   - root `Any` / `AnyRef` behavior remains available.
4. Keep static-only objects out of value lookup. Improve the diagnostic for `val x = StaticObject`.
5. Give runtime objects the same implicit `AnyRef`/`Any` ancestry as ordinary parentless classes, in addition to their declared trait parents.
6. In `parsePostfix`, handle the current ambiguity where an unresolved receiver followed by `.method(...)` first probes the static `Name.method` namespace: if `Name` is a runtime object, reinterpret it as a singleton value expression and perform normal method lookup instead.

**Negative tests to add in this task:**

- `tests/test_error_runtime_object_missing_abstract.nabla` + `.diagnostic`: object declares a trait but omits one abstract method.
- `tests/test_error_runtime_object_override_no_match.nabla` + `.diagnostic`: `override def` has no inherited method with that name/signature.
- `tests/test_error_runtime_object_override_signature.nabla` + `.diagnostic`: inherited method name exists but parameter or return type does not match.
- `tests/test_error_runtime_object_default_conflict.nabla` + `.diagnostic`: two traits provide conflicting concrete defaults and the object does not override them.
- `tests/test_error_runtime_object_non_trait_parent.nabla` + `.diagnostic`: `object X with ConcreteClass` is rejected because V0 runtime objects can compose traits only.
- `tests/test_error_runtime_object_name_collides_with_class.nabla` + `.diagnostic`: `class X ...` plus `object X with Trait ...` is rejected to avoid type/class-id collision; parentless static companions remain allowed.
- `tests/test_error_runtime_object_extends.nabla` + `.diagnostic`: `object X extends Parent(...)` is rejected with a clear V0 diagnostic.

**Targeted tests:**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_error_static_object_as_value.nabla
```

Expected: exact diagnostic matches the new `.diagnostic` file.

---

## Task 5: Lower runtime object value expressions to IR

**Objective:** Emit an address/value for the singleton when a runtime object name is used as an expression.

**Files:**
- Modify: `src/ast.cpp`
- Modify: `src/ir.*` if a new IR opcode is needed
- Modify: `src/ir_codegen.cpp`
- Modify: `src/runtime_asm.cpp` if static data emission belongs there

**Implementation options:**

- **Preferred V0:** Add an IR operation like `SingletonObjectRef` whose codegen returns the static object label pointer.
- Alternative: lower `Name` to a zero-argument synthetic function call that returns the singleton pointer. Use this only if it avoids invasive codegen changes without weakening diagnostics.

**Runtime layout:**

The singleton should look like a normal heap object to existing dispatch:

```text
[ class-id/header ][ no fields ]
```

Because there are no fields, the object can live in static data as an aligned label instead of being heap-allocated on first use. Ensure the pointer is aligned consistently with other heap object references.

Implementation must also specify:

- how runtime object names enter the same class-id namespace used by `classIdFor(...)`;
- where the data-section symbol is emitted and how it stores slot 0 as that class ID;
- how class-id collision is avoided between ordinary classes, generic base classes, boxed/string runtime tags, and singleton objects;
- whether runtime objects are included in `context.classes` directly or whether `classIdFor` learns about `objectInfos` explicitly.

Add focused tests for `Any` root behavior on singleton values:

- `tests/test_runtime_object_any_equals_identity.nabla`: two references to the same singleton compare equal through `Any.equals` / `==`.
- `tests/test_runtime_object_anyref_assignability.nabla`: `val x: AnyRef = Enabled` type-checks and can call `toString`/`hashCode` through the root type.
- `tests/test_runtime_object_any_tostring_hashcode.nabla`: a singleton override of `toString` and `hashCode` redispatches through an `Any`-typed helper.

**Targeted tests:**

```bash
rm -f test_runtime_object_trait_dispatch
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_trait_dispatch.nabla
./test_runtime_object_trait_dispatch
printf 'exit=%s\n' $?
```

Expected: `exit=42`.

---

## Task 6: Wire dynamic dispatch for singleton object methods

**Objective:** Make calls through trait/parent-typed values dispatch to object instance methods.

**Files:**
- Modify: `src/semantic_analyzer.cpp`
- Modify: `src/ir.cpp`
- Modify: `src/ir_codegen.cpp`
- Modify: `src/runtime_asm.cpp` if class ID / method table emission needs object entries

**Steps:**

1. Assign runtime objects class IDs or dispatch IDs in the same namespace as classes.
2. Include runtime object methods when building method dispatch candidates.
3. Ensure parent trait calls (`flag.enabled()`) resolve to the object's override at runtime.
4. Ensure `Any.equals` identity fallback works for two references to the same singleton.

**Targeted tests:**

```bash
rm -f test_runtime_object_trait_dispatch test_runtime_object_value_identity test_runtime_object_direct_method_call
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_trait_dispatch.nabla && ./test_runtime_object_trait_dispatch
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_value_identity.nabla && ./test_runtime_object_value_identity
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_direct_method_call.nabla && ./test_runtime_object_direct_method_call
```

Expected: all exit with `42`.

---

## Task 7: Preserve and test static namespace object behavior

**Objective:** Prove existing `object` companion/static use cases did not change.

**Files:**
- Create: `tests/test_static_object_namespace_regression.nabla`
- Create: `tests/test_static_object_namespace_regression.expected`

Test:

```nabla
object Tools {
    def answer(): Int = { 42 }
}

def main(): Int = {
    Tools.answer()
}
```

Expected:

```text
42
```

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_static_object_namespace_regression.nabla && ./test_static_object_namespace_regression
```

Expected: `exit=42`.

---

## Task 8: Documentation updates

**Objective:** Document the precise V0 semantics and non-goals.

**Files:**
- Modify: `docs/language.md`
- Modify: `docs/internals.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`
- Modify: `docs/plans/scala-like-list.md`

Required content:

- `object` has two modes:
  - static namespace mode: `object Name { def ... }`;
  - runtime singleton mode: `object Name with Trait { override def ... }`.
- Runtime singleton objects can be used as values and passed through trait/parent types.
- Runtime objects have no fields, no type parameters, and no constructors in V0.
- `object Nil with List[Nothing]` is still deferred until `Nothing`/variance design.
- `collections.list` still uses `Nil[T](defaultValue)` until the next phase.

---

## Task 9: Full verification and review

**Objective:** Ensure the feature is safe before PR.

Run targeted tests first:

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_trait_dispatch.nabla && ./test_runtime_object_trait_dispatch
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_value_identity.nabla && ./test_runtime_object_value_identity
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_runtime_object_direct_method_call.nabla && ./test_runtime_object_direct_method_call
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_static_object_namespace_regression.nabla && ./test_static_object_namespace_regression
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_error_static_object_as_value.nabla
```

Then run the full matrix:

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
PATH=/opt/data/local/usr/bin:$PATH make stdlib-docs
git diff --exit-code docs/stdlib
PATH=/opt/data/local/usr/bin:$PATH g++ -std=c++17 -Wall -Wextra -Werror src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o /tmp/nablac-werror
git diff --check
```

`make stdlib-docs && git diff --exit-code docs/stdlib` should be clean unless the implementation deliberately changes `stdlib/**/*.nabla` doc comments. If generated stdlib docs are intentionally changed, use the generated-doc idempotence pattern from `AGENTS.md` instead: save the current `git diff -- docs/stdlib`, run `make stdlib-docs`, save it again, and compare the two diff files.

Review requirements:

1. **Spec review:** verify static namespace behavior remains intact, runtime singleton V0 syntax is narrow, and non-goals are not accidentally implemented.
2. **Regression-risk review:** inspect parser ambiguity, object/class ID collisions, dynamic dispatch candidate selection, object value lowering, and identity equality.

---

## PR shape

Recommended branch:

```bash
git checkout -b feat/runtime-singleton-objects
```

Recommended commit message:

```bash
git commit -m "feat: add runtime singleton objects"
```

Recommended PR summary:

- adds runtime singleton `object Name with Trait` syntax;
- preserves static namespace `object Name { ... }` calls;
- supports singleton object values through trait dispatch;
- documents limitations before `Nothing`/variance and Scala-like `Nil`.
