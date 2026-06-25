# Scala-like List Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Introduire une `List[T]` idiomatique inspirée de Scala sans brûler les étapes : d'abord rendre `Cons(val head, val tail)` agréable, puis ajouter une `collections.list` V0 sûre, et enfin préparer `object Nil` / `Nothing` comme chantiers ultérieurs.

**Architecture:** Nabla sait déjà modéliser une liste fonctionnelle avec `trait List[T]`, `class Nil[T]` et `class Cons[T]`, mais il manque trois briques Scala-like : accesseurs de champs constructeur, singleton runtime `object`, et type bottom/covariance pour un `Nil` unique. Le plan livre d'abord les accesseurs `val` en constructeur et une stdlib `List[T]` expérimentale basée sur `Nil[T](defaultValue)`, puis documente explicitement les limites avant de concevoir le vrai `Nil` singleton.

**Tech Stack:** C++17 compiler (`src/parser.cpp`, `src/ast.*`, `src/semantic_analyzer.cpp`, `src/ir.*`, `src/ir_codegen.cpp`), stdlib Nabla (`stdlib/collections/*.nabla`, `stdlib/core/*.nabla`), docs générées (`tools/generate_stdlib_docs.py`), tests `.nabla` / `.expected` / `.stdout`.

**Status:** Phase 1 livrée : les paramètres constructeur `val` génèrent des accesseurs synthétiques zéro-argument, y compris pour les classes génériques, et peuvent satisfaire des méthodes abstraites de trait.

---

## Constat actuel

Ce qui marche déjà aujourd'hui :

```nabla
trait List[T] {
    def isEmpty(): Bool
    def head(): T
    def tail(): List[T]
}

class Nil[T](defaultValue: T) with List[T] {
    override def isEmpty(): Bool = { true }
    override def head(): T = { defaultValue }
    override def tail(): List[T] = { this }
}

class Cons[T](headValue: T, tailValue: List[T]) with List[T] {
    override def isEmpty(): Bool = { false }
    override def head(): T = { headValue }
    override def tail(): List[T] = { tailValue }
}
```

Les probes locales ont montré :

- `trait List[T]` + classes génériques `Nil[T]` / `Cons[T]` + dispatch via trait : faisable.
- `class Cons[T](val head: T) with List[T] {}` satisfait désormais `def head(): T` via un accesseur synthétique.
- `xs.head` ne parse pas : l'accès externe passe aujourd'hui par des méthodes `xs.head()`.
- `object Nil with List[Int]` ne parse pas : `object` est aujourd'hui un namespace statique, sans identité runtime ni `extends` / `with`.
- `Nil` Scala-like exige à terme `Nothing` + variance (`List[+T]`) ou une règle spéciale d'assignabilité.

## Décisions de scope

### Livrer maintenant

1. Syntaxe `val` dans les paramètres constructeur de classe :

   ```nabla
   class Cons[T](val head: T, val tail: List[T]) with List[T] {
       override def isEmpty(): Bool = { false }
   }
   ```

2. Le `val` génère un accesseur méthode zéro-argument `head(): T` / `tail(): List[T]`.
3. Un accesseur généré peut satisfaire une méthode abstraite de trait de même signature.
4. Accès utilisateur conservé en V0 sous forme méthode : `xs.head()` et non `xs.head`.
5. `collections.list` V0 avec `List[T]`, `Nil[T](defaultValue)`, `Cons[T](val head, val tail)`, et compagnon `object List`.

### Reporter explicitement

- `object Nil extends List[Nothing]` / `object Nil with List[Nothing]`.
- `Nothing`.
- Variance `List[+T]`.
- Pattern matching structurel `case Nil` / `case Cons(h, t)`.
- Accès propriété sans parenthèses `xs.head`.
- Vtables complètes si les tests V0 n'en ont pas besoin.

---

## Phase 1 — Accesseurs `val` de constructeur

### Task 1: Ajouter des tests RED pour les paramètres constructeur `val`

