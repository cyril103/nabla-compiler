# AGENTS.md

Ce fichier sert de guide de travail et de feuille de route pour les agents qui
contribuent au compilateur Nabla.

## Regle De Maintenance

- Avant chaque commit realise par un agent, mettre a jour ce fichier.
- Inclure la mise a jour de `AGENTS.md` dans le meme commit que le changement.
- Mettre a jour au minimum les sections `Etat Actuel`, `Feuille De Route` et
  `Journal Des Jalons` lorsque le changement les affecte.
- Ne pas marquer une etape comme terminee sans tests automatises correspondants.
- Regenerer la reference HTML avec `make stdlib-docs` quand une API publique de
  `stdlib/`, un commentaire `///`, une directive `@signature` ou une directive
  `@symbol` change, puis inclure le resultat `docs/stdlib/` dans le commit.

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

## Fil Conducteur

Le projet a depasse le stade du compilateur jouet. Les prochaines evolutions
doivent privilegier la coherence utilisateur, la lisibilite de la stdlib et la
formalisation des conventions internes plutot que l'ajout rapide de nouvelles
features visibles.

Principes de direction :

- preferer une API publique simple et uniforme : `Array[T]`, `Option[T]`,
  `Set[T]`, `String`, classes, methodes, lambdas;
- cacher progressivement les details d'implementation (`IntArray`,
  `LongArray`, `ObjectArray[T]`, `ArrayObject[T]`, helpers `arrayBase...`,
  fonctions specialisees internes) derriere des facades documentees;
- maintenir les specialisations runtime comme optimisations internes, pas comme
  concepts que l'utilisateur doit connaitre pour ecrire du code propre;
- formaliser les conventions runtime avant de les propager : tagging de
  `Int`/`Long`/`Bool`, valeurs raw de `Float`/`Double`, objets heap, tableaux
  natifs, slots nuls et erreurs runtime;
- continuer a supprimer les fallbacks implicites dans parser, semantique, IR et
  backend au profit d'erreurs explicites;
- garder des diagnostics orientes utilisateur, en evitant d'exposer les noms
  internes quand une forme source plus claire existe;
- considerer la documentation HTML de la stdlib comme une surface produit :
  elle doit montrer les API publiques et masquer ou marquer les helpers
  internes;
- conserver un typage simple : sous-typage nominal pour les classes,
  generiques invariants par defaut, conversions explicites ou fonctions stdlib
  plutot que magie implicite.

Priorites structurantes :

1. Stabiliser l'API publique de `Array[T]`, `Option[T]` et `Set[T]` en suivant
   `docs/stdlib-api.md`.
2. Classer la stdlib en surface publique et modules/helpers internes dans
   `docs/stdlib-api.md` avant d'ajouter de nouveaux symboles.
3. Maintenir la specification vivante `docs/internals.md` pour les types,
   le runtime et les regles de typage.
4. Maintenir le check CI qui verifie que `make stdlib-docs` ne laisse aucun diff.
5. Produire des exemples idiomatiques n'utilisant pas les API internes.
6. Reporter les grosses nouvelles structures (`Result[T]`, `Map[K,V]`, GC,
   variance avancee) tant que l'ergonomie des collections et options n'est pas
   stabilisee.

## Etat Actuel

Le pipeline implemente actuellement :

- tokenisation et parsing des classes, imports, fonctions et methodes avec
  parametres,
  expressions arithmetiques, comparaisons, `if`, `else if`, `match`, `while`,
  `for`, `val` et `var`;
- identifiants et chemins d'import avec lettres, chiffres et `_`;
- resolution des imports relatifs, depuis la racine projet et depuis `stdlib/`,
  avec protection contre les cycles;
- objets avec champs de constructeur et appels de methodes parametres;
- fonctions globales appelables avec parametres;
- surcharge V1.1 des fonctions globales non generiques par signature exacte,
  avec nom IR unique par variante, wrappers `math.sqrt(Float)` /
  `math.sqrt(Double)` et references resolues quand un type fonction est attendu
  en argument;
- types fonction-valeur canoniques `Fn(...)->...`, references de fonctions
  nommees et appels indirects, avec lambdas sans capture `(x: Int) => { ... }` et
  `(acc: Int, value: Int) => { ... }`;
- closures avec capture par valeur pour les lambdas de types fonction
  canoniques;
- representation interne canonique des types fonction sous forme `Fn(...)->...`;
- syntaxe de types fonction parenthesee comme `(Int) => Int`, `(Int) => Unit`,
  `(Int, Int) => Int` et `(String) => Int`;
- premiere syntaxe de types parametres, avec aliases standard extensibles
  comme `Array[Int]` canonise vers la facade existante `ArrayInt`, y compris
  dans les types imbriques et types fonction, et `Option[T]` porte par une
  vraie classe generique standard;
- facade generique standard limitee pour `Array[T]` dans les signatures
  generiques utilisateur, avec inference de `T` depuis les specialisations
  concretes `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` et
  `ArrayBool`;
- appels de methodes communs sur la facade generique `Array[T]` dans les corps
  generiques, specialises vers `ArrayInt`, `ArrayLong`, `ArrayFloat`,
  `ArrayDouble` ou `ArrayBool` pour `length`, `size`, `isEmpty`, `nonEmpty`,
  `get`, `set`, `map` et `foreach`;
- constructeur ergonomique `new Array[T](size)` pour les types primitifs et
  objets utilisateur via les facades `Array[T]`, avec noms utilisateur
  `Array.fill[T](size, value)`, `Array.range(size)` et
  `Array.rangeUntil(start, until)` pour eviter d'exposer `IntArray` /
  `ArrayInt` / `ObjectArray[T]` dans les cas simples;
- declarations de classes generiques simples comme `Box[T]`, instanciables avec
  `Box[Int]` ou `Box[String]`, avec substitution des champs, retours de methodes
  et types fonction comme `(T) => T`;
- allocations de classes generiques conservees comme types concrets dans l'IR,
  par exemple `new Box[Int]`;
- appels de methodes de classes generiques abaisses vers des corps IR
  specialises comme `Box[Int].get`, avec substitution de `T` dans les
  parametres, retours, types fonction et acces aux champs;
- methodes generiques avec arguments de type explicites ou inferes comme
  `option.map[String](...)` et `option.map(...)`, specialisees dans l'IR avec le
  type de classe et les types de methode;
- declarations de fonctions generiques simples comme `identity[T]`, appelees avec
  arguments de type explicites comme `identity[Int](42)`, avec substitution dans
  les parametres, retours et types fonction, puis monomorphisees en corps IR
  specialises comme `identity[Int]`;
- references de fonctions generiques specialisees comme `identity[Int]`
  utilisables comme valeurs fonction, monomorphisees a la demande;
- inference des arguments de type des fonctions generiques depuis les arguments
  d'appel, y compris pour typer une lambda suivante comme dans
  `applyOnce(41, value => value + 1)`;
- lambdas et appels indirects avec plusieurs parametres, dans les limites de la
  convention d'appel actuelle;
- closures testees avec parametres, captures et retours non limites a `Int`;
- champs de type fonction appelables depuis les methodes de leur classe;
- inference du type du parametre pour les lambdas mono-parametre en position
  d'argument, par exemple `xs.map(value => { value + 1 })`;
- lambdas mono-expression inferees en argument, par exemple
  `xs.map(value => value + 1)`;
- inference des types de parametres pour lambdas multi-parametres en argument,
  par exemple `xs.fold(0, (acc, value) => acc + value)`;
- types fonction imbriques en retour, par exemple `(Int) => ((Int) => Int)`;
- optimisation backend des appels recursifs terminaux directs, y compris dans
  une branche `if` qui alimente le retour de fonction;
- types `Bool`, `Unit`, `Long`, `Float` et `Double` formalises; les
  comparaisons retournent `Bool` et les conditions `if` / `while` attendent
  `Bool`;
- constantes d'encodage runtime centralisees dans `src/runtime_values.hpp`;
  les constantes IR `Bool` sont emises et validees sous leur forme taggee
  runtime (`false = 1`, `true = 3`) ;
- expressions `if` typees avec le type commun des branches, ou `Unit` quand les
  branches ont des types differents et servent d'effets de bord;
- operateurs booleens `&&`, `||` et `!`, avec court-circuit pour `&&` et
  `||`;
- operateur unaire `-` pour les types numeriques (`Int`, `Long`, `Float`,
  `Double`) ;
- operateur reste de division `%` pour `Int` et `Long`;
- invocation NASM/ld via `fork` + `execvp` (sans `std::system`) pour limiter
  l'injection shell, avec diagnostic explicite quand une commande externe comme
  `nasm` ou `ld` est introuvable,
  et vérifications runtime renforcées pour `parseInt`/division par zéro.
- collection native `IntArray` avec `length`, `get` et `set`;
- collection native `LongArray` avec `length`, `get` et `set`;
- collection native `FloatArray` avec `length`, `get` et `set`;
- collection native `DoubleArray` avec `length`, `get` et `set`;
- collection native `BoolArray` avec `length`, `get` et `set`;
- entiers immediats `Int` et `Long` avec pointer tagging, litteraux decimaux
  `Float` / `Double` portes par l'IR typee, conversion `Int.toLong`,
  litteraux `Char` ASCII et
  litteraux `String`;
- chaines `String` stockees comme buffers de bytes, avec `length` et
  `charAt(index): Char`, `toCharArray(): ArrayObject[Char]`, `toInt`,
  `substring(from, until)`, `repeat(count)`, `trim`, `split(separator)`,
  `indexOf`, `contains`, `isEmpty`, `nonEmpty`,
  `startsWith`, `endsWith`, `+`, `==` et `!=`;
