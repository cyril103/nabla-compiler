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
