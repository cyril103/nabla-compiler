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
- Pour une nouvelle feature, utiliser `docs/feature-integration.md` comme
  checklist de reprise: branche propre, plan actif si necessaire, tests, docs,
  validation locale et hygiene Markdown avant PR.

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
  `Set[T]`, `Map[K, V]`, `Sized`, `Iterable[T]`, `String`, classes, methodes, lambdas;
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
  plutot que magie implicite;
- utiliser les champs constructeur `var` pour les optimisations internes de
  collections, sans exposer de setter public tant que les règles de visibilité
  ne sont pas formalisées.

Priorites structurantes :

1. Stabiliser l'API publique de `Array[T]`, `Option[T]`, `Set[T]`,
   `Map[K, V]` et `Sized` en suivant `docs/stdlib-api.md`.
2. Classer la stdlib en surface publique et modules/helpers internes dans
   `docs/stdlib-api.md` avant d'ajouter de nouveaux symboles.
3. Maintenir la specification vivante `docs/internals.md` pour les types,
   le runtime et les regles de typage.
4. Maintenir le check CI qui verifie que `make stdlib-docs` ne laisse aucun diff.
5. Produire des exemples idiomatiques n'utilisant pas les API internes.
6. Demarrer les nouvelles features depuis un etat de reprise clair, en suivant
   `docs/feature-integration.md` et en gardant `docs/plans/` reserve aux plans
   actifs.
7. Reporter les grosses nouvelles structures (`Result[T]`, GC,
   variance avancee) tant que l'ergonomie des collections et options n'est pas
   stabilisee.

## Etat Actuel

Le pipeline implemente actuellement :

- tokenisation et parsing des classes, imports, fonctions et methodes avec
  parametres,
  expressions arithmetiques, comparaisons, `if`, `else if`, `match`, `while`,
  `for`, `val` et `var` avec annotations de type locales optionnelles;
- identifiants et chemins d'import avec lettres, chiffres et `_`;
- resolution des imports relatifs, depuis la racine projet et depuis `stdlib/`,
  avec protection contre les cycles;
- objets avec champs de constructeur et appels de methodes parametres; les
  paramètres constructeur `val` et `var` exposent des getters synthétiques, et
  les champs `var` sont réassignables depuis les méthodes de la classe;
- fonctions globales appelables avec parametres;
- fonctions locales `def` dans les blocs, abaissees en fonctions cachees non
  surchargees; elles supportent l'appel direct, la recursion sur elles-memes,
  les appels vers des helpers locaux declares plus haut et les contextes
  generiques des fonctions/methodes/classes/traits englobants. Les references
  comme valeurs fonction restent supportees pour les helpers declares hors
  contexte generique; les captures implicites et fonctions locales generiques
  restent reportees;
- proprietes calculees `def` sans liste de parametres, abaissees comme des
  fonctions/methodes zero-argument: `def pi: Double = 3.14` s'utilise comme
  `pi`, `Config.base` appelle `Config.base()`, `value.head` appelle
  `value.head()`, et les membres abstraits de trait peuvent s'ecrire
  `def head: T`;
- surcharge V1.2 des fonctions globales par signature exacte, avec nom IR unique
  par variante, priorite aux variantes concretes sur les variantes generiques
  inferees, references typees capables d'inferer les variantes generiques, et
  references resolues quand un type fonction est attendu en argument ou via une
  annotation locale; les echecs et ambiguites de resolution listent la forme
  appelee et les signatures candidates; `math` consomme directement les noms
  surcharges idiomatiques pour `abs`, `min`, `max`, `clamp`, `pow` et `sqrt`;
- surcharge V1 des methodes de classe par signature exacte, avec nom IR unique
  par variante, resolution par les types d'arguments et priorite aux variantes
  concretes sur les variantes generiques inferables; les lambdas inferees en
  argument recoivent un type attendu quand la forme d'appel selectionne une
  surcharge unique, y compris apres inference de type depuis les arguments deja
  lus d'une methode generique, et les cas ambigus produisent un diagnostic dedie;
- types fonction-valeur canoniques `Fn(...)->...`, references de fonctions
  nommees et appels indirects, avec lambdas sans capture `() => { ... }`,
  `(x: Int) => { ... }` et `(acc: Int, value: Int) => { ... }`, y compris
  l'appel direct d'une expression retournant une fonction comme
  `makeAnswer()()` ou `makeAdder(10)(32)`, ainsi que les `def` curryfies
  globaux a une liste supplementaire comme `def sum(f)(a, b): Int`;
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
  `get`, `set`, `map`, `foreach` et `sorted(lessThan)`;
- constructeur ergonomique `new Array[T](size)` pour les types primitifs et
  objets utilisateur via les facades `Array[T]`, avec noms utilisateur
  `Array.fill[T](size, value)`, `Array.fill[T](n)(elem)` par nom et les
  surcharges `Array.range(size)` / `Array.range(start, until)` pour eviter
  d'exposer `IntArray` / `ArrayInt` / `ObjectArray[T]` dans les cas simples;
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
- lambdas et appels indirects avec zero, un ou plusieurs parametres, dans les
  limites de la convention d'appel actuelle;
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
- chaines `String` stockees comme objets heap tagges pointant vers des buffers
  de bytes, avec `length` et `charAt(index): Char`,
  `toCharArray(): ArrayObject[Char]`, `toInt`, `substring(from, until)`,
  `repeat(count)`, `trim`, `split(separator)`, `indexOf`, `contains`,
  `isEmpty`, `nonEmpty`, `startsWith`, `endsWith`, `+`, `==` et `!=`; le tag
  permet a `Any.toString`, `Any.hashCode` et `Any.equals` de conserver la
  semantique de contenu quand une chaine est manipulee via `Any`;
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
- module de bibliotheque standard `util` avec `RandomState`,
  `RandomResult[T]`, `RandomIntResult`, `RandomBoolResult`,
  `RandomChoiceResult[T]`, `randomSeed`, `randomSeedNow`, `randomInt`,
  `randomIntRange` et `randomBool` pour une API pseudo-aléatoire deterministe
  basée sur une seed; `randInt()` fournit une commodite sans etat explicite
  pour les petits programmes et expressions par nom comme `Array.fill[Int](n)(randInt())`;
  `randomSeedTime` et `randomIntInRange` restent des alias de compatibilite;
- module de bibliotheque standard `math` avec `abs(Int/Long/Float/Double)`,
  `absDiff(Int/Long/Float/Double)`, `max(Int/Long/Float/Double)`,
  `min(Int/Long/Float/Double)`, `clamp(Int/Long/Float/Double)`,
  `sign(Int/Long/Float/Double)`, `isEven(Int/Long)`, `isOdd(Int/Long)`,
  `isBetween(Int/Long)`, `gcd(Int/Long)`, `lcm(Int/Long)`,
  `pow(Int/Float/Double)`, `factorial(Int)`, `isClose(Float/Double)`,
  `sqrt(Float/Double)`, `degreesToRadians(Float/Double)`,
  `radiansToDegrees(Float/Double)`, `hypotenuse(Float/Double)`, ainsi que
  `piFloat`, `piDouble`, `twoPiFloat` et `twoPiDouble`; les wrappers publics
  suffixes des familles surchargees (`absInt`, `gcdLong`, `sqrtDouble`, etc.)
  ont ete retires;
- module de bibliotheque standard `core.option_int` avec `OptionInt`,
  `optionIntSome`, `optionIntNone`, `map`, `filter` et `orElse`;
- module de bibliotheque standard `core.option` avec `Option[T]`,
  `Option.some[T]`, `Option.none[T]()`, `optionSome` par compatibilite,
  `isDefined`, `isEmpty`, `nonEmpty`,
  `map[U]`, `filter`, `flatMap[U]`, `foreach`, `orElse` et `getOrElse`; les
  methodes internes construisent directement `Option[T]` sans repasser par les
  wrappers publics de compatibilite;
- module de bibliotheque standard `core.sized` avec le trait public `Sized`,
  `size()`, `isEmpty()` et `nonEmpty()` pour les types ayant une taille
  logique;
- module de bibliotheque standard `core.iterable` avec le trait public generique
  `Iterable[T]`, qui herite de `Sized` et ajoute `foreach(f: (T) => Unit)` pour
  les collections parcourables polymorphes;
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
  `objectStringArrayMkString` (compatibilité) et `ArrayObject[T].shuffle`;
  `ArrayObject[T]` implemente aussi le trait public `Sized`, mais reste une
  representation/compatibilite a eviter dans les exemples applicatifs quand
  `Array[T]` suffit.
- module de bibliotheque standard `collections.set` avec `Set[T]`,
  `Set.empty[T]`, `Set.fromArray[T]`, `add`, `remove`, `union`, `intersect`,
  `difference`, `setEmpty`, `setFromArray` et
  `toString`, en utilisant un index de seaux hashés (`hashCode()`) pour des
  opérations de présence à coût moyen réduit; `Set[T]` implemente aussi le
  trait public `Sized`.
- module de bibliotheque standard `collections.map` avec `Map[K, V]`,
  `Map.empty[K, V]`, `Map.fromArray[K, V]`, `apply[K, V]`, `size`, `isEmpty`,
  `nonEmpty`, `containsKey`, `contains`, `getOption(key)`, `getOrElse`, `updated`,
  `removed`, `clear`, `keys`, `values`, `toArray`, `foreachEntry`, `mapValues[U]`,
  `filterKeys`, `mkString` et `toString` pour la représentation textuelle;
  `Map[K, V]` implemente aussi le trait public `Sized`;
- module de bibliotheque standard `collections.list` experimental avec
  `List[T]`, le singleton runtime `Nil: List[Nothing]` assignable a `List[T]`,
  `Cons[T]`, `List.empty[T]()`,
  `List.cons[T]`, `List.fold[T, U]`, `List.map[T, U]`, `List.filter[T]`,
  `List.fromArray[T]`, `head`, `tail`, `headOption`, `prepend`, `prepended`,
  `appended`, `concat`, `reverse`, `reverseConcat`, `take`, `drop`, `slice`,
  `mkString` et implementation de `Iterable[T]`;
- module de bibliotheque standard `collections.array` comme point d'entree
  commun pour les tableaux specialises, avec `Array.fill[T]`, les surcharges
  `Array.range`, `Array.rangeUntil` comme alias de compatibilite,
  `ArrayFill[T]`, `ArrayRange`, `arrayFill[T]`, `arrayMap[T]`,
  `arrayMap[T, U]`, `arrayFilter[T]`, `arrayFold[T]`, `arrayFold[T, U]`,
  `arrayFlatMap[T]`, `arrayFlatMap[T, U]`, `arrayForeach[T]` et
  `Array[T].sorted(lessThan)` resolus vers les
  specialisations `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` ou
  `ArrayBool` pour `T = Int`, `Long`, `Float`, `Double` ou `Bool`, et vers
  `ArrayObject[T]` pour les types concrets non specialises comme `String` et
  `Array[Int]`;
  `arrayMap[Primitive, U]`, `arrayFlatMap[Primitive, U]`, `map[U]`,
  `flatMap[U]`, `mapObject[U]` et `flatMapObject[U]` produisent
  `ArrayObject[U]` quand la sortie ne reste pas dans la meme primitive
  specialisee;
