# Surface publique stdlib 0.1

## Objectif

Clarifier la documentation utilisateur de la stdlib avant la RC 0.1, sans changer
le comportement du compilateur ni l'API effective.

La surface recommandee doit rester petite et lisible :

- `Array[T]` et le compagnon `Array` pour les tableaux applicatifs ;
- `Option[T]` pour les absences de valeur ;
- `Set[T]`, `Map[K, V]` et `Sized` pour les collections publiques ;
- `String`, `IO`, `Math` et `Random` pour les programmes simples.

## Hors scope

- Pas de changement de type de retour dans la stdlib.
- Pas de notion de modules prives.
- Pas de suppression des noms de compatibilite.
- Pas de renommage des helpers internes.

## Travail prevu

1. Revoir `docs/stdlib-api.md` pour expliciter que c'est la source de verite de
   classification public / compatibilite / interne.
2. Renforcer les commentaires `///` publics dans `stdlib/collections/*.nabla` :
   les exemples doivent nommer `Array[T]`, `Set[T]` et `Map[K, V]`, tandis que
   `ArrayObject[T]`, `ObjectArray[T]`, `IntArray`, `ArrayInt` et `arrayBase...`
   restent des details de representation ou de compatibilite.
3. Regenerer `docs/stdlib/` avec `make stdlib-docs`.
4. Verifier qu'aucun changement de comportement n'a ete introduit.

## Validation

- `make stdlib-docs`
- `make all-tests`
- `make examples`
- `make tooling-tests`
- `git diff --check`
- review spec/accuracy independante
