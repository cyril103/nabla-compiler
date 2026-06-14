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
- identifiants et chemins d'import avec lettres, chiffres et `_`;
- resolution des imports relatifs, depuis la racine projet et depuis `stdlib/`,
  avec protection contre les cycles;
- objets avec champs de constructeur et appels de methodes parametres;
- fonctions globales appelables avec parametres;
- types fonction-valeur canoniques `Fn(...)->...`, references de fonctions
  nommees et appels indirects, avec lambdas sans capture `(x: Int) => { ... }` et
  `(acc: Int, value: Int) => { ... }`;
- closures avec capture par valeur pour les lambdas de types fonction
  canoniques;
- representation interne canonique des types fonction sous forme `Fn(...)->...`;
- syntaxe de types fonction parenthesee comme `(Int) => Int`, `(Int) => Unit`,
  `(Int, Int) => Int` et `(String) => Int`;
- premiere syntaxe de types parametres `Option[Int]` et `Array[Int]`, canonisee
  vers les facades standard existantes `OptionInt` et `ArrayInt`;
- declarations de classes generiques simples comme `Box[T]`, instanciables avec
  `Box[Int]` ou `Box[String]`, avec substitution des champs, retours de methodes
  et types fonction comme `(T) => T`;
- lambdas et appels indirects avec plusieurs parametres, dans les limites de la
  convention d'appel actuelle;
- closures testees avec parametres, captures et retours non limites a `Int`;
- inference du type du parametre pour les lambdas mono-parametre en position
  d'argument, par exemple `xs.map(value => { value + 1 })`;
- lambdas mono-expression inferees en argument, par exemple
  `xs.map(value => value + 1)`;
- inference des types de parametres pour lambdas multi-parametres en argument,
  par exemple `xs.fold(0, (acc, value) => acc + value)`;
- types fonction imbriques en retour, par exemple `(Int) => ((Int) => Int)`;
- types `Bool`, `Unit`, `Long`, `Float` et `Double` formalises; les
  comparaisons retournent `Bool` et les conditions `if` / `while` attendent
  `Bool`;
- operateurs booleens `&&`, `||` et `!`, avec court-circuit pour `&&` et
  `||`;
- collection native `IntArray` avec `length`, `get` et `set`;
- entiers immediats `Int` et `Long` avec pointer tagging, litteraux decimaux
  `Float` / `Double` portes par l'IR typee et litteraux `String`;
- affichage console de `String` via la primitive globale `print`;
- premier module de bibliotheque standard `io` avec `println`;
- module de bibliotheque standard `core.option_int` avec `OptionInt`,
  `optionIntSome`, `optionIntNone`, `map`, `filter` et `orElse`;
- premier module de bibliotheque standard `collections.int_array` avec
  `intArraySum`, `intArrayFill`, `intArrayRange`, `intArrayMap`,
  `intArrayFilter` et la facade objet `ArrayInt` avec `map`, `filter`,
  `fold`, `foreach`, `exists`, `forall`, `contains`, `size`, `isEmpty` et
  `nonEmpty`, `head`, `last`, `append`, `prepend`, `reverse`, `concat`,
  `take`, `drop`, `slice`, `indexOf`, `indexOfOption`, `count`, `find`, `min`, `max`,
  `getOption`, `headOption`, `lastOption`, `takeWhile`, `dropWhile`,
  `takeRight`, `dropRight`, `span`, `partition`, `flatMap`,
  `zipWithIndex`, `grouped`, `ArrayIntNested`, `ArrayIntNested.flatten()`,
  `ArrayIntNested.rowSize` et `ArrayIntNested.mapRows`.
  avec predicats booleens;
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
- tests de compilation et d'execution via `make all-tests`.

Limites importantes :

- les fonctions globales sont limitees a 6 parametres et les methodes a 5,
  conformement a la convention d'appel actuelle;
- `Float` et `Double` couvrent les litteraux, operations, comparaisons,
  fonctions, lambdas et champs, mais pas encore `toString` ni les collections
  specialisees;
- la genericite actuelle est un pont syntaxique pour `Option[Int]` et
  `Array[Int]`, et un premier support de classes generiques partageant le code
  du template; les fonctions generiques utilisateur et la monomorphisation
  specialisee complete restent a faire;