- documentation utilisateur recentree sur `Array[T]`, `Option[T]`, `Set[T]`,
  `Map[K, V]`, `List[T]` experimental et `Sized` comme surface recommandee; `ArrayObject[T]`,
  `ArrayInt` et les tableaux bruts sont decrits comme compatibilite ou details
  de representation, y compris pour les retours actuels de `String.split`,
  `String.toCharArray`, `Set.toArray`, `Map.keys`, `Map.values` et
  `Map.toArray`;
- `ArrayObject[T]`, les facades de tableaux specialisees, `Set[T]`,
  `Map[K, V]` et `List[T]` implementent le trait public `Iterable[...]`; `Iterable[T]`
  herite de `Sized` et expose `foreach`, `size`, `isEmpty` et `nonEmpty` comme
  contrat commun de collection;
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
  héritées, détection de conflits champ/méthode visibles et détection de cycles;
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
- support editeur VS Code dans `editor/vscode` avec detection `*.nabla`,
  grammaire TextMate, configuration langage, snippets et test d'outillage dedie
  via `make tooling-tests`;
- cible de formatage sans dependance externe: `make format` normalise les
  espaces de fin de ligne et le retour final des sources/docs suivis, tandis
  que `make format-check` verifie l'arbre et rejoint `make tooling-tests`;
- `v0.1.0` est tagué; `docs/releases/0.1.md` conserve les notes de release
  et la matrice de validation 0.1.x;
- `docs/feature-integration.md` formalise la reprise d'une nouvelle feature:
  controle des PRs/branches locales, choix entre plan actif et delta-plan,
  definition of done tests/docs, hygiene Markdown et validation avant PR;
- reference HTML de la stdlib generee depuis `///`, avec directive `@status`
  pour distinguer visuellement API recommandee, compatibilite et helpers
  internes; les pages publiees utilisent une mise en page type reference Scala
  avec sidebar par types/fabriques/methodes, signatures ancrees, blocs
  `@example ... @end`, chemins CSS relatifs verifies et test d'outillage dedie
  aux liens/IDs/layout HTML.
- examples Project Euler 1 a 10 (`examples/euler1.nabla` ... `examples/euler10_functional.nabla`)
  comme banc progressif pour exercer le langage et la bibliotheque standard.
- `examples/student_scores.nabla` comme exemple idiomatique vérifié pour
  `Array[T]`, `Array.fill[T]`, `Option.some[T]` / `Option.none[T]`, classes,
  lambdas et sorties console.
- `examples/workshop_set_inheritance.nabla` comme exemple vérifié pour
  `Array[T]`, `Set[T]`, `Set.fromArray[T]`, opérations d'ensemble et héritage
  avec `override`; la friction restante porte surtout sur les collections
  polymorphes de type parent comme `Set[Person]`.
- `examples/game_of_life.nabla` comme exemple interactif compile par
  `make examples` pour `Array[T]` et `Array.fill[T]`, sans import applicatif de
  `IntArray` ni des helpers `intArray...`.
- `examples/stdlib_collections_cookbook.nabla`,
  `examples/stdlib_list_cookbook.nabla` et
  `examples/stdlib_text_cookbook.nabla` comme cookbooks non interactifs verifies
  pour `Array[T]`, `Set[T]`, `Map[K, V]`, `List[T]`, `Option[T]`, `Sized` et les operations
  texte, sans construction directe de `ObjectArray[T]`, `ArrayObject[T]` ou
  `ArrayInt`.
- les tests et exemples ordinaires privilegient maintenant les surcharges
  `Array.range`, `Array.fill[T]`, `Option.some` / `Option.none`,
  `Set.fromArray`, `Set.empty`, `Map.empty`, `Map.fromArray` et `List.empty`;
  les anciens noms encore toleres ou desormais rejetes restent couverts dans les
  tests explicitement dedies aux alias ou diagnostics legacy.
- `List.fromArray[T]` garde son parcours récursif dans un helper local interne;
  l'ancien helper global `listFromArrayFrom` n'est plus import-visible et son
  diagnostic recommande `List.fromArray`.
- `List[T].appended(value)` garde aussi son parcours récursif dans un helper
  local interne; l'ancien helper global `listAppend` n'est plus import-visible.

Limites importantes :

- les fonctions globales sont limitees a 6 parametres et les methodes a 5,
  conformement a la convention d'appel actuelle;
- les `def` curryfies V0 sont limites aux fonctions globales et objets
  statiques; les methodes de classe/trait curryfiees sont encore rejetees
  explicitement;
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
  position d'argument avec type fonction attendu; les diagnostics d'echec
  affichent les candidats disponibles, mais les references sans type attendu ne
  sont pas encore supportees;
  `Array[Int]` reste une facade specialisee vers `ArrayInt`; `Array[Long]`
  reste une facade specialisee vers `ArrayLong`; `Array[Float]` reste une
  facade specialisee vers `ArrayFloat`; `Array[Double]` reste une facade
  specialisee vers `ArrayDouble`; `Array[Bool]` reste une facade specialisee
  vers `ArrayBool`; `arrayFill[T]`, `arrayMap[T]`, `arrayMap[T, U]`,
  `arrayFilter[T]`, `arrayFold[T]`, `arrayFold[T, U]`, `arrayFlatMap[T]`,
  `arrayFlatMap[T, U]` et `arrayForeach[T]` sont des fonctions standard
  generiques specialisees pour `Int`, `Long`, `Float`, `Double` et `Bool`, mais
  pas encore une implementation unique de tableau generique;
  `arrayMap[Primitive, U]`, `arrayFlatMap[Primitive, U]`, `map[U]`,
  `flatMap[U]`, `mapObject[U]` et `flatMapObject[U]` peuvent mapper une
  facade primitive vers `ArrayObject[U]`, avec `map[U]` / `flatMap[U]` comme
  noms source recommandes;
  `Array[T]` est valide dans les
  signatures generiques utilisateur et se specialise correctement quand `T`
  devient concret, avec une premiere surface de methodes communes; `map[U]`
  existe sur `ArrayObject[T]` et sur les facades primitives pour les sorties
  generiques; `flatMap[U]` existe aussi sur les facades primitives pour les
  sorties generiques; `filter` existe sur les facades primitives et `ArrayObject[T]`,
  tandis que les operations non communes comme `sum` ou
  `countTrue` restent specialisees; `fold` existe sur les facades primitives
  avec accumulateur du meme type, et `fold[U]` / `flatMap[U]` existent sur
  `ArrayObject[T]`;
  l'objectif retenu est de conserver les tableaux primitifs specialises et
  d'ajouter un fallback generique `ObjectArray[T]` / `ArrayObject[T]` pour les
  autres types; le premier fallback couvre `String`, `Array[Array[Int]]`,
  `Array[Option[String]]` et les tableaux d'objets utilisateur, tandis que les
  operations avancees restent a faire; la monomorphisation complete des classes
  generiques reste a durcir;
- le runtime initialise par défaut un tas de 8 MiB par `mmap`, configurable à la
  compilation avec `nablac --heap-size <octets>`, avec allocations bump
  alignees sur 8 octets et verification de depassement, mais pas de
  ramasse-miettes;
- les acces hors bornes de `IntArray` terminent le programme avec le code 254;
- `Nothing` est un type bottom builtin assignable a tout type attendu sans
  valeur instanciable; les primitives globales `panic(message: String)` et
  `error(message: String)` produisent `Nothing`, typent les branches qui ne
  retournent pas et terminent le runtime avec le statut 250;
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
  instancié et méthode générique spécialisée; `super` reste statique. Le slot 0
  des objets utilisateur pointe désormais vers une vtable backend, avec des
  slots par propriétaire statique + méthode résolue afin de couvrir les
  surcharges, defaults de traits, `Iterable[T]`, `Sized`, parents génériques et
  méthodes génériques spécialisées. La méthode `toString()`, `hashCode()` et
  `equals(...)` redispatchent aussi depuis `Any` pour stabiliser l'égalité et
  l'index hashé de `Set[Parent]`; une regression couvre aussi les clefs
  `Map[Parent, V]` construites avec des sous-classes. La friction restante
  concerne les règles avancées d'égalité dans les hiérarchies complexes.
- Le mot-clé `override` est supporté pour marquer explicitement les
  redéfinitions de méthodes héritées, et il est obligatoire quand une méthode
  redéfinit une méthode provenant d'un parent.
  Les constructeurs hérités peuvent désormais être exposés avec une signature
  typée dans `extends Parent(...)`: le préfixe reprend les champs du parent
  direct, et le suffixe déclare les champs propres de l'enfant.
- Les traits riches V1 existent comme contrats nominaux sans etat: `trait`
  accepte des signatures abstraites sans corps et des methodes concretes par
  defaut, se compose par `with` dans les classes et les traits, impose
  l'implementation des methodes abstraites dans les classes concretes, exige
  `override` pour les implementations/remplacements de methodes de trait, et
  rejette les conflits de defaults multiples sans override explicite. Les traits
  non generiques peuvent etre implementes par des classes generiques et
  redispatchent correctement via le type du trait. Le trait stdlib public
  `Sized` couvre maintenant `ArrayObject[T]`, `Set[T]` et `Map[K, V]`. Les
  traits ne sont pas instanciables et ne declarent ni champs, ni constructeurs,
  ni appels `super`.
- Les collections basées sur `Any` utilisent `==` pour l'égalité et
  `hashCode()` pour l'index interne. `==` / `!=` sur objets passent par
  `Any.equals(...)`, et `toString()`, `hashCode()` et `equals(...)`
  redispatchent vers les overrides utilisateur même quand la valeur passe par
  `Any`, un type parent ou un paramètre générique spécialisé. La primitive
  globale `print(value)` suit la même règle : elle accepte `Any`, abaisse la
  valeur vers `Any.toString()` puis appelle le runtime d'écriture de chaîne.
- Les `object` existent comme namespaces statiques: leurs `def` sont abaissés
  vers des fonctions globales qualifiées (`Name.method`) et peuvent servir de
  compagnons de surface à une `class Name`. Les `object Name with Trait`
  existent comme singletons runtime V0: ils sont des valeurs stables assignables
  a leurs traits, a `AnyRef` et a `Any`, avec méthodes validées comme celles
  d'une classe pour les abstraits, `override`, signatures et conflits de
  defaults. Les singletons runtime V0 n'ont pas encore champs, constructeur,
  `extends`, arguments de type ou initialisation dédiée; ils ne sont pas
  instanciables avec `new` et ne peuvent pas servir de parents de classe.

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

Validation complete a executer avant chaque commit qui touche `src/`, le
runtime, le parser, l'analyse semantique, l'IR, le backend, les conventions de
typage, les imports ou une zone stdlib transverse :

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

Pour un changement localise a la stdlib, aux docs ou aux tests, sans modification
de `src/`, une validation ciblee est acceptable si elle couvre directement la
surface modifiee :

- compiler et executer les tests ajoutes ou modifies ;
- executer les tests voisins du module touche, par exemple `tests/test_stdlib_map*.nabla`
  pour `stdlib/collections/map.nabla` ;