**Objective:** Définir le comportement attendu avant de toucher au parser ou à l'analyse sémantique.

**Files:**
- Create: `tests/test_constructor_val_accessor.nabla`
- Create: `tests/test_constructor_val_accessor.expected`
- Create: `tests/test_constructor_val_trait_implementation.nabla`
- Create: `tests/test_constructor_val_trait_implementation.expected`
- Create: `tests/test_error_constructor_val_duplicate_method.nabla`
- Create: `tests/test_error_constructor_val_duplicate_method.diagnostic`

**Step 1: Add positive accessor test**

`tests/test_constructor_val_accessor.nabla`:

```nabla
class Point(val x: Int, val y: Int) {
}

def main(): Int = {
    val p = new Point(40, 2)
    p.x() + p.y()
}
```

`tests/test_constructor_val_accessor.expected`:

```text
42
```

**Step 2: Add trait satisfaction test**

`tests/test_constructor_val_trait_implementation.nabla`:

```nabla
trait Named {
    def name(): String
}

class Person(val name: String) with Named {
}

def label(value: Named): String = {
    value.name()
}

def main(): Int = {
    if label(new Person("Ada")) == "Ada" { 42 } else { 1 }
}
```

`tests/test_constructor_val_trait_implementation.expected`:

```text
42
```

**Step 3: Add duplicate explicit method diagnostic test**

`tests/test_error_constructor_val_duplicate_method.nabla`:

```nabla
class Bad(val name: String) {
    def name(): String = {
        "manual"
    }
}

def main(): Int = { 0 }
```

`tests/test_error_constructor_val_duplicate_method.diagnostic` should expect a semantic error similar to:

```text
tests/test_error_constructor_val_duplicate_method.nabla:2:9: parser error: méthode déjà déclarée avec cette signature dans 'Bad': name
```

Exact wording can be adjusted, but keep it specific and stable.

**Step 4: Run RED checks**

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make nablac
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_accessor.nabla
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_trait_implementation.nabla
PATH=/opt/data/local/usr/bin:$PATH make all-tests
```

Expected: the new positive tests fail before implementation; the duplicate-method diagnostic may fail by parse error or missing diagnostic. Do not modify goldens to match old behavior.

**Step 5: Commit tests when implementation is not included in same commit**

```bash
git add tests/test_constructor_val_*.nabla tests/test_constructor_val_*.expected tests/test_error_constructor_val_duplicate_method.*
git commit -m "test: specify constructor val accessors"
```

---

### Task 2: Représenter `val` dans les métadonnées de champs

**Objective:** Conserver l'information “ce champ expose un accesseur public généré”.

**Files:**
- Modify: `src/compiler_context.hpp:37-41`
- Modify if needed: `src/ast.hpp` / `src/ast.cpp` field/parameter structures

**Step 1: Extend field metadata**

In `CompilerContext::FieldInfo`, add a flag:

```cpp
struct FieldInfo {
    std::string name;
    std::string type;
    SourceLocation location;
    bool isPublicVal = false;
};
```

If an AST-level constructor parameter struct exists separately, add the same flag there. If constructor fields are only represented through `FieldInfo`, keep the change limited to `FieldInfo`.

**Step 2: Update aggregate initializers**

Search and update every `FieldInfo` initializer. Existing fields must default to `false`; explicit `val` fields set `true`.

Useful search:

```bash
rg "FieldInfo|fields\.push_back|\{fieldName, fieldType" src
```

Use `search_files` in Hermes instead of raw `rg` when operating through tools.

**Step 3: Build**

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make nablac
```

Expected: C++ builds cleanly.

**Step 4: Commit**

```bash
git add src/compiler_context.hpp src/ast.hpp src/ast.cpp src/parser.cpp src/semantic_analyzer.cpp
git commit -m "refactor: track constructor val fields"
```

---

### Task 3: Parser `val` dans les paramètres constructeur de classe

