# Nabla Roadmap

Ce document capture l'etat courant du projet et les prochaines pistes de travail
pour reprendre facilement apres une pause.

## Etat Actuel

- Backend natif Linux x86_64 via assembleur, avec backend IR utilise par defaut.
- Typage statique avec fonctions, methodes, classes, generiques simples et lambdas.
- Controle de flux : `if` expression, `else if`, `match`, `while`, `for`.
- Standard library deja utile :
  - collections typées et facade `Array[T]`
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

1. Ameliorer l'ecriture de texte avec espaces dans `command_shell`.
   - Probleme actuel : `write PATH TEXT` et `append PATH TEXT` ne gerent qu'un
     token de texte.
   - Realise : `parts.drop(2).mkString(" ")` via `joinFrom` dans
     `examples/command_shell.nabla`.
   - Verifie avec `make test SRC=examples/command_shell.nabla`.

2. Exposer/renforcer `mkString` pour les tableaux de strings.
   - Realise : `objectArrayMkString` + alias compat `objectStringArrayMkString`
     pour `ArrayObject[String]`.
   - Verifie avec `make test SRC=tests/test_array_object_string_mk_string.nabla`.

3. Finaliser `match` V2.
   - Ajouter les gardes de branche (`motif if condition`).
   - Valider les gardes de type `Bool`.
   - Ajouter des diagnostics dédiés pour la branche finale `_`.

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