- executer `make stdlib-docs` puis `git diff --exit-code docs/stdlib` quand une
  API publique, un commentaire `///`, `@signature` ou `@symbol` change ;
- executer `make examples` si l'API publique touchee est utilisee par des
  exemples ou si le changement affecte l'ergonomie utilisateur ;
- executer `git diff --check` au minimum sur les fichiers modifies quand
  l'environnement local produit du bruit CRLF global.

La CI reste l'arbitre final pour la suite complete. En cas de doute sur la
portee reelle d'un changement, preferer la validation complete.

Les tests normaux utilisent un fichier voisin `<nom>.expected`. Un fichier
optionnel `<nom>.stdout` valide la sortie console. Les fichiers dont le nom
contient `error` ou `fail` doivent echouer pendant la compilation. La cible
`unit-tests` compile `tests/frontend_unit_tests.cpp` et exerce directement le
lexer, le parser et l'analyse semantique sans assembler ni executer de programme
Nabla. Le harness couvre aussi des chemins front-end fragiles de surcharge,
d'inference generique et de typage contextuel des lambdas.

## Feuille De Route

### P0 - Parametres De Fonctions Et Methodes

- [x] Parser des parametres nommes et types.
- [x] Enregistrer les signatures completes dans le registre semantique.
- [x] Valider nombre et types des arguments.
- [x] Definir la convention d'appel x86-64 Nabla.
- [x] Ajouter des tests pour fonctions, methodes et erreurs d'appel.
- [x] Supporter les appels postfixes directs sur une expression de type
  fonction, par exemple `makeAdder(10)(32)`.
- [x] Ajouter une premiere syntaxe de `def` curryfiee a une liste de parametres
  supplementaire, abaissee vers un retour de type fonction.
- [x] Autoriser les fonctions locales `def` non generiques dans les contextes
  generiques de fonctions, methodes, classes et traits englobants, avec appels
  directs et recursion specialises via l'IR generique existante.

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
- [x] Exposer les noms mathematiques surcharges `abs`, `min`, `max`, `clamp`,
  `pow` et `sqrt`.
- [x] Supprimer les wrappers publics suffixes des familles `math` deja
  surchargees.
- [x] Migrer les helpers internes `math` hors wrappers vers les noms
  idiomatiques surcharges.
- [x] Etendre `collections.set` avec des operations immutables
  (`union`, `intersect`, `difference`).
- [x] Ajouter `setFromArray[T](values: ArrayObject[T]): Set[T]` pour la
  construction dédupliquante depuis un tableau source.
- [x] Améliorer les opérations de `collections.set` (contains, set op, setFromArray)
  avec un index hashé interne basé sur `hashCode()` pour réduire la complexité
  moyenne des recherches de présence.
- [x] Enrichir `collections.map` avec les méthodes publiques `contains`,
  `foreachEntry`, `mapValues[U]` et `filterKeys`, couvertes par
  `tests/test_stdlib_map_comfort_methods.nabla`.
- [x] Ajouter le module standard `util` avec `randomSeed`, `randomInt`,
  `randomIntRange` et `randomBool` pour une API pseudo-aléatoire
  deterministe.
- [x] Etendre `util` avec `randomIntInRange`,
  `RandomChoiceResult` et `randomSeedTime()`, puis exposer `randomChoice`
  sur `ArrayInt`, `LongArray`, `FloatArray`, `DoubleArray`, `BoolArray` et
  `ArrayObject[T]`.
- [x] Ajouter `randomSeedNow()` au module standard `util`, basé sur une source de
  timestamp en runtime pour initialiser un générateur avec une seed temporelle,
  puis exposer `randInt()` pour les appels sans etat explicite.
- [x] Migrer les tests et exemples ordinaires de collections/options/set vers
  l'API publique recommandee (`Array.*`, `Option.*`, `Set.*`) en conservant les
  tests d'alias legacy.
- [x] Ajouter `Option.none[T]()` et `Map.getOption(key)` comme formes publiques
  naturelles; les anciennes formes avec valeur factice ont ensuite ete retirees
  par diagnostics d'arite exacts; couvert par
  `tests/test_stdlib_option_none_empty.nabla` et
  `tests/test_stdlib_map_get_option_empty.nabla`.
- [x] Clarifier la documentation utilisateur et la reference stdlib autour de
  la surface publique `Array[T]`, `Option[T]`, `Set[T]`, `Map[K, V]` et
  `Sized`, en classant `ArrayObject[T]`, `ArrayInt` et les tableaux bruts comme
  compatibilite ou details de representation.

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
- [x] Ajouter les traits riches V1: declarations `trait`, signatures abstraites,
  methodes par defaut, composition par `with`, obligation d'implementation par
  les classes concretes, `override` obligatoire et rejet des conflits de
  defaults.
- [x] Ajouter les proprietes `def` sans parametres pour fonctions globales,
  methodes de classes/objets, overrides et signatures abstraites de traits,
  avec acces sucre `name` / `receiver.name` vers les appels zero-argument.
- [x] Autoriser les classes generiques a implementer des traits non generiques,
  avec dispatch des methodes abstraites et des defaults via le type du trait.
- [x] Ajouter le trait stdlib public `Sized` et l'appliquer a `ArrayObject[T]`,
  `Set[T]` et `Map[K, V]`.

- [x] Formaliser `Int`, `Bool`, `Char`, `String`, `IntArray`, les types fonction canoniques et
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
- [x] Valider les types des branches, boucles et operateurs de facon uniforme.
- [x] Ajouter une premiere syntaxe de types parametres pour `Option[Int]` et
  `Array[Int]` via aliases standard.
- [x] Centraliser les aliases de types standard comme `Array[Int] -> ArrayInt`.
- [x] Ajouter les declarations de classes generiques utilisateur `class Box[T]`
  avec substitution des champs, methodes et types fonction.
- [x] Ajouter les fonctions generiques utilisateur explicites
  `def identity[T](value: T): T`.
- [x] Ajouter une premiere surcharge des fonctions globales non generiques par
  signature exacte.
- [x] Prioriser les surcharges globales concretes sur les generiques inferables.
- [x] Autoriser les references de fonctions surchargees quand un type fonction
  est attendu en argument.
- [x] Inferer les references de fonctions generiques surchargees depuis le type
  fonction attendu.
- [x] Ajouter les annotations de type locales pour `val` / `var`.
- [x] Enrichir les diagnostics de surcharge avec forme appelee et candidats.
- [x] Ajouter une premiere surcharge des methodes de classe par signature exacte.
- [x] Prioriser les surcharges de methodes concretes sur les generiques
  inferables.
- [x] Inferer les lambdas dans les appels de methodes generiques surchargees
  quand les arguments precedents resolvent les parametres de type.
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
- [x] Ajouter `map[U]` sur les facades primitives comme nom recommande pour les
  conversions vers `ArrayObject[U]`, en gardant les surcharges same-type
  specialisees.
- [x] Ajouter `flatMapObject[U]` sur les facades primitives.
- [x] Ajouter `flatMap[U]` sur les facades primitives comme nom recommande pour
  les conversions vers `ArrayObject[U]`, en gardant les surcharges same-type
  specialisees.
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
- [x] Permettre de configurer la capacité du heap par exécutable avec
  `nablac --heap-size <octets>` tout en gardant 8 MiB par défaut.
- [x] Definir de vraies vtables backend pour les objets utilisateur et
  singletons runtime, tout en conservant les tags runtime reserves pour les
  primitives boxees et `String`.
- [x] Ajouter le type bottom `Nothing` et les primitives `panic` / `error` pour
  les chemins qui ne retournent pas.
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
- [x] Factoriser le chemin runtime ASM commun de `writeFile` et `appendFile`
  derriere un helper interne parametre par les flags `open(2)`.
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
- [x] Ajouter une cible de formatage.
- [x] Ajouter une integration continue.
- [x] Ajouter une premiere cible `unit-tests` pour le lexer, parser et analyseur
  semantique, integree aux tests d'outillage, puis l'etendre aux surcharges et
  generiques front-end.
- [x] Completer les diagnostics de compatibilite des factories Array legacy:
  `ArrayEmpty` / `ArrayTabulate` et les helpers specialises mal parametres
  recommandent `Array.empty` / `Array.tabulate`.
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
- [x] Ajouter un support VS Code minimal pour `*.nabla` avec grammaire TextMate,
  configuration langage, snippets et test d'outillage.
- [x] Ajouter des tests Project Euler progressifs pour guider les extensions du
  langage.
- [x] Vérifier `examples/student_scores.nabla` avec un code de sortie et une
  sortie console attendus.
- [x] Vérifier `examples/workshop_set_inheritance.nabla` avec un code de sortie
  et une sortie console attendus.
- [x] Ajouter des cookbooks stdlib executables et verifies pour collections et
  traitement texte, avec sorties console deterministes.
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
- [x] Tagger explicitement les objets `String` runtime et aligner les littéraux
  de chaînes afin que `String` conserve `toString`, `hashCode` et `equals` par
  contenu quand une valeur est manipulée via `Any`.
- [x] Ajouter `object Name { def ... }` comme namespace statique et supporter
  les compagnons de surface `class Name` + `object Name`.
- [x] Ajouter `object Name with Trait { ... }` comme singleton runtime V0,
  assignable comme valeur a ses traits, a `AnyRef` et a `Any`, avec diagnostics
  class-like pour abstraits manquants, `override`, signatures et conflits de
  defaults.
- [x] Ajouter un test d'outillage pour vérifier le diagnostic quand une commande
  externe requise (`nasm`) est absente du `PATH`.
- [x] Ajouter le support `write` / `append` multi-mots dans
  `examples/command_shell.nabla`.
- [x] Exposer `objectArrayMkString` et l'alias compat
  `objectStringArrayMkString` pour `ArrayObject[String]`.
- [x] Valider explicitement les ambiguïtés de résolution de méthode héritée
  (`extends` + `with`) et améliorer le diagnostic associé.
- [x] Rendre configurable la capacité du heap runtime par exécutable avec
  `nablac --heap-size <octets>`.

## Journal Des Jalons

- `local` - Masquer le helper interne `listAppend`: `List[T].appended(value)`
  utilise maintenant un `def appendTo(...)` local pour son parcours recursif,
  sans changement de comportement public. Un test négatif exact vérifie que
  l'ancien helper global n'est plus import-visible. Le plan actif temporaire
  n'est pas conserve dans la PR.
  - Fichiers / tests associes: `stdlib/collections/list.nabla`,
    `tests/test_error_list_append_helper_hidden.nabla`,
    `tests/test_error_list_append_helper_hidden.diagnostic`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/hide-list-append-helper.md`.

- `local` - Masquer le helper interne `listFromArrayFrom`: `List.fromArray[T]`
  utilise maintenant un `def fromIndex(...)` local pour son parcours recursif,
  sans changement de comportement public. Un test négatif exact vérifie que
  l'ancien helper global n'est plus import-visible et recommande `List.fromArray`.
  Le plan actif temporaire n'est pas conserve dans la PR.
  - Fichiers / tests associes: `stdlib/collections/list.nabla`,
    `tests/test_error_list_fromarray_helper_hidden.nabla`,
    `tests/test_error_list_fromarray_helper_hidden.diagnostic`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/hide-list-fromarray-helper.md`.