**Objective:** Accepter `class Point(val x: Int)` dans les classes, tout en rejetant `val` dans les traits et dans les paramètres de fonctions.

**Files:**
- Modify: `src/parser.cpp:149-402` (`Parser::parseClassDefinition`)
- Modify if needed: `src/token.*` only if `val` is not already tokenized as `KW_VAL`
- Test: tests from Task 1

**Step 1: Locate constructor parameter parsing**

`Parser::parseClassDefinition` currently consumes fields around the class header and pushes into:

```cpp
context.classes[className].fields.push_back(...);
context.classLayouts[className][fieldName] = offset;
```

Before reading `fieldName`, detect optional `KW_VAL` only for non-trait class constructor fields.

**Step 2: Implement parser change**

Pseudo-code for both ordinary constructor field parsing and inherited constructor signature suffix parsing:

```cpp
bool isPublicVal = false;
if (!isTrait && match(TokenType::KW_VAL)) {
    isPublicVal = true;
}
Token fieldNameToken = consume(TokenType::IDENTIFIER, "Nom de champ attendu");
...
context.classes[className].fields.push_back({fieldName, fieldType, fieldTypeLocation, isPublicVal});
```

Do not allow `val` in `trait` constructor positions because traits have no constructor fields.

**Step 3: Add parser diagnostic if `val` appears in a forbidden position**

If `trait Bad(val x: Int)` is syntactically reachable, add a negative test:

- Create: `tests/test_error_trait_constructor_val.nabla`
- Create: `tests/test_error_trait_constructor_val.diagnostic`

Expected: parser or semantic error that traits cannot declare constructor fields / `val` fields.

**Step 4: Run focused checks**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_accessor.nabla
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_error_trait_constructor_val.nabla
```

Expected: positive may still fail semantically until generated accessors exist; forbidden-position test should now fail with the intended diagnostic.

**Step 5: Commit**

```bash
git add src/parser.cpp tests/test_error_trait_constructor_val.*
git commit -m "feat: parse constructor val fields"
```

---

### Task 4: Générer les signatures d'accesseurs zéro-argument

**Objective:** Faire apparaître `val head: T` comme méthode `head(): T` dans `context.classes[className].methods`, pour appels utilisateurs et satisfaction des traits.

**Files:**
- Modify: `src/parser.cpp` or `src/semantic_analyzer.cpp`
- Modify: `src/compiler_context.hpp` if helper needed
- Test: `tests/test_constructor_val_accessor.nabla`
- Test: `tests/test_constructor_val_trait_implementation.nabla`
- Test: `tests/test_error_constructor_val_duplicate_method.nabla`

**Step 1: Choose generation point**

Preferred location: after parsing a class header and before/while validating method duplicates, generate synthetic signatures for every `FieldInfo{isPublicVal=true}`.

Generated signature shape:

```cpp
CompilerContext::FunctionSignature sig;
sig.parameters = {};
sig.returnType = field.type;
sig.location = field.location;
sig.returnTypeLocation = field.location;
sig.isOverride = false;   // see Step 3 for trait satisfaction
sig.isAbstract = false;
```

Store it under method name `field.name` and in `methodOverloads[field.name]` using the same mangling/signature helpers as normal zero-arg methods.

**Step 2: Detect duplicate explicit methods**

If the class body defines `def name(): ...` and constructor has `val name: ...`, report a stable diagnostic instead of silently choosing one.

Suggested diagnostic:

```text
parser error: méthode déjà déclarée avec cette signature dans 'Bad': name
```

**Step 3: Satisfy inherited abstract methods without requiring source `override`**

A constructor `val` is intentionally Scala-like: it should be able to implement a trait abstract `def` without a separate `override def`.

Rules:

- If `val name: String` matches an inherited abstract `def name(): String`, accept it.
- If it matches an inherited concrete method, require explicit design decision. For V0, reject with a clear diagnostic requiring an explicit method override; this avoids accidental shadowing.
- If return type mismatches, reuse existing override/abstract diagnostic path and list inherited candidates if available.

**Step 4: Run focused tests**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_accessor.nabla
./test_constructor_val_accessor
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_trait_implementation.nabla
./test_constructor_val_trait_implementation
PATH=/opt/data/local/usr/bin:$PATH make all-tests
```

