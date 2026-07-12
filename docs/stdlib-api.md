# Nabla Standard Library API Surface

Ce document classe la bibliotheque standard en surface publique, compatibilite
temporaire et details internes. Il sert de garde-fou pendant la stabilisation de
Nabla : les exemples utilisateur et la documentation principale doivent viser la
surface publique, pas les helpers historiques.

## Regle Generale

- **Public** : symbole recommande dans le code utilisateur et dans les exemples.
- **Compat temporaire** : symbole accepte pour ne pas casser les tests/exemples
  existants, mais a migrer vers une forme publique plus simple.
- **Interne** : implementation de stdlib ou convention compilateur/runtime. Ces
  symboles ne doivent pas etre presentes comme API idiomatique.

La reference HTML generee par `make stdlib-docs` reste volontairement opt-in :
seules les declarations precedees de commentaires `///` sont publiees. Les
helpers sans commentaire public doivent rester absents de `docs/stdlib/`. Les
commentaires publics peuvent inclure des blocs `@example ... @end` ; ces blocs
sont rendus comme exemples de code sur la page du module ou du symbole.

Quand une signature publique mentionne `Array[T]`, l'implementation peut encore
utiliser une facade specialisee (`ArrayInt`, `ArrayObject[T]`, etc.) en interne.
La documentation utilisateur doit presenter `Array[T]` comme contrat observable
et reserver ces noms aux sections internes ou de compatibilite.

## Surface Publique Recommandee

### Core

- `panic(message: String): Nothing` / `error(message: String): Nothing`
  - primitives compilateur pour les chemins qui ne retournent pas normalement;
    elles quittent l'executable avec le statut runtime `250`.
- `Option[T]`
  - `Option.some[T](value)`
  - `Option.none[T]()`
  - `isDefined()`
  - `isEmpty()`
  - `getOrElse(default)`
  - `map(...)`
  - `flatMap(...)`
  - `filter(...)`
  - `foreach(...)`
  - `orElse(...)`
- `OptionInt` reste public tant que les generiques primitifs ne couvrent pas
  totalement les besoins de performance/representation.

### Collections

- `Sized`
  - `size()`
  - `isEmpty()`
  - `nonEmpty()`
- `Iterator[T]`
  - `hasNext()`
  - `next()`
  - `isEmpty()`, `nonEmpty()`
  - `foreach(f)`
  - `exists(predicate)`, `forall(predicate)`, `count(predicate)`
- `Iterable[T]`
  - herite de `Sized`
  - `iterator()`
  - `head()`
  - `foreach(f)`
  - `exists(predicate)`, `forall(predicate)`, `count(predicate)`
- `Builder[A, C]` (**experimental**)
  - `add(value)`
  - `result()`
- `IterableFactory[CC[_]]` (**experimental**)
  - `empty[A](): CC[A]`
  - `newBuilder[A](): Builder[A, CC[A]]`
  - `CC[_]` est le premier usage public des constructeurs de type d'arite 1,
    destine aux familles de collections (`List`, `Set`, etc.).
- `IterableOps[A, CC[_], C]` (**experimental**)
  - herite de `Iterable[A]`
  - `iterableFactory(): IterableFactory[CC]`
  - `map[B](f): CC[B]` et `filter(predicate): CC[A]` reconstruisent via
    `IterableFactory.newBuilder`; cette surface reste experimentale et validee
    par une regression de collection generique minimale avant d'etre branchee
    sur les collections publiques.
- `Array[T]`
- `Array(value1, value2, ...)`
- `Array.apply[T](values: T*)`
- `existingArray: _*` pour passer un tableau a un parametre repete
- `Array.empty[T]()`
- `Array.fill[T](size, value)`
- `Array.fill[T](n)(elem)` avec `elem` par nom, reevalue pour chaque case
- `Array.tabulate[T](size, f)`
- `Array.range(size)`
- `Array.range(start, until)`
- `Array.rangeUntil(start, until)` — alias de compatibilite
- `Array.factory()` retourne une `IterableFactory[Array]` experimentale.
- `Array.newBuilder[T]()` retourne un `Builder[T, Array[T]]` experimental; le
  builder preserve l'ordre d'ajout mais materialise actuellement un tableau
  generique, sans promettre de specialisation primitive.
- `ArrayFactory` implemente `IterableFactory[Array]`; `ArrayBuilder[T]`
  implemente `Builder[T, Array[T]]` et peut etre obtenu via
  `IterableFactory[Array].newBuilder[T]()`.