- `local` - Completer les diagnostics legacy des helpers collections: les noms
  anciens ou devines `MapEmpty`, `MapFromArray` et `ListEmpty` affichent
  maintenant un suffixe `Nom recommande` vers `Map.empty`, `Map.fromArray` et
  `List.empty`, et la table de recommandations couvre aussi les helpers internes
  `mapEmpty`, `mapFromArray`, `listEmpty`, `listCons` et `listFromArrayFrom`.
  Aucun programme accepte ni API stdlib ne change; le plan actif temporaire
  n'est pas conserve dans la PR.
  - Fichiers / tests associes: `src/compiler_context.hpp`,
    `tests/test_error_legacy_map_empty_suggestion.nabla`,
    `tests/test_error_legacy_map_empty_suggestion.diagnostic`,
    `tests/test_error_legacy_map_fromarray_suggestion.nabla`,
    `tests/test_error_legacy_map_fromarray_suggestion.diagnostic`,
    `tests/test_error_legacy_list_empty_suggestion.nabla`,
    `tests/test_error_legacy_list_empty_suggestion.diagnostic`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/collection-helper-diagnostics.md`.

- `local` - Completer les diagnostics legacy des factories Array: les anciens
  noms publics `ArrayEmpty` et `ArrayTabulate` affichent maintenant le suffixe
  `Nom recommande` vers `Array.empty` / `Array.tabulate`, et les helpers
  specialises comme `arrayIntEmpty[Int]()` ou `arrayIntTabulate[Int](...)`
  gardent aussi cette recommandation quand ils sont mal appeles avec des type
  arguments. Aucun programme accepté ni API stdlib ne change; le plan actif
  temporaire n'est pas conserve dans la PR.
  - Fichiers / tests associes: `src/compiler_context.hpp`,
    `tests/test_error_legacy_array_empty_suggestion.nabla`,
    `tests/test_error_legacy_array_empty_suggestion.diagnostic`,
    `tests/test_error_legacy_array_tabulate_suggestion.nabla`,
    `tests/test_error_legacy_array_tabulate_suggestion.diagnostic`,
    `tests/test_error_legacy_array_empty_helper_type_args.nabla`,
    `tests/test_error_legacy_array_empty_helper_type_args.diagnostic`,
    `tests/test_error_legacy_array_tabulate_helper_type_args.nabla`,
    `tests/test_error_legacy_array_tabulate_helper_type_args.diagnostic`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/array-factory-legacy-diagnostics.md`.

- `local` - Factoriser les primitives runtime ASM d'ecriture fichier:
  `Runtime_writeFile` et `Runtime_appendFile` conservent leurs labels backend
  publics mais sautent vers `Runtime_writeStringToFileWithFlags`, qui partage la
  copie du chemin, l'ouverture, l'ecriture, la fermeture et le retour entier
  tagge. Aucun symbole stdlib ni comportement utilisateur n'est modifie; les
  regressions I/O existantes couvrent `write`, `append`, lecture, suppression,
  renommage, creation de dossier et EOF. Le plan actif temporaire n'est pas
  conserve dans la PR.
  - Fichiers / tests associes: `src/runtime_asm.cpp`, `docs/internals.md`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/runtime-io-asm-helpers.md`, tests I/O cibles.

- `local` - Ajouter une premiere cible de formatage sans dependance externe:
  `tools/format_sources.py` supprime les espaces de fin de ligne et garantit le
  retour final des sources/docs suivis, `make format` applique la
  normalisation, `make format-check` verifie l'arbre et rejoint
  `tooling-tests`. Le test d'outillage couvre le mode correction et le mode
  `--check`; le plan actif temporaire n'est pas conserve dans la PR.
  - Fichiers / tests associes: `tools/format_sources.py`,
    `tests/test_format_sources.py`, `Makefile`, `docs/roadmap.md`, `AGENTS.md`,
    `make format-check`, `make tooling-tests`.

- `local` - Etendre le harness de tests unitaires front-end aux chemins de
  surcharge et generiques: resolution de surcharge globale/methode par type
  d'argument, diagnostic d'ambiguite de surcharge, inference de fonction
  generique depuis les arguments et typage attendu d'une lambda passee a une
  fonction generique. Le plan actif temporaire n'est pas conserve dans la PR.
  - Fichiers / tests associes: `tests/frontend_unit_tests.cpp`,
    `docs/roadmap.md`, `AGENTS.md`, `make unit-tests`.

- `local` - Ajouter une premiere cible de tests unitaires front-end: le harness
  C++ `tests/frontend_unit_tests.cpp` couvre directement les tokens/positions du
  lexer, l'acceptation et le rejet parser, puis des diagnostics semantiques de
  controle de flux sans passer par NASM/ld. La cible `unit-tests` est integree a
  `tooling-tests`, et le plan actif temporaire est retire avant PR.
  - Fichiers / tests associes: `Makefile`, `tests/frontend_unit_tests.cpp`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/frontend-unit-tests.md`, `make unit-tests`.

- `local` - Durcir la validation des types de controle de flux: les diagnostics
  de `if`, `while`, `for`, `!` et du cote gauche de `&&` / `||` pointent
  maintenant l'expression fautive, avec regressions dediees pour le compteur
  `for` non-`Int`, le cote gauche de `&&` et le cote droit de `||`. Le plan
  temporaire est retire dans la meme PR que le durcissement.
  - Fichiers / tests associes: `src/ast.cpp`,
    `tests/test_error_for_count_type.nabla`,
    `tests/test_error_bool_and_left_type.nabla`,
    `tests/test_error_bool_or_right_type.nabla`,
    `tests/test_error_int_condition.diagnostic`,
    `tests/test_error_loop_as_int.diagnostic`, `docs/language.md`,
    `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/control-flow-type-validation.md`, tests cibles.

- `local` - Cadrer explicitement `mapObject[U]` et `flatMapObject[U]` comme
  compatibilite stdlib: les regressions dediees portent desormais le marqueur
  `compat` (`test_compat_...` ou `test_error_compat_...`), tandis que les tests
  de surface recommandee restent centres sur `map[U]` et `flatMap[U]`. Aucun
  symbole public n'est retire dans cette tranche.
  - Fichiers / tests associes: `tests/test_compat_primitive_map_object_method.nabla`,
    `tests/test_compat_primitive_flatmap_object_method.nabla`,
    `tests/test_error_compat_primitive_map_object_return_type.nabla`,
    `tests/test_error_compat_primitive_flatmap_object_return_type.nabla`,
    `docs/stdlib-api.md`, `AGENTS.md`, tests cibles.

- `local` - Ajouter les surcharges generiques `flatMap[U]` aux facades
  primitives `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` et
  `ArrayBool` pour que les conversions vers `Array[U]` utilisent le nom
  idiomatique `flatMap` sans exposer `flatMapObject`; les surcharges same-type
  specialisees restent verifiees par regression sur chaque facade primitive. Le
  plan de travail temporaire a ete retire dans la meme PR que la feature.
  - Fichiers / tests associes: `stdlib/collections/*_array.nabla`,
    `tests/test_generic_array_primitive_flatmap_method.nabla`,
    `docs/stdlib-api.md`, `docs/roadmap.md`, `AGENTS.md`, suppression de
    `docs/plans/primitive-array-generic-flatmap.md`, tests cibles.

- `local` - Retirer le plan actif termine `primitive-array-generic-map` apres
  squash-merge: `docs/plans/` revient a l'etat sans plan actif, le perimetre
  livre restant documente dans ce journal, `docs/roadmap.md`, les tests et
  l'historique de PR.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`, suppression
    de `docs/plans/primitive-array-generic-map.md`, `git diff --check`.

- `local` - Ajouter les surcharges generiques `map[U]` aux facades primitives
  `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble` et `ArrayBool` pour que
  les conversions vers `Array[U]` utilisent le nom idiomatique `map` sans
  exposer `mapObject`; les surcharges same-type specialisees restent verifiees
  par regression sur chaque facade primitive.
  - Fichiers / tests associes: `stdlib/collections/*_array.nabla`,
    `tests/test_generic_array_primitive_map_method.nabla`,
    `examples/student_scores.nabla`, `docs/stdlib-api.md`, `docs/roadmap.md`,
    `AGENTS.md`, tests cibles.

- `local` - Ajouter une regression post-0.1 pour le dispatch dynamique de
  `hashCode()` / `equals(...)` quand une map est typée `Map[Parent, V]` mais
  reçoit des instances de sous-classes comme clefs. Le test vérifie la
  déduplication, `contains`, `getOrElse`, `updated` et `removed` afin de couvrir
  l'index hashé et l'égalité via le type parent.
  - Fichiers / tests associes: `tests/test_inheritance_map_parent_dispatch.nabla`,
    `tests/test_inheritance_map_parent_dispatch.expected`, `docs/roadmap.md`,
    `AGENTS.md`, test cible.

- `local` - Ajouter `randInt()` au module `util`: cette commodite retourne un
  entier pseudo-aleatoire non negatif depuis une seed temporelle, sans exposer
  `RandomState`, afin de nourrir des expressions par nom comme
  `Array.fill[Int](n)(randInt())`; le chemin reproductible reste
  `randomSeed(...)` + `randomInt(state)`.
  - Fichiers / tests associes: `stdlib/util.nabla`, `docs/stdlib-api.md`,
    `tests/test_stdlib_util_rand_int_fill.nabla`, `make test` cible. Le plan
    actif `docs/plans/rand-int-convenience.md` a ete retire apres squash-merge.

- `local` - Retirer le plan actif termine `Array.sorted` apres squash-merge:
  `docs/plans/` revient a l'etat sans plan actif, le perimetre livre restant
  documente dans ce journal, `docs/roadmap.md`, les tests et l'historique de PR.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`, suppression
    de `docs/plans/array-sorted.md`, `git diff --check`.

- `local` - Ajouter `Array.sorted`: les facades `ArrayInt`, `ArrayLong`,
  `ArrayFloat`, `ArrayDouble`, `ArrayBool` et `ArrayObject[T]` retournent une
  copie stable triee sans muter la source; les primitives ont `sorted()` par
  ordre naturel et toutes exposent `sorted(lessThan)` pour l'ordre utilisateur,
  utilisable aussi depuis une signature generique `Array[T]`.
  - Fichiers / tests associes: `stdlib/collections/*_array.nabla`,
    `src/compiler_context.hpp`, `docs/stdlib-api.md`,
    `tests/test_stdlib_array_sorted_int.nabla`,
    `tests/test_stdlib_array_sorted_generic.nabla`, `make test` cible.

- `local` - Ajouter une extension VS Code locale pour Nabla: detection des
  fichiers `*.nabla`, grammaire TextMate, configuration de langage, snippets,
  documentation d'installation et test d'outillage rattache a `make tooling-tests`.
  - Fichiers / tests associes: `editor/vscode/`, `tools/test_editor_vscode.py`,
    `Makefile`, `README.md`, `AGENTS.md`, `python tools/test_editor_vscode.py`.

