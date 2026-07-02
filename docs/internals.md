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
├── AnyRef
└── Nothing
```

`AnyVal` et `AnyRef` sont des types builtin abstraits. Les primitives `Unit`,
`Bool`, `Int`, `Long`, `Float`, `Double` et `Char` sont assignables a `AnyVal`
et `Any`. Les références heap (`String`, tableaux, fonctions/closures et classes
utilisateur) sont assignables a `AnyRef` et `Any`. Une classe utilisateur sans
parent explicite hérite implicitement de `AnyRef`, pas directement de `Any`.
`Nothing` est le bottom type builtin : la relation d'assignabilite accepte
`Nothing` pour tout type attendu, mais aucune expression normale ne produit de
valeur `Nothing`. Les primitives globales `panic(message: String)` et
`error(message: String)` l'utilisent pour typer les chemins qui ne retournent
pas; le backend les abaisse vers `Runtime_panic`, qui termine le processus avec
le statut `250`.

Cette hiérarchie est une convention de type, pas une promesse de représentation
objet uniforme : les chemins spécialisés peuvent conserver des valeurs
immédiates ou brutes. Quand une primitive builtin est passée à un paramètre
`Any` ou `AnyVal` d'une fonction ou méthode, ou à la primitive globale
`print(value)`, le lowering IR insère un boxing heap explicite afin que les
méthodes dynamiques communes comme `toString()` puissent retrouver le type
runtime d'origine. `print(value)` est ensuite abaissé en appel à
`Any.toString()` suivi du runtime d'écriture de chaîne.

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

`Int` et `Long` partagent l'encodage immediat, mais restent des types source et
IR distincts. Les operations arithmetiques, comparaisons, champs, parametres et
retours conservent ce type jusqu'au backend; une conversion `Int.toLong` est
explicite. Les tests de mismatch entre `Int` et `Long` couvrent cette frontiere.

`Char` est aussi une valeur immediate taggee dans les chemins actuels. Sa
surface source reste limitee aux litteraux ASCII, aux retours de
`String.charAt(index)` et aux conversions/affichages specialises.

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
slot 0      : pointeur vers la vtable backend pour les objets utilisateur
slot 1..n   : champs de constructeur et champs hérités
```

Les paramètres constructeur `val` et `var` partagent cette représentation de
slot. Tous deux génèrent un getter synthétique zéro-argument abaissé comme une
méthode normale. Le flag `var` est uniquement une règle source/sémantique : une
affectation simple dans une méthode de la classe peut être résolue vers un champ
constructeur mutable et l'IR émet alors un `FieldStore` sur le slot de `this`.
Les champs `val` restent rejetés avant lowering. La V1 ne crée pas encore de
setter public ni d'affectation externe `receiver.field = value`.

Les vtables sont générées par le backend ASM pour les classes concrètes,
spécialisations génériques nécessaires et singletons runtime. Les slots sont
indexés par propriétaire statique + méthode résolue, afin de distinguer les
overloads et les méthodes génériques spécialisées. Elles servent au dispatch
dynamique des méthodes utilisateur quand une valeur est manipulée via un type
parent, un trait comme `Iterable[Int]`, `Sized`, `Any` ou `AnyRef`. Les appels à
`Any.toString`, `Any.hashCode` et `Any.equals` passent par ces mêmes entrées
pour redispatcher vers les overrides utilisateur avant de tomber sur le fallback
runtime des valeurs taggées. `super` est abaissé comme appel statique. Les
closures réutilisent leur propre convention de header pour stocker le pointeur
de code.

Les objets `String` utilisent un tag runtime réservé dans leur slot 0. Ce tag
permet aux méthodes racine `Any.toString`, `Any.hashCode` et `Any.equals` de
reconnaître les chaînes même après effacement statique vers `Any` ou `AnyRef` :
`toString` rend la chaîne elle-même, `hashCode` réutilise le hash byte-based de
`String`, et `equals` compare le contenu lorsque les deux opérandes sont des
chaînes. Les littéraux de chaînes placent leur objet sur un alignement 8 octets
afin que les tests de tag immédiat ne puissent pas confondre un pointeur de
chaîne avec un entier taggé.

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

`IntArray`, `LongArray`, `FloatArray`, `DoubleArray` et `BoolArray` sont des
types natifs spécialisés. Leur type d'element est fixe et leurs methodes
`length`, `get` et `set` sont typees par la specialisation. Un `set` avec une
valeur d'un autre type doit echouer en analyse semantique, avant la generation
ASM.