Expected: the two positive tests return `42`; the duplicate-method diagnostic matches.

**Step 5: Commit**

```bash
git add src tests/test_constructor_val_*.expected tests/test_error_constructor_val_duplicate_method.diagnostic
git commit -m "feat: generate constructor val accessors"
```

---

### Task 5: Émettre le code des accesseurs générés

**Objective:** Faire fonctionner les appels `p.x()` / `xs.head()` au runtime en chargeant le slot de champ existant.

**Files:**
- Modify: `src/ir.cpp` / `src/ir.hpp` if accessors are lowered through IR generation
- Modify: `src/ir_codegen.cpp` if synthetic methods need special emission
- Modify: `src/runtime_asm.cpp` only if method symbol emission requires runtime metadata
- Test: `tests/test_constructor_val_accessor.nabla`
- Test: `tests/test_constructor_val_trait_implementation.nabla`

**Step 1: Locate method codegen path**

Find where class methods are emitted and how field loads are generated. Existing helpers include field lookup/layout and IR field load (`emitFieldLoad`).

**Step 2: Implement synthetic getter body**

For each generated accessor method, emit equivalent of:

```nabla
def head(): T = {
    head
}
```

Implementation choices:

- Option A: create a synthetic AST `FunctionDefNode` during parsing.
- Option B: teach IR/codegen to emit synthetic methods recorded in context.

Prefer Option A if existing method emission expects AST nodes and dynamic dispatch registration is AST-driven.

**Step 3: Verify direct and trait-dispatched calls**

Run:

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_constructor_val_trait_implementation.nabla
./test_constructor_val_trait_implementation
```

Expected: direct call and call through trait both return `42`.

**Step 4: Add a generic accessor regression**

Create: `tests/test_constructor_val_generic_accessor.nabla`

```nabla
class Box[T](val value: T) {
}

def main(): Int = {
    val i = new Box[Int](42)
    val s = new Box[String]("ok")
    if i.value() == 42 && s.value() == "ok" { 42 } else { 1 }
}
```

Create expected file returning `42`.

**Step 5: Commit**

```bash
git add src tests/test_constructor_val_generic_accessor.*
git commit -m "feat: emit constructor val getter methods"
```

---

### Task 6: Documenter les accesseurs `val`

**Objective:** Expliquer la syntaxe et ses limites avant d'utiliser `List` dessus.

**Files:**
- Modify: `docs/language.md`
- Modify: `docs/internals.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md` if the feature changes the known status/limits

**Step 1: Update language guide**

In `docs/language.md` near “Classes”, add:

```nabla
class Point(val x: Int, val y: Int) {
}

val p = new Point(40, 2)
p.x() + p.y()
```

Clarify:

- `val` constructor params are immutable fields.
- They expose zero-argument accessors.
- They can satisfy abstract trait methods with the same name and return type.
- Property syntax `p.x` is not part of this PR.

**Step 2: Update internals**

In `docs/internals.md`, note that public constructor vals still use normal field slots; the only addition is a generated getter method and dispatch metadata.

**Step 3: Update roadmap**

Mention constructor `val` as a stepping stone for `List[T]` and Scala-like collection ergonomics.

**Step 4: Verify docs-only consistency**

```bash
git diff --check
```

**Step 5: Commit**

```bash
git add docs/language.md docs/internals.md docs/roadmap.md AGENTS.md
git commit -m "docs: document constructor val accessors"
```

---

## Phase 2 — `collections.list` V0

### Task 7: Ajouter un module stdlib `collections.list`

**Objective:** Introduire une liste immutable minimale, déjà utile, sans prétendre être le modèle Scala final.

**Files:**
- Create: `stdlib/collections/list.nabla`
- Modify: generated docs under `docs/stdlib/` after `make stdlib-docs`
- Test: later tasks

**Step 1: Create module skeleton**

`stdlib/collections/list.nabla`:

```nabla
/// @module List
/// Liste chaînée immutable expérimentale.
///
/// List[T] est inspirée de Scala, mais la V0 utilise encore Nil[T](defaultValue)
/// au lieu d'un singleton Nil polymorphe. Utiliser List.empty[T](defaultValue)
/// et List.cons[T](head, tail) pour construire les listes.
/// @example Construire et lire une liste
/// import collections.list
///
/// val xs = List.cons[Int](1, List.cons[Int](2, List.empty[Int](0)))
/// xs.head() + xs.tail().head()
/// @end