- `local` - Retirer le plan actif termine de la troisieme tranche de cleanup
  stdlib `ArrayFloat` apres squash-merge: `docs/plans/` revient a l'etat sans
  plan actif, le perimetre livre restant documente dans ce journal, les tests et
  l'historique de PR.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`, suppression
    de `docs/plans/stdlib-local-helper-tranche-3.md`, `git diff --check`.

- `local` - Cacher une troisieme tranche de helpers stdlib non documentes via
  fonctions locales: les wrappers `floatArrayLength`, `floatArrayGet` et
  `floatArraySet` ne sont plus des symboles module-level de
  `collections.float_array`; les methodes `ArrayFloat` declarent les petits
  adaptateurs au plus pres des appels `arrayBase...`.
  - Fichiers / tests associes: `stdlib/collections/float_array.nabla`,
    `AGENTS.md`, tests cibles `ArrayFloat`, `make stdlib-docs` sans diff
    attendu.

- `local` - Retirer le plan actif termine de la deuxieme tranche de cleanup
  stdlib `ArrayDouble` apres squash-merge: `docs/plans/` revient a l'etat sans
  plan actif, le perimetre livre restant documente dans ce journal, les tests et
  l'historique de PR.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`, suppression
    de `docs/plans/stdlib-local-helper-tranche-2.md`, `git diff --check`.

- `local` - Cacher une deuxieme tranche de helpers stdlib non documentes via
  fonctions locales: les wrappers `doubleArrayLength`, `doubleArrayGet` et
  `doubleArraySet` ne sont plus des symboles module-level de
  `collections.double_array`; les methodes `ArrayDouble` declarent les petits
  adaptateurs au plus pres des appels `arrayBase...`.
  - Fichiers / tests associes: `stdlib/collections/double_array.nabla`,
    `AGENTS.md`, tests cibles `ArrayDouble`, `make stdlib-docs` sans diff
    attendu.

- `local` - Ajouter la surcharge curryfiee `Array.fill[T](n)(elem)` avec
  parametre par nom reevalue par element, et corriger la specialisation IR des
  closures generiques qui capturent des valeurs.
- `local` - Nettoyer l'etat de reprise du depot pour les prochaines features:
  suppression locale des anciennes branches de PR mergees, ajout de
  `docs/feature-integration.md`, liens depuis README/roadmap/plans et alignement
  d'`AGENTS.md` afin de clarifier la checklist tests/docs/hygiene avant PR.
  - Fichiers / tests associes: `README.md`, `docs/feature-integration.md`,
    `docs/plans/README.md`, `docs/roadmap.md`, `AGENTS.md`, `git diff --check`.

- `local` - Retirer les plans actifs termines `docs/plans/by-name-parameters.md` et `docs/plans/constructor-var-fields.md` après squash-merge des PRs associées; `docs/plans/README.md` revient à l'état sans plan actif.
  - Fichiers / tests associes: `docs/plans/README.md`, suppression de `docs/plans/by-name-parameters.md` et `docs/plans/constructor-var-fields.md`, `git diff --check`.

- `local` - Ajouter les paramètres par nom Scala-like `name: => T` : les signatures gardent un marqueur `isByName` et un type interne `Fn()->T`, les arguments sont abaissés en closures zéro-argument, les références nues dans le corps deviennent des appels indirects, et les types fonction imbriqués `Fn(Fn()->T)->R` sont parsés correctement pour les fonctions curryfiées.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/parser.hpp`, `src/parser.cpp`, `tests/test_by_name_parameter_*.nabla`, `tests/test_error_by_name_type_position.nabla`, `docs/language.md`, `docs/internals.md`, `docs/roadmap.md`, `make all-tests`.

- `local` - Ajouter les types fonction et lambdas zero-argument `() => T` / `() => { ... }` : representation canonique `Fn()->T`, inference en argument attendu, references de fonctions zero-parametre comme valeurs, appels indirects et closures avec capture par valeur. Ce jalon prepare l'etude d'un futur passage d'argument par nom sur base de thunks.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/parser.cpp`, `tests/test_lambda_zero_parameters*.nabla`, `tests/test_function_reference_zero_parameters.nabla`, `tests/test_function_returning_zero_parameter_lambda.nabla`, `tests/test_overloaded_function_returning_zero_parameter_lambda_argument.nabla`, `docs/language.md`, `docs/internals.md`, `docs/roadmap.md`, `make all-tests`.

- `local` - Ajouter les champs constructeur `var` : parser, getters
  synthétiques, affectation interne typée vers les slots objet, diagnostics
  négatifs et tests `List[T]` préparant des builders inspirés de Scala.

- `local` - Nettoyer `docs/plans/` apres livraison du retrait des fabriques
  publiques legacy `SetEmpty` / `SetFromArray`: le plan actif a ete retire de
  l'arbre, dont le perimetre reste documente dans ce journal, les tests et
  l'historique de PR.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`,
    suppression de `docs/plans/remove-set-compat-factories.md`,
    `make all-tests`.

- `local` - Retirer les fabriques publiques de compatibilite `SetEmpty[T]()` et
  `SetFromArray[T](values)`: la surface utilisateur passe uniquement par
  `Set.empty[T]()` et `Set.fromArray[T](values)`, tandis que les helpers bas
  niveau `setEmpty` / `setFromArray` restent internes. Les anciens noms ont des
  diagnostics inconnus avec recommandation vers les compagnons publics.
  - Fichiers / tests associes: `stdlib/collections/set.nabla`,
    `src/compiler_context.hpp`, `tests/test_error_set_*compat_removed.nabla`,
    `docs/stdlib-api.md`, `docs/language.md`,
    `docs/plans/remove-set-compat-factories.md`, `make all-tests`.

- `local` - Nettoyer `docs/plans/` apres livraison des nettoyages stdlib
  `List`/`Option`/`Map`: les plans termines de suppression des compatibilites a
  valeur factice ont ete retires de l'arbre actif, dont le perimetre reste
  documente dans ce journal, les tests et l'historique des PRs.
  - Fichiers / tests associes: `docs/plans/README.md`, `AGENTS.md`,
    suppression de `docs/plans/remove-list-defaultvalue-compat.md` et
    `docs/plans/remove-option-map-default-compat.md`, `make all-tests`.

- `local` - Retirer les formes de compatibilite a valeur factice pour
  `Option`/`Map`: `Option.none[T](default)`, `optionNone[T](default)` et
  `Map.getOption(default, key)`. Les appels positifs utilisent desormais
  `Option.none[T]()` et `Map.getOption(key)`, tandis que les anciens chemins ont
  des diagnostics dedies.
  - Fichiers / tests associes: `stdlib/core/option.nabla`,
    `stdlib/collections/map.nabla`, `tests/test_error_option_none_*`,
    `tests/test_error_map_getoption_default_compat_removed.nabla`,
    `docs/stdlib-api.md`, `docs/language.md`, `make all-tests`.

- `local` - Retirer les surcharges de compatibilite `defaultValue` de
  `collections.list`: `List.empty`, `List.map`, `List.filter`, `List.fromArray`
  et leurs helpers internes utilisent desormais seulement la forme Scala-like
  sans valeur factice, rendue possible par `Nil: List[Nothing]`. Les anciens
  appels avec argument ignore sont couverts par des diagnostics d'arite exacts.
  - Fichiers / tests associes: `stdlib/collections/list.nabla`,
    `tests/test_stdlib_list_operations.nabla`,
    `tests/test_stdlib_list_from_array.nabla`,
    `tests/test_error_list_*defaultvalue_compat_removed.nabla`,
    `docs/stdlib-api.md`, `docs/stdlib/collections/list.html`,
    `make all-tests`.

- `local` - Remplacer la representation temporaire de la liste vide par le
  singleton runtime Scala-like `object Nil with List[Nothing]`: `Nil` est une
  valeur `AnyRef`, assignable a `List[T]` via `Nothing`, et les fabriques
  recommandees n'ont plus besoin de `defaultValue` (`List.empty[T]()`,
  `List.map(values, f)`, `List.filter(values, predicate)`,
  `List.fromArray(values)`). L'assignabilite accepte maintenant les arguments
  generiques compatibles par bottom type (`List[Nothing]` vers `List[Int]`) sans
  ouvrir la variance generique globale.
  - Fichiers / tests associes: `src/compiler_context.hpp`,
    `src/semantic_analyzer.cpp`, `stdlib/collections/list.nabla`,
    `tests/test_stdlib_list_nil_singleton_basic.nabla`,
    `tests/test_stdlib_list_nil_singleton_factories.nabla`,
    `tests/test_stdlib_list_nil_singleton_runtime_object.nabla`,
    `tests/test_nil_does_not_break_subtype_overload.nabla`,
    `tests/test_error_nil_no_global_generic_covariance.nabla`,
    `examples/stdlib_list_cookbook.nabla`, `docs/stdlib-api.md`,
    `docs/stdlib/collections/list.html`, `make all-tests`.

- `local` - Enrichir `collections.list` avec une couche de methodes inspirees
  de Scala `List`: `prepended`, `appended`, `concat` (spelling Nabla de `:::`),
  `reverse`, `reverseConcat` (equivalent de `reverse_:::`), `take`, `drop` et
  `slice`. Ce jalon avait d'abord utilise une representation temporaire pour la
  liste vide; elle est maintenant remplacee par le jalon `Nil` singleton
  ci-dessus, et les details historiques vivent dans l'historique des PRs plutôt
  que dans `docs/plans/`.
  - Fichiers / tests associes: `stdlib/collections/list.nabla`,
    `tests/test_stdlib_list_scala_concat_reverse.nabla`,
    `tests/test_stdlib_list_scala_take_drop_slice.nabla`,
    `tests/test_stdlib_list_scala_edge_cases.nabla`,
    `examples/stdlib_list_cookbook.nabla`, `docs/stdlib-api.md`,
    `docs/stdlib/collections/list.html`, `make all-tests`, `make examples`,
    `make tooling-tests`, `make stdlib-docs`.

