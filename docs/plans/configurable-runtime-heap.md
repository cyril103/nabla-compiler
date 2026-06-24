# Configurable Runtime Heap Plan

**Goal:** Make the runtime bump-allocated heap configurable per compiled executable while keeping the 8 MiB default stable.

**Scope:** Compiler CLI + generated runtime assembly + tooling regression. No GC, no allocator redesign, no runtime environment-variable reader.

## Tasks

1. Add `--heap-size <octets>` to `nablac`.
   - Default remains `8388608` bytes.
   - Reject non-integers and values below one page (`4096`).
2. Thread the selected capacity into the IR backend runtime emitter.
   - `RuntimeASM::emit(...)` writes `heap_capacity: dq <value>`.
3. Add a tooling test that:
   - compiles a small program with `--heap-size 1048576 --keep-temp`;
   - verifies the kept assembly contains the requested capacity;
   - runs the executable;
   - verifies invalid heap-size diagnostics.
4. Document the option in user/internal docs and update project roadmap notes.

## Verification

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
git diff --check
```