import core.iterable
import core.option
import collections.array

/// Liste chaînée immutable.
/// @signature trait List[T]
/// @status Experimentale
trait List[T] with Iterable[T] {
    /// Indique si la liste est vide.
    /// @signature def isEmpty(): Bool
    /// @status Recommandee
    def isEmpty(): Bool

    /// Retourne la tête de liste, ou la valeur par défaut du Nil V0.
    /// @signature def head(): T
    /// @status Experimentale
    def head(): T

    /// Retourne la queue de liste.
    /// @signature def tail(): List[T]
    /// @status Recommandee
    def tail(): List[T]

    /// Retourne la tête si elle existe.
    /// @signature def headOption(): Option[T]
    /// @status Recommandee
    def headOption(): Option[T]
}
```

**Step 2: Add classes**

```nabla
/// Liste vide V0.
/// @status Compatibilite
class Nil[T](defaultValue: T) with List[T] {
    override def isEmpty(): Bool = { true }
    override def size(): Int = { 0 }
    override def foreach(f: (T) => Unit): Unit = { while false { 0 } }
    override def head(): T = { defaultValue }
    override def tail(): List[T] = { this }
    override def headOption(): Option[T] = { Option.none[T]() }
}

/// Cellule non vide.
/// @status Recommandee
class Cons[T](val head: T, val tail: List[T]) with List[T] {
    override def isEmpty(): Bool = { false }
    override def size(): Int = { 1 + tail.size() }
    override def foreach(f: (T) => Unit): Unit = {
        f(head)
        tail.foreach(f)
    }
    override def headOption(): Option[T] = { Option.some[T](head) }
}
```

Note: this assumes Phase 1 constructor `val` generates `head()` / `tail()`.

**Step 3: Add companion constructors**

```nabla
object List {
    /// Construit une liste vide V0.
    /// @signature def empty[T](defaultValue: T): List[T]
    /// @status Experimentale
    def empty[T](defaultValue: T): List[T] = {
        new Nil[T](defaultValue)
    }

    /// Ajoute un élément en tête.
    /// @signature def cons[T](head: T, tail: List[T]): List[T]
    /// @status Recommandee
    def cons[T](head: T, tail: List[T]): List[T] = {
        new Cons[T](head, tail)
    }
}
```

**Step 4: Compile a scratch import**

```bash
cat > /tmp/list_smoke.nabla <<'EOF'
import collections.list