- jointure texte de tableaux via `ArrayInt` / `ArrayLong` /
  `ArrayBool.mkString(separator)` et `arrayMkString[Int/Long/Bool/String]`;
- affichage console de `String` via la primitive globale `print`;
- lecture console de `String` via la primitive globale `readLine`;
- lecture/ecriture de fichiers texte via `readFile`, `writeFile`,
  `appendFile`, `deleteFile`, `fileExists` et les wrappers `io.readTextFile` /
  `io.writeTextFile` / `io.appendTextFile` / `io.deleteTextFile` /
  `io.renameTextFile` / `io.createDirectory` / `io.pathExists`;
- parsing decimal de `String` vers `Int` via `parseInt(value)`;
- module standard `strings` avec `words(text)` pour decouper une ligne en
  tokens separes par des espaces en ignorant les segments vides;
- premier module de bibliotheque standard `io` avec `println`, `input` et les
  wrappers `io.readTextFile`, `io.writeTextFile`, `io.appendTextFile`,
  `io.deleteTextFile`, `io.renameTextFile` et `io.createDirectory` /
  `io.pathExists`;
- module de bibliotheque standard `util` avec `randomSeed`, `randomInt`,
  `randomIntRange`, `randomIntInRange`, `RandomChoiceResult`,
  `randomBool`, `randomSeedNow` et `randomSeedTime` pour une API pseudo-aléatoire
  deterministe basée sur une seed;
- module de bibliotheque standard `math` avec `absInt`, `absLong`, `absFloat`,
  `absDouble`, `absDiffInt`, `absDiffLong`, `absDiffFloat`,
  `absDiffDouble`, `maxInt`, `maxLong`, `maxFloat`, `maxDouble`, `minInt`,
  `minLong`, `minFloat`, `minDouble`, `clampInt`, `clampLong`, `clampFloat`,
  `clampDouble`, `signInt`, `signLong`, `signFloat`, `signDouble`,
  `isEvenInt`, `isOddInt`, `isEvenLong`, `isOddLong`, `isBetweenInt`,
  `isBetweenLong`, `gcdInt`, `lcmInt`, `gcdLong`, `lcmLong`, `powInt`,
  `powFloat`, `powDouble`, `factorialInt`, `isCloseFloat`, `isCloseDouble`,
  `sqrtFloat`, `sqrtDouble`, `sqrt(Float)`, `sqrt(Double)`, `piFloat`,
  `piDouble`, `twoPiFloat`, `twoPiDouble`, `degreesToRadiansFloat`, `radiansToDegreesFloat`,
  `degreesToRadiansDouble`, `radiansToDegreesDouble`, `hypotenuseFloat`,
  `hypotenuseDouble`.
- module de bibliotheque standard `core.option_int` avec `OptionInt`,
  `optionIntSome`, `optionIntNone`, `map`, `filter` et `orElse`;
- module de bibliotheque standard `core.option` avec `Option[T]`,
  `Option.some[T]`, `Option.none[T]`, `optionSome`, `optionNone`, `isDefined`,
  `isEmpty`, `nonEmpty`, `map[U]`, `filter`, `flatMap[U]`, `foreach`, `orElse`
  et `getOrElse`;
- premier module de bibliotheque standard `collections.int_array` avec
  `intArraySum`, `intArrayFill`, `intArrayRange`, `intArrayMap`,
  `intArrayFilter` et la facade objet `ArrayInt` avec `map`, `filter`,
  `fold`, `foreach`, `exists`, `forall`, `contains`, `size`, `isEmpty` et
  `nonEmpty`, `head`, `last`, `append`, `prepend`, `reverse`, `concat`,
  `take`, `drop`, `slice`, `indexOf`, `indexOfOption`, `count`, `find`, `min`, `max`,
  `getOption`, `headOption`, `lastOption`, `takeWhile`, `dropWhile`,
  `takeRight`, `dropRight`, `span`, `partition`, `flatMap`,
  `zipWithIndex`, `grouped`, `ArrayIntNested`, `ArrayIntNested.flatten()`,
  `ArrayIntNested.rowSize`, `ArrayIntNested.mapRows` et `shuffle`.
  avec predicats booleens;
- plage lazy specialisee `RangeInt` via `intRangeUntil(start, until)`, avec
  `size`, `isEmpty`, `nonEmpty`, `foreach`, `fold`, `exists`, `forall`,
  `count`, `find`, `filter`, `map`, `max` et `foldLong`;
- module de bibliotheque standard `collections.long_array` avec `ArrayLong`,
  `longArrayFill`, `longArraySum`, `longArrayMap`, `arrayLongFill`, `map`,
  `flatMap`, `foreach`, `sum`, `size`, `isEmpty`, `nonEmpty`, `get`, `set` et
  `raw` et `shuffle`;
- module de bibliotheque standard `collections.float_array` avec `ArrayFloat`,
  `floatArrayFill`, `floatArrayMap`, `arrayFloatFill`, `map`, `flatMap`,
  `foreach`, `size`, `isEmpty`, `nonEmpty`, `get`, `set`, `raw` et
  `shuffle`;
- module de bibliotheque standard `collections.double_array` avec `ArrayDouble`,
  `doubleArrayFill`, `doubleArrayMap`, `arrayDoubleFill`, `map`, `flatMap`,
  `foreach`, `size`, `isEmpty`, `nonEmpty`, `get`, `set`, `raw` et
  `shuffle`;
- module de bibliotheque standard `collections.bool_array` avec `ArrayBool`,
  `boolArrayFill`, `boolArrayCountTrue`, `boolArrayAll`, `boolArrayAny`,
  `boolArrayMap`, `arrayBoolFill`, `map`, `flatMap`, `foreach`, `countTrue`,
  `all`, `any`, `size`, `isEmpty`, `nonEmpty`, `get`, `set`, `raw` et
  `shuffle`;
- module de bibliotheque standard `collections.object_array` avec `ObjectArray[T]`,
  `ArrayObject[T]`, `objectArrayShuffle`, `objectArrayMkString`,
  `objectStringArrayMkString` (compatibilité) et `ArrayObject[T].shuffle`.
- module de bibliotheque standard `collections.set` avec `Set[T]`,
  `Set.empty[T]`, `Set.fromArray[T]`, `SetEmpty[T]`, `SetFromArray[T]`, `add`,
  `remove`, `union`, `intersect`, `difference`, `setEmpty`, `setFromArray` et
  `toString`, en utilisant un index de seaux hashés (`hashCode()`) pour des
  opérations de présence à coût moyen réduit.
- module de bibliotheque standard `collections.array` comme point d'entree
  commun pour les tableaux specialises, avec `Array.fill[T]`, `Array.range`,
  `Array.rangeUntil`, `ArrayFill[T]`, `ArrayRange`, `arrayFill[T]`, `arrayMap[T]`,
  `arrayMap[T, U]`, `arrayFilter[T]`, `arrayFold[T]`, `arrayFold[T, U]`,
  `arrayFlatMap[T]`, `arrayFlatMap[T, U]` et `arrayForeach[T]` resolus vers les
  specialisations `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` ou
  `ArrayBool` pour `T = Int`, `Long`, `Float`, `Double` ou `Bool`, et vers
  `ArrayObject[T]` pour les types concrets non specialises comme `String` et
  `Array[Int]`;
  `arrayMap[Primitive, U]`, `arrayFlatMap[Primitive, U]`, `mapObject[U]` et
  `flatMapObject[U]` produisent
  `ArrayObject[U]` quand la sortie ne reste pas dans la meme primitive
  specialisee;
- collection native `ObjectArray[T]` stockant des slots runtime de 64 bits, avec
  facade standard `ArrayObject[T]`, `objectArrayFill[T]`,
  `objectArrayMap[T, U]`, `objectArrayFilter[T]`, `objectArrayFold[T, U]`,
  `objectArrayFlatMap[T, U]` et `objectArrayForeach[T]`;
- `examples/command_shell.nabla` accepte désormais `write` et `append` avec un
  texte complet pouvant contenir des espaces.
- portees lexicales locales, mutabilite et allocation statique des emplacements
  de pile;
- analyse semantique des classes, constructeurs, methodes, types de retour et
  affectations;
- support de l'héritage de méthodes via `extends` + `with` (un parent explicite
  puis mixins), avec validation des parents, détection de conflits de méthodes
  héritées et détection de cycles;
- appel de la méthode parente via `super`, y compris en présence de chaînes
  d'héritage et de mixins; diagnostic explicite quand `super` est utilisé sans
  parent explicite.
- résolution et résolution en cascade des champs hérités (ordre de résolution par
  hiérarchie, conflit de définition détecté) + mise à jour des layouts de classes
  pour l'allocation cohérente des offsets.
- diagnostics uniformes avec fichier, ligne, colonne et phase du compilateur;
- IR textuelle pour les fonctions globales, entiers, variables, affectations,
  operations binaires, appels de fonctions globales, `if`, `match`, `while`,
  `for`, objets et methodes;
- backend ASM par defaut depuis l'IR couvrant la suite positive actuelle
  (fonctions, variables, controle de flux, imports, objets et methodes);
- tests de compilation et d'execution via `make all-tests`.
- CI GitHub avec `make all-tests`, `make examples`, `make tooling-tests`,
  vérification `make stdlib-docs` sans diff, compilation `-Werror` et
  `git diff --check`;
- reference HTML de la stdlib generee depuis `///`, avec directive `@status`
  pour distinguer visuellement API recommandee, compatibilite et helpers
  internes; toutes les pages actuellement publiees affichent les statuts des
  symboles documentes, et les modules `io`, `math`, `strings` et `OptionInt`
  explicitent leurs conventions utilisateur principales.
- examples Project Euler 1 a 10 (`examples/euler1.nabla` ... `examples/euler10_functional.nabla`)
  comme banc progressif pour exercer le langage et la bibliotheque standard.
