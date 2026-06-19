# Nabla Internals

Ce document capture les conventions internes actuelles du compilateur et du
runtime. Il décrit l'implémentation existante, pas une promesse de stabilité
publique.

## Pipeline

```text
source .nabla
  -> lexer
  -> parser / AST
  -> analyse semantique
  -> IR Nabla
  -> generation ASM x86-64
  -> nasm -f elf64
  -> ld
```

Le backend IR est le chemin par défaut. L'option historique `--backend-ir` est
conservée pour compatibilité.

## Commandes Externes

Le compilateur invoque `nasm` et `ld` via `fork` + `execvp`, sans passer par un
shell. Une commande absente du `PATH` doit produire un diagnostic explicite du
type :

```text
Erreur: commande externe introuvable: nasm (...)
```

La cible `make tooling-tests` couvre ce comportement.

## Valeurs Runtime

Nabla utilise une représentation uniforme par slots de 64 bits.

`src/runtime_values.hpp` centralise les constantes d'encodage utilisees par le
backend et le runtime assembleur : tag, valeurs booleennes, zero tagge, slot nul,
taille de slot et bornes des entiers immediats.

### Hiérarchie de types racine

Le type system expose une hiérarchie Scala-like :

```text
Any
├── AnyVal
└── AnyRef
```

`AnyVal` et `AnyRef` sont des types builtin abstraits. Les primitives `Unit`,
`Bool`, `Int`, `Long`, `Float`, `Double` et `Char` sont assignables a `AnyVal`
et `Any`. Les références heap (`String`, tableaux, fonctions/closures et classes
utilisateur) sont assignables a `AnyRef` et `Any`. Une classe utilisateur sans
parent explicite hérite implicitement de `AnyRef`, pas directement de `Any`.

Cette hiérarchie est une convention de type, pas une promesse de représentation
objet uniforme : les chemins spécialisés peuvent conserver des valeurs
immédiates ou brutes. Quand une primitive builtin est passée à un paramètre
`Any` ou `AnyVal` d'une fonction ou méthode, le lowering IR insère un boxing
heap explicite afin que les méthodes dynamiques communes comme `toString()`
puissent retrouver le type runtime d'origine.

### Valeurs taggées

`Int`, `Long` et `Bool` utilisent le tagging par bit de poids faible :

- bit 0 à `1` : valeur immédiate taggée ;
- bit 0 à `0` : pointeur heap aligné ou slot nul selon le contexte.

Pour les entiers, l'encodage est :

```text
slot = (valeur << 1) | 1
valeur = slot >> 1
```

Les booléens suivent la même famille de représentation :

- `false` runtime : `1` ;
- `true` runtime : `3`.

Les constantes `Bool` dans l'IR sont déjà émises sous cette forme taggée. Le
backend refuse une constante IR `Bool` qui ne vaut pas `1` ou `3`. Les
comparaisons et conditions doivent manipuler ces valeurs runtime, pas des
booléens C++ implicites.

### Valeurs flottantes

`Float` et `Double` sont portés comme valeurs numériques brutes dans les chemins
IR/backend qui les manipulent. Les tableaux natifs de flottants utilisent des
slots initialisés à zéro IEEE, pas au zéro taggé. Le boxing heap explicite est
actuellement émis pour les appels de fonctions/méthodes dont le paramètre
attendu est `Any` ou `AnyVal`; le payload conserve les bits IEEE et le tag de
box permet à `Any.toString()` de redispatcher vers `Float.toString()` /
`Double.toString()`.

### Objets heap

Les objets sont alloués dans le heap runtime par bump allocation. Le layout est
linéaire par slots de 8 octets :

```text
slot 0      : identifiant de classe runtime pour les objets utilisateur
slot 1..n   : champs de constructeur et champs hérités
```