- le tas est fixe et possede une verification de depassement, mais pas de
  ramasse-miettes;
- les acces hors bornes de `IntArray` terminent le programme avec le code 254;

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
  src/ir_codegen.cpp src/runtime_asm.cpp \
  -o /tmp/nablac-werror
git diff --check
```

Les tests normaux utilisent un fichier voisin `<nom>.expected`. Un fichier
optionnel `<nom>.stdout` valide la sortie console. Les fichiers dont le nom
contient `error` ou `fail` doivent echouer pendant la compilation.

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
- [x] Deplacer l'allocation de pile et les conventions d'appel vers le backend.
- [x] Couvrir le backend ASM depuis IR pour tout le langage actuel.
- [x] Generer l'assembleur par defaut depuis l'IR.
- [x] Supprimer l'ancien code de generation directe depuis l'AST.

### P2 - Systeme De Types

- [ ] Formaliser `Int`, `Bool`, `String`, `IntArray`, les types fonction canoniques et
  les types de classes.
- [x] Ajouter `Long` avec litteraux suffixes `L`, arithmetique, comparaisons,
  fonctions, champs et `toString`.
- [x] Ajouter `Float` et `Double` avec litteraux decimaux, IR typee et
  generation SSE.
- [x] Formaliser `Unit` pour les fonctions a effet et les boucles.
- [x] Ajouter les booleens et typer les conditions en `Bool`.
- [x] Ajouter les operateurs booleens `&&`, `||` et `!`.
- [x] Ajouter le court-circuit pour `&&` et `||`.
- [ ] Ajouter les champs et methodes herites si l'heritage est retenu.
- [ ] Valider les types des branches, boucles et operateurs de facon uniforme.
- [x] Ajouter une premiere syntaxe de types parametres pour `Option[Int]` et
  `Array[Int]` via aliases standard.
- [x] Ajouter les declarations de classes generiques utilisateur `class Box[T]`
  avec substitution des champs, methodes et types fonction.
- [ ] Ajouter les fonctions generiques utilisateur `def identity[T](value: T): T`.
- [ ] Ajouter la monomorphisation des classes et fonctions generiques.
- [x] Generaliser `IntUnaryFn` vers des types fonction canoniques.
- [x] Introduire une representation interne commune des types fonction.
- [x] Ajouter la syntaxe de types fonction parenthesee pour les aliases actuels.
- [x] Autoriser les lambdas a plus de deux parametres.
- [x] Autoriser les types fonction en position de retour.
- [x] Retirer les aliases historiques `IntUnaryFn`, `IntConsumerFn` et
  `IntBinaryFn`.
- [x] Ajouter `IntBinaryFn` pour `fold` et operations binaires de collections.
- [x] Ajouter les lambdas sans capture pour `IntUnaryFn`.
- [x] Ajouter `IntConsumerFn` pour les fonctions `Int => Unit`.
- [x] Ajouter `arrayIntRange` et `ArrayInt.foreach` comme premiers parcours
  fonctionnels de collection.
- [x] Ajouter les closures avec capture par valeur pour les lambdas `Int`.
- [x] Generaliser les closures aux types fonction complets.
- [x] Ajouter l'inference simple des types de parametres de lambda.
- [x] Ajouter les lambdas mono-expression sans bloc.
- [x] Ajouter l'inference des lambdas multi-parametres.
- [x] Ajouter `ArrayInt.exists`, `ArrayInt.forall` et `ArrayInt.contains`.
- [x] Typer les predicats de collections avec `Bool`.
- [x] Ajouter `ArrayInt.size`, `isEmpty` et `nonEmpty`.
- [x] Ajouter `ArrayInt.head` et `ArrayInt.last`.
- [x] Ajouter `ArrayInt.append` et `ArrayInt.prepend`.
- [x] Ajouter `ArrayInt.reverse`.
- [x] Ajouter `ArrayInt.concat`.
- [x] Ajouter `ArrayInt.take` et `ArrayInt.drop` avec bornes clampees.
- [x] Ajouter `ArrayInt.slice` avec bornes clampees.
- [x] Ajouter `ArrayInt.indexOf` et `ArrayInt.count`.
- [x] Ajouter `ArrayInt.indexOfOption`.
- [x] Ajouter `OptionInt` puis `ArrayInt.min` et `ArrayInt.max`.
- [x] Ajouter `ArrayInt.getOption`, `headOption`, `lastOption` et `find`.
- [x] Ajouter `ArrayInt.takeWhile` et `ArrayInt.dropWhile`.
- [x] Ajouter `ArrayInt.takeRight`, `ArrayInt.dropRight` et `ArrayInt.span`.
- [x] Ajouter `ArrayInt.partition` et `ArrayInt.flatMap`.
- [x] Ajouter `ArrayInt.zipWithIndex`.
- [x] Ajouter `ArrayIntNested` et `ArrayIntNested.flatten()`.
- [x] Ajouter `ArrayInt.grouped`, `ArrayIntNested.rowSize` et
  `ArrayIntNested.mapRows` pour une API imbriquee de collections imbriquees.

### P2 - Runtime Et Objets

- [x] Verifier les depassements du tas.
- [ ] Definir et utiliser de vraies vtables ou retirer leur emplacement reserve.
- [x] Extraire le runtime ASM commun du backend IR.
- [x] Stabiliser la representation de `String`.
- [ ] Choisir une strategie memoire a long terme.
- [x] Ajouter une primitive d'affichage console pour `String`.
- [x] Ajouter une premiere collection native `IntArray`.

### P3 - Outillage

- [x] Retirer les binaires suivis sous `build/`.
- [ ] Ajouter une cible de formatage.
- [x] Ajouter une integration continue.
- [ ] Ajouter des tests unitaires du lexer, parser et analyseur semantique.
- [x] Ajouter une racine `stdlib/` importable.
- [x] Ajouter un premier module de collections dans `stdlib/`.

## Journal Des Jalons

- `1d9fc36` - Ajouter les classes generiques utilisateur simples comme `Box[T]`,
  avec instanciations concretes `Box[Int]` / `Box[String]` et substitution de
  `T` dans les signatures de methodes.
- `6fd4884` - Ajouter la syntaxe de types parametres `Option[Int]` et `Array[Int]`
  avec canonisation vers `OptionInt` et `ArrayInt`.
- `94b4440` - Ajouter `Float` et `Double`, les litteraux decimaux, l'IR typee, les
  operations SSE et les tests positifs et negatifs associes.
- `53b77c1` - Ajouter `Long`, les litteraux `42L`, les operations numeriques Long,
  les champs/fonctions Long et `Long.toString`.
- `a9f183d` - Ajouter `this` comme mot-clé et variable implicite dans les méthodes;
  corriger la résolution de `this` (`symbolName == "this"`) dans
  `IdentifierNode` pour permettre son usage en bibliothèque standard.
- `0f33697` - Lier `this` dans le codegen des méthodes et corriger sa
  résolution de portée.
- `ef2609c` - Ajouter `ArrayInt.indexOfOption`.
- `813100b` - Ajout de `OptionInt` et des bornes `ArrayInt`.
- `3b6760f` - Ajout de `ArrayInt.indexOf` et `ArrayInt.count`.
- `faaa691` - Ajout de `ArrayInt.slice`.
- `1bbd935` - Ajout de `ArrayInt.take` et `ArrayInt.drop`.
- `fe4ee39` - Ajout de `ArrayInt.concat`.
- `4b1e449` - Ajout de `ArrayInt.reverse`.
- `689f55c` - Ajout de `ArrayInt.append` et `ArrayInt.prepend`.
- `60de4bf` - Ajout de `ArrayInt.head` et `ArrayInt.last`.
- `e96457c` - Ajout des predicats de taille `ArrayInt`.
- `7b534be` - Ajout du court-circuit booleen.
- `842a8c1` - Ajout des operateurs booleens.
- `d03b1f6` - Introduction du type `Bool`.
- `8f21d03` - Ajout de predicats de collection `ArrayInt`.
- `6f896e5` - Inference des lambdas multi-parametres.
- `decbfee` - Ajout des lambdas mono-expression inferees.
- `00f0d39` - Inference simple des parametres de lambda.
- `1243161` - Generalisation des closures aux types fonction canoniques.
- `917c2a2` - Retrait des aliases de types fonction historiques.
- `771223d` - Ajout des types fonction en retour.
- `c8a69eb` - Generalisation des lambdas multi-parametres.
- `75a24fa` - Generalisation canonique des types fonction.
- `cc342e4` - Ajout de la syntaxe de types fonction parenthesee.
- `6534f3c` - Introduction d'une representation interne des types fonction.
- `53533a2` - Ajout des closures avec capture par valeur.
- `8f71165` - Ajout de `IntConsumerFn` pour `foreach`.
- `a40144e` - Formalisation de `Unit` pour les boucles et `foreach`.
- `70a808d` - Ajout de `arrayIntRange` et `ArrayInt.foreach`.
- `51cba13` - Ajout de `IntBinaryFn` et `ArrayInt.fold`.
- `fa730c6` - Ajout de `ArrayInt.filter`.
- `209119f` - Ajout des fonctions valeurs `IntUnaryFn`, de `intArrayMap` et de
  la facade objet `ArrayInt`.
- `827c162` - Ajout du module `collections.int_array`.
- `9d1cab5` - Ajout d'une racine `stdlib/` et du module `io`.
- `e0e611a` - Ajout de la collection native `IntArray`.
- `b2996ad` - Ajout des litteraux `String`.
- `92650b3` - Ajout de la primitive console `print`.
- `0203ccb` - Ajout d'une verification de depassement du tas.
- `48dacc8` - Stabilisation de la representation runtime de `String`.
- `0af02cd` - Ajout de l'integration continue GitHub Actions.
- `b1a6938` - Extraction du runtime ASM commun du backend IR.
- `ed879e1` - Retrait des binaires historiques suivis sous `build/`.
- `63c286e` - Structuration de la frame et de la convention d'appel du backend
  IR.
- `c771250` - Suppression de l'ancien code de generation ASM depuis l'AST.
- `1266271` - Retrait du repli CLI vers l'ancien backend AST.
- `3d93076` - Bascule du backend ASM par defaut vers l'IR.
- `baefaa6` - Ajout des objets et methodes au backend ASM depuis IR.
- `5a97aa4` - Ajout du controle de flux au backend ASM depuis IR.
- `48f7e1d` - Ajout d'un backend ASM experimental depuis l'IR.
- `0f33a89` - Ajout des objets, champs et appels de methodes dans l'IR.
- `f38e0a6` - Ajout du controle de flux `if`/`while`/`for` dans l'IR.
- `1dcff81` - Ajout de l'IR minimale, de `--emit-ir` et des snapshots IR.
- `8b7be03` - Ajout des diagnostics sources uniformes, de `CompilerError` et des
  tests de diagnostics exacts.
- `5b7a7ce` - Ajouter `ArrayInt.takeWhile` et `ArrayInt.dropWhile`.
- `94eeecf` - Ajouter `ArrayInt.takeRight`, `ArrayInt.dropRight` et `ArrayInt.span`.
- `1bc9417` - Ajouter `ArrayInt.zipWithIndex`, `ArrayInt.partition`,
  `ArrayInt.flatMap` et `ArrayIntNested.flatten()`.
- `baacd10` - Ajouter `ArrayInt.grouped`, `ArrayIntNested.rowSize` et
  `ArrayIntNested.mapRows`.
- `1062f09` - Ajout des parametres de fonctions et methodes, appels globaux,
  validation des arguments et convention d'appel x86-64.
- `93b942f` - Ajout de `AGENTS.md`, des conventions de contribution et de la
  feuille de route maintenue.
- `469f535` - Ajout de la phase d'analyse semantique et des validations de types.
- `165a521` - Stabilisation des variables locales, portees, pile et allocations
  d'objets imbriquees; execution verifiee dans la suite de tests.
- `45e893d` - Ajout initial de `val`, `var` et de la table des symboles locale.

## Prochaine Etape Recommandee

Introduire une IR typee pour les temporaires numeriques, puis ajouter les
litteraux `Float` / `Double` et la generation SSE (`addss` / `addsd`, etc.).
