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
  - `pathExists`
- Exemple principal `examples/command_shell.nabla` utilise maintenant `match`,
  les commandes fichier, et sert de vitrine pour l'ergonomie du langage.
- Support Vim minimal disponible dans `editor/vim`.
- Suite `make all-tests` verte au moment de cette mise a jour.

## Priorites Prochaine Session

1. Ameliorer l'ecriture de texte avec espaces dans `command_shell`.
   - Probleme actuel : `write PATH TEXT` et `append PATH TEXT` ne gerent qu'un
     token de texte.
   - Piste : ajouter un helper stdlib pour reconstruire une sous-liste de mots,
     par exemple `drop(2).mkString(" ")`, ou une fonction string dediee.

2. Exposer/renforcer `mkString` pour les tableaux de strings.
   - Verifier ce qui existe deja cote runtime/stdlib.
   - Rendre l'API accessible depuis `ArrayObject[String]` ou la facade
     generique si possible.
   - Ajouter des tests sur `ArrayObject[String].mkString`.

3. Ajouter de petites operations fichiers complementaires.
   - `renameFile(from, to): Bool`
   - eventuellement `createDir(path): Bool`
   - garder les retours simples (`Bool`) tant que Nabla n'a pas de `Result`.

4. Ameliorer `match` V2.
   - Gardes de branches, par exemple `_ if condition => ...`.
   - Eventuellement motifs de constantes nommees plus tard.
   - Garder la V1 actuelle simple : litteraux + `_`.

5. Nettoyer les diagnostics autour de `match` et `else if`.
   - Messages plus specifiques pour les branches manquantes.
   - Tests d'erreur pour branche apres `_`.

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