Les vraies vtables ne sont pas encore formalisées. Le header sert aujourd'hui
au dispatch dynamique des méthodes utilisateur quand une valeur est manipulée
via un type parent, y compris pour des parents génériques instanciés et des
méthodes génériques spécialisées. Les appels à `Any.toString`,
`Any.hashCode` et `Any.equals` utilisent le même identifiant de classe pour
redispatcher vers les overrides utilisateur avant de tomber sur le fallback
runtime. `super` est abaissé comme appel statique.
Les identifiants de classes commencent à `1000` pour éviter les tags runtime
boxed. Les closures réutilisent leur propre convention de header pour stocker
le pointeur de code.

### Slots nuls

Les tableaux d'objets utilisent des slots nuls pour les cases non initialisées.
Les accès utilisateur doivent passer par les méthodes/facades standard quand
possible, afin de préserver les diagnostics et conventions d'erreur.

## Tableaux

Les collections natives spécialisées existent pour éviter de payer une
allocation objet par élément primitif :

- `IntArray`
- `LongArray`
- `FloatArray`
- `DoubleArray`
- `BoolArray`
- `ObjectArray[T]`

Les valeurs par défaut suivent le type d'élément :

- `Int`, `Long`, `Bool` : zéro/faux taggé (`1`) ;
- `Float`, `Double` : zéro IEEE (`0`) ;
- objets : slot nul (`0`).

La surface utilisateur cible est la façade `Array[T]` et les wrappers standard
comme `ArrayInt`, `ArrayObject[T]`, etc. Les fonctions `arrayBase...` et helpers
spécialisés sont des détails d'implémentation.

## Chaînes Et Caractères

`String` est actuellement byte-based/ASCII pour :

- `length()` ;
- `charAt(index)` ;
- `substring(from, until)` ;
- `split(separator)` ;
- `trim()` ;
- prédicats comme `startsWith` / `endsWith`.

`Char` représente un caractère ASCII. Une future prise en charge Unicode devra
réviser explicitement ces conventions.

## Fonctions, Lambdas Et Closures

Les types fonction sont canoniques sous la forme interne :

```text
Fn(T1,T2)->R
```

La syntaxe source `(T1, T2) => R` est abaissée vers cette représentation.

Les lambdas sans capture et closures avec capture par valeur sont supportées.
Les captures sont matérialisées côté IR/runtime comme valeurs stockées dans une
structure de closure. Les limites de paramètres restent liées à la convention
d'appel actuelle.

## Classes, Héritage Et `Any`

Les classes sans parent explicite héritent implicitement de `Any`.

`Any` fournit actuellement :

- `toString(): String` ;
- `hashCode(): Int` ;
- `equals(other: Any): Bool`.

Les méthodes héritées sont résolues dans la classe courante, puis le parent, puis
les mixins dans l'ordre d'énonciation. Les conflits hérités doivent être
signalés explicitement. Une redéfinition de méthode héritée requiert `override`
et sa signature est validée strictement : arité, paramètres, retour, paramètres
génériques de méthode et substitutions des types hérités.

`super` cible la classe parente immédiate dans une méthode de classe. Son usage
hors classe ou sans parent explicite valide doit produire un diagnostic dédié.

## Génériques

Les génériques sont principalement monomorphisés vers des corps spécialisés. Les
aliases standard de collections, par exemple `Array[Int] -> ArrayInt`, sont des
règles internes de canonisation et ne doivent pas apparaître comme détails
obligatoires dans les exemples utilisateur.

Les génériques sont invariants par défaut. Le sous-typage reste nominal pour les
classes.

## Erreurs Runtime Connues

Certaines opérations runtime utilisent des codes de sortie dédiés, par exemple :

- division par zéro ;
- parsing entier invalide ;
- accès hors bornes sur chaînes/tableaux ;
- dépassement du heap.

Ces codes doivent rester documentés au fur et à mesure qu'ils deviennent une
surface observable par l'utilisateur.

## Principes De Maintenance

- Centraliser toute nouvelle convention de représentation dans ce document.
- Ajouter un test de régression avant de changer une convention runtime.
- Garder la surface utilisateur orientée `Array[T]`, `Option[T]`, `Set[T]`,
  `String`, classes, méthodes et lambdas.
- Éviter d'exposer dans les diagnostics des noms internes lorsqu'une forme
  source plus claire existe.
