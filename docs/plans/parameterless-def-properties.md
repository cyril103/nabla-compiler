# Parameterless `def` Properties Plan

**Goal:** Allow `def` to declare parameterless computed properties in addition to normal functions/methods, e.g. `def pi: Double = 3.14` and trait members like `def head: T`.

## Semantics

- `def name: T = expr` is sugar for a zero-argument function/method whose body is evaluated every time `name` is used.
- `def name: T` inside a trait is an abstract zero-argument method.
- `value.name` is sugar for `value.name()` when `name` resolves to a zero-argument method.
- `Object.name` is sugar for `Object.name()` when a static object/namespace member is a non-generic zero-argument def.
- Bare `name` is sugar for `name()` when `name` resolves to a unique zero-argument global function and no local variable/field/runtime object shadows it.
- `val` remains eager/storage-oriented: RHS evaluated at binding/allocation time. Parameterless `def` is computed on demand and not memoized.
- Decimal literals use `.` in source (`3.14`), not French comma notation.

## Initial scope

- Global parameterless defs: `def pi: Double = 3.14` usable as `pi`.
- Static object/namespace parameterless defs: `object Config { def base: Int = 40 }` usable as `Config.base`.
- Class/object methods: `def answer: Int = 42`, `override def answer: Int = ...`.
- Trait abstract members: `def head: T`.
- Constructor `val` accessors can satisfy trait `def head: T` because both are zero-argument methods.
- No property assignment syntax.
- No cached/lazy-val semantics; repeated use recomputes.

## TDD tests

- `test_parameterless_def_global_property`: simple RHS and bare use.
- `test_parameterless_def_lazy_evaluation`: body with `print` proves every use re-evaluates.
- `test_parameterless_def_trait_property`: trait abstract `def head: T`, constructor `val` implementation, external `xs.head` access.
- `test_parameterless_def_override_property`: class override with `override def value: Int = ...`, dynamic dispatch through parent type.
- `test_parameterless_def_static_object_property`: static object property `Config.base` plus compatibility call `Config.bonus()`.
- `test_error_parameterless_def_duplicate_paren_method`: `def value: Int` conflicts with `def value(): Int`.

## Implementation notes

- Parser: after `def name` and optional type parameters, accept either `(` parameters `)` or directly `: ReturnType`.
- Register parameterless property defs as ordinary zero-argument `FunctionSignature`s so existing overload/override/dispatch paths continue to work.
- Body parser should accept both `= expression` and `= { block }` for parameterless defs; normal paren functions can remain `= { block }` for now unless the implementation naturally generalizes.
- Expression parser: transform bare zero-arg global function references into `FunctionCallNode(..., args={})` only when unique and non-generic, preserving function-value references where a function type is expected.
- Postfix parser: transform `receiver.name` into `MethodCallNode(..., args={})` when `name` resolves to a zero-argument method; reject ambiguous or parameterized methods with a clear diagnostic.

## Validation

Run targeted tests first, then:

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
PATH=/opt/data/local/usr/bin:$PATH make stdlib-docs
PATH=/opt/data/local/usr/bin:$PATH g++ -std=c++17 -Wall -Wextra -Werror \
  src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp \
  src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o /tmp/nablac-werror
git diff --check
```