- `array.map(f)` pour conserver le meme type specialise quand il existe, et
  `array.map[U](f)` pour produire un tableau generique `Array[U]` depuis une
  facade primitive sans passer par le nom de compatibilite `mapObject`.
- `array.flatMap(f)` pour conserver le meme type specialise quand il existe, et
  `array.flatMap[U](f)` pour produire un tableau generique `Array[U]` depuis une
  facade primitive sans passer par le nom de compatibilite `flatMapObject`.
- `array.sorted(lessThan)` retourne une copie triee par predicat utilisateur;
  les facades primitives `Int`, `Long`, `Float`, `Double` et `Bool` exposent aussi `sorted()` par ordre naturel (`false < true` pour `Bool`)
- `Set[T]`
  - implemente `IterableOps[T, Set, Set[T]]`; les methodes instance
    experimentales `map[U](f)` et `filter(predicate)` reconstruisent un
    `Set[...]` via `IterableFactory[Set].newBuilder` et conservent la
    deduplication d'ensemble.
  - `contains(value)`
  - `add(value)`, `remove(value)`, `union(other)`, `intersect(other)`,
    `difference(other)`, `clear()`
  - `toArray()`
  - `mkString(separator)` et `toString()`
- Constructeurs/fabriques :
  - `Set(value1, value2, ...)`
  - `Set.apply[T](values: T*)`
  - `Set(values: _*)` pour dedupliquer un tableau existant
  - `Set.empty[T]()`
  - `Set.factory()` retourne une `IterableFactory[Set]` experimentale.
  - `Set.newBuilder[T]()` retourne un `Builder[T, Set[T]]` experimental;
    le builder applique la semantique de `Set.add` et deduplique les doublons.
  - `SetFactory` implemente `IterableFactory[Set]`; `SetBuilder[T]`
    implemente `Builder[T, Set[T]]` et peut etre obtenu via
    `IterableFactory[Set].newBuilder[T]()`.
  - `Set.fromArray[T](values)`
- `Map[K, V]`
  - `Map(value1 -> value2, ...)`
  - `Map.apply[K, V](entries: Tuple2[K, V]*)`
  - `Map.empty[K, V]()`
  - `Map.fromArray[K, V](entries)`
  - `Map.newBuilder[K, V]()` retourne un `Builder[Tuple2[K, V], Map[K, V]]`
    experimental; le builder applique la semantique de `updated`, donc la
    derniere valeur ajoutee pour une meme cle gagne.
  - `MapBuilder[K, V]` implemente `Builder[Tuple2[K, V], Map[K, V]]`. `Map` ne
    rejoint pas encore `IterableOps`, car les constructeurs de type d'arite 2
    restent reportes.
  - `contains(key)` / `containsKey(key)`
  - `getOption(key)`
  - `getOrElse(key, default)`
  - `updated(key, value)`, `removed(key)`, `clear()`
  - `keys()`, `values()`, `toArray()`
  - `foreachEntry(f)`
  - `mapValues[U](default, f)`
  - `filterKeys(predicate)`
  - `mkString(separator)` et `toString()`
- `List[T]` (**experimentale**)
  - `Nil` est le singleton vide runtime, declare comme `List[Nothing]` et
    assignable a `List[T]`.
  - `List[T]` implemente `IterableOps[T, List, List[T]]`; les methodes instance
    experimentales `map[U](f)` et `filter(predicate)` reconstruisent une
    `List[...]` via `IterableFactory[List].newBuilder`.
  - `List.empty[T]()` et `List.cons[T](head, tail)`
  - `List.factory()` retourne une `IterableFactory[List]` experimentale.
  - `List.newBuilder[T]()` retourne un `Builder[T, List[T]]` experimental.
  - `ListFactory` implemente `IterableFactory[List]`; `ListBuilder[T]`
    implemente `Builder[T, List[T]]`, preserve l'ordre d'ajout dans `result()`
    et peut etre obtenu via `IterableFactory[List].newBuilder[T]()`.
  - `isEmpty()`, `nonEmpty()`, `size()`, `foreach(f)`, `head()`, `tail()`,
    `headOption()`, `prepend(value)`, `prepended(value)`, `appended(value)`,
    `concat(suffix)`, `reverse()`, `reverseConcat(suffix)`, `take(n)`,
    `drop(n)`, `slice(from, until)` et `mkString(separator)`
  - operations generiques via compagnon : `List.fold[T, U](values, initial, f)`,
    `List.map[T, U](values, f)`, `List.filter[T](values, predicate)` et
    `List.fromArray[T](values)`; `List.map` et `List.filter` restent disponibles
    comme fonctions de compatibilite recommandees et passent par `ListBuilder`
    pour centraliser la reconstruction ordonnee.
  - `Array[T]` reste la collection indexee principale.

