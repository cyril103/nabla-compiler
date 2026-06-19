# Nabla Roadmap

Ce document capture l'etat courant du projet et les prochaines pistes de travail
pour reprendre facilement apres une pause.

## Etat Actuel

- Backend natif Linux x86_64 via assembleur, avec backend IR utilise par defaut.
  Le compilateur signale explicitement une commande externe introuvable comme
  `nasm` ou `ld`.
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

### Fil conducteur produit/langage

La prochaine phase doit transformer Nabla de "langage qui compile beaucoup de
cas" en "langage ou l'utilisateur sait naturellement quelle API employer".
Le cap prioritaire est donc la coherence de surface, pas l'accumulation de
features.

- API publique cible : `Array[T]`, `Option[T]`, `Set[T]`, `String`, classes,
  methodes, lambdas et modules standards documentes.
- Details a cacher ou marquer internes : `IntArray`, `LongArray`,
  `FloatArray`, `DoubleArray`, `BoolArray`, `ObjectArray[T]`,
  `ArrayObject[T]`, helpers `arrayBase...` et fonctions specialisees de pont.
- Runtime a formaliser : valeurs taggees `Int`/`Long`/`Bool`, valeurs raw
  `Float`/`Double`, objets heap, tableaux natifs, slots nuls, conventions
  d'erreur et limites memoire.
- Typage a garder simple : sous-typage nominal pour les classes, generiques
  invariants par defaut, conversions explicites ou helpers stdlib.
- Documentation : la reference HTML doit devenir une doc utilisateur claire,
  avec une separation visible entre API publique et helpers internes.

Actions recommandees :

1. Ajouter un check CI pour `make stdlib-docs`.
2. Creer une courte specification vivante (`docs/spec.md` ou
   `docs/internals.md`) pour types, runtime, IR et conventions de stdlib.
3. Revoir la doc stdlib pour masquer ou signaler les helpers internes.
4. Stabiliser `Array[T]`, `Option[T]` et `Set[T]` comme surfaces utilisateur
   principales.
5. Ajouter des exemples idiomatiques sans `IntArray` / `ObjectArray[T]` quand
   une facade publique existe.
6. Reporter `Result[T]`, `Map[K,V]`, variance avancee et GC tant que cette
   surface n'est pas propre.

### Revue de code (16/06/2026, corrigée)

- P2 — Représentation des `Bool` correcte dans les chemins actuels, mais trop implicite :
  - `src/ast.cpp:233` émet les constantes booléennes IR comme `1` / `0`.
  - `src/ir_codegen.cpp:455` boxe ensuite ces constantes, donc `false` devient `1`
    runtime et `true` devient `3`.
  - `src/ir_codegen.cpp:338` teste bien le faux runtime par `cmp rax, 1`, et
    `src/runtime_asm.cpp:853` suit la même convention pour `Bool.toString`.
  - Correction de la revue précédente : l'inversion logique n'est pas confirmée
    dans les tests actuels. Le risque réel est une convention d'encodage booléen
    dispersée entre IR/codegen/runtime, fragile pour de futurs chemins backend.
- Corrigé — Vérification incomplète de `override` :
  - Le flag `override` est parsé et stocké (`src/parser.cpp:1252`) puis seules les règles de présence/supériorité sont contrôlées en semantique (`src/semantic_analyzer.cpp:205`).
  - La validation compare maintenant arité, paramètres, retour et paramètres génériques de méthode, avec substitutions des types hérités.
- Corrigé — Initialisation des tableaux natifs par défaut à valeur fixe `1` :
  - `src/ir_codegen.cpp:746` remplit maintenant chaque slot des tableaux natifs selon le type d’élément.
  - Le backend initialise désormais `FloatArray`, `DoubleArray` et `ObjectArray[T]` avec `0`, et conserve `1` pour les zéros/faux taggés de `Int`, `Long` et `Bool`.
- Corrigé — Fallbacks silencieux vers `Int` quand un type est inconnu :
  - `src/parser.cpp:1071` utilise maintenant un marqueur `<unresolved>` pour les identifiants non résolus.
  - `src/ir_codegen.cpp:211` échoue maintenant explicitement si un type IR est vide ou introuvable.
- P3 — Corrigé : la référence HTML de la stdlib est vérifiée par la CI :
  - `.github/workflows/ci.yml` lance maintenant `make stdlib-docs` puis
    `git diff --exit-code docs/stdlib` pour empêcher une documentation générée
    désynchronisée.

Actions suggérées pour la suite :

1. Centraliser la convention d'encodage booléen (tag/valeurs) et ajouter des tests de régression bool via constantes, comparaisons, fonctions et tableaux.

1. Finaliser la sémantique d'héritage.
   - Valider la résolution des champs hérités et les conflits de noms entre
     champs/méthodes.
   - Ajouter une erreur explicite quand un membre est ambigu.
2. Consolider le système `super`.
   - Confirmer les cas `super` dans chaînes de mixins / héritage.
   - Définir règles et tests pour les masquages explicites.
3. Améliorer l’ergonomie héritage/collisions de types.
   - Simplifier les constructeurs d’héritage (appel parent depuis sous-classe).
   - Introduire/valider `override` pour la redéfinition explicite.
   - Rendre la résolution des champs/méthodes héritées plus prédictible dans les
     exemples concrets.
4. Revenir sur le chantier `match` avancé.
   - Finaliser les motifs nommés et les gardes (`motif if condition`).
   - Ajouter des diagnostics propres pour les erreurs de portée/typage des gardes.
5. Réduire la friction entre héritage et collections.
   - Permettre `Set[Person]` et `ArrayObject[Person]` avec des instances
     `Student`, `Instructor`, `Volunteer` sans contorsions de type.
   - Documenter des motifs d’exemple pour utiliser ce cas facilement.
6. Ajouter des tests “mélange” héritage + autres fonctionnalités.
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
