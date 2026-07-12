# Array public facade

## Objectif

Faire de `Array[T]` le type de surface recommande et observable pour les fabriques
stdlib, tout en conservant les representations specialisees (`ArrayInt`,
`ArrayObject[T]`, etc.) comme details internes/compatibilite.

## Contraintes

- Ne pas supprimer brutalement les types de compatibilite existants: les tests et
  la stdlib peuvent encore les utiliser comme implementation.
- Les signatures publiques documentees doivent parler de `Array[T]`.
- Les expressions utilisateur telles que `Array.fill[String](...)`,
  `Array.empty[String]()` et `Array.tabulate[String](...)` doivent inferer un type
  assignable/observable comme `Array[String]`, pas forcer l'utilisateur a nommer
  `ArrayObject[String]`.
- Les specialisations primitives restent disponibles via le lowering et les
  alias existants.

## Option B retenue : facade publique, backends internes

`Array[T]` devient la facade nominale que l'utilisateur voit dans les
signatures, les annotations et les diagnostics. Les classes specialisees
existantes (`ArrayInt`, `ArrayLong`, `ArrayObject[T]`, etc.) restent les
backends de representation et de compatibilite : le compilateur peut les choisir
pour le lowering, mais la surface stdlib recommandee doit continuer a parler de
`Array[T]`.

Cette option evite deux mondes publics de tableaux. Elle impose en revanche que
les signatures publiques et les messages d'erreur effacent les noms de backend,
et que les tests de regression couvrent les chemins ou ces noms pourraient
reapparaitre.

## Etapes

1. Ajouter une regression montrant que les fabriques generiques `Array` se
   manipulent uniquement via `Array[T]` dans les annotations utilisateur.
2. Aligner les signatures stdlib effectives de `Array.empty`, `Array.fill` et
   `Array.tabulate` sur leur signature publique `Array[T]`.
3. Reduire les diagnostics/docs qui exposent `ArrayObject[T]` pour les fabriques
   recommandees.
4. Regenerer la documentation stdlib si les commentaires ou signatures publiques
   changent.
5. Valider avec les tests cibles array puis la matrice pertinente.

## Critere d'acceptation

- Les fabriques recommandees (`Array.apply`, `Array.empty`, `Array.fill`,
  `Array.tabulate`, `Array.range`) annoncent `Array[...]` dans leur signature
  publique.
- Les retours specialises restent autorises comme implementation interne, mais
  un code utilisateur annote avec `Array[T]` doit compiler pour les primitives
  comme pour les types reference/generiques supportes.
- Les diagnostics destines a l'utilisateur ne doivent pas exposer
  `ArrayObject[...]` quand `Array[...]` est le type de facade equivalent.
