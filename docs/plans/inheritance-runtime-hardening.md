# Inheritance Runtime Hardening Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Harden Nabla post-0.1 inheritance/runtime semantics by making member ambiguity explicit, extending mixed inheritance regression coverage, and documenting the current dispatch/equality model.

**Architecture:** Keep this post-0.1 pass narrow. Start with diagnostics and tests around already-supported inheritance features, then document the actual runtime dispatch strategy in `docs/internals.md` and refresh project roadmaps from RC mode to post-release mode. Avoid introducing new public language features in this pass.

**Tech Stack:** Nabla compiler C++17, existing `.nabla` test harness, generated x86-64 NASM runtime, Markdown docs.

---

### Task 1: Reject field/method member name ambiguity

**Objective:** Add an exact diagnostic for class hierarchies where a visible field and a visible method share the same source member name.

**Files:**
- Create: `tests/test_error_inheritance_field_method_conflict.nabla`
- Create: `tests/test_error_inheritance_field_method_conflict.diagnostic`
- Modify: `src/semantic_analyzer.cpp`

**TDD steps:**
1. Add negative programs where one parent contributes field `value` and another contributes method `value()` through `extends ... with ...`, including a generic field provider after type substitution.
2. Add a negative program where the current class declares/overrides a method whose source name conflicts with a visible inherited field.
3. Run the narrow compile commands and confirm they fail because compilation currently succeeds or produces the wrong diagnostic.
4. Add a side-effect-free semantic check after inherited method/field providers are collected.
5. Re-run the narrow tests and confirm the exact diagnostics match.

**Expected user behavior:** An unqualified member name must not silently prefer a field over a method when multiple inherited providers make the name ambiguous.

### Task 2: Add runtime regression for inherited field access plus dynamic method dispatch

**Objective:** Prove that inherited field reads and dynamic override dispatch keep working together when values are passed through a parent type.

**Files:**
- Create: `tests/test_inheritance_field_dispatch_mix.nabla`
- Create: `tests/test_inheritance_field_dispatch_mix.expected`

**TDD steps:**
1. Add a positive program with a parent field, a parent method reading it, and a child override combining `super` with child behavior.
2. Run the narrow compile/run command and confirm it fails if the behavior regresses.
3. No production change should be needed if the current compiler already supports it; keep it as a regression test.

### Task 3: Document dispatch/equality runtime boundaries

**Objective:** Update `docs/internals.md` with the current post-0.1 rules for class IDs, dynamic dispatch, `super`, `Any`/`String`, and equality/hash semantics.

**Files:**
- Modify: `docs/internals.md`

**Verification:** Documentation should match current tests and code; do not claim full Scala linearization, GC, or stable ABI.

### Task 4: Refresh post-0.1 roadmap and agent guide

**Objective:** Move project status away from release-candidate language and record the post-0.1 hardening focus.

**Files:**
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

**Verification:** The docs should say `v0.1.0` is already tagged and identify inheritance/runtime hardening as the next phase.

### Final verification

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
git diff --check
```
