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
helpers sans commentaire public doivent rester absents de `docs/stdlib/`.

## Surface Publique Recommandee

### Core

- `Option[T]`
  - `Option.some[T](value)`
  - `Option.none[T](default)`
  - `isDefined()`
  - `isEmpty()`
  - `getOrElse(default)`
  - `map(...)`
  - `flatMap(...)`
  - `filter(...)`
  - `foreach(...)`
  - `orElse(...)`
- Constructeurs/fabriques :
  - `optionSome[T](value)` et `optionNone[T](default)` restent disponibles par
    compatibilite.
- `OptionInt` reste public tant que les generiques primitifs ne couvrent pas
  totalement les besoins de performance/representation.

### Collections

- `Array[T]`
- `Array(value1, value2, ...)`
- `Array.apply[T](values: T*)`
- `Array.empty[T]()`
- `Array.fill[T](size, value)`
- `Array.tabulate[T](size, f)`
- `Array.range(size)`
- `Array.rangeUntil(start, until)`
- Facades specialisees exposees par compatibilite utile :
  - `ArrayInt`
  - `ArrayLong`
  - `ArrayFloat`
  - `ArrayDouble`
  - `ArrayBool`
  - `ArrayObject[T]`
- `Set[T]`
- Constructeurs/fabriques :
  - `Set.empty[T]()`
  - `Set.fromArray[T](values)`

Les exemples doivent privilegier `Array[T]` et les fonctions de haut niveau. Les
facades specialisees restent acceptables dans les tests de backend/runtime et les
zones de stdlib ou le type brut compte vraiment.

### Strings

- Methodes natives de `String` documentees dans `docs/language.md`.
- Helpers publics de `stdlib/strings.nabla` :
  - `stringNonEmpty(value)`
  - `words(text)`

La semantique actuelle est byte-based/ASCII ; le code utilisateur ne doit pas
supposer une prise en charge Unicode complete.

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
- Compatibilite `SetEmpty[T]()` et `SetFromArray[T](values)` quand
  `Set.empty[T]()` ou `Set.fromArray[T](values)` suffit.
- Fonctions bas niveau `setEmpty`, `setFromArray` et variantes specialisees
  quand la fabrique de `Set` exprime mieux l'intention.
- Compatibilite `randomSeedTime()` quand `randomSeedNow()` exprime mieux
  l'intention.
- Compatibilite `randomIntInRange(...)` quand `randomIntRange(...)` suffit.

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
2. Ajouter un commentaire `///` uniquement pour les symboles publics.
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
- `String`, `Math`, `IO`, `Random` pour les programmes simples ;
- classes, heritage, mixins et lambdas comme modele principal de composition.

Les helpers internes resteront tolerés tant que necessaire, mais chaque nouvelle
fonctionnalite doit eviter d'augmenter la surface publique accidentelle.
