# Local `def` lowering via function-value infrastructure

## Goal

Treat block-local `def` declarations as named function symbols during parsing and semantic resolution, then lower them through the same hidden-function/function-reference path already used by lambdas when a local function is taken as a value.

## V0 scope

- Allow `def name(args...): Return = body` inside expression blocks.
- Keep local `def` scoped to the enclosing block from its declaration onward.
- Support direct calls and self-recursive direct calls by resolving the local source name to a mangled hidden function symbol.
- Allow a local function body to call previously declared local sibling functions, because those are already hidden function symbols and do not require value capture.
- Support passing a local `def` as a function value by emitting a `FunctionReferenceNode` to the hidden function.
- Do not expose local helpers through module/global function overload discovery, stdlib docs, or public API names.

## Deliberate V0 limits

- No local overloads.
- No local generic functions.
- No implicit capture of enclosing locals, parameters, or `this`; local functions compile as hidden top-level functions and must receive needed values explicitly.
- No local functions inside generic function/class contexts yet; this avoids hidden non-generic helpers leaking unresolved type parameters.

## Tests

- Positive: local helper direct call.
- Positive: recursive local helper.
- Positive: local helper passed as function value.
- Positive: same local helper name in two different functions does not collide.
- Positive: local helper can call a previously declared sibling helper.
- Negative: helper is not visible outside the enclosing block/function.
- Negative: local generic/context-generic/overload/capture restrictions produce user-facing diagnostics.