`ArrayObject[T]`, les facades de tableaux specialisees, `Set[T]`, `Map[K, V]` et
`List[T]` implementent `Iterable[...]` pour permettre un `foreach` polymorphe.
`Iterable[T]` herite de `Sized` et expose aussi `iterator()`: les appels communs
`size()`, `isEmpty()`, `nonEmpty()`, `foreach(...)` et le parcours explicite via
`Iterator[T]` restent disponibles via le meme contrat nominal minimal. Par defaut,
`Iterable.foreach(...)`, `head(...)`, `exists(...)`, `forall(...)` et `count(...)`
creent un nouvel `iterator()` puis deleguent aux methodes derivees de `Iterator[T]`;
les collections peuvent garder des overrides specialises si utile. Un `Iterator[T]`
expose `hasNext()`, `next()`, `isEmpty()`, `nonEmpty()`, `foreach(...)`, `exists(...)`,
`forall(...)` et `count(...)`; `next()`, les predicats et `foreach(...)` consomment les
elements restants.

Les exemples doivent privilegier `Array[T]` et les fonctions de haut niveau. Les
facades specialisees restent acceptables dans les tests de backend/runtime et les
zones de stdlib ou le type brut compte vraiment.

Dans la documentation utilisateur, `ArrayObject[T]` et `ArrayInt` /
`ArrayLong` / `ArrayFloat` / `ArrayDouble` / `ArrayBool` doivent etre presentes
comme formes de compatibilite ou representations actuelles, pas comme choix
idiomatiques.

### Strings

- Methodes natives de `String` documentees dans `docs/language.md`.
- Helpers publics de `stdlib/strings.nabla` :
  - `stringNonEmpty(value)`
  - `words(text)`

La semantique actuelle est byte-based/ASCII ; le code utilisateur ne doit pas
supposer une prise en charge Unicode complete.

`String.toCharArray()` et `String.split(...)` retournent la facade publique
`Array[...]`. Leur stockage runtime peut rester objet-backed; les exemples
doivent manipuler ces valeurs par les operations communes de tableau (`size`,
`get`, `mkString`, `foreach`, etc.) et preferer `Array[T]` pour construire de
nouveaux tableaux.

### IO

- `println(value)`
- `input()`
- `readTextFile(path)`
- `writeTextFile(path, content)`
- `appendTextFile(path, content)`
- `renameTextFile(from, to)`
- `createDirectory(path)`
- `deleteTextFile(path)`
- `pathExists(path)`

### Math

Les operations communes utilisent les noms idiomatiques surcharges quand la
signature suffit a choisir l'implementation :

- `sqrt(value: Float): Float`
- `sqrt(value: Double): Double`
- `abs(value: Int | Long | Float | Double)`
- `min(left, right)` et `max(left, right)` pour `Int`, `Long`, `Float` et
  `Double`
- `clamp(value, minimum, maximum)` pour `Int`, `Long`, `Float` et `Double`
- `pow(base, exponent: Int)` pour `Int`, `Float` et `Double`
- `sign(value)` pour `Int`, `Long`, `Float` et `Double`
- `isEven(value)` et `isOdd(value)` pour `Int` et `Long`
- `isBetween(value, minimum, maximum)` pour `Int` et `Long`
- `gcd(left, right)` et `lcm(left, right)` pour `Int` et `Long`
- `absDiff(left, right)` pour `Int`, `Long`, `Float` et `Double`
- `isClose(left, right, epsilon)` pour `Float` et `Double`
- `degreesToRadians(value)` et `radiansToDegrees(value)` pour `Float` et
  `Double`
- `hypotenuse(opposite, adjacent)` pour `Float` et `Double`
- `factorial(value: Int): Int`

Les fonctions suffixees historiques de ces familles (`absInt`, `maxLong`,
`sqrtDouble`, `gcdInt`, `signFloat`, etc.) ont ete retirees de l'API publique.
Les constantes sans argument restent suffixees (`piFloat`, `piDouble`,
`twoPiFloat`, `twoPiDouble`) car la surcharge ne choisit pas encore sur le type
de retour seul.

