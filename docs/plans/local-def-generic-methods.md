# Local def in generic functions and methods

## Goal

Allow block-local `def` helpers inside generic contexts, including global generic functions, methods on generic classes, generic methods, and trait default methods. V1 keeps existing local-function limits: no local generic functions, no overloads in the same local scope, and no implicit capture of enclosing values/parameters.

## TDD scope

- Convert the existing rejected generic-context probes into positive runtime regressions:
  - local helper in a generic function;
  - local helper in a method of a generic class.
- Add runtime coverage for:
  - local helper in a generic method;
  - local recursive helper in a generic trait default method;
  - local helper in a method of a generic trait/class using receiver values through explicit parameters.
- Preserve negative diagnostics for:
  - local generic `def name[T]`;
  - local overloads;
  - implicit capture of enclosing values/parameters.

## Implementation sketch

1. Remove the parser-level ban that rejects local functions when `currentFunctionTypeParameters` is non-empty.
2. Register local helper signatures with the surrounding type-parameter list so normal generic function validation and IR specialization can infer concrete type arguments from direct calls.
3. Parse local helper bodies with the same surrounding type-parameter list so signatures and body expressions can mention owner/method type parameters, while keeping `currentParsingClass` cleared to avoid implicit field/`this` capture.
4. Generate hidden `FunctionDefNode`s with the surrounding type parameters.
5. For direct calls to local helpers, pass the surrounding type-parameter list as explicit generic arguments so helpers that do not mention every surrounding type parameter in argument position still specialize correctly.
6. Keep function-value references to local helpers in generic contexts out of scope for V1, with an exact negative regression.

## Verification

- Targeted local-def regression tests.
- `make all-tests`.
- `make examples`.
- `make tooling-tests`.
- `git diff --check`.