- `examples/student_scores.nabla` comme exemple idiomatique vérifié pour
  `Array[T]`, `Array.fill[T]`, `Option.some[T]` / `Option.none[T]`, classes,
  lambdas et sorties console.
- `examples/workshop_set_inheritance.nabla` comme exemple vérifié pour
  `Array[T]`, `Set[T]`, `Set.fromArray[T]`, opérations d'ensemble et héritage
  avec `override`; la friction restante porte surtout sur les collections
  polymorphes de type parent comme `Set[Person]`.

Limites importantes :

- les fonctions globales sont limitees a 6 parametres et les methodes a 5,
  conformement a la convention d'appel actuelle;
- `Float` et `Double` couvrent les litteraux, operations, comparaisons,
  fonctions, lambdas, champs, collections specialisees et `toString`;
- `String` et `Char` sont actuellement byte-based/ASCII pour les operations de
  longueur et d'indexation; les bytes UTF-8 sont conserves pour l'affichage et
  l'entree, mais `length` ne compte pas encore les code points Unicode;
- la genericite actuelle couvre `Option[T]`, les fonctions generiques
  monomorphisees et les methodes de classes generiques specialisees avec
  inference des arguments de type; les references explicites comme
  `identity[Int]` sont utilisables comme valeurs, mais les fonctions generiques
  ne sont pas encore des valeurs vraiment polymorphes;
  la surcharge de fonctions couvre pour l'instant les appels globaux resolus par
  types d'arguments exacts et les references de fonctions surchargees en
  position d'argument avec type fonction attendu; les references sans type
  attendu ne sont pas encore supportees;
  `Array[Int]` reste une facade specialisee vers `ArrayInt`; `Array[Long]`
  reste une facade specialisee vers `ArrayLong`; `Array[Float]` reste une
  facade specialisee vers `ArrayFloat`; `Array[Double]` reste une facade
  specialisee vers `ArrayDouble`; `Array[Bool]` reste une facade specialisee
  vers `ArrayBool`; `arrayFill[T]`, `arrayMap[T]`, `arrayMap[T, U]`,
  `arrayFilter[T]`, `arrayFold[T]`, `arrayFold[T, U]`, `arrayFlatMap[T]`,
  `arrayFlatMap[T, U]` et `arrayForeach[T]` sont des fonctions standard
  generiques specialisees pour `Int`, `Long`, `Float`, `Double` et `Bool`, mais
  pas encore une implementation unique de tableau generique;
  `arrayMap[Primitive, U]`, `arrayFlatMap[Primitive, U]`,
  `mapObject[U]` et `flatMapObject[U]` peuvent mapper une facade primitive
  vers `ArrayObject[U]`;
  `Array[T]` est valide dans les
  signatures generiques utilisateur et se specialise correctement quand `T`
  devient concret, avec une premiere surface de methodes communes; `map[U]`
  existe sur `ArrayObject[T]`; `filter` existe sur les facades primitives et
  `ArrayObject[T]`, tandis que les operations non communes comme `sum` ou
  `countTrue` restent specialisees; `fold` existe sur les facades primitives
  avec accumulateur du meme type, et `fold[U]` / `flatMap[U]` existent sur
  `ArrayObject[T]`;
  l'objectif retenu est de conserver les tableaux primitifs specialises et
  d'ajouter un fallback generique `ObjectArray[T]` / `ArrayObject[T]` pour les
  autres types; le premier fallback couvre `String`, `Array[Array[Int]]`,
  `Array[Option[String]]` et les tableaux d'objets utilisateur, tandis que les
  operations avancees restent a faire; la monomorphisation complete des classes
  generiques reste a durcir;
- le runtime initialise un tas de 8 MiB par `mmap`, avec allocations bump
  alignees sur 8 octets et verification de depassement, mais pas de
  ramasse-miettes;
- les acces hors bornes de `IntArray` terminent le programme avec le code 254;
- `match` supporte désormais les motifs litteraux, les motifs nommes (`identifiant`)
  et des gardes de branche de la forme `motif if condition`, ainsi que la branche
  finale `_` (avec ou sans garde selon la position; la branche finale `_` ne
  peut pas porter de garde). Les motifs nommes sont locaux à la branche et ne
  fuient pas hors de l'expression `match`.
- L'héritage fonctionne avec les collections typées par un parent dans les cas
  simples: `Array[Person]` peut contenir des instances de `Student`,
  `Instructor` et `Volunteer`, puis alimenter `Set.fromArray[Person]`.
  L'exemple public utilise désormais `Array[T]` et `Set.fromArray[T]` pour éviter
  d'exposer `ObjectArray[T]` / `ArrayObject[T]` dans le chemin applicatif
  principal. Les appels de méthodes utilisateur sont virtuels par défaut quand
  une valeur est manipulée par son type parent, y compris parent générique
  instancié et méthode générique spécialisée; `super` reste statique. La méthode
  `toString()`, `hashCode()` et `equals(...)` redispatchent aussi depuis `Any`
  pour stabiliser l'égalité et l'index hashé de `Set[Parent]`. La friction
  restante concerne les vtables complètes et les règles avancées d'égalité dans
  les hiérarchies complexes.
- Le mot-clé `override` est supporté pour marquer explicitement les
  redéfinitions de méthodes héritées, et il est obligatoire quand une méthode
  redéfinit une méthode provenant d'un parent.
  Les constructeurs hérités peuvent désormais être exposés avec une signature
  typée dans `extends Parent(...)`: le préfixe reprend les champs du parent
  direct, et le suffixe déclare les champs propres de l'enfant.
- Les collections basées sur `Any` utilisent `==` pour l'égalité et
  `hashCode()` pour l'index interne. `==` / `!=` sur objets passent par
  `Any.equals(...)`, et `toString()`, `hashCode()` et `equals(...)`
  redispatchent vers les overrides utilisateur même quand la valeur passe par
  `Any`, un type parent ou un paramètre générique spécialisé.
- Les `object` existent comme namespaces statiques: leurs `def` sont abaissés
  vers des fonctions globales qualifiées (`Name.method`) et peuvent servir de
  compagnons de surface à une `class Name`. Ils ne sont pas encore des valeurs
  singleton runtime avec champs, identité ou initialisation dédiée.

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
make examples
make tooling-tests
make stdlib-docs
git diff --exit-code docs/stdlib
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
- [x] Optimiser les appels recursifs terminaux directs en sauts backend.

### P2 - Systeme De Types
- [x] Etendre la bibliotheque standard `math` avec `clamp`, `sign`, predicates
  `isEven`, `isOdd`, `isBetween`, `gcd`/`lcm` pour `Long`, `absDiff`,
  `pow` pour `Float` et `Double`, `isClose`, `sqrt`, constantes `pi`
  (approximatives) et conversions degrés/radians.
- [x] Etendre `collections.set` avec des operations immutables
  (`union`, `intersect`, `difference`).
- [x] Ajouter `setFromArray[T](values: ArrayObject[T]): Set[T]` pour la
  construction dédupliquante depuis un tableau source.
- [x] Améliorer les opérations de `collections.set` (contains, set op, setFromArray)
  avec un index hashé interne basé sur `hashCode()` pour réduire la complexité
  moyenne des recherches de présence.
- [x] Ajouter le module standard `util` avec `randomSeed`, `randomInt`,
  `randomIntRange` et `randomBool` pour une API pseudo-aléatoire
  deterministe.
- [x] Etendre `util` avec `randomIntInRange`,
  `RandomChoiceResult` et `randomSeedTime()`, puis exposer `randomChoice`
  sur `ArrayInt`, `LongArray`, `FloatArray`, `DoubleArray`, `BoolArray` et
  `ArrayObject[T]`.
- [x] Ajouter `randomSeedNow()` au module standard `util`, basé sur une source de
  timestamp en runtime pour initialiser un générateur avec une seed temporelle.

### P2 - Héritage Et Mixins

- [x] Ajouter la syntaxe `class X extends A with B` pour un parent + mixins.
- [x] Résoudre les méthodes héritées dans la hiérarchie (`resolveClassMethodInHierarchy`).
- [x] Valider l'existence et l'arité des parents, et détecter les cycles.
- [x] Ajouter la hiérarchie racine `Any` / `AnyVal` / `AnyRef`, avec `AnyRef`
  implicite pour les classes sans parent explicite.
- [x] Ajouter la résolution de conflits de membres dupliqués entre parent et mixins.
- [x] Ajouter `super` pour appeler une méthode de la classe parente immédiate.
- [x] Consolider la résolution de `super` en présence de chaînes d'héritage et de
  mixins, avec diagnostic dédié pour l'utilisation de `super` sans parent.
- [x] Valider strictement les signatures `override` héritées, y compris arité,
  paramètres, retour, paramètres génériques de méthode et substitutions de types
  de classes parentes génériques.
- [x] Formaliser l'héritage implicite depuis `Any` pour les classes sans parent
  explicite.
- [x] Formaliser la résolution des conflits hérités pour les champs et méthodes
  provenant d'instances génériques distinctes du même ancêtre.
- [x] Formaliser la résolution des membres hérités (champs + ordre de résolution) pour supprimer les ambiguïtés restantes.

- [ ] Formaliser `Int`, `Bool`, `Char`, `String`, `IntArray`, les types fonction canoniques et
  les types de classes.
- [x] Ajouter `Long` avec litteraux suffixes `L`, arithmetique, comparaisons,
  fonctions, champs et `toString`.
- [x] Ajouter `Float` et `Double` avec litteraux decimaux, IR typee et
  generation SSE.
- [x] Formaliser `Unit` pour les fonctions a effet et les boucles.
- [x] Ajouter les booleens et typer les conditions en `Bool`.
- [x] Centraliser et verifier l'encodage runtime de `Bool` dans l'IR et le
  backend (`false = 1`, `true = 3`).
