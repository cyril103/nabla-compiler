# Stdlib local helper cleanup — tranche 2

## Objectif

Réduire la surface module-level non documentée en cachant une petite tranche de helpers spécialisés derrière des `def` locaux, sans changer l'API publique ni la documentation générée.

## Candidats retenus

- `stdlib/collections/double_array.nabla`
  - `doubleArrayLength`
  - `doubleArrayGet`
  - `doubleArraySet`

Ces helpers ne portent pas de doc `///`, ne sont pas listés dans `docs/stdlib-api.md`, et ne sont utilisés que par les méthodes de `ArrayDouble` pour déléguer vers les helpers génériques `arrayBase...`.

## Étapes

1. Déplacer les trois wrappers dans les méthodes `ArrayDouble` qui en ont besoin, en dupliquant les petits `def` locaux lorsque nécessaire.
2. Vérifier que `make stdlib-docs` ne modifie pas `docs/stdlib/`.
3. Lancer des tests ciblés couvrant `ArrayDouble` (`generic_array_primitive_filter_fold`, `generic_array_float_double`, `stdlib_array_to_string`, etc.).
4. Lancer la matrice Nabla standard adaptée à un cleanup stdlib : `make all-tests`, `make examples`, `make tooling-tests`, `git diff --check`.
5. Faire une revue API/docs avant commit.
