# Stdlib local helper cleanup — tranche 3

## Objectif

Continuer la réduction de la surface module-level non documentée en cachant les petits wrappers spécialisés `ArrayFloat` derrière des `def` locaux, sans changement d'API publique ni de documentation générée.

## Candidats retenus

- `stdlib/collections/float_array.nabla`
  - `floatArrayLength`
  - `floatArrayGet`
  - `floatArraySet`

Ces helpers ne portent pas de doc `///`, ne sont pas listés dans `docs/stdlib-api.md`, et ne sont utilisés que par les méthodes de `ArrayFloat` pour déléguer vers les helpers génériques `arrayBase...`.

## Étapes

1. Déplacer les trois wrappers dans les méthodes `ArrayFloat` qui en ont besoin, en dupliquant les petits `def` locaux lorsque nécessaire.
2. Vérifier que `make stdlib-docs` ne modifie pas `docs/stdlib/`.
3. Lancer des tests ciblés couvrant `ArrayFloat` et les tableaux primitifs génériques.
4. Lancer la matrice Nabla standard adaptée à un cleanup stdlib : `make all-tests </dev/null`, `make examples`, `make tooling-tests`, `git diff --check`.
5. Faire une revue API/docs avant commit.
