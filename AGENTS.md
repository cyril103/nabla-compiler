# AGENTS.md

Ce fichier sert de guide de travail et de feuille de route pour les agents qui
contribuent au compilateur Nabla.

## Regle De Maintenance

- Avant chaque commit realise par un agent, mettre a jour ce fichier.
- Inclure la mise a jour de `AGENTS.md` dans le meme commit que le changement.
- Mettre a jour au minimum les sections `Etat Actuel`, `Feuille De Route` et
  `Journal Des Jalons` lorsque le changement les affecte.
- Ne pas marquer une etape comme terminee sans tests automatises correspondants.

## Vision

Nabla est un langage inspire de Scala compile directement vers de l'assembleur
x86-64 ELF Linux. Le langage vise une syntaxe concise, un modele objet simple,
un typage statique et un runtime minimal.

Pipeline cible :

```text
Source Nabla
  -> Lexer
  -> Parser / AST
  -> Analyse semantique
  -> Representation intermediaire
  -> Generation x86-64
  -> NASM + ld
```

## Etat Actuel

Le pipeline implemente actuellement :

- tokenisation et parsing des classes, imports, fonctions et methodes avec
  parametres,
  expressions arithmetiques, comparaisons, `if`, `while`, `for`, `val` et `var`;
- resolution des imports avec protection contre les cycles;
- objets avec champs de constructeur et appels de methodes parametres;
- fonctions globales appelables avec parametres;
- entiers immediats avec pointer tagging;
- portees lexicales locales, mutabilite et allocation statique des emplacements
  de pile;
- analyse semantique des classes, constructeurs, methodes, types de retour et
  affectations;
- diagnostics uniformes avec fichier, ligne, colonne et phase du compilateur;
- IR textuelle pour les fonctions globales, entiers, variables, affectations,
  operations binaires, appels de fonctions globales, `if`, `while`, `for`,
  objets et methodes;
- backend ASM par defaut depuis l'IR couvrant la suite positive actuelle
  (fonctions, variables, controle de flux, imports, objets et methodes);
- ancien backend direct depuis l'AST disponible temporairement via
  `--backend-ast`;
- tests de compilation et d'execution via `make all-tests`.

Limites importantes :

- les fonctions globales sont limitees a 6 parametres et les methodes a 5,
  conformement a la convention d'appel actuelle;
- le backend direct depuis l'AST est conserve comme repli temporaire;
- le tas est fixe et ne possede ni verification de depassement ni ramasse-miettes;
- les binaires historiques sous `build/` sont encore suivis par Git.

## Invariants D'Architecture

- Le parser construit la structure syntaxique et collecte les declarations.
- L'analyse semantique valide les noms et les types avant toute generation ASM.
- Le generateur ASM ne doit pas deviner le type d'une expression.
- Les emplacements de variables locales sont reserves une seule fois dans le
  prologue de fonction.
- Une allocation imbriquee ne doit jamais modifier l'adresse de l'objet parent.
- Les methodes sauvegardent `this` dans leur frame avant tout appel imbrique.
- Toute nouvelle fonctionnalite du langage doit avoir au moins un test positif.
- Toute nouvelle validation doit avoir au moins un test d'erreur.

## Commandes De Validation

Executer avant chaque commit :

```bash
make all-tests
g++ -std=c++17 -Wall -Wextra -Werror \
  src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp \
  src/ir_codegen.cpp \
  -o /tmp/nablac-werror
git diff --check
```

Les tests normaux utilisent un fichier voisin `<nom>.expected`. Les fichiers dont
le nom contient `error` ou `fail` doivent echouer pendant la compilation.

## Feuille De Route

### P0 - Parametres De Fonctions Et Methodes

- [x] Parser des parametres nommes et types.
- [x] Enregistrer les signatures completes dans le registre semantique.
- [x] Valider nombre et types des arguments.
- [x] Definir la convention d'appel x86-64 Nabla.
- [x] Ajouter des tests pour fonctions, methodes et erreurs d'appel.

### P1 - Diagnostics Sources

- [x] Ajouter colonne et fichier aux tokens.
- [x] Attacher une position source aux noeuds AST.
- [x] Introduire un type d'erreur commun pour lexer, parser et semantique.
- [x] Afficher des diagnostics sous la forme `fichier:ligne:colonne`.

### P1 - Representation Intermediaire

- [x] Definir une IR minimale independante de l'AST.
- [x] Abaisser tout l'AST semantiquement valide vers l'IR.
- [x] Representer les branchements et boucles dans l'IR.
- [x] Representer les objets, champs et appels de methodes dans l'IR.
- [ ] Deplacer l'allocation de pile et les conventions d'appel vers le backend.
- [x] Couvrir le backend ASM depuis IR pour tout le langage actuel.
- [x] Generer l'assembleur par defaut depuis l'IR.
- [ ] Retirer l'ancien backend direct depuis l'AST.

### P2 - Systeme De Types

- [ ] Formaliser `Unit`, `Int`, `String` et les types de classes.
- [ ] Ajouter les booleens ou definir officiellement `Int` comme condition.
- [ ] Ajouter les champs et methodes herites si l'heritage est retenu.
- [ ] Valider les types des branches, boucles et operateurs de facon uniforme.

### P2 - Runtime Et Objets

- [ ] Verifier les depassements du tas.
- [ ] Definir et utiliser de vraies vtables ou retirer leur emplacement reserve.
- [ ] Stabiliser la representation de `String`.
- [ ] Choisir une strategie memoire a long terme.

### P3 - Outillage

- [ ] Retirer les binaires suivis sous `build/`.
- [ ] Ajouter une cible de formatage.
- [ ] Ajouter une integration continue.
- [ ] Ajouter des tests unitaires du lexer, parser et analyseur semantique.

## Journal Des Jalons

- Prochain commit - Bascule du backend ASM par defaut vers l'IR.
- `baefaa6` - Ajout des objets et methodes au backend ASM depuis IR.
- `5a97aa4` - Ajout du controle de flux au backend ASM depuis IR.
- `48f7e1d` - Ajout d'un backend ASM experimental depuis l'IR.
- `0f33a89` - Ajout des objets, champs et appels de methodes dans l'IR.
- `f38e0a6` - Ajout du controle de flux `if`/`while`/`for` dans l'IR.
- `1dcff81` - Ajout de l'IR minimale, de `--emit-ir` et des snapshots IR.
- `8b7be03` - Ajout des diagnostics sources uniformes, de `CompilerError` et des
  tests de diagnostics exacts.
- `1062f09` - Ajout des parametres de fonctions et methodes, appels globaux,
  validation des arguments et convention d'appel x86-64.
- `93b942f` - Ajout de `AGENTS.md`, des conventions de contribution et de la
  feuille de route maintenue.
- `469f535` - Ajout de la phase d'analyse semantique et des validations de types.
- `165a521` - Stabilisation des variables locales, portees, pile et allocations
  d'objets imbriquees; execution verifiee dans la suite de tests.
- `45e893d` - Ajout initial de `val`, `var` et de la table des symboles locale.

## Prochaine Etape Recommandee

Retirer progressivement l'ancien backend direct depuis l'AST apres une derniere
passe de stabilisation du backend IR.