La surface utilisateur cible est la façade `Array[T]` et les wrappers standard
comme `ArrayInt`, `ArrayObject[T]`, etc. Les fonctions `arrayBase...` et helpers
spécialisés sont des détails d'implémentation. Les façades publiques de tableaux,
`Set[T]` et `Map[K, V]` implémentent `Iterable[...]`; ce contrat hérite de
`Sized` et fournit `foreach` comme point commun polymorphe.

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

Une valeur `String` est une reference heap vers un objet taggé contenant la
longueur et un pointeur vers un buffer de bytes runtime. Les operations `+`,
`==` et `!=` sont des primitives specialisees sur cette representation
byte-based; `toCharArray()` produit un `ArrayObject[Char]` afin de ne pas
exposer de tableau natif specialise pour les caracteres.

## Runtime I/O Fichier

Les primitives fichier restent des entrees runtime internes appelees par le
backend IR (`Runtime_readFile`, `Runtime_writeFile`, `Runtime_appendFile`,
`Runtime_fileExists`, `Runtime_deleteFile`, `Runtime_renameFile` et
`Runtime_createDir`). Les labels d'entree sont stables pour le backend, mais les
sequences assembleur partagees peuvent etre factorisees en helpers runtime
prives.

`Runtime_writeFile` et `Runtime_appendFile` divergent uniquement par les flags
`open(2)` (`O_TRUNC` contre `O_APPEND`) et sautent vers
`Runtime_writeStringToFileWithFlags`. Ce helper copie le chemin Nabla vers une
chaine C temporaire, ouvre le fichier avec les flags fournis, ecrit le payload
`String`, ferme le descripteur puis retourne le resultat de `write(2)` encode
comme entier tagge, comme les anciennes entrees dediees.

## Fonctions, `def`, Lambdas Et Closures

Les `def` source restent le concept utilisateur pour les fonctions, méthodes et
propriétés calculées. Un `def name: T = expr` sans liste de paramètres est
représenté comme une fonction ou méthode zéro-argument ordinaire; les usages
`name`, `Object.name` ou `value.name` sont résolus comme appels zéro-argument
quand le symbole cible est unique et non ambigu. Il n'y a pas de stockage ni de
mémoïsation implicite: le corps est évalué à chaque appel.

Les fonctions locales de bloc `def name(args...): R = { ... }` sont abaissées en
symboles de fonction cachés non surchargés. Les appels directs restent des appels
de fonction directs vers ce symbole caché; une référence en position valeur
réutilise le chemin `FunctionReferenceNode` / `Fn` pour les helpers déclarés hors
contexte générique. La V1 autorise récursion directe, appels vers des helpers locaux
déclarés précédemment, et réutilisation des paramètres de type des fonctions,
méthodes, classes et traits englobants. Les signatures et corps de helpers
locaux peuvent donc mentionner `T`, `U`, `List[T]` ou `(T) => U` quand ces types
sont déjà en portée. Les captures implicites de valeurs/paramètres englobants,
fonctions locales génériques et overloads locaux dans une même portée restent
rejetés explicitement.

Les types fonction sont canoniques sous la forme interne :

```text
Fn(T1,T2)->R
Fn()->R
```

La syntaxe source `(T1, T2) => R` est abaissée vers cette représentation; la
forme zero-argument `() => R` est representee par `Fn()->R`.
Cette forme canonique est celle utilisee pour comparer les signatures, typer les
annotations locales, resoudre les references de fonctions et representer les
champs de type fonction. Les paramètres par nom `name: => T` sont stockés dans
les signatures comme `Fn()->T` avec un marqueur `isByName`; le parser transforme
chaque argument source en closure zéro-argument et chaque référence nue au
paramètre dans le corps en appel indirect `name()`.

Les lambdas sans capture et closures avec capture par valeur sont supportées.
Les captures sont matérialisées côté IR/runtime comme valeurs stockées dans une
structure de closure. Les limites de paramètres restent liées à la convention
d'appel actuelle.

## Objets Statiques Et Singletons Runtime

Les `object` source sont abaissés comme namespaces de fonctions globales
qualifiées. Par exemple `object Math { def abs(...) }` enregistre une fonction
`Math.abs` dans `CompilerContext::functions`, et l'appel `Math.abs(...)` devient
un appel de fonction normal côté IR.

Un `object Name with Trait` est le singleton runtime V0. Le parser enregistre
`Name` dans `CompilerContext::runtimeObjects` et cree une entree `ClassInfo`
sans champs, avec parent implicite `AnyRef` puis les traits de la clause `with`.
Les `def` du bloc sont enregistrées comme méthodes de classe `Name`, pas comme
fonctions statiques. Les validations d'héritage class-like s'appliquent donc :
traits uniquement apres `with`, méthodes abstraites obligatoires, `override`
obligatoire pour les membres hérités, signature stricte et conflits de defaults
explicites.