### Util

- `RandomState`
- `RandomResult[T]`
- `RandomIntResult`
- `RandomBoolResult`
- `RandomChoiceResult[T]`
- `randomSeed(...)`
- `randomSeedNow()`
- `randInt()`
- `randomInt(...)`
- `randomIntRange(...)`
- `randomBool(...)`

## Compatibilite Temporaire

Ces symboles peuvent apparaitre dans d'anciens tests ou exemples, mais les
nouveaux exemples devraient preferer la surface publique ci-dessus :

- `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble`, `ArrayBool`,
  `ArrayObject[T]` quand `Array[T]` suffit.
- Compatibilite `ArrayFill[T](size, value)`, `ArrayRange(size)` et
  `arrayIntRangeUntil(start, until)` quand une fabrique `Array` suffit.
- Fabriques specialisees `arrayIntFill`, `arrayLongFill`, etc. quand une
  fabrique `Array` exprime mieux l'intention.
- Helpers `arrayIntMap`, `arrayObjectMap`, etc. quand une methode de facade
  existe (`xs.map(...)`, `xs.filter(...)`, `xs.fold(...)`, etc.).
- `mapObject[U](...)` sur les facades primitives quand `map[U](...)` exprime la
  meme conversion sans exposer le detail `ArrayObject[U]` dans le code source.
- `flatMapObject[U](...)` sur les facades primitives quand `flatMap[U](...)`
  exprime la meme conversion sans exposer le detail `ArrayObject[U]` dans le
  code source.

Les tests qui couvrent explicitement ces deux noms portent le marqueur
`compat` (`test_compat_...` ou `test_error_compat_...`) afin de les distinguer
des regressions de surface publique `map[U]` / `flatMap[U]`.

- Fonctions bas niveau `setEmpty`, `setFromArray` et variantes specialisees
  quand la fabrique de `Set` exprime mieux l'intention.
- Compatibilite `randomSeedTime()` quand `randomSeedNow()` exprime mieux
  l'intention.
- Compatibilite `randomIntInRange(...)` quand `randomIntRange(...)` suffit.
- Compatibilite `optionSome[T](value)` quand `Option.some[T](value)` suffit.

## Details Internes

Ces symboles sont reserves a l'implementation de la stdlib ou au compilateur :

- `arrayBase...` dans `collections.array_base`.
- Classes de base comme `ArrayPrimitiveBase`.
- Types bruts runtime : `IntArray`, `LongArray`, `FloatArray`, `DoubleArray`,
  `BoolArray`, `ObjectArray[T]`.
- Fonctions brutes associees : `intArrayLength`, `intArrayGet`,
  `objectArraySet`, etc.
- Helpers de conversion/canonisation que le compilateur injecte ou substitue.

Ces noms peuvent rester visibles tant que le langage n'a pas de notion de
modules prives, mais ils ne doivent pas etre documentes comme API utilisateur.

## Regles Pour Les Nouveaux Symboles Stdlib

1. Choisir d'abord la categorie : public, compat temporaire ou interne.
2. Ajouter un commentaire `///` uniquement pour les symboles publics. Pour les
   modules ou methodes tres utilises, ajouter au moins un bloc `@example ...
   @end` court et idiomatique.
3. Si un symbole est interne, preferer un nom explicite (`arrayBase...`,
   `...Raw`, `...Unchecked`) et ne pas l'utiliser dans les exemples utilisateur.
4. Ajouter un test positif sur la surface publique.
5. Ajouter un test negatif seulement quand le compilateur sait diagnostiquer la
   mauvaise utilisation ; ne pas inventer un test qui dependrait d'une notion de
   visibilite pas encore implementee.
6. Regenerer la documentation :

```bash
make stdlib-docs
git diff --exit-code docs/stdlib
```

## Objectif De Stabilisation

Pour Nabla 0.1, la priorite est une surface petite et coherent :

- `Array[T]` pour le code utilisateur ;
- `Option[T]` pour les absences de valeur ;
- `Set[T]`, `Map[K, V]` et le trait `Sized` pour les collections publiques ;
- `String`, `Math`, `IO`, `Random` pour les programmes simples ;
- classes, heritage, mixins et lambdas comme modele principal de composition.

Les helpers internes resteront tolerés tant que necessaire, mais chaque nouvelle
fonctionnalite doit eviter d'augmenter la surface publique accidentelle.