- `local` - Ajouter le type bottom `Nothing`: le parser et l'analyse semantique
  le reconnaissent comme type builtin, l'assignabilite l'accepte vers tout type
  attendu, les expressions `if` prennent le type de la branche normale quand
  l'autre branche est `Nothing`, et les primitives `panic(message)` / `error(message)`
  s'abaissent vers `Runtime_panic` (exit 250). Tests positifs pour retour et
  branche `if`, plus diagnostic negatif pour tentative de construire un
  `Nothing` depuis `Int`.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/parser.cpp`,
    `src/ast.cpp`, `src/ir_codegen.cpp`, `src/runtime_asm.cpp`,
    `tests/test_nothing_panic_return.nabla`,
    `tests/test_nothing_if_branch.nabla`,
    `tests/test_error_nothing_value.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/stdlib-api.md`, `make all-tests`.

- `local` - Finaliser le passage du dispatch dynamique vers des vtables backend:
  le slot 0 des objets utilisateur et singletons runtime pointe vers une table
  de fonctions, les slots sont indexés par propriétaire statique + méthode
  résolue pour éviter les collisions d'overloads, et les entrées couvrent
  parents génériques instanciés, méthodes génériques spécialisées, defaults de
  traits, `Sized`, `Iterable[T]`, `Any.toString` / `Any.hashCode` /
  `Any.equals`, ainsi que les facades `ArrayObject[Char]` et
  `ArrayObject[String]` retournées par `String.toCharArray` / `String.split`.
  - Fichiers / tests associes: `src/ir.hpp`, `src/ir.cpp`,
    `src/ir_codegen.cpp`, `src/runtime_asm.hpp`, `src/runtime_asm.cpp`,
    `tests/test_inheritance_dynamic_dispatch_generic_parent.nabla`,
    `tests/test_inheritance_dynamic_dispatch_generic_method.nabla`,
    `tests/test_trait_default_overload_no_conflict.nabla`,
    `tests/test_stdlib_iterable_trait_arrays.nabla`,
    `tests/test_stdlib_iterable_trait_set_map.nabla`,
    `tests/test_stdlib_list_iterable_trait.nabla`,
    `tests/test_string_to_char_array.nabla`, `tests/test_string_split.nabla`,
    `make all-tests`, `make examples`.

- `local` - Ajouter une syntaxe de `def` curryfiee V0 pour une liste de
  parametres supplementaire: `def sum(f)(a, b): Int = ...` s'abaisse vers une
  fonction retournant une closure de type `Fn(...)->...`, appelable directement
  avec `sum(...)(...)`; les methodes curryfiees restent rejetees en V0.
  - Fichiers / tests associes: `src/parser.cpp`,
    `tests/test_curried_function_def.nabla`,
    `tests/test_error_curried_method.nabla`, `AGENTS.md`.

- `local` - Stabiliser les fins de ligne du depot avec une regle
  `.gitattributes` globale en LF, en gardant des exceptions CRLF pour les
  scripts Windows natifs futurs.
  - Fichiers / tests associes: `.gitattributes`, `AGENTS.md`,
    `git add --renormalize .`.

- `local` - Ajouter les appels postfixes directs sur les expressions de type
  fonction: une fonction retournant `Fn(...)` peut etre appelee immediatement
  avec une seconde liste d'arguments, sans passer par une variable intermediaire.
  - Fichiers / tests associes: `src/parser.cpp`,
    `tests/test_function_returning_function_direct_call.nabla`.

- `local` - Autoriser les fonctions locales `def` non generiques dans les
  contextes generiques des fonctions, methodes, classes et traits englobants:
  les helpers conservent les limites V1 (pas de capture implicite, pas de
  surcharge locale, pas de type parameter propre), mais leurs signatures/corps
  peuvent mentionner les paramètres de type environnants et les appels directs
  s'inferent/se specialisent normalement.
  - Fichiers / tests associes: `src/parser.cpp`,
    `tests/test_local_def_generic_*_context.nabla`, anciens probes
    `test_error_local_def_generic_*_context` promus positifs,
    `docs/internals.md`, `docs/roadmap.md`, `AGENTS.md`.

- `local` - Aligner `docs/internals.md` avec les conventions livrees pour
  proprietes `def` zero-argument et fonctions locales de bloc: les proprietes
  restent des fonctions/methodes zero-argument sans memoisation, et les helpers
  locaux V0 s'abaissent en fonctions cachees avec limites explicites sur
  captures/generiques/overloads.
  - Fichiers / tests associes: `docs/internals.md`, `docs/roadmap.md`,
    `AGENTS.md`.

- `local` - Nettoyer `docs/plans/` apres les jalons post-0.1 deja livres:
  les plans historiques `local-def-lambda-lowering`,
  `parameterless-def-properties`, `runtime-singleton-object` et
  `scala-like-list` ont ete retires de l'arbre actif. Le cleanup courant retire
  aussi les plans termines `scala-inspired-list-methods` et
  `nil-singleton-list-nothing`; `docs/plans/README.md` rappelle que les prochains
  chantiers doivent creer un plan court centre sur le delta restant.
  - Fichiers / tests associes: `docs/plans/README.md`, suppressions sous
    `docs/plans/`, `AGENTS.md`, `docs/roadmap.md`.

- `local` - Retirer le plan actif termine `docs/plans/local-def-generic-methods.md`
  apres livraison des fonctions locales dans les contextes generiques. Le
  perimetre livre reste documente dans ce journal, `docs/roadmap.md`,
  `docs/internals.md`, les tests et l'historique de PR.
  - Fichiers / tests associes: suppression de
    `docs/plans/local-def-generic-methods.md`, `AGENTS.md`, `git diff --check`.

- `local` - Utiliser les fonctions locales pour cacher une premiere tranche de
  helpers stdlib non documentes: `util.randomStep` devient un detail local de
  `randomInt`, et la conversion `Bool` -> `String` de `collections.bool_array`
  est definie au plus pres de `boolArrayMkString` / `arrayBool` au lieu de
  rester un symbole module-level.
  - Fichiers / tests associes: `stdlib/util.nabla`,
    `stdlib/collections/bool_array.nabla`, `AGENTS.md`, `make nablac`, tests
    cibles util/bool.

- `local` - Ajouter les fonctions locales `def` avec parametres dans les blocs:
  les helpers sont enregistres comme fonctions cachees non surchargees,
  appelables directement, recursifs sur eux-memes, capables d'appeler les
  helpers locaux declares plus haut et referencables comme valeurs fonction via
  l'infrastructure existante de `Fn`. Les captures, surcharges locales,
  fonctions locales generiques et contextes generiques sont rejetes explicitement
  en V0.
  - Fichiers / tests associes: `src/parser.cpp`, `src/parser.hpp`,
    `src/ast.cpp`, `src/ast.hpp`, `tests/test_local_def_*.nabla`,
    `tests/test_error_local_def_*.nabla`, `docs/roadmap.md`, `AGENTS.md`.
  - Validation: `make all-tests`.

- `local` - Ajouter les proprietes `def` sans parametres: le parseur accepte
  `def name: T = expr`, `def name: T = { ... }` et les signatures abstraites
  `def name: T`; la resolution abaisse les usages nus globaux,
  `Object.member` pour les namespaces statiques, et `receiver.name` vers les
  appels zero-argument existants, tout en conservant `receiver.name()` et en
  detectant les doublons avec `def name(): T`.
  - Fichiers / tests associes: `src/parser.cpp`, `src/ast.cpp`,
    `tests/test_parameterless_def_*.nabla`,
    `tests/test_error_parameterless_def_duplicate_paren_method.nabla`,
    `docs/roadmap.md`, `make nablac`.

- `local` - Recommander `Array.range(start, until)` comme surcharge publique
  de plage bornee: `Array.rangeUntil(start, until)` reste un alias de
  compatibilite, les diagnostics legacy `ArrayRangeUntil` / `arrayIntRangeUntil`
  pointent vers `Array.range`, et les exemples/tests ordinaires migrent vers la
  surcharge recommandee.
  - Fichiers / tests associes: `stdlib/collections/array.nabla`,
    `src/compiler_context.hpp`, `tests/test_stdlib_array_int_range_overload.nabla`,
    `tests/test_error_legacy_array_range_until_suggestion.diagnostic`,
    `examples/euler5_fold.nabla`, `examples/euler6.nabla`, `docs/language.md`,
    `docs/stdlib-api.md`, `docs/stdlib/`, `AGENTS.md`.
  - Validation: `make all-tests`, `make examples`, `make tooling-tests`,
    `make stdlib-docs`, `g++ -std=c++17 -Wall -Wextra -Werror ...`,
    `git diff --check`.

- `local` - Migrer `examples/game_of_life.nabla` vers la facade publique
  `collections.array`: signatures `Array[Int]`, allocations
  `Array.fill[Int]` et somme via methode publique, sans changer le comportement
  interactif compile par `make examples`.
  - Fichiers associés: `examples/game_of_life.nabla`, `docs/roadmap.md`,
    `AGENTS.md`.
  - Validation: `make examples-quick`.

- `local` - Faire accepter `Any` par `print(value)` et abaisser l'argument vers
  `Any.toString()` avant l'appel runtime, afin que primitives, chaînes, valeurs
  typées `Any` et classes utilisateur avec override `toString()` s'impriment sans
  conversion explicite. Le helper stdlib `io.println` suit la même signature
  `println(value: Any)`.
  - Fichiers associés: `src/ast.cpp`, `stdlib/io.nabla`,
    `tests/test_print_any_tostring.nabla`,
    `tests/test_stdlib_io_println_any.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/roadmap.md`,
    `docs/stdlib/`, `AGENTS.md`.

- `local` - Refaire la reference HTML de la stdlib dans un style proche Scala:
  chemins CSS corriges pour les modules racine, sidebar par types/fabriques/
  methodes, signatures ancrees, support `trait`/`object` documentes, blocs
  `@example ... @end`, exemples sur `Array`, `Option`, `Map`, `Set` et `Math`,
  et test d'outillage contre liens casses, IDs dupliques et pages sans layout
  API.
  - Fichiers associés: `tools/generate_stdlib_docs.py`,
    `tests/test_stdlib_docs_html.py`, `Makefile`, `stdlib/collections/array.nabla`,
    `stdlib/collections/map.nabla`, `stdlib/collections/set.nabla`,
    `stdlib/core/option.nabla`, `stdlib/math.nabla`, `docs/stdlib/`,
    `docs/stdlib-api.md`,
    `docs/roadmap.md`, `AGENTS.md`.

- `local` - Rendre le heap runtime configurable sans modifier l'allocateur:
  `nablac --heap-size <octets>` conserve 8 MiB par défaut, rejette les valeurs
  invalides ou inférieures à 4096 octets, et propage la capacité choisie dans
  `heap_capacity` du runtime assembleur généré.
  - Fichiers associés: `src/main.cpp`, `src/runtime_asm.cpp`,
    `src/runtime_asm.hpp`, `src/ir_codegen.cpp`, `src/ir_codegen.hpp`,
    `tests/test_configurable_heap_size.nabla`,
    `tests/test_configurable_heap_size.expected`,
    `tests/test_configurable_heap_size.sh`, `Makefile`,
    `docs/internals.md`,
    `docs/language.md`, `docs/roadmap.md`, `README.md`, `AGENTS.md`.

- `local` - Étendre le hardening post-0.1 aux collections parent-typées:
  `Array[Parent]` et `Set.fromArray[Parent]` ont maintenant une régression
  dédiée prouvant que des instances enfant conservent `toString`, `hashCode` et
  `equals` dynamiques après passage par les APIs génériques de collections.
  - Fichiers associés: `tests/test_inheritance_parent_collections_dispatch.nabla`,
    `tests/test_inheritance_parent_collections_dispatch.expected`,
    `docs/roadmap.md`,
    `AGENTS.md`.

- `local` - Démarrer le hardening post-0.1 de l'héritage/runtime: les
  hiérarchies composées rejettent maintenant un nom de membre visible à la fois
  comme champ et comme méthode, un test positif couvre champs hérités + dispatch
  virtuel via type parent, et `docs/internals.md` précise les limites actuelles
  du dispatch, de `super` et des pseudo-vtables.
  - Fichiers associés: `src/semantic_analyzer.cpp`,
    `tests/test_error_inheritance_field_method_conflict.nabla`,
    `tests/test_error_inheritance_field_method_conflict.diagnostic`,
    `tests/test_error_inheritance_generic_field_method_conflict.nabla`,
    `tests/test_error_inheritance_generic_field_method_conflict.diagnostic`,
    `tests/test_error_inheritance_own_method_field_conflict.nabla`,
    `tests/test_error_inheritance_own_method_field_conflict.diagnostic`,
    `tests/test_inheritance_field_dispatch_mix.nabla`,
    `tests/test_inheritance_field_dispatch_mix.expected`,
    `docs/internals.md`,
    `docs/roadmap.md`, `AGENTS.md`.
- `local` - Figer puis publier le jalon 0.1 sans changement de comportement:
  `docs/releases/0.1.md` conserve maintenant les notes de release, la matrice
  de validation 0.1.x et la politique post-0.1. Le plan temporaire de
  préparation du tag a été retiré après publication.
  - Fichiers associes: `docs/releases/0.1.md`, `docs/roadmap.md`, `AGENTS.md`.
- `local` - Durcir le dispatch racine de `String` effacé vers `Any`: les objets
  `String` portent maintenant un tag runtime dédié, les littéraux sont alignés
  sur 8 octets, et `Any.toString` / `Any.hashCode` / `Any.equals` reconnaissent
  les chaînes pour conserver le rendu, le hash et l'égalité par contenu.
  - Fichiers / tests associes: `src/runtime_values.hpp`,
    `src/runtime_asm.cpp`, `src/ir_codegen.cpp`,
    `tests/test_any_string_dispatch.nabla`, `docs/internals.md`,
    `AGENTS.md`.
- `local` - Ajouter deux cookbooks stdlib publics non interactifs pour les
  collections et le traitement de texte, avec oracles de code de sortie et
  stdout deterministe, plus une courte page `docs/cookbook.md`.
  - Fichiers / tests associes:
    `examples/stdlib_collections_cookbook.nabla`,
    `examples/stdlib_collections_cookbook.expected`,
    `examples/stdlib_collections_cookbook.stdout`,
    `examples/stdlib_text_cookbook.nabla`,
    `examples/stdlib_text_cookbook.expected`,
    `examples/stdlib_text_cookbook.stdout`, `docs/cookbook.md`,
    `docs/language.md`, `docs/roadmap.md`, `AGENTS.md`, `make examples`.
- `local` - Nettoyer la documentation publique stdlib/utilisateur apres
  l'ajout de `Sized`: `docs/stdlib-api.md`, `docs/language.md` et
  `docs/roadmap.md` presentent maintenant `Array[T]`, `Option[T]`, `Set[T]`,
  `Map[K, V]` et `Sized` comme surface recommandee; les facades specialisees,
  `ArrayObject[T]` et les tableaux bruts sont decrits comme compatibilite ou
  details de representation. Les commentaires `///` de `Array`, `Set`, `Map`
  et `Sized` ont ete ajustes pour regenerer la reference HTML sans changer le
  comportement.
  - Fichiers / tests associes: `docs/stdlib-api.md`, `docs/language.md`,
    `docs/roadmap.md`, `stdlib/collections/array.nabla`,
    `stdlib/collections/set.nabla`, `stdlib/collections/map.nabla`,
    `stdlib/core/sized.nabla`, `docs/stdlib/`, `AGENTS.md`,
    `make stdlib-docs`.
