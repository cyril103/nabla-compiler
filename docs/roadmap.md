# Nabla Roadmap

Ce document capture l'etat courant du projet et les prochaines pistes de travail
pour reprendre facilement apres une pause.

## Etat Actuel

- Backend natif Linux x86_64 via assembleur, avec backend IR utilise par defaut.
- Typage statique avec fonctions, methodes, classes, generiques simples et lambdas.
- Controle de flux : `if` expression, `else if`, `match`, `while`, `for`.
- Support de l'héritage objet avec `extends` + `with` (mixins), classe racine
  implicite `Any` avec `toString(): String` et `hashCode(): Int`
  redéfinissables, détection de cycles / conflits et appel `super`.
  La conversion `toString()` est désormais disponible de façon uniforme pour les
  types primitifs usuels (`Int`, `Long`, `Float`, `Double`, `Bool`, `Char`) au niveau du
  backend runtime.
- Standard library deja utile :
  - collections typées et facade `Array[T]`
  - `collections.set` avec `Set[T]`, `add`, `remove`, `union`, `intersect`,
    `difference`
  - `Option[T]`
  - operations `String`
  - I/O console et fichiers texte
- I/O fichiers texte :
  - `readTextFile`
  - `writeTextFile`
  - `appendTextFile`
  - `deleteTextFile`
  - `renameTextFile`
  - `createDirectory`
  - `pathExists`
- Exemple principal `examples/command_shell.nabla` utilise maintenant `match`,
  les commandes fichier, et sert de vitrine pour l'ergonomie du langage,
  notamment `write` / `append` avec texte multi-mots.
- Support Vim minimal disponible dans `editor/vim`.
- Suite `make all-tests` verte au moment de cette mise a jour.

## Priorites Prochaine Session

1. Finaliser la sémantique d'héritage.
   - Valider la résolution des champs hérités et les conflits de noms entre
     champs/méthodes.
   - Ajouter une erreur explicite quand un membre est ambigu.
2. Consolider le système `super`.
   - Confirmer les cas `super` dans chaînes de mixins / héritage.
   - Définir règles et tests pour les masquages explicites.
3. Revenir sur le chantier `match` avancé.
   - Finaliser les motifs nommés et les gardes (`motif if condition`).
   - Ajouter des diagnostics propres pour les erreurs de portée/typage des gardes.
4. Ajouter des tests “mélange” héritage + autres fonctionnalités.
   - Cas de régression couvrant `super`, champs hérités, shadowing contrôlé
     et conflit translatif.

## Pistes Plus Larges

- Introduire un type `Result[T]` ou une convention d'erreurs plus riche pour
  l'I/O et le parsing.
- Agrandir ou rendre configurable le tas statique du runtime.
- Refactoriser les primitives runtime I/O pour eviter la duplication autour des
  chemins C.
- Ajouter un support editeur supplementaire :
  - VS Code/TextMate grammar
  - Treesitter plus tard si le langage se stabilise.
- Ajouter plus d'exemples applicatifs :
  - outil de notes avec fichiers
  - mini transformateur de texte
  - Game of Life avec sauvegarde/chargement de grille.

## Regle De Reprise

Avant de commencer une nouvelle feature :

```bash
git status --short
make all-tests
```

Puis travailler en petits commits, comme pendant cette session.
