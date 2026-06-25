# Trait Iterable[T] pour les collections

## Objectif

Introduire un trait standard public `Iterable[T]` pour exprimer les collections
itérables par `foreach`, en cohérence avec `Sized` et avec la surface publique
`Array[T]`, `Set[T]` et `Map[K, V]`.

## Portée V1

- Ajouter `core.iterable` avec `trait Iterable[T]`.
- Le contrat minimal est `foreach(f: (T) => Unit): Unit`.
- `Iterable[T]` étend `Sized` afin que les consommateurs polymorphes disposent
  aussi de `size`, `isEmpty` et `nonEmpty`.
- Adapter les collections dont le type d'élément est clair :
  - `ArrayObject[T]` avec `Iterable[T]` ;
  - `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble`, `ArrayBool` avec leur
    élément spécialisé ;
  - `RangeInt` / classes de plage int si le support trait générique est fiable ;
  - `Set[T]` avec `Iterable[T]` ;
  - `Map[K, V]` avec `Iterable[Tuple2[K, V]]` via ses entrées.
- Documenter que `keys()`, `values()` et `toArray()` restent des représentations
  de tableau actuelles, mais deviennent consommables via `Iterable[...]` quand
  le type statique le permet.

## Risques techniques

- Les traits génériques sont parsés, mais le backend peut manquer de chemins pour
  dispatcher `Iterable[Int].foreach` vers une classe concrète générique ou
  spécialisée. Ajouter d'abord un test minimal pour exposer ce trou.
- Les lambdas de type `(T) => Unit` nécessitent un corps à effet (`print`, appel
  Unit, etc.) ; une affectation seule peut inférer `Int` et fausser le test.
- Les méthodes de trait générique doivent être résolues par signature complète,
  comme les autres méthodes héritées, sans casser les traits non génériques
  existants (`Sized`).

## Tests RED/GREEN ciblés

1. `tests/test_trait_generic_iterable_dispatch.nabla` : trait générique minimal
   local, classe générique `Box[T]`, appel via `Iterable[Int]` et `foreach`.
2. `tests/test_stdlib_iterable_trait_arrays.nabla` : `Array.range`,
   `Array.fill[String]` ou `ArrayObject[String]` consommés via `Iterable[...]`.
3. `tests/test_stdlib_iterable_trait_set_map.nabla` : `Set[String]` consommé
   via `Iterable[String]` et `Map[String, Int]` via
   `Iterable[Tuple2[String, Int]]`.
4. Mettre à jour les tests `Sized` si nécessaire pour vérifier que `Iterable[T]`
   hérite bien de `Sized` sans régression.

## Implémentation

- Corriger la résolution/codegen des appels de méthode sur traits génériques si
  le test RED local échoue.
- Ajouter `stdlib/core/iterable.nabla`, importer `core.sized`, puis intégrer les
  imports `core.iterable` dans les modules de collection concernés.
- Ajouter `with Iterable[...]` aux classes publiques, en gardant `Sized` explicite
  seulement si nécessaire pour compatibilité/diagnostics ; sinon le trait parent
  de `Iterable` suffit.
- Annoter `foreach` existant en `override` dans les classes qui implémentent
  `Iterable`.
- Mettre à jour `docs/stdlib-api.md`, `docs/roadmap.md`, `docs/internals.md`,
  `AGENTS.md` et régénérer `docs/stdlib/`.

## Validation

- Tests ciblés ci-dessus, exécutables compris.
- `make all-tests`
- `make examples`
- `make tooling-tests`
- `make stdlib-docs` puis `git diff --exit-code docs/stdlib` si la doc générée
  est régénérée.
- `git diff --check`
- Revue indépendante spec/qualité avant commit.