- [x] Ajouter `else if`.
- [x] Typer les branches `if` heterogenes comme `Unit` pour les usages a effets
  de bord.
- [x] Ajouter `match` V1 avec motifs litteraux et branche finale `_`.
- [x] Ajouter `Char` ASCII, les litteraux de caractere et `String.charAt`.
- [x] Ajouter `String.==` et `String.!=` byte-based.
- [x] Ajouter `String.isEmpty`, `nonEmpty`, `startsWith` et `endsWith`.
- [x] Ajouter `String.toInt` et `parseInt`.
- [x] Ajouter `String.toCharArray` vers `ArrayObject[Char]`.
- [x] Ajouter `String.substring(from, until)`.
- [x] Ajouter `String.repeat(count)`.
- [x] Ajouter `String.trim`.
- [x] Ajouter `String.split(separator)`.
- [x] Ajouter `String.indexOf` et `String.contains`.
- [x] Ajouter `String.+` et `mkString` pour `ArrayInt`, `ArrayLong`,
  `ArrayBool` et `Array[String]`.
- [x] Ajouter les operateurs booleens `&&`, `||` et `!`.
- [x] Ajouter l'opérateur unaire `-` pour les types numériques.
- [x] Ajouter le court-circuit pour `&&` et `||`.
- [x] Ajouter le reste de division `%` pour `Int` et `Long`.
- [x] Ajouter la conversion `Int.toLong`.
- [x] Ajouter les champs et methodes herites si l'heritage est retenu.
- [ ] Valider les types des branches, boucles et operateurs de facon uniforme.
- [x] Ajouter une premiere syntaxe de types parametres pour `Option[Int]` et
  `Array[Int]` via aliases standard.
- [x] Centraliser les aliases de types standard comme `Array[Int] -> ArrayInt`.
- [x] Ajouter les declarations de classes generiques utilisateur `class Box[T]`
  avec substitution des champs, methodes et types fonction.
- [x] Ajouter les fonctions generiques utilisateur explicites
  `def identity[T](value: T): T`.
- [x] Ajouter une premiere surcharge des fonctions globales non generiques par
  signature exacte.
- [x] Autoriser les references de fonctions surchargees quand un type fonction
  est attendu en argument.
- [x] Ajouter l'inference des arguments de type des fonctions generiques.
- [x] Autoriser les references de fonctions generiques specialisees comme
  valeurs, par exemple `identity[Int]`.
- [ ] Autoriser les fonctions generiques comme valeurs polymorphes.
- [x] Distinguer les allocations de classes generiques concretes dans l'IR.
- [x] Ajouter des wrappers IR specialises pour les methodes de classes
  generiques concretes.
- [x] Monomorphiser les corps des methodes de classes generiques dans l'IR.
- [x] Ajouter la monomorphisation des fonctions generiques.
- [x] Ajouter `Option[T]` dans la bibliotheque standard generique.
- [x] Ajouter les methodes generiques explicites comme `Option[T].map[U]`.
- [x] Inferer les arguments de type des methodes generiques.
- [x] Ajouter `Option[T].flatMap[U]`.
- [x] Ajouter `Option[T].foreach`.
- [ ] Ajouter la monomorphisation complete des classes generiques.
- [x] Generaliser `IntUnaryFn` vers des types fonction canoniques.
- [x] Introduire une representation interne commune des types fonction.
- [x] Ajouter la syntaxe de types fonction parenthesee pour les aliases actuels.
- [x] Autoriser les lambdas a plus de deux parametres.
- [x] Autoriser les types fonction en position de retour.
- [x] Autoriser l'appel des champs de type fonction depuis les methodes de
  classe.
- [x] Retirer les aliases historiques `IntUnaryFn`, `IntConsumerFn` et
  `IntBinaryFn`.
- [x] Ajouter `IntBinaryFn` pour `fold` et operations binaires de collections.
- [x] Ajouter les lambdas sans capture pour `IntUnaryFn`.
- [x] Ajouter `IntConsumerFn` pour les fonctions `Int => Unit`.
- [x] Ajouter `arrayIntRange` et `ArrayInt.foreach` comme premiers parcours
  fonctionnels de collection.
- [x] Ajouter `arrayIntRangeUntil(start, until)` pour les plages `Int` bornees.
- [x] Ajouter `intRangeUntil(start, until)` comme premiere plage `Int` lazy.
- [x] Ajouter `exists`, `forall`, `count`, `find` et `filter` lazy sur
  `RangeInt`.
- [x] Ajouter `RangeInt.foldLong` pour accumuler une plage `Int` vers `Long`.
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
- [x] Ajouter `collections.array` et `arrayFill[T]` comme premiere API commune
  pour `Array[Int]`, `Array[Long]`, `Array[Float]`, `Array[Double]` et
  `Array[Bool]`.
- [x] Ajouter `arrayMap[T]` et `arrayForeach[T]` comme operations generiques
  standard specialisees pour `Array[Int]`, `Array[Long]`, `Array[Float]`,
  `Array[Double]` et `Array[Bool]`.
- [x] Autoriser `Array[T]` dans les signatures generiques utilisateur et inferer
  `T` depuis `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` et
  `ArrayBool`.
- [x] Ajouter les appels de methodes sur receveur generique `Array[T]` dans les
  corps generiques pour `size`, `length`, `isEmpty`, `nonEmpty`, `get`, `set`,
  `map` et `foreach`.
- [x] Definir la representation generique `Any` / slot runtime commune pour les
  valeurs stockees dans une collection generique.
- [x] Ajouter une collection native `ObjectArray` stockant des slots/pointeurs
  generiques.
- [x] Ajouter la facade standard `ArrayObject[T]` pour les types non specialises,
  avec `length`, `size`, `get`, `set`, `map[U]` et `foreach`.
- [x] Ajouter `ArrayObject[T].filter`.
- [x] Ajouter `ArrayObject[T].fold[U]`.
- [x] Ajouter `ArrayObject[T].flatMap[U]`.
- [x] Etendre `ArrayObject[T].map` en `map[U]`.
- [x] Ajouter `arrayMap[Primitive, U]` vers `ArrayObject[U]`.
- [x] Ajouter `arrayFilter[T]`.
- [x] Ajouter `arrayFold[T]` et `arrayFold[T, U]`.
- [x] Ajouter `arrayFlatMap[T]` et `arrayFlatMap[T, U]`.
- [x] Completer `arrayFlatMap[T]` pour `Long`, `Float`, `Double` et `Bool`.
- [x] Ajouter `mapObject[U]` sur les facades primitives.
- [x] Ajouter `flatMapObject[U]` sur les facades primitives.
- [x] Ajouter `filter` et `fold` sur `ArrayLong`, `ArrayFloat`,
  `ArrayDouble` et `ArrayBool`.
- [x] Etendre les aliases standard pour choisir automatiquement :
  `Array[Int] -> ArrayInt`, `Array[Long] -> ArrayLong`,
  `Array[Float] -> ArrayFloat`, `Array[Double] -> ArrayDouble`,
  `Array[Bool] -> ArrayBool`, sinon `Array[T] -> ArrayObject[T]`.
- [x] Ajouter les tests de vraie genericite `Array[String]`,
  `Array[Array[Int]]`, `Array[Option[String]]` et tableaux d'objets utilisateur.
- [x] Etendre le fallback `ArrayObject[T]` vers `filter`, `fold` et `flatMap`.
- [x] Ajouter les primitives `renameFile(from, to)` et `createDir(path)`.
- [x] Exposer `io.renameTextFile` et `io.createDirectory`.

### P2 - Runtime Et Objets

- [x] Verifier les depassements du tas.
- [x] Initialiser le tas runtime via `mmap` et aligner les allocations bump.
- [ ] Definir de vraies vtables ou stabiliser le header runtime actuel.
- [x] Extraire le runtime ASM commun du backend IR.
- [x] Stabiliser la representation de `String`.
- [x] Ajouter `String.charAt(index): Char` sur la representation byte-based.
- [ ] Choisir une strategie memoire a long terme: heap structure, header
  d'objet, liberation des objets non references et eventuel GC mark-and-sweep.
- [x] Ajouter une primitive d'affichage console pour `String`.
- [x] Ajouter une primitive d'entree console `readLine(): String`.
- [x] Ajouter une premiere lecture/ecriture de fichiers texte.
- [x] Ajouter verification d'existence et append pour fichiers texte.
- [x] Lire les fichiers texte complets au lieu d'un bloc fixe.
- [x] Ajouter suppression de fichiers texte.
- [x] Documenter les conventions d'erreur actuelles de l'IO fichier.
- [x] Ajouter une premiere collection native `IntArray`.
- [x] Ajouter une collection native `LongArray`.
- [x] Ajouter une collection native `FloatArray`.
- [x] Ajouter une collection native `DoubleArray`.
- [x] Ajouter une collection native `BoolArray`.
- [x] Ajouter `shuffle` sur `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble`
  et `ArrayBool` via le generateur pseudo-aléatoire `util`.
- [x] Initialiser les tableaux natifs selon l'encodage runtime de leur élément :
  zéros/faux taggés pour `Int`, `Long` et `Bool`, zéros IEEE pour `Float` /
  `Double`, et slots nuls pour `ObjectArray[T]`.

### P3 - Outillage

- [x] Retirer les binaires suivis sous `build/`.
- [ ] Ajouter une cible de formatage.
- [x] Ajouter une integration continue.
- [ ] Ajouter des tests unitaires du lexer, parser et analyseur semantique.
- [x] Ajouter une racine `stdlib/` importable.
- [x] Ajouter un premier module de collections dans `stdlib/`.
- [x] Ajouter une premiere documentation utilisateur du langage.
- [x] Ajouter une reference HTML de la stdlib generee depuis les commentaires
  `///` publics.