def main(): Int = {
    val xs = List.cons[Int](1, List.cons[Int](41, List.empty[Int](0)))
    xs.head() + xs.tail().head()
}
EOF
PATH=/opt/data/local/usr/bin:$PATH build/nablac /tmp/list_smoke.nabla
./list_smoke
```

Expected process exit: `42`.

**Step 5: Commit**

```bash
git add stdlib/collections/list.nabla
git commit -m "feat(stdlib): add experimental List module"
```

---

### Task 8: Ajouter les opérations minimales de `List[T]`

**Objective:** Rendre `List` utile sans surcharger l'API : `nonEmpty`, `prepend`, `map`, `filter`, `fold`, `mkString`.

**Files:**
- Modify: `stdlib/collections/list.nabla`
- Test: `tests/test_stdlib_list_operations.nabla`
- Test: `tests/test_stdlib_list_operations.stdout` if printed output is used
- Test: `tests/test_stdlib_list_operations.expected`

**Step 1: Extend trait with defaults where safe**

Add to `trait List[T]`:

```nabla
def nonEmpty(): Bool = { !this.isEmpty() }
def prepend(value: T): List[T] = { new Cons[T](value, this) }
def fold[U](initial: U, f: (U, T) => U): U
def map[U](defaultValue: U, f: (T) => U): List[U]
def filter(defaultValue: T, predicate: (T) => Bool): List[T]
def mkString(separator: String): String
```

Be careful: if default trait methods calling `new Cons[T]` cause codegen issues, move those methods to `Nil` / `Cons` implementations instead.

**Step 2: Implement `Nil`**

```nabla
override def fold[U](initial: U, f: (U, T) => U): U = { initial }
override def map[U](defaultValue: U, f: (T) => U): List[U] = { List.empty[U](defaultValue) }
override def filter(defaultValue: T, predicate: (T) => Bool): List[T] = { this }
override def mkString(separator: String): String = { "" }
```

**Step 3: Implement `Cons`**

```nabla
override def fold[U](initial: U, f: (U, T) => U): U = {
    tail.fold[U](f(initial, head), f)
}

override def map[U](defaultValue: U, f: (T) => U): List[U] = {
    List.cons[U](f(head), tail.map[U](defaultValue, f))
}

override def filter(defaultValue: T, predicate: (T) => Bool): List[T] = {
    val filteredTail = tail.filter(defaultValue, predicate)
    if predicate(head) {
        List.cons[T](head, filteredTail)
    } else {
        filteredTail
    }
}

override def mkString(separator: String): String = {
    if tail.isEmpty() {
        head.toString()
    } else {
        head.toString() + separator + tail.mkString(separator)
    }
}
```

If `head.toString()` on generic `T` reveals a current generic dispatch limitation, reduce V0 `mkString` scope or implement a `mkStringWith(show: (T) => String, separator: String)` instead.

**Step 4: Add operation test**

`tests/test_stdlib_list_operations.nabla`:

```nabla
import collections.list

// helper avoids relying only on process exit
def sum(values: List[Int]): Int = {
    values.fold[Int](0, (acc: Int, value: Int) => acc + value)
}

def main(): Int = {
    val xs = List.cons[Int](1, List.cons[Int](2, List.cons[Int](39, List.empty[Int](0))))
    val ys = xs.map[Int](0, x => x + 1)
    val zs = ys.filter(0, x => x > 2)
    if sum(xs) == 42 && sum(ys) == 45 && sum(zs) == 42 && xs.mkString(",") == "1,2,39" {
        42
    } else {
        1
    }
}
```

**Step 5: Run focused tests**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_stdlib_list_operations.nabla
./test_stdlib_list_operations
```

Expected process exit: `42`.

**Step 6: Commit**

```bash
git add stdlib/collections/list.nabla tests/test_stdlib_list_operations.*
git commit -m "feat(stdlib): add List operations"
```

---

### Task 9: Ajouter conversions `List` / `Array` sans exposer les internes

**Objective:** Faciliter l'adoption de `List` depuis les collections publiques existantes.

**Files:**
- Modify: `stdlib/collections/list.nabla`
- Create: `tests/test_stdlib_list_from_array.nabla`
- Create: `tests/test_stdlib_list_from_array.expected`

**Step 1: Add `List.fromArray`**

Because Nabla currently has no general while loop over `Array[T]` with mutation-free local recursion guaranteed for every `T`, implement the simplest correct version that the current `Array[T]` facade supports.

Candidate API:

```nabla
object List {
    def fromArray[T](values: Array[T], defaultValue: T): List[T] = {
        List.fromArrayFrom[T](values, defaultValue, 0)
    }

    def fromArrayFrom[T](values: Array[T], defaultValue: T, index: Int): List[T] = {
        if index >= values.size() {
            List.empty[T](defaultValue)
        } else {
            List.cons[T](values.get(index), List.fromArrayFrom[T](values, defaultValue, index + 1))
        }
    }
}
```

Adjust method names to existing `Array[T]` facade (`size()` / `length()` / `get()`) after inspecting `stdlib/collections/array.nabla`.

**Step 2: Test with public `Array[T]` only**

```nabla
import collections.array
import collections.list

// no IntArray, no arrayBase helpers

def main(): Int = {
    val values = Array(10, 20, 12)
    val list = List.fromArray[Int](values, 0)
    if list.size() == 3 && list.fold[Int](0, (acc: Int, value: Int) => acc + value) == 42 {
        42
    } else {
        1
    }
}
```

**Step 3: Run focused test**

```bash
PATH=/opt/data/local/usr/bin:$PATH build/nablac tests/test_stdlib_list_from_array.nabla
./test_stdlib_list_from_array
```

Expected process exit: `42`.

**Step 4: Commit**

```bash
git add stdlib/collections/list.nabla tests/test_stdlib_list_from_array.*
git commit -m "feat(stdlib): build List from Array facade"
```

---

### Task 10: Ajouter cookbook et documentation utilisateur

**Objective:** Présenter `List` comme expérimentale et claire, sans masquer les limites de `Nil[T](defaultValue)`.

**Files:**
- Create: `examples/stdlib_list_cookbook.nabla`
- Create: `examples/stdlib_list_cookbook.expected`
- Modify: `docs/stdlib-api.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`
- Generated: `docs/stdlib/collections/list.html` and navigation updates after `make stdlib-docs`

**Step 1: Add example cookbook**

`examples/stdlib_list_cookbook.nabla`:

```nabla
import collections.list

// List V0: defaultValue sert uniquement au Nil temporaire.
def main(): Int = {
    val xs = List.cons[Int](1, List.cons[Int](2, List.cons[Int](39, List.empty[Int](0))))
    val doubled = xs.map[Int](0, x => x * 2)
    val kept = doubled.filter(0, x => x > 2)
    val total = kept.fold[Int](0, (acc: Int, value: Int) => acc + value)
    if xs.mkString(",") == "1,2,39" && total == 82 {
        42
    } else {
        1
    }
}
```

**Step 2: Wire example into examples make target**

Inspect existing `Makefile` examples list/pattern. Add the cookbook if examples are enumerated; otherwise ensure `.expected` naming follows the existing pattern.

**Step 3: Update stdlib classification**

In `docs/stdlib-api.md`, add `List[T]` as `Experimentale`, not yet fully recommended, with explicit note:

- recommended for experimenting with immutable recursive collections;
- `Array[T]` remains the primary indexed collection;
- `Nil[T](defaultValue)` is temporary until runtime singleton `object` + `Nothing`/variance design.

**Step 4: Generate docs**

```bash
PATH=/opt/data/local/usr/bin:$PATH make stdlib-docs
git diff -- docs/stdlib | less
```

Verify generated `docs/stdlib/collections/list.html` includes:

- module hero;
- import line;
- `@status Experimentale` badge;
- examples rendered;
- no references to `ArrayObject[T]`, `ObjectArray[T]`, or arrayBase helpers.

**Step 5: Run examples**

```bash
PATH=/opt/data/local/usr/bin:$PATH make examples
```

Expected: green.

**Step 6: Commit**

```bash
git add stdlib/collections/list.nabla docs/stdlib docs/stdlib-api.md docs/roadmap.md AGENTS.md examples/stdlib_list_cookbook.* Makefile
git commit -m "docs: document experimental List collection"
```

---

## Phase 3 — Design ultérieur de `Nil` Scala-like