- `local` - Ajouter le trait stdlib public `Sized` et l'appliquer strictement a
  `ArrayObject[T]`, `Set[T]` et `Map[K, V]`, avec tests TDD de dispatch via le
  type du trait.
  - Fichiers / tests associes: `stdlib/core/sized.nabla`,
    `stdlib/collections/object_array.nabla`, `stdlib/collections/set.nabla`,
    `stdlib/collections/map.nabla`,
    `tests/test_stdlib_sized_trait_collections.nabla`,
    `tests/test_stdlib_sized_trait_array_object.nabla`, `docs/stdlib-api.md`,
    `docs/roadmap.md`, `AGENTS.md`.
- `local` - Ajouter les traits riches V1 au langage, avec tests TDD couvrant les
  signatures abstraites, methodes par defaut, composition `with`, interdiction
  de l'instanciation/champs/constructeurs/`super`, obligation d'implementation
  et conflits de defaults.
  - Fichiers / tests associés: `src/lexer.hpp`, `src/parser.cpp`,
    `src/parser.hpp`, `src/compiler_context.hpp`, `src/semantic_analyzer.cpp`,
    `src/ast.cpp`, `src/ir_codegen.cpp`, `tests/test_trait_*.nabla`,
    `tests/test_error_trait_*.nabla`, `docs/language.md`, `docs/roadmap.md`,
    `AGENTS.md`.
- `local` - Autoriser les classes generiques comme implementors de traits non
  generiques, en supprimant le diagnostic temporaire V1 et en couvrant le
  dispatch abstrait et les methodes par defaut sur type trait.
  - Fichiers / tests associes: `src/semantic_analyzer.cpp`,
    `tests/test_trait_generic_class_abstract.nabla`,
    `tests/test_trait_generic_class_default_dispatch.nabla`,
    `docs/language.md`, `docs/roadmap.md`, `AGENTS.md`.
- `local` - Ajouter la creation naturelle `Option.none[T]()` et la methode
  `Map.getOption(key)` sans valeur de secours, tout en conservant les anciennes
  formes avec `default` par compatibilite.
  - Fichiers / tests associés: `stdlib/core/option.nabla`,
    `stdlib/collections/map.nabla`, `tests/test_stdlib_option_none_empty.nabla`,
    `tests/test_stdlib_map_get_option_empty.nabla`, `examples/student_scores.nabla`,
    `docs/language.md`, `docs/stdlib-api.md`, `docs/stdlib/`, `AGENTS.md`.
- `local` - Ajouter les méthodes publiques de confort `Map.contains`,
  `Map.foreachEntry`, `Map.mapValues[U]` et `Map.filterKeys`, avec
  classification dans `docs/stdlib-api.md`, test dédié et documentation HTML
  régénérée.
  - Fichiers / tests associés: `stdlib/collections/map.nabla`,
    `tests/test_stdlib_map_comfort_methods.nabla`, `docs/stdlib-api.md`,
    `docs/stdlib/collections/map.html`, `AGENTS.md`.