- [x] Ajouter des badges de statut `@status` dans la reference HTML pour
  distinguer API recommandee, compatibilite et helpers internes documentes.
- [x] Ajouter un check CI qui lance `make stdlib-docs` et echoue si la
  generation laisse un diff non commite.
- [x] Ajouter un check CI qui lance `make examples` pour vérifier les exemples
  publics.
- [x] Ajouter une courte specification vivante (`docs/internals.md`) pour les
  types, le runtime, l'IR et les conventions de stdlib.
- [x] Ajouter une roadmap de reprise dans `docs/roadmap.md`.
- [x] Ajouter un support Vim minimal pour `*.nabla`.
- [x] Ajouter des tests Project Euler progressifs pour guider les extensions du
  langage.
- [x] Vérifier `examples/student_scores.nabla` avec un code de sortie et une
  sortie console attendus.
- [x] Vérifier `examples/workshop_set_inheritance.nabla` avec un code de sortie
  et une sortie console attendus.
- [x] Migrer `examples/workshop_set_inheritance.nabla` vers l'API publique
  `Array[T]` + `Set.fromArray[T]` au lieu de `ObjectArray[T]` / `ArrayObject[T]`
  dans son chemin principal.
- [x] Ajouter une régression `Set[Person]` construite depuis `Array[Person]`
  contenant des instances de sous-types (`Student`, `Instructor`, `Volunteer`).
- [x] Ajouter un dispatch dynamique runtime pour les méthodes override
  utilisateur appelées via un type parent, y compris parents génériques
  instanciés et méthodes génériques spécialisées, avec `super` maintenu
  statique.
- [x] Ajouter une signature constructeur héritée typée dans `extends Parent(...)`
  pour rendre explicites les champs transmis au parent et les champs propres.
- [x] Redispatcher `Any.toString()`, `Any.hashCode()` et `Any.equals(...)` vers
  les overrides utilisateur pour les valeurs parent-typées ou génériques
  utilisées par `Set[T]`.
- [x] Ajouter `object Name { def ... }` comme namespace statique et supporter
  les compagnons de surface `class Name` + `object Name`.
- [x] Ajouter un test d'outillage pour vérifier le diagnostic quand une commande
  externe requise (`nasm`) est absente du `PATH`.
- [x] Ajouter le support `write` / `append` multi-mots dans
  `examples/command_shell.nabla`.
- [x] Exposer `objectArrayMkString` et l'alias compat
  `objectStringArrayMkString` pour `ArrayObject[String]`.
- [x] Valider explicitement les ambiguïtés de résolution de méthode héritée
  (`extends` + `with`) et améliorer le diagnostic associé.

## Journal Des Jalons
- `local` - Ajouter la surcharge V1 des fonctions globales par signature exacte:
  le contexte garde un index de surcharges par nom source, les variantes sont
  abaissées vers des noms IR uniques, les appels choisissent la variante depuis
  les types d'arguments, et `math` expose maintenant `sqrt(value: Float)` /
  `sqrt(value: Double)` en plus des noms suffixes de compatibilite.
  - Limites actuelles: resolution exacte uniquement, references de fonctions
    surchargees sans type attendu non supportees, methodes et generiques a
    etendre plus tard.
  - Fichiers / tests associés: `src/compiler_context.hpp`, `src/parser.cpp`,
    `src/ast.hpp`, `src/ast.cpp`, `stdlib/math.nabla`,
    `tests/test_function_overload.nabla`,
    `tests/test_function_overload_math_sqrt.nabla`,
    `tests/test_error_function_overload_duplicate.nabla`,
    `tests/test_error_function_overload_no_match.nabla`, `docs/language.md`,
    `docs/stdlib-api.md`, `docs/roadmap.md`, `make all-tests`.
- `local` - Etendre la surcharge aux references de fonctions avec type attendu:
  le parseur resout maintenant `sqrt` ou une surcharge utilisateur en position
  d'argument quand la signature attendue est un type fonction, puis l'AST garde
  le nom source pour les diagnostics et le nom IR unique pour l'abaissement.
  - Limites actuelles: pas encore d'annotation de type locale pour ecrire
    `val f: (Float) => Float = sqrt`, references surchargees nues toujours
    refusees, generiques/methodes surchargees a definir plus tard.
  - Fichiers / tests associes: `src/parser.cpp`, `src/parser.hpp`,
    `src/ast.hpp`, `src/ast.cpp`,
    `tests/test_function_overload_reference_expected.nabla`,
    `tests/test_error_function_overload_reference_no_expected.nabla`,
    `docs/language.md`, `docs/roadmap.md`, `make all-tests`.
- `local` - Enrichir les descriptions utilisateur de la reference stdlib pour
  `io`, `math`, `strings` et `OptionInt`: conventions de retour I/O,
  comportements limites de `pow*` / `sqrt*`, separation de `words`, et raison
  d'etre de `OptionInt`. La doc `math` note aussi le futur objectif de surcharge
  (`sqrt(value)`) pour remplacer les noms suffixes.
  - Fichiers / tests associés: `stdlib/io.nabla`, `stdlib/math.nabla`,
    `stdlib/strings.nabla`, `stdlib/core/option_int.nabla`, `docs/stdlib/`,
    `docs/roadmap.md`, `make stdlib-docs`.
- `local` - Ajouter les statuts visuels dans la reference HTML de la stdlib:
  le generateur lit `@status`, affiche des badges `Recommandee`,
  `Compatibilite` ou `Interne`, et les surfaces `Array`, `Option` et `Set`
  distinguent les compagnons recommandes des anciens alias compatibles.
  - Fichiers / tests associés: `tools/generate_stdlib_docs.py`,
    `stdlib/collections/array.nabla`, `stdlib/core/option.nabla`,
    `stdlib/collections/set.nabla`, `docs/stdlib/`, `docs/roadmap.md`,
    `make stdlib-docs`, `git diff --exit-code docs/stdlib`.
- `local` - Etendre les statuts `Recommandee` aux modules stdlib publics restants
  deja presents dans la reference HTML: `OptionInt`, `io`, `math` et `strings`,
  sans publier de nouveaux symboles.
  - Fichiers / tests associés: `stdlib/core/option_int.nabla`,
    `stdlib/io.nabla`, `stdlib/math.nabla`, `stdlib/strings.nabla`,
    `docs/stdlib/`, `docs/roadmap.md`, `make stdlib-docs`.
- `local` - Ameliorer les diagnostics de compatibilite stdlib: les appels via
  anciens noms (`ArrayFill`, `SetFromArray`, `SetEmpty`, `optionSome`) gardent
  le nom source dans les erreurs meme apres resolution vers un helper interne,
  et ajoutent une suggestion vers le compagnon recommande.
  - Fichiers / tests associés: `src/compiler_context.hpp`, `src/ast.hpp`,
    `src/ast.cpp`, `src/parser.cpp`,
    `tests/test_error_legacy_array_fill_type.nabla`,
    `tests/test_error_legacy_array_fill_type_args.nabla`,
    `tests/test_error_legacy_set_from_array_type.nabla`,
    `tests/test_error_legacy_set_empty_arity.nabla`,
    `tests/test_error_legacy_option_some_type.nabla`, `docs/roadmap.md`.
- `local` - Nettoyer les exemples publics vers la surface compagnon: migrer
  `examples/euler1_array.nabla` vers `Array.range`, exposer `Array[T]` dans les
  signatures de `examples/command_shell.nabla` et `examples/student_scores.nabla`
  au lieu de `ArrayObject[T]`, et conserver les exemples bas niveau uniquement
  quand ils exercent volontairement une facade specialisee.
  - Fichiers / tests associés: `examples/euler1_array.nabla`,
    `examples/command_shell.nabla`, `examples/student_scores.nabla`,
    `docs/releases/0.1.md`, `AGENTS.md`, `make examples`.
- `local` - Exposer le compagnon standard `object Array` comme surface
  idiomatique: `Array.fill[T](size, value)`, `Array.range(size)` et
  `Array.rangeUntil(start, until)`, avec résolution des alias specialises pour
  les tableaux primitifs et conservation de `ArrayFill` / `ArrayRange` comme
  compatibilite.
  - Fichiers / tests associés: `src/compiler_context.hpp`,
    `stdlib/collections/array.nabla`, `tests/test_user_friendly_array.nabla`,
    `tests/test_stdlib_array_generic_fill.nabla`,
    `examples/euler5_fold.nabla`, `examples/euler6.nabla`,
    `examples/student_scores.nabla`, `docs/language.md`,
    `docs/stdlib-api.md`.
- `local` - Exposer le compagnon standard `object Option` comme surface
  idiomatique: `Option.some[T](value)` et `Option.none[T](default)`, tout en
  gardant `optionSome` / `optionNone` comme compatibilite. L'absence garde pour
  l'instant une valeur interne de secours, conformement a la representation
  actuelle de `Option[T]`.
  - Fichiers / tests associés: `stdlib/core/option.nabla`,
    `tests/test_stdlib_option_string.nabla`, `tests/test_stdlib_option_int.nabla`,
    `tests/test_stdlib_option_map_int.nabla`,
    `tests/test_stdlib_option_map_string.nabla`,
    `tests/test_stdlib_option_filter.nabla`,
    `tests/test_stdlib_option_or_else.nabla`,
    `tests/test_stdlib_option_flatmap_int.nabla`,
    `tests/test_stdlib_option_flatmap_string.nabla`,
    `tests/test_stdlib_option_flatmap_none.nabla`,
    `tests/test_stdlib_option_foreach_string.nabla`,
    `tests/test_stdlib_option_foreach_none.nabla`,
    `examples/student_scores.nabla`, `docs/language.md`,
    `docs/stdlib-api.md`, `docs/roadmap.md`.
