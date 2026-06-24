# Documentation HTML stdlib façon Scala

## Objectif

Rendre la référence HTML de la stdlib plus fiable et plus utile : les pages doivent charger leur CSS quel que soit leur niveau de répertoire, présenter une navigation API proche d'une référence Scala, lister clairement les types/fabriques/méthodes utilisables et montrer des exemples courts.

## Changements prévus

1. Corriger la génération des chemins relatifs (`style.css`, `index.html`) pour les modules racine comme `math.html` et `strings.html`.
2. Moderniser `tools/generate_stdlib_docs.py` :
   - support des déclarations `trait` et `object` documentées ;
   - sidebar de navigation par groupes : types, constructeurs/fabriques, méthodes ;
   - cards de signatures détaillées avec ancres stables ;
   - directive `@example ... @end` dans les commentaires `///`.
3. Ajouter des exemples utilisateur sur les pages principales : `Array`, `Option`, `Map`, `Set`, `Math`.
4. Compléter la page `Option` pour exposer ses méthodes publiques.
5. Ajouter un test d'outillage qui vérifie les liens locaux, les IDs dupliqués, la présence du layout API et les exemples générés.

## Vérification

- `make stdlib-docs`
- `tests/test_stdlib_docs_html.py`
- `make tooling-tests`
- `make all-tests`
- `make examples`
- `git diff --check`