- `local` - Résoudre la limitation du compilateur lors de la génération de code pour les appels de méthode sur des paramètres de type génériques (comme `K.toString()` générant des références non résolues comme `K.toString` dans l'assembleur) en les redirigeant de manière dynamique vers `Any.toString()`, et implémenter `Map.toString` et `Map.mkString` dans le module `collections.map` de la bibliothèque standard.
  - Fichiers / tests associés: `src/ir.hpp`, `src/ir.cpp`, `src/ast.cpp`, `stdlib/collections/map.nabla`, `tests/test_tuple2_generic_tostring.nabla`, `tests/test_stdlib_map_tostring.nabla`, `docs/stdlib/collections/map.html`, `AGENTS.md`.
- `local` - Configurer l'environnement de développement WSL 2 sous Windows avec installation d'Ubuntu et des dépendances (`g++`, `nasm`, `make`, `dos2unix`). Ajouter `.gitattributes` pour imposer les sauts de ligne `LF` sur les fichiers du projet et résoudre les écarts Windows/Linux pour la suite de tests.
  - Fichiers associés: `.gitattributes`, `docs/stdlib/`, `AGENTS.md`.
- `local` - Nettoyer la surface documentaire de `io` et `strings`: les pages
  HTML evitent maintenant les marqueurs Markdown bruts, les conventions de
  retour I/O affichent directement `Bool`, `Int`, `true`, `false` et `-1`, et
  `strings.words` decrit son decoupage sur espaces sans syntaxe de rendu brute.
  - Fichiers / tests associes: `stdlib/io.nabla`, `stdlib/strings.nabla`,
    `docs/stdlib/io.html`, `docs/stdlib/strings.html`, `AGENTS.md`,
    `make stdlib-docs`.
- `local` - Nettoyer la surface documentaire de `core.option` et
  `core.option_int`: les pages HTML evitent maintenant les marqueurs Markdown
  bruts, `optionSome` pointe explicitement vers `Option.some`, `Option[T]`
  liste ses methodes principales et
  `OptionInt` est presente comme specialisation utile plutot que choix par
  defaut.
  - Fichiers / tests associes: `stdlib/core/option.nabla`,
    `stdlib/core/option_int.nabla`, `docs/stdlib/core/option.html`,
    `docs/stdlib/core/option_int.html`, `AGENTS.md`, `make stdlib-docs`.
- `local` - Nettoyer la surface documentaire de `collections.array`: la page
  HTML evite maintenant les marqueurs Markdown bruts, `ArrayFill` et
  `ArrayRange` pointent explicitement vers `Array.fill` et `Array.range`, et la
  description de `Array[T]` clarifie que le compilateur choisit la
  representation adaptee.
  - Fichiers / tests associes: `stdlib/collections/array.nabla`,
    `docs/stdlib/collections/array.html`, `AGENTS.md`, `make stdlib-docs`.
- `local` - Nettoyer la surface documentaire de `collections.set`: la page
  HTML evite maintenant les marqueurs Markdown bruts, les alias
  `SetFromArray` / `SetEmpty` pointent explicitement vers `Set.fromArray` /
  `Set.empty`, et `Set[T]` documente la convention `==` / `hashCode()` utilisee
  pour la deduplication.
  - Fichiers / tests associes: `stdlib/collections/set.nabla`,
    `docs/stdlib/collections/set.html`, `AGENTS.md`, `make stdlib-docs`.
- `local` - Nettoyer la surface documentaire de `stdlib/util.nabla`: les types
  de resultat et les fonctions recommandees sont maintenant publies dans la
  reference HTML avec `@status`, tandis que la primitive de pas pseudo-aleatoire
  est cachee comme fonction locale de `randomInt` et que
  `randomSeedTime` / `randomIntInRange` sont classes en compatibilite.
  - Fichiers / tests associes: `stdlib/util.nabla`, `docs/stdlib-api.md`,
    `docs/stdlib/`, `AGENTS.md`, `make stdlib-docs`,
    `make all-tests`.
- `local` - Formaliser les conventions internes des types deja stabilises:
  `Int`/`Long` immediats, `Bool` tagge, `Char` ASCII, `String` byte-based,
  tableaux natifs specialises, types fonction canoniques et types de classes
  nominaux avec layouts herites. La specification vivante explicite maintenant
  les frontieres source/IR/runtime que la suite existante couvre deja.
  - Fichiers / tests associes: `docs/internals.md`, `AGENTS.md`,
    `make all-tests`.
- `local` - Nettoyer la premiere couche Collections/Core dans les usages
  ordinaires: les tests et exemples migrent de `arrayIntRange`,
  `array<Type>Fill`, des anciens helpers `Option` et de `SetFromArray` /
  `SetEmpty` vers `Array.range`, `Array.fill[T]`, `Option.some` /
  `Option.none`, `Set.fromArray` et `Set.empty`. Les tests `*_alias*` et
  `test_error_legacy_*` restent en place pour couvrir les chemins de
  compatibilite encore supportes ou les diagnostics de migration. `Option[T]`
  evite aussi ses wrappers de compat dans ses methodes
  internes, et `Array.range` / `Array.rangeUntil` descendent directement vers les
  helpers bruts internes.
  - Fichiers / tests associes: `stdlib/collections/array.nabla`,
    `stdlib/core/option.nabla`, tests collections/options/set mis a jour,
    diagnostics de colonnes associes, `make all-tests`.
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
- `local` - Ajouter les annotations de type locales pour `val` et `var`: la
  syntaxe `val name: Type = expr` / `var name: Type = expr` valide
  l'assignabilite de l'initialiseur, donne un type attendu aux lambdas inferees
  et permet maintenant `val f: (Float) => Float = sqrt` pour choisir une
  surcharge par signature.
  - Limites actuelles: les references surchargees sans type attendu restent
    refusees; les fonctions generiques surchargees et les methodes surchargees
    restent a definir.
  - Fichiers / tests associes: `src/parser.cpp`, `src/ast.hpp`,
    `src/ast.cpp`, `tests/test_local_type_annotation_function_reference.nabla`,
    `tests/test_local_type_annotation_var.nabla`,
    `tests/test_error_local_type_annotation_mismatch.nabla`,
    `docs/language.md`, `docs/roadmap.md`, `make all-tests`.
- `local` - Migrer une premiere surface `math` vers les noms idiomatiques
  surcharges: `abs`, `min` et `max` couvrent maintenant `Int`, `Long`, `Float`
  et `Double`, en plus de `sqrt(Float)` / `sqrt(Double)`. Les noms suffixes
  restent publics pour compatibilite, et le backend protege le symbole ASM
  reserve `abs` avec un nom assembleur sûr.
  - Fichiers / tests associes: `stdlib/math.nabla`, `src/ir_codegen.cpp`,
    `tests/test_stdlib_math.nabla`, `docs/stdlib-api.md`, `make stdlib-docs`,
    `make all-tests`.
- `local` - Completer la surface mathematique surchargee avec `clamp` pour
  `Int`, `Long`, `Float`, `Double` et `pow` pour `Int`, `Float`, `Double`.
  Les wrappers deleguent aux noms suffixes existants et la reference HTML stdlib
  expose ces signatures comme API recommandee.
  - Fichiers / tests associes: `stdlib/math.nabla`,
    `tests/test_stdlib_math.nabla`, `docs/stdlib-api.md`, `docs/stdlib/`,
    `make stdlib-docs`, `make all-tests`.
- `local` - Enrichir les diagnostics de surcharge: les appels sans candidat
  indiquent maintenant la forme appelee (`foo(Int, String)`) et les signatures
  candidates; les references surchargees avec type attendu incompatible
  affichent aussi les candidats disponibles.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/ast.cpp`,
    `src/parser.cpp`, `tests/test_error_function_overload_no_match.nabla`,
    `tests/test_error_function_overload_arity.nabla`,
    `tests/test_error_function_overload_reference_expected_mismatch.nabla`,
    `make all-tests`.
- `local` - Migrer les helpers internes `math` vers la surface surchargee:
  `hypotenuse*`, `gcd*`, `lcm*` et `absDiff*` utilisent maintenant `sqrt(...)`
  ou `abs(...)` quand ils ne sont pas eux-memes des wrappers de compatibilite.
  Le test stdlib central couvre aussi `sqrt(Float)` et `sqrt(Double)` via le nom
  surcharge.
  - Fichiers / tests associes: `stdlib/math.nabla`,
    `tests/test_stdlib_math.nabla`, `make all-tests`, `make stdlib-docs`.
- `local` - Ajouter la surcharge V1 des methodes de classe par signature exacte:
  les classes gardent un index de surcharges par nom source, les variantes sont
  abaissees vers des noms IR uniques, les appels choisissent la variante depuis
  les types d'arguments, et les diagnostics listent les signatures candidates.
  - Limites actuelles: resolution exacte uniquement; les lambdas en argument
    d'une methode surchargee ne recoivent un type attendu que si la forme
    d'appel selectionne une surcharge unique; generiques et conversions
    implicites restent a cadrer.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/parser.cpp`,
    `src/ast.hpp`, `src/ast.cpp`, `src/semantic_analyzer.cpp`,
    `tests/test_method_overload.nabla`,
    `tests/test_error_method_overload_duplicate.nabla`,
    `tests/test_error_method_overload_no_match.nabla`, `make all-tests`.
- `local` - Typer les lambdas inferees dans les appels de methodes surchargees:
  le parseur inspecte la forme d'appel, l'arite et les positions de lambdas pour
  fournir un type attendu lorsqu'une seule surcharge reste compatible, par
  exemple `runner.run(value => { value + 1 })`.
  - Fichiers / tests associes: `src/parser.hpp`, `src/parser.cpp`,
    `tests/test_method_overload_inferred_lambda.nabla`, `make all-tests`.
- `local` - Diagnostiquer explicitement les lambdas inferees ambigues dans les
  appels de methodes surchargees: quand plusieurs candidats fonctionnels restent
  possibles, le parseur indique l'ambiguite et liste les signatures candidates.
  - Fichiers / tests associes: `src/parser.cpp`,
    `tests/test_error_method_overload_inferred_lambda_ambiguous.nabla`,
    `make all-tests`.
- `local` - Etendre la resolution des surcharges globales aux fonctions
  generiques inferables: une variante concrete exacte gagne sur une variante
  generique compatible, une variante generique seule reste appelable, et
  plusieurs variantes generiques compatibles produisent un diagnostic
  d'ambiguite avec les candidats.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/ast.cpp`,
    `tests/test_function_overload_generic_priority.nabla`,
    `tests/test_error_function_overload_generic_ambiguous.nabla`,
    `make all-tests`.
- `local` - Inferer les references de fonctions generiques surchargees depuis
  le type fonction attendu: une reference concrete exacte reste prioritaire, une
  reference generique compatible recoit ses arguments de type inferes, et
  plusieurs generiques compatibles produisent un diagnostic d'ambiguite avec les
  candidats.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/parser.cpp`,
    `tests/test_function_overload_generic_reference.nabla`,
    `tests/test_error_function_overload_generic_reference_ambiguous.nabla`,
    `make all-tests`.
- `local` - Etendre la resolution des surcharges de methodes aux signatures
  generiques inferables: une methode concrete exacte gagne sur une methode
  generique compatible, une methode generique seule reste appelable, et plusieurs
  methodes generiques compatibles produisent un diagnostic d'ambiguite avec les
  candidats.
  - Fichiers / tests associes: `src/compiler_context.hpp`, `src/ast.cpp`,
    `tests/test_method_overload_generic_priority.nabla`,
    `tests/test_error_method_overload_generic_ambiguous.nabla`,
    `make all-tests`.
- `local` - Utiliser la surcharge selectionnee par la forme des lambdas pour
  parser les arguments de methodes surchargees avec `parseFunctionCallArguments`:
  les arguments precedents peuvent inferer les parametres de type d'une methode
  generique avant de fournir le type attendu a une lambda suivante, et plusieurs
  variantes generiques compatibles restent diagnostiquees comme ambigues.
  - Fichiers / tests associes: `src/parser.hpp`, `src/parser.cpp`,
    `tests/test_method_overload_generic_inferred_lambda.nabla`,
    `tests/test_error_method_overload_generic_inferred_lambda_ambiguous.nabla`,
    `make all-tests`.
- `local` - Nettoyer l'API publique de `stdlib/math.nabla` en supprimant les
  wrappers suffixes des familles deja surchargees (`abs*`, `min*`, `max*`,
  `clamp*`, `pow*`, `sqrt*`, puis `sign*`, `isEven*`, `isOdd*`, `isBetween*`,
  `gcd*`, `lcm*`, `absDiff*`, `isClose*`, `degreesToRadians*`,
  `radiansToDegrees*`, `hypotenuse*` et `factorialInt`): les implementations
  vivent directement sous les noms idiomatiques surcharges, les tests math
  utilisent ces noms, et la doc API note le retrait des anciens wrappers publics.
  - Fichiers / tests associes: `stdlib/math.nabla`,
    `tests/test_stdlib_math.nabla`, `docs/stdlib-api.md`, `make all-tests`.
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
- `local` - Etendre les suggestions de diagnostics stdlib vers les compagnons
  publics pour les formes compatibilite manquantes `OptionSome`, `OptionNone`
  et `ArrayRangeUntil`, avec tests d'erreur dédiés.
  - Fichiers / tests associés: `src/compiler_context.hpp`,
    `tests/test_error_legacy_option_some_suggestion.nabla`,
    `tests/test_error_legacy_option_none_suggestion.nabla`,
    `tests/test_error_legacy_array_range_until_suggestion.nabla`,
    `docs/roadmap.md`, `make all-tests`.
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
  idiomatique: `Option.some[T](value)` et la forme historique
  `Option.none[T](default)`, avec `optionSome` / `optionNone` comme
  compatibilite initiale. Les formes d'absence avec valeur factice, y compris
  `optionNone`, ont ensuite ete retirees au profit de `Option.none[T]()`.
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
- `local` - Ajouter les singletons runtime V0: `object Name with Trait { ... }`
  produit une valeur singleton stable assignable aux traits, a `AnyRef` et a
  `Any`, tout en conservant les `object` sans `with` comme namespaces statiques.
  Les validations class-like couvrent abstraits manquants, `override` sans
  methode hereditee, signature incompatible et conflits de defaults.
  - Fichiers / tests associes: `src/parser.cpp`, `src/ast.cpp`, `src/ast.hpp`,
    `src/compiler_context.hpp`, `src/semantic_analyzer.cpp`, `src/ir.cpp`,
    `src/ir.hpp`, `src/ir_codegen.cpp`,
    `tests/test_runtime_object_*.nabla`,
    `tests/test_error_runtime_object_*.nabla`,
    `tests/test_error_static_object_as_value.nabla`, `docs/language.md`,
    `docs/internals.md`, `docs/roadmap.md`, `AGENTS.md`.
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

Le tag `v0.1.0` existe déjà; poursuivre en phase post-release avec des
changements étroits qui clarifient le comportement observable sans élargir trop
vite la surface publique :

- consolider les limites runtime documentées (`--heap-size`, erreurs runtime,
  dispatch via `Any`, layout objets/tableaux) avec tests de régression;
- continuer le durcissement héritage/dispatch et les règles d'égalité/hash dans
  les hiérarchies complexes;
- améliorer les diagnostics et la documentation utilisateur avant de créer de
  nouvelles abstractions comme `Result[T]`, variance avancée ou GC;
- conserver les helpers internes spécialisés de collections tant qu'ils
  expriment des contraintes runtime/IR réelles;
- demarrer chaque nouvelle feature par la checklist `docs/feature-integration.md`
  pour eviter les plans historiques ressuscites, les branches locales obsoletes
  et les PRs redondantes.