- `local` - Exposer le compagnon standard `object Set` comme surface
  idiomatique: `Set.empty[T]()` et `Set.fromArray[T](values)`, avec résolution
  des alias specialisees pour les tableaux primitifs et conservation de
  `SetEmpty` / `SetFromArray` comme compatibilite.
  - Fichiers / tests associés: `src/compiler_context.hpp`, `src/parser.cpp`,
    `stdlib/collections/set.nabla`, `tests/test_stdlib_set.nabla`,
    `tests/test_stdlib_set_int_array_object.nabla`,
    `tests/test_inheritance_collection_parent_type.nabla`,
    `tests/test_inheritance_set_equals_override.nabla`,
    `tests/test_inheritance_set_hashcode_override.nabla`,
    `examples/workshop_set_inheritance.nabla`, `docs/language.md`,
    `docs/stdlib-api.md`, `docs/stdlib/collections/set.html`,
    `docs/roadmap.md`.
- `local` - Ajouter les objets statiques façon Scala: `object Name { def ... }`
  déclare des fonctions globales qualifiées appelables via `Name.method(...)`,
  avec support des méthodes génériques et des compagnons de surface
  `class Name` + `object Name`.
  - Fichiers / tests associés: `src/lexer.hpp`, `src/parser.hpp`,
    `src/parser.cpp`, `src/compiler_context.hpp`,
    `tests/test_object_static_methods.nabla`,
    `tests/test_object_companion_namespace.nabla`,
    `tests/test_error_object_override.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/roadmap.md`.
- `local` - Ajouter l'égalité personnalisée de base pour les objets: `Any`
  expose `equals(other: Any): Bool`, `==` / `!=` sur objets s'abaissent vers
  `Any.equals(...)`, et `Any.toString()` / `Any.hashCode()` / `Any.equals(...)`
  redispatchent vers les overrides utilisateur pour les valeurs parent-typées
  ou génériques.
  - Fichiers / tests associés: `src/semantic_analyzer.cpp`, `src/ast.cpp`,
    `src/ir_codegen.cpp`, `src/runtime_asm.cpp`,
    `tests/test_any_equals_default.nabla`,
    `tests/test_inheritance_set_equals_override.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/roadmap.md`.
- `local` - Étendre le dispatch dynamique au modèle attendu de polymorphisme
  runtime: parents génériques instanciés (`Parent[Int]`) et méthodes génériques
  spécialisées (`tag[Int]`) redispatchent vers l'override runtime; l'IR builder
  reçoit désormais le contexte pour émettre les spécialisations override
  nécessaires même sans appel direct à la sous-classe.
  - Fichiers / tests associés: `src/ir.hpp`, `src/ir.cpp`, `src/main.cpp`,
    `src/ir_codegen.cpp`,
    `tests/test_inheritance_dynamic_dispatch_generic_parent.nabla`,
    `tests/test_inheritance_dynamic_dispatch_generic_method.nabla`,
    `docs/language.md`, `docs/internals.md`, `docs/roadmap.md`.
- `local` - Redispatcher `Any.hashCode()` vers les overrides utilisateur quand
  une valeur est manipulée via `Any`, un type parent ou un paramètre générique
  spécialisé, et décaler les identifiants de classes runtime hors de la plage
  des tags boxed.
  - Fichiers / tests associés: `src/ir_codegen.cpp`,
    `tests/test_inheritance_set_hashcode_override.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/roadmap.md`, `AGENTS.md`.
- `local` - Ajouter la signature constructeur héritée typée dans
  `extends Parent(...)`: le préfixe valide les champs du parent direct et le
  suffixe devient les champs propres de la classe enfant. Migrer le workshop
  héritage vers `extends Person(nameValue: String)`.
  - Fichiers / tests associés: `src/parser.cpp`, `src/semantic_analyzer.cpp`,
    `src/compiler_context.hpp`,
    `tests/test_inheritance_constructor_inherited_signature.nabla`,
    `tests/test_error_inheritance_constructor_signature_parent_mismatch.nabla`,
    `examples/workshop_set_inheritance.nabla`, `docs/language.md`,
    `docs/roadmap.md`.
- `local` - Ajouter un premier dispatch dynamique pour les overrides utilisateur
  non génériques via identifiant de classe runtime dans le header objet, et
  distinguer les appels `super` comme appels statiques dans l'IR.
  - Fichiers / tests associés: `src/ir.hpp`, `src/ir.cpp`, `src/ast.cpp`,
    `src/ir_codegen.cpp`, `tests/test_inheritance_dynamic_dispatch_parent.nabla`,
    `tests/test_inheritance_dynamic_dispatch_super_static.nabla`,
    `tests/test_inheritance_collection_parent_type.nabla`, `README.md`,
    `docs/language.md`, `docs/internals.md`, `docs/roadmap.md`.
- `local` - Corriger le dédoublonnage des spécialisations IR de méthodes afin
  qu'un même corps spécialisé comme `Set[Person].contains` ne soit pas émis
  plusieurs fois lorsque les appels passent des sous-types différents.
  - Fichiers / tests associés: `src/ir.cpp`,
    `tests/test_inheritance_collection_parent_type.nabla`, `AGENTS.md`,
    `docs/roadmap.md`.
- `local` - Migrer l'exemple public `workshop_set_inheritance` vers la surface
  utilisateur recommandee : `collections.array`, `new Array[Student](...)` et
  `Set.fromArray[Student](...)`, tout en conservant les oracles de sortie.
  - Fichiers / tests associés: `examples/workshop_set_inheritance.nabla`,
    `AGENTS.md`, `docs/roadmap.md`, `make test SRC=examples/workshop_set_inheritance.nabla`.
- `local` - Rendre explicite l'encodage runtime de `Bool` dans l'IR :
  constantes source taggees des le lowering, validation backend des constantes
  `Bool` et test de regression couvrant constantes, comparaisons, retours de
  fonctions, logique et `BoolArray`.
  - Fichiers / tests associés: `src/ast.cpp`, `src/ir_codegen.cpp`,
    `tests/test_bool_runtime_encoding_regression.nabla`,
    `tests/test_bool_runtime_encoding_regression.expected`,
    `tests/test_bool_runtime_encoding_regression.ir`, `docs/internals.md`,
    `docs/roadmap.md`, `AGENTS.md`.
- `local` - Vérifier `examples/workshop_set_inheritance.nabla` comme exemple
  public d'héritage et de `collections.set`, avec code de sortie et oracle
  stdout sous `make examples`.
  - Fichiers / tests associés: `examples/workshop_set_inheritance.nabla`,
    `examples/workshop_set_inheritance.expected`,
    `examples/workshop_set_inheritance.stdout`, `AGENTS.md`,
    `docs/roadmap.md`.
- `local` - Ajouter `make examples` à la CI GitHub pour vérifier les exemples
  publics à chaque push et pull request.
  - Fichiers / tests associés: `.github/workflows/ci.yml`, `AGENTS.md`,
    `docs/roadmap.md`.
- `local` - Vérifier `examples/student_scores.nabla` comme exemple public
  idiomatique : sortie console nettoyée, code de sortie attendu et oracle
  stdout sous `make examples`.
  - Fichiers / tests associés: `examples/student_scores.nabla`,
    `examples/student_scores.expected`, `examples/student_scores.stdout`,
    `AGENTS.md`, `docs/roadmap.md`.
- `local` - Centraliser les constantes d'encodage runtime dans
  `src/runtime_values.hpp` et les utiliser depuis le backend IR et le runtime
  assembleur pour les entiers tagges, booleens et slots nuls.
  - Fichiers / tests associés: `src/runtime_values.hpp`, `src/ir_codegen.cpp`,
    `src/runtime_asm.cpp`, `docs/internals.md`, `AGENTS.md`.
- `local` - Definir le perimetre de Nabla 0.1 : objectifs, inclusions,
  non-objectifs, criteres de sortie et roadmap courte avant tag.
  - Fichiers / tests associés: `docs/releases/0.1.md`, `README.md`,
    `AGENTS.md`.
- `local` - Classer la surface de la bibliotheque standard avec
  `docs/stdlib-api.md` : API publique, compatibilite temporaire et details
  internes, afin de guider les prochains exemples et durcissements de
  diagnostics.
  - Fichiers / tests associés: `docs/stdlib-api.md`, `README.md`,
    `docs/roadmap.md`, `AGENTS.md`.
- `local` - Ajouter `docs/internals.md` comme specification vivante des
  conventions internes actuelles : pipeline, commandes externes, tagging,
  layout objets/tableaux, chaînes, closures, héritage, génériques et erreurs
  runtime observables.
  - Fichiers / tests associés: `docs/internals.md`, `README.md`,
    `docs/roadmap.md`, `AGENTS.md`.
- `local` - Améliorer le diagnostic d'outillage externe : si `execvp` ne trouve
  pas `nasm` ou `ld`, le compilateur affiche maintenant la commande manquante et
  `make tooling-tests` couvre le cas `nasm` absent du `PATH`.
  - Fichiers / tests associés: `src/main.cpp`, `Makefile`,
    `.github/workflows/ci.yml`, `tests/test_missing_external_tools.sh`,
    `README.md`, `docs/roadmap.md`, `AGENTS.md`.
- `local` - Ajouter un garde-fou CI pour la documentation générée de la stdlib :
  `make stdlib-docs` est exécuté puis `git diff --exit-code docs/stdlib`
  échoue si la référence HTML n'est pas committée.
  - Fichiers / tests associés: `.github/workflows/ci.yml`, `AGENTS.md`,
    `docs/roadmap.md`.