### Task 11: Écrire le plan séparé pour `object` singleton runtime

**Objective:** Ne pas mélanger la stdlib `List` V0 avec la refonte runtime nécessaire à `object Nil`.

**Files:**
- Create: `docs/plans/runtime-singleton-object.md`

**Step 1: Capture target syntax**

Document target examples:

```nabla
trait IntList {
    def isEmpty(): Bool
}

object IntNil with IntList {
    override def isEmpty(): Bool = { true }
}

def main(): Int = {
    val xs: IntList = IntNil
    if xs.isEmpty() { 42 } else { 1 }
}
```

Do not start with generic `Nil` yet.

**Step 2: List compiler implications**

Plan must cover:

- parser accepts `object Name with Trait { ... }`;
- `object` gains ClassInfo-like runtime metadata;
- singleton allocation/static object label in ASM;
- object value expression `Name` when used as a value, while preserving `Name.method()` static namespace calls;
- dispatch through trait/parent type;
- docs update that `object` is no longer only a namespace.

**Step 3: Add explicit non-goals**

Defer:

- fields in objects;
- generic singleton objects;
- `Nothing`;
- variance;
- pattern matching.

**Step 4: Commit plan**

```bash
git add docs/plans/runtime-singleton-object.md
git commit -m "docs: plan runtime singleton objects"
```

---

### Task 12: Écrire le plan séparé pour `Nothing` et variance minimale

**Objective:** Préparer le vrai `Nil` Scala-like uniquement après les singletons runtime.

**Files:**
- Create: `docs/plans/nothing-and-list-variance.md`

**Step 1: Target syntax**

```nabla
trait List[+T] with Iterable[T] {
    def isEmpty(): Bool
    def headOption(): Option[T]
}

object Nil with List[Nothing] {
    override def isEmpty(): Bool = { true }
}

class Cons[T](val head: T, val tail: List[T]) with List[T] {
    override def isEmpty(): Bool = { false }
}

def main(): Int = {
    val xs: List[Int] = Nil
    42
}
```

**Step 2: Decide between real variance and special-case Nil**

The plan must explicitly compare:

- real covariance syntax `trait List[+T]`;
- a smaller `Nothing` bottom type with `List[Nothing]` assignability special-case;
- keeping `Nil[T]` factory as long-term pragmatic choice.

**Step 3: Define acceptance tests**

At minimum:

- `Nil` assignable to `List[Int]` and `List[String]`;
- `Cons[Int](1, Nil)` type-checks;
- `List[String]` does not become assignable to `List[Any]` unless variance is actually implemented;
- diagnostics explain invariant generic mismatch.

**Step 4: Commit plan**

```bash
git add docs/plans/nothing-and-list-variance.md
git commit -m "docs: plan Nothing and List variance"
```

---

## Verification matrix for implementation PRs

Run before every PR that changes compiler/runtime behavior:

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
PATH=/opt/data/local/usr/bin:$PATH make stdlib-docs
git diff --exit-code docs/stdlib
g++ -std=c++17 -Wall -Wextra -Werror src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o /tmp/nablac-werror
git diff --check
```

For docs-only plan PRs, run at least:

```bash
git diff --check
```

For `List` stdlib PRs, also run focused smoke tests by compiling/running the new `tests/test_stdlib_list_*.nabla` files directly before the full matrix.

## Review requirements

For Phase 1 compiler changes, run two independent reviews before merge:

1. **Spec review:** constructor `val` semantics, trait satisfaction, duplicate/shadowing rules, non-goals.
2. **Regression-risk review:** parser/codegen interaction, dynamic dispatch metadata, generic substitution, field layout stability.

For Phase 2 stdlib changes, review specifically:

- no accidental exposure of `ArrayObject[T]` / raw arrays in user docs;
- `Nil[T](defaultValue)` limitations are stated honestly;
- operations terminate for normal small lists;
- recursion depth is acceptable for V0 examples but not oversold.