Une reference source nue a un singleton runtime produit un noeud AST
`SingletonObjectNode`, abaissé en opcode IR `SingletonObjectRef`. Le backend
émet une cellule statique alignée dans `.data` :

```text
singleton.Name: dq <class-id>
```

Cette adresse est la valeur runtime stable du singleton. Elle est assignable a
`Name`, aux traits composés, a `AnyRef` et a `Any`, et le slot 0 contient le
même identifiant de classe que les objets heap pour permettre le redispatch de
méthodes utilisateur et de `Any.toString` / `Any.hashCode` / `Any.equals`.

V0 ne fournit pas encore de champs d'objet, constructeur, `extends`,
arguments de type, initialisation lazy/eager, ni vraie ABI de singleton. Un
singleton runtime ne peut pas être instancié via `new` ni utilisé comme parent
de classe. Un `object` sans `with` reste uniquement un namespace statique;
l'utiliser comme valeur produit un diagnostic dédié.

## Classes, Héritage Et `Any`

Les classes sans parent explicite héritent implicitement de `AnyRef`. `AnyRef`
remonte ensuite vers `Any` dans la hiérarchie racine.

`Any` fournit actuellement :

- `toString(): String` ;
- `hashCode(): Int` ;
- `equals(other: Any): Bool`.

Les méthodes héritées sont résolues dans la classe courante, puis le parent, puis
les mixins dans l'ordre d'énonciation. Les conflits hérités doivent être
signalés explicitement. Une redéfinition de méthode héritée requiert `override`
et sa signature est validée strictement : arité, paramètres, retour, paramètres
génériques de méthode et substitutions des types hérités.

Les noms de membres visibles doivent rester non ambigus. Deux champs hérités du
même nom sont rejetés, deux méthodes concrètes héritées avec la même signature
sont rejetées, et un champ visible ne peut pas partager son nom source avec une
méthode visible provenant d'un autre provider. Cette règle évite qu'un accès nu
comme `value` masque silencieusement un appel potentiel `value()` dans une
hiérarchie composée par `extends ... with ...`.

Un paramètre constructeur `val` reste un champ de layout normal, mais il ajoute
aussi une signature de méthode synthétique zéro-argument dans
`ClassInfo::methods` / `methodOverloads`. Le parser génère ensuite un corps
synthétique qui lit le champ via `FieldLoad`. Ces accesseurs peuvent implémenter
une méthode abstraite de trait de même signature, mais ils ne dispensent pas du
mot-clé `override` lorsqu'ils masqueraient une méthode concrète héritée.

Les types de classes sont nominaux. Deux classes avec le meme layout ne sont pas
interchangeables sans relation d'heritage, et les classes generiques instanciees
restent distinguees par leurs arguments de type dans les signatures et les corps
IR specialises. Les champs herites sont integres au layout avant les champs
propres afin que les offsets restent coherents dans les appels parent-types.

Le slot 0 des objets utilisateur contient un pointeur de vtable backend. Le
backend l'utilise pour les appels virtuels quand le type statique est un parent,
un trait instancié comme `Iterable[Int]`, ou `Any`/`AnyRef`. Les overrides de
`toString`, `hashCode` et `equals` participent à ce dispatch, ce qui rend
`Set[Parent]` et les comparaisons via `Any` cohérents dans les cas couverts par
les tests. Cette vtable reste une convention interne du backend, pas une ABI
publique stable.

`super` cible statiquement la classe parente immédiate dans une méthode de
classe. Il ne suit pas le dispatch virtuel et ne modélise pas une linéarisation
Scala complète des mixins. Son usage hors classe ou sans parent explicite valide
doit produire un diagnostic dédié.

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

Le heap runtime est un bump allocator alloué par `mmap`. Sa capacité par défaut
reste `8388608` octets (8 MiB), mais le compilateur accepte
`--heap-size <octets>` pour générer un exécutable avec une autre valeur dans le
symbole assembleur `heap_capacity`. Les valeurs non entières ou inférieures à
4096 octets sont rejetées par `nablac` avant la génération.

Ces codes doivent rester documentés au fur et à mesure qu'ils deviennent une
surface observable par l'utilisateur.

## Principes De Maintenance

- Centraliser toute nouvelle convention de représentation dans ce document.
- Ajouter un test de régression avant de changer une convention runtime.
- Garder la surface utilisateur orientée `Array[T]`, `Option[T]`, `Set[T]`,
  `String`, classes, méthodes et lambdas.
- Éviter d'exposer dans les diagnostics des noms internes lorsqu'une forme
  source plus claire existe.