- `local` - Corriger les principaux points de la revue du 16/06/2026 :
  validation stricte des signatures `override`, initialisation par type des
  tableaux natifs, marqueur `<unresolved>` côté parser et erreur explicite côté
  backend lorsqu'un type IR manque.
  - Fichiers / tests associés: `src/semantic_analyzer.cpp`,
    `src/ir_codegen.cpp`, `src/parser.cpp`,
    `tests/test_error_override_return_type.nabla`,
    `tests/test_error_override_parameter_type.nabla`,
    `tests/test_error_override_generic_signature.nabla`,
    `tests/test_inheritance_override_generic_method.nabla`,
    `tests/test_inheritance_override_generic_parent.nabla`,
    `tests/test_float_array_default_zero.nabla`,
    `tests/test_double_array_default_zero.nabla`, `docs/roadmap.md`.
- `local` - Détecter et signaler explicitement les ambiguïtés de méthodes
  héritées entre plusieurs définitions disponibles dans la hiérarchie
  (`extends` + `with`), via une validation sémantique explicite et des tests
  de régression ciblés.
  - Fichiers / tests associés: `src/compiler_context.hpp`, `src/parser.hpp`,
    `src/parser.cpp`, `src/ast.cpp`, `tests/test_error_inheritance_generic_field_conflict.nabla`.
- `local` - Implémenter le mot-clé `override` pour les méthodes de classes et
  valider son usage lorsque la cible héritée est absente, et imposer son usage
  explicite lors d’une redéfinition de méthode héritée.
  - Fichiers / tests associés: `src/compiler_context.hpp`, `src/lexer.hpp`,
    `src/parser.cpp`, `src/semantic_analyzer.cpp`,
    `tests/test_inheritance_override.nabla`,
    `tests/test_error_override_no_match.nabla`,
    `tests/test_error_override_missing_parent.nabla`,
    `tests/test_error_override_without_keyword.nabla`.
- `1c77da3` - Etendre `collections.set` avec des opérations d’ensemble
  immutables `union`, `intersect`, `difference`, et tests de régression.
  - Fichiers / tests associés: `stdlib/collections/set.nabla`,
    `tests/test_stdlib_set.nabla`, `tests/test_stdlib_set.expected`.
- `local` - Ajouter `setFromArray[T](values: ArrayObject[T]): Set[T]` avec
  déduplication par ordre d’apparition dans la source, et tests associés.
  - Fichiers / tests associés: `stdlib/collections/set.nabla`,
    `tests/test_stdlib_set.nabla`, `docs/language.md`.
- `local` - Utiliser `hashCode()` de `Any` dans `collections.set` pour améliorer
  la complexité moyenne de `contains`, `union`, `intersect`, `difference` et
  `setFromArray` tout en conservant l’égalité par `==` et l’ordre stable.
  - Fichiers / tests associés: `stdlib/collections/set.nabla`,
    `tests/test_stdlib_set.nabla`, `docs/language.md`, `AGENTS.md`.
- `local` - Documenter les limites ergonomiques observées sur l'exemple
  `examples/workshop_set_inheritance.nabla` :
  - appel de constructeurs d'héritage trop verbeux,
  - usage explicite d’`override` requis pour toute redéfinition.
  - frictions de typage dans `Set[Person]` avec sous-types.
  - Fichiers / notes associés: `AGENTS.md`, `docs/roadmap.md`,
    `examples/workshop_set_inheritance.nabla`.
- `local` - Mettre la stdlib/tests en conformité avec la règle
  d’`override` obligatoire sur les méthodes héritées (`toString`, `hashCode`,
  redéfinitions de `Any` et hiérarchies de base).
  - Fichiers associés: `stdlib/collections/bool_array.nabla`,
    `stdlib/collections/double_array.nabla`,
    `stdlib/collections/float_array.nabla`,
    `stdlib/collections/int_array.nabla`,
    `stdlib/collections/long_array.nabla`,
    `stdlib/collections/object_array.nabla`,
    `stdlib/collections/set.nabla`,
    `tests/test_any_base_methods.nabla`,
    `tests/test_inheritance_mixin_override.nabla`,
    `tests/test_inheritance_super.nabla`,
    `tests/test_inheritance_super_chain.nabla`,
    `tests/test_inheritance_super_with_mixin_override.nabla`,
    `tests/test_stdlib_array_to_string.nabla`, `tests/test_stdlib_set.nabla`,
    `examples/workshop_set_inheritance.nabla`.
- `local` - Ajouter `examples/workshop_set_inheritance.nabla` montrant
  `collections.set` (avec construction depuis tableau, `union`, `intersect`,
  `difference`) et l’héritage via `Person`, `Student`, `Instructor`,
  `Volunteer`.
- `local` - Renforcer la résolution des membres hérités en cas d'ambiguïtés de type
  générique (`Holder[Int]` vs `Holder[String]`) et formaliser la racine `Any`
  implicite pour les classes sans `extends`.
  - Tests: `test_error_inheritance_generic_field_conflict.nabla`,
    `test_error_inheritance_generic_method_conflict.nabla`.
- `local` - Consolider la résolution `super` en présence de chaînes d'héritage,
  de mixins et d'erreur dédiée pour `super` sans parent explicite.
  - Tests: `test_inheritance_super_chain.nabla`,
    `test_inheritance_super_with_mixin.nabla`,
    `test_inheritance_super_with_mixin_override.nabla`,
    `test_error_super_without_parent.nabla`.
- `local` - Formaliser la résolution des membres hérités (champs + méthode) en
  présence de `extends` + `with`, détecter les conflits de champs translatifs et
  construire les layouts hiérarchiques pour les allocations de champ.
  - Tests: `test_inheritance_inherited_field.nabla`,
    `test_error_inheritance_inherited_field_conflict.diagnostic`.
- `712c2c5` - Etendre l'héritage par mixins avec résolution hiérarchique, détection de conflits et support de `super` pour la classe parente directe.
  - Fichiers / tests associés: `lexer.hpp`, `parser.cpp`, `ast.hpp`, `ast.cpp`,
    `semantic_analyzer.cpp`, `compiler_context.hpp`, `docs/language.md`,
    `tests/test_inheritance_super.nabla`, `tests/test_error_super_outside_class.nabla`,
    `tests/test_inheritance_mixin_override.nabla`,
    `tests/test_error_inheritance_mixin_conflict_transitive.nabla`.
- `local` - Ajouter le support de l'héritage (`extends` + `with`) sur les classes:
  parent explicite, mixins, résolution hiérarchique des méthodes, validation des
  parents inconnus/argumentation et détection de cycles, classe racine `Any`, et
  détection de conflits de méthodes héritées entre mixins/parent (directs et
  transitatifs).
  - Tests: `test_inheritance_simple`, `test_inheritance_multiple`,
    `test_error_inheritance_unknown_parent`, `test_error_inheritance_cycle`,
    `test_error_inheritance_mixin_conflict`,
    `test_error_inheritance_mixin_conflict_transitive`.
- `local` - Ajouter `super` pour appeler explicitement la méthode de la classe
  parente directe (`super.method(...)`) et contourner les redéfinitions de la
  classe courante.
  - Tests: `test_inheritance_super`, `test_error_super_outside_class`.

- `local` - Durcir la compilation backend : passage `std::system` -> `fork` +
  `execvp` dans `main.cpp`, ajout de contrôle division par zéro en codegen `/` et
  `%`, et contrôles d'overflow/limites runtime pour `parseInt` + parsing +/-.

- `local` - Ajouter `shuffle` aux facades de collection `ArrayInt`,
  `ArrayLong`, `ArrayFloat`, `ArrayDouble`, `ArrayBool` et
  `ArrayObject[T]`.

- `local` - Etendre la bibliotheque standard `math` avec constantes `pi`,
  conversion degrés/radians et `hypotenuse`.

- `local` - Ajouter le module standard `util` (pseudo-random deterministic)
  et le test `test_stdlib_util_random`.
- `local` - Ajouter `randomSeedNow()` en `stdlib/util` via `timeSeed` pour une seed
  temporelle de départ.
- `local` - Etendre `stdlib/util` avec `randomInt(min, max)`, `randomSeedTime()`,
  `RandomChoiceResult` et `*Array.randomChoice(...)` avec fallback `default` sur
  les tableaux `Int`, `Long`, `Float`, `Double`, `Bool` et `Object`.

- `local` - Ajouter les tests Project Euler 10 imperatif et fonctionnel, avec
  `Int.toLong` et `RangeInt.foldLong`.
- `1b3be08` - Etendre `RangeInt` avec `exists`, `forall`, `count`, `find` et
  `filter` lazy.
- `66299f2` - Remplacer le tas statique de 64 KiB par un tas runtime `mmap` de
  8 MiB avec allocations alignees et un test positif au-dessus de 64 KiB.
- `e28f460` - Ajouter un support Vim minimal pour `*.nabla`.
- `f6bdacb` - Utiliser `match` dans les handlers de `examples/command_shell.nabla`.
- `4eb09e3` - Simplifier le dispatch de `examples/command_shell.nabla`.
- `local` - Ajouter le support `write` / `append` multi-mots dans
  `examples/command_shell.nabla`.
  - Tests: `make test SRC=examples/command_shell.nabla` via scénario manuel.
- `local` - Exposer `objectArrayMkString` + alias compat
  `objectStringArrayMkString` pour `ArrayObject[String]`.
  - Tests: `make test SRC=tests/test_array_object_string_mk_string.nabla`.
- `40f2e17` - Ajouter les expressions `match` avec motifs litteraux et `_`.
- `3419402` - Ajouter `deleteFile` / `io.deleteTextFile` et la commande `rm`.
- `ea2a09e` - Ajouter les expressions `else if`.
- `local` - Ajouter l'opérateur unaire `-` pour les types numériques et les
  tests correspondants (`test_unary_minus`, `test_error_unary_minus_not_numeric`).
