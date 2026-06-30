# Array.sorted

## Objectif

Ajouter une operation Scala-inspiree `sorted` sur la facade publique `Array[T]`.

## Semantique V0

- `array.sorted(lessThan)` retourne une nouvelle instance triee sans muter le tableau source.
- `lessThan(left, right)` joue le role d'ordre strict utilisateur, en attendant un futur `Ordering[T]` plus proche de Scala.
- Les tableaux primitifs exposent aussi `sorted()` quand un ordre naturel existe (`Int`, `Long`, `Float`, `Double`, `Bool`).
- Le tri est stable pour les elements equivalents selon le predicat.

## Plan TDD

1. Ajouter des tests runtime pour `Array[Int].sorted()`, `Array[Int].sorted((a,b)=>...)` et `Array[String].sorted(...)`.
2. Ajouter les helpers stdlib de copie + insertion sort dans les modules array primitifs et objet.
3. Etendre la signature compiler de la facade generique `Array[T]` pour `sorted(lessThan)` dans les contextes generiques.
4. Documenter l'API publique et regenerer `docs/stdlib`.
5. Valider avec tests cibles puis matrice Nabla standard.