- `6751bd6` - Montrer l'I/O fichier dans `examples/command_shell.nabla`.
- `290fbab` - Lire les fichiers texte complets.
- `17b184f` - Documenter les helpers d'I/O texte.
- `local` - Ajouter les tests Project Euler 9 imperatif et fonctionnel.
- `local` - Ajouter `RangeInt` lazy et l'utiliser dans la variante collections
  de Project Euler 8.
- `local` - Autoriser l'appel des champs de type fonction depuis les methodes.
- `local` - Ajouter les tests Project Euler 8 imperatif et collections.
- `local` - Typer les branches `if` heterogenes comme `Unit` et simplifier le
  test Project Euler 7.
- `local` - Ajouter un test Project Euler 7 avec recherche du 10001e nombre
  premier.
- `local` - Ajouter l'optimisation backend des appels recursifs terminaux et
  l'utiliser dans le `gcd` de Project Euler 5.
- `local` - Ajouter un test Project Euler 6 avec `arrayIntRangeUntil`, `map` et
  `sum`.
- `local` - Ajouter `arrayIntRangeUntil(start, until)` et simplifier la
  variante `fold` de Project Euler 5.
- `local` - Ajouter les tests Project Euler 5 imperatif et `fold`.
- `local` - Ajouter un test Project Euler 4 avec palindrome numerique et
  boucles imbriquees.
- `local` - Ajouter un test Project Euler 3 avec factorisation `Long`.
- `local` - Ajouter un test Project Euler 2 avec Fibonacci imperatif et
  verification stdout.
- `local` - Ajouter l'operateur `%` pour `Int` et `Long`, avec tests Project
  Euler 1 imperatif et `ArrayInt.range/filter/sum`.
- `local` - Ajouter `Char` ASCII et `String.charAt(index): Char`.
- `local` - Ajouter `String.==` et `String.!=`.
- `local` - Ajouter `String.isEmpty`, `nonEmpty`, `startsWith` et `endsWith`.
- `local` - Ajouter `String.toInt` et `parseInt(value)`.
- `local` - Ajouter `String.toCharArray(): ArrayObject[Char]`.
- `local` - Ajouter `String.substring(from, until)`.
- `local` - Ajouter `String.repeat(count)`.
- `local` - Ajouter `String.trim`.
- `local` - Ajouter `String.split(separator)`.
- `local` - Ajouter `examples/command_shell.nabla` pour demontrer
  `readLine`, `trim`, `split`, `repeat` et `toInt`.
- `local` - Ajouter `strings.words(text)` et l'utiliser dans
  `examples/command_shell.nabla`.
- `local` - Ajouter `docs/language.md` comme premiere documentation utilisateur
  du langage.
- `local` - Ajouter `String.indexOf` et `String.contains`.
- `local` - Ajouter `String.+` et `mkString` pour `ArrayInt`, `ArrayLong`,
  `ArrayBool` et `Array[String]`.
- `local` - Ajouter la primitive d'entree console `readLine(): String` et
  `io.input()`.
- `local` - Ajouter `readFile`, `writeFile` et les wrappers
  `io.readTextFile` / `io.writeTextFile`.
- `local` - Ajouter `fileExists`, `appendFile` et les wrappers
  `io.pathExists` / `io.appendTextFile`.
- `local` - Ajouter `renameFile(from, to)` et `createDir(path)` avec wrappers
  `io.renameTextFile` et `io.createDirectory`.
- `local` - Documenter les conventions actuelles de l'IO fichier.
- `local` - Ajouter les gardes de branche dans `match` (`motif if condition`),
  avec validation de type Bool pour la garde et diagnostics dédiés.
- `local` - Ajouter les motifs nommes (`identifiant`) pour capturer la valeur dans
  le corps d'une branche.
- `local` - Valider la portée des motifs nommes en ajoutant des tests
  positifs/négatifs (`test_match_named_pattern_scoped`,
  `test_error_match_named_pattern_scope_leak`) pour confirmer l'isolation
  de portée.
- `local` - Ajouter la facade standard `arrayFold[T]` / `arrayFold[T, U]`.
- `local` - Completer `arrayFlatMap[T]` pour toutes les facades primitives.
- `local` - Ajouter la facade standard `arrayFilter[T]`.
- `local` - Factoriser le routage des aliases standards de tableaux dans
  `compiler_context.hpp`.
- `local` - Ajouter la facade standard `arrayFlatMap[T]` / `arrayFlatMap[T, U]`.
- `local` - Refaire `examples/student_scores.nabla` comme exemple vitrine de
  `Array[Student]`, `map[U]`, `filter`, `fold[U]`, `flatMap[U]` et
  `mapObject[U]`.
- `local` - Ajouter `flatMapObject[U]` sur les facades primitives avec wrappers
  top-level.
- `local` - Ajouter `mapObject[U]` sur les facades primitives et autoriser la
  specialisation IR des methodes generiques sur classes non generiques.
- `local` - Ajouter `filter` et `fold` aux facades primitives `Long`, `Float`,
  `Double` et `Bool`.
- `local` - Ajouter les ponts `arrayMap[Primitive, U]` vers `ArrayObject[U]`
  pour `Int`, `Long`, `Float`, `Double` et `Bool`.
- `local` - Etendre `ArrayObject[T].map` en `map[U]` et propager les
  specialisations de methodes generiques via les aliases `Array[T]`.
- `local` - Ajouter `ArrayObject[T].flatMap[U]` et le tester avec `String` et
  `Option[String]` en sortie.
- `local` - Ajouter `ArrayObject[T].fold[U]` et le tester avec accumulateurs
  `Int`, `String` et `Option[String]`.
- `local` - Ajouter `ArrayObject[T].filter` et le tester sur `String`,
  `Option[String]` et objets utilisateur.
- `local` - Ajouter `FloatArray` / `DoubleArray`, les facades `ArrayFloat` /
  `ArrayDouble` et les alias `Array[Float]` / `Array[Double]` dans
  `collections.array`.
- `local` - Durcir le fallback `ArrayObject[T]` avec `Array[Option[String]]` et
  les tableaux d'objets utilisateur, y compris l'inference vers `Array[T]`.
- `local` - Ajouter le fallback `ObjectArray[T]` / `ArrayObject[T]` pour
  `Array[String]` et `Array[Array[Int]]`, avec specialisation IR des methodes
  generiques et alias `arrayFill/map/foreach` vers la facade objet.
- `a0db6bb` - Ajouter les appels de methodes communs sur receveur generique
  `Array[T]`, avec specialisation vers les facades concretes et lambdas
  generiques specialisees.
- `c34468f` - Ajouter `arrayMap[T]` et `arrayForeach[T]` comme operations communes
  specialisees pour `Array[Int]`, `Array[Long]` et `Array[Bool]`, et autoriser
  `Array[T]` comme facade de signature generique inferee depuis les
  specialisations concretes.
- `a86a9ea` - Ajouter `collections.array` et `arrayFill[T]`, resolu vers les
  specialisations `ArrayInt`, `ArrayLong` et `ArrayBool`.
- `31b39ad` - Ajouter `BoolArray`, la facade standard `ArrayBool` et l'alias
  `Array[Bool]`.
- `5392788` - Ajouter `LongArray`, la facade standard `ArrayLong` et l'alias
  `Array[Long]`.
- `6c0005b` - Centraliser les aliases de types standard, notamment
  `Array[Int] -> ArrayInt`, et les tester dans `Option[...]` et les types
  fonction.
- `fcce070` - Ajouter `Option[T].foreach` pour les effets controles sur
  `Some`.
- `7bc14aa` - Ajouter `Option[T].flatMap[U]` a la bibliotheque standard generique.
- `3aa4fe5` - Autoriser les references de fonctions generiques specialisees comme
  valeurs fonction, par exemple `identity[Int]`.
- `01beb83` - Inferer les arguments de type des methodes generiques, notamment
  `Option[T].map(...)`.
- `11cd62d` - Ajouter les methodes generiques explicites, utilisees par
  `Option[T].map[U]`.
- `e8ed453` - Ajouter `core.option` avec `Option[T]`, `optionSome`, `optionNone`,
  `filter`, `orElse` et `getOrElse`.
- `7f5ac21` - Monomorphiser les fonctions generiques en corps IR specialises comme
  `identity[Int]` et `applyOnce[Int]`.
- `4c7e3be` - Monomorphiser les corps des methodes de classes generiques dans l'IR,
  par exemple `Box[Int].get` charge directement `Box[Int].value`.
- `75b59ec` - Ajouter des wrappers IR specialises pour les methodes de classes
  generiques concretes comme `Box[Int].get`.
- `7147281` - Conserver les allocations de classes generiques concretes dans l'IR,
  par exemple `new Box[Int]`, avec resolution de layout via le template.
- `1a10139` - Ajouter l'inference des arguments de type des fonctions generiques,
  y compris le typage progressif des lambdas en argument.
- `5b33e25` - Ajouter les fonctions generiques explicites comme `identity[T]`,
  appelees avec `identity[Int](...)`, et la substitution de `T` dans les
  signatures.
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

Etendre la surcharge de fonctions au-dela de la V1 :

- ajouter les annotations de type locales pour pouvoir ecrire
  `val f: (Float) => Float = sqrt`;
- definir la strategie pour les fonctions generiques surchargees;
- enrichir les diagnostics pour distinguer absence de candidat, arite
  incompatible et ambiguite reelle;
- migrer progressivement `math` vers des noms idiomatiques surcharges (`abs`,
  `min`, `max`, `pow`) en gardant les noms suffixes comme compatibilite.
