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

Le heap runtime est alloué par `mmap`. Sa capacité par défaut reste `8388608`
octets (8 MiB), mais le compilateur accepte
`--heap-size <octets>` pour générer un exécutable avec une autre valeur dans le
symbole assembleur `heap_capacity`. Les valeurs non entières, inférieures à
4096 octets ou supérieures au payload `Int` taggé représentable sont rejetées
par `nablac` avant la génération.

`Runtime_alloc` aligne la taille demandée sur 8 octets et retourne un pointeur
payload stable. Chaque allocation possède désormais un header caché de 16 octets
juste avant ce payload : `header[0]` contient la taille payload alignée avec les
bits hauts `mark` et `free`, et `header[1]` contient le lien `next` quand le bloc
est dans `heap_free_list` (sinon 0). Le layout observé par le code Nabla reste
inchangé : objets, chaînes, tableaux, closures et boîtes commencent toujours à
l'adresse payload retournée.

L'allocation essaie d'abord de réutiliser un bloc libre assez grand depuis
`heap_free_list`, puis tente d'avancer `heap_pointer` en réservant
`payload_size + 16`. Si le bump échoue, `Runtime_alloc` appelle `Runtime_gc`,
retente la free-list puis le bump, et n'émet `Runtime_heap_overflow` qu'après cet
échec. Le sweep fusionne les suites adjacentes de blocs morts/libres en un seul
bloc réutilisable. Quand un bloc libre est nettement plus grand que la demande,
`Runtime_alloc` le découpe : le préfixe devient le bloc alloué, et la queue reste
un bloc libre avec son propre header de 16 octets dans `heap_free_list`. Si la
queue ne peut pas contenir un header plus au moins un mot payload aligné, le bloc
entier est consommé. Le payload rendu au programme est remis à zéro; les queues
libres ne sont pas scannées par le marqueur conservateur tant qu'elles restent
marquées `free`.

`Runtime_gc` est une première collecte active réelle, conservative, traçante et
non compactante, entièrement côté runtime assembleur. `_start` enregistre
`gc_stack_top` avant l'initialisation du heap. Lors d'une collecte, le runtime
scanne les mots pairs entre la pile native courante et ce sommet, y compris les
registres généraux spillés par `Runtime_alloc`, puis traite tout mot qui tombe
dans le payload d'un bloc heap alloué comme une racine candidate. Les payloads
des blocs marqués sont ensuite rescannés conservativement jusqu'à fixpoint
(`gc_mark_changed`). Le sweep parcourt linéairement les headers de `heap_start`
à `heap_pointer` : un bloc marqué est conservé et son bit mark effacé; un bloc
non marqué est ajouté à `heap_free_list` avec le bit free. Aucun payload pointer
n'est déplacé.

Cette stratégie accepte les faux positifs : un entier raw, un fragment de bytes
ou une ancienne valeur de pile peut ressembler à un pointeur heap et retenir un
bloc trop longtemps. Elle ne consomme pas encore les métadonnées exactes de
racines, layouts et points d'appel décrites plus bas; ces cartes restent
additives pour la prochaine étape. `Runtime_heapUsed` retourne donc encore
`heap_pointer - heap_start` comme high-water mark bump, pas la mémoire vivante
après sweep, tandis que `Runtime_heapCapacity` retourne `heap_capacity`; les
primitives source `heapUsed()` / `heapCapacity()` exposent ces compteurs. Pour
l'observabilité GC, `Runtime_gc` incrémente `gc_collections` au début de chaque
collecte et remet à zéro puis remplit `gc_last_freed_bytes` avec les payloads
nouvellement récupérés et `gc_last_largest_free_block` avec le plus gros bloc
libre produit par le sweep. Les primitives
`gcCollections()`, `gcLastFreedBytes()` et `gcLastLargestFreeBlock()` exposent
ces valeurs de dernière collecte. `Runtime_heapFreeBytes` et
`Runtime_heapLargestFreeBlock` parcourent `heap_free_list` à la demande pour
exposer `heapFreeBytes()` et `heapLargestFreeBlock()`, utiles pour diagnostiquer
la fragmentation sans changer la sémantique high-water de `heapUsed()`.

Si `Runtime_initHeap` ne peut pas obtenir le heap demandé, si une allocation ne
peut toujours pas être satisfaite après `Runtime_gc`, ou si une taille
d'allocation déborde pendant l'alignement ou le calcul de taille des tableaux
natifs, le runtime écrit `Nabla runtime error: heap exhausted` sur stderr puis
termine avec le code 255. Les mitigations sûres restent : augmenter
`--heap-size` pour les charges connues, éviter les concaténations ou `repeat` non
bornés dans les boucles, et réutiliser des tableaux mutables lorsque le
traitement peut être exprimé en place. Un `delete` utilisateur serait donc une
nouvelle stratégie mémoire, pas un simple wrapper autour du runtime actuel;
il devra traiter aliasing, échappement depuis tableaux/closures/collections,
cycle de vie des chaînes et interaction avec le GC conservateur courant ou
d'éventuelles arènes spécialisées.

La direction active est suivie dans `docs/plans/runtime-memory-management.md` :
garder la surface normale sûre (`AnyRef`, `Option[T]`, collections) et durcir le
GC traçant simple non compactant. Les arènes et `unsafe.memory` restent reportés
à des besoins spécialisés; les prochaines étapes consistent à remplacer
progressivement le scan conservateur par les cartes exactes déjà émises.

### Inventaire Des Familles Heap Pour Le Futur GC

Cet inventaire décrit les objets que le futur marqueur devra reconnaître. Il
reste une convention interne : aucun tag, offset ou pointeur de vtable listé ici
n'est une ABI source publique.

- **Chaînes heap `String`** : le mot `+0` vaut `kStringTag`, `+8` contient la
  longueur non taggée en octets, et `+16` contient un pointeur vers les octets.
  Les littéraux placent ces octets en `.data`; les chaînes construites au
  runtime placent généralement les octets à partir de `+24` dans la même
  allocation. Le futur GC doit donc traiter `+16` comme pointeur interne ou
  pointeur statique, pas comme référence Nabla à marquer.
- **Stockage de tableaux natifs et `ObjectArray[T]` brut** : le mot `+0` est
  aujourd'hui un slot nul, `+8` contient la longueur taggée, puis les éléments
  commencent à `+16`. Pour `IntArray`, `LongArray`, `FloatArray`, `DoubleArray`
  et `BoolArray`, les éléments sont des valeurs; pour `ObjectArray[T]`, les
  éléments sont des slots potentiellement références et devront être scannés.
- **Façades `ArrayObject[T]`** : ce sont des instances utilisateur/génériques
  avec vtable à `+0` et champ `values` à `+8`; ce champ référence le stockage
  brut `ObjectArray[T]` dont les éléments commencent à `+16`. Le futur GC devra
  donc marquer d'abord le champ `values`, puis scanner le stockage brut selon
  son type d'élément.
- **Instances utilisateur et façades objet** : le mot `+0` contient un pointeur
  de vtable backend. Les champs de layout commencent aux offsets calculés dans
  `context.classLayouts`; seuls les champs dont le type peut porter une
  référence (`Any`, `AnyRef`, `String`, tableaux, classes, closures ou
  paramètres de type conservés comme objets) devront être marqués.
- **Closures** : une référence de fonction alloue `16 + 8 * captures` octets;
  `+0` contient le pointeur code, `+8` un indicateur de capture, puis les
  captures commencent à `+16`. Chaque capture est un slot Nabla et devra être
  scannée selon son type statique conservé par le backend.
- **Valeurs boxées `Any` / `AnyVal`** : les tags `kBoxedIntTag`,
  `kBoxedLongTag`, `kBoxedFloatTag`, `kBoxedDoubleTag`, `kBoxedBoolTag`,
  `kBoxedCharTag` et `kBoxedUnitTag` occupent le mot `+0`; la valeur est à `+8`.
  Ces boîtes ne contiennent pas de référence aujourd'hui.
- **Singletons runtime** : les objets listés dans `context.runtimeObjects` vivent
  en `.data` et contiennent un pointeur de vtable. Ils doivent être considérés
  comme racines statiques plutôt que comme allocations bump-heap.
- **Valeurs immédiates** : les `Int`/`Bool`/`Char` taggés et les slots nuls
  (`kNullSlot`) ne sont pas des pointeurs heap et doivent être ignorés par le
  marqueur.

Les métadonnées exactes devront donc pouvoir distinguer au minimum :
`String`, tableau de valeurs, tableau de références, instance utilisateur,
closure, boîte primitive et singleton statique. Le backend produit déjà des
métadonnées additives pour les slots de frame, les champs d'objets et les
captures de closures référence-capables; le GC actif actuel ne les consomme pas
encore et se limite au scan conservateur pile + payload.

### Inventaire Des Racines Backend Pour Le Futur GC

Le backend actuel abaisse chaque fonction en un frame `rbp` simple. Toutes les
valeurs IR nommées sont matérialisées dans des slots `StackFrame` à offset fixe
via `collectSlots()`, puis lues/écrites par `loadValue()` et `storeRegister()`.
Ce modèle est pratique pour un premier GC stop-the-world : les slots candidats
peuvent être décrits comme des offsets `rbp - offset` plutôt que comme une pile
machine conservatrice.

Racines à exposer avant toute collecte :

- **Paramètres de fonction et de méthode** : au prologue, les registres d'appel
  sont copiés dans les slots `%param`. Les paramètres dont le type peut porter
  une référence (`Any`, `AnyRef`, `String`, tableaux/façades, classes,
  closures, type parameters objectifiés) doivent être listés comme racines.
- **Temporaires IR nommés** : chaque résultat d'instruction avec `result` reçoit
  un slot. Les temporaires produits par `StringLiteral`, `Call`,
  `FunctionReference`, `SingletonObjectRef`, `NewObject`, `New*Array`,
  `ObjectArrayGet`, `FieldLoad`, `Load`, `Phi`, appels de méthode et appels
  indirects sont des candidats racines si leur type est référence-capable.
- **Variables locales mutables** : les `Store` ajoutent aussi le symbole stocké
  au frame. Comme les slots de `var` peuvent survivre à plusieurs branches ou
  boucles, ils doivent rester scannables tant que la fonction est active.
- **Arguments et temporaires en registres pendant un appel runtime** : le codegen
  peut charger des valeurs heap dans `rsi`, `rdx`, `rcx`, `r8`, `r9`, `r10`,
  `r11`, `r12` ou `r13` juste avant `Runtime_alloc` et les helpers de chaînes;
  `rdi` porte la taille demandée au safepoint `Runtime_alloc` et ne constitue
  pas une racine implicite pour cet appel. La collecte conservatrice courante
  rend ces valeurs visibles en spillant les registres généraux (`rbx`, `rcx`,
  `rdx`, `rsi`, `r8`-`r15`) dans la pile native avant `Runtime_gc`. Les helpers
  qui écrasent un owner heap pour préparer une allocation doivent continuer à le
  spiller explicitement avant de charger la nouvelle taille. Les cartes exactes
  futures devront remplacer ce contrat conservateur par des racines consommables.
- **Racines statiques** : les singletons `context.runtimeObjects`, vtables et
  littéraux de chaînes en `.data` ne sont pas des slots du bump heap. Les
  singletons doivent être traités comme racines statiques; les vtables et bytes
  de chaînes comme données non scannées.
- **État runtime transitoire** : les helpers assembleur qui allouent pendant
  qu'une référence heap précédente reste nécessaire, ou qui enchaînent plusieurs
  allocations (`Runtime_stringSplit`, `Runtime_buildArgsArray`, conversions
  `toString`, wrappers `ArrayObject[T]`), conservent parfois une allocation
  précédente dans un registre ou sur la pile native avant l'allocation suivante.
  Ces valeurs sont aujourd'hui protégées soit par le spill général de
  `Runtime_alloc`, soit par des spills manuels documentés quand le helper doit
  écraser un registre owner avant l'appel. Les helpers à allocation unique, comme
  `Runtime_stringConcat`, restent concernés par la protection de leurs arguments
  vivants si une future étape remplace le scan conservateur par des cartes
  exactes consommées.

Non-racines explicites : valeurs taggées immédiates, slots `kNullSlot`, raw
`Float`/`Double`, pointeurs de vtable et pointeurs de bytes internes aux
`String`.

Le backend émet maintenant une première carte additive par point d'appel
`Runtime_alloc` généré par le code utilisateur. Elle réutilise les slots de frame
référence-capables comme ensemble conservateur de racines scannables au moment de
l'allocation. Le collecteur actif de cette PR ne consomme pas encore ces cartes :
il s'appuie plutôt sur le scan conservateur de la pile native et des payloads
heap, ce qui garde les registres spillés visibles sans figer immédiatement le
format exact.

### Métadonnées De Racines De Frame GC

Le backend émet désormais, dans `.data`, un descripteur additif par fonction :
`nabla_gc_frame_roots_<fonction>`. Le format initial est volontairement minimal
et non consommé par le runtime : `dq count, offset1, offset2, ...`, où chaque
offset est la distance positive utilisée par le frame `rbp` (`[rbp - offset]`).
Des commentaires assembleur adjacents conservent le nom IR et le type statique de
chaque racine, par exemple `; gc root [rbp - 8] %name: String`, pour rendre la
carte vérifiable dans les tests sans figer une ABI utilisateur.

La sélection est conservatrice mais typée : `Any`, `AnyVal`, `AnyRef`, `String`,
tableaux natifs, `ObjectArray[T]`, `ArrayObject[T]`, fonctions/closures et
classes connues sont racines; les valeurs primitives (`Int`, `Long`, `Float`,
`Double`, `Bool`, `Char`, `Unit`) ne le sont pas. Cette métadonnée ne protège pas
encore les registres transitoires autour de `Runtime_alloc`; elle prépare
seulement la première carte de slots de pile candidats, testable sans collecte.

### Métadonnées De Layout Heap GC

Le backend émet aussi des descripteurs additifs non consommés pour les layouts
heap dont les offsets référence-capables sont connus au moment de la génération :

- `nabla_gc_object_layout_<classe>` : `dq count, offset1, offset2, ...` pour les
  champs d'instances utilisateur/façades dont le type statique peut porter une
  référence. Les commentaires adjacents suivent la forme
  `; gc field [Classe + offset] champ: Type`.
- `nabla_gc_closure_layout_<fonction>_<result>` : `dq count, offset1, offset2,
  ...` pour les captures référence-capables d'une allocation de closure capturante
  précise. Les captures commencent à l'offset `+16`, après le pointeur code et
  l'indicateur de capture.

Ces descripteurs couvrent la partie classes/closures du parcours heap mais ne
suffisent pas encore à activer un GC : `String`, `ObjectArray[T]` brut, valeurs
boxées et singletons runtime gardent des conventions spécialisées.

### Métadonnées De Points D'Allocation GC

Le backend émet aussi, pour chaque fonction, un index additif
`nabla_gc_alloc_calls_<fonction>`. Son premier mot est le nombre de points
d'allocation IR qui appellent directement `Runtime_alloc` dans le code utilisateur
(`NewObject`, `New*Array`, `FunctionReference` et boxing), suivi des adresses des
cartes `nabla_gc_alloc_call_<fonction>_<index>`.

Chaque carte garde le format minimal `dq count, offset1, offset2, ...`, où les
offsets réutilisent les mêmes slots `rbp` positifs que
`nabla_gc_frame_roots_<fonction>`. Des commentaires assembleur adjacents indiquent
le type d'allocation (`object`, `IntArray`, `ObjectArray`, `closure`,
`boxed-Int`, etc.), le résultat IR (`; gc alloc call ...`) et les racines de
frame candidates (`; gc alloc root [rbp - ...]`). Ces cartes sont volontairement
conservatrices, provisoires et non consommées : elles décrivent les slots de
frame référence-capables déjà produits dans le parcours IR linéaire avant le point
d'allocation. Elles ne sont pas encore dominance-aware pour les branches, ne
protègent pas encore les registres transitoires chargés juste avant
`Runtime_alloc`, et ne couvrent pas les allocations réalisées à l'intérieur des
helpers assembleur runtime.

### Inventaire Des Allocations Internes Aux Helpers Runtime

Les allocations réalisées directement dans `src/runtime_asm.cpp` restent hors des
cartes `nabla_gc_alloc_call_*`, parce qu'elles ne passent pas par une instruction
IR utilisateur. Elles sont maintenant inventoriées par test pour éviter qu'une
nouvelle allocation helper soit ajoutée sans décision GC explicite. Les nombres
ci-dessous comptent les sites statiques `call Runtime_alloc`; deux sites peuvent
être alternatifs sur des branches différentes :

- `Runtime_cStringToString` (1) : construit un `String` depuis un C string.
- `Runtime_buildArgsArray` (2) : construit le tableau brut d'arguments puis la
  façade `ArrayObject[String]`.
- `Runtime_readLine` (1) : alloue le buffer `String` de lecture.
- `Runtime_copyPathToCString` (1) : alloue un buffer C transitoire.
- `Runtime_emptyString` (1) : alloue la chaîne vide runtime.
- `Runtime_readFile` (1) : alloue le `String` contenant le fichier lu.
- `Runtime_stringConcat` (1), `Runtime_stringSubstring` (1),
  `Runtime_stringRepeat` (1), `Runtime_stringTrim` (1) : allouent un `String`
  résultat.
- `Runtime_stringToCharArray` (2) : alloue le tableau brut de caractères puis la
  façade `ArrayObject[Char]`.
- `Runtime_stringSplit` (3) : alloue le tableau brut de segments dans les chemins
  séparateur normal/vide puis la façade finale.
- `Runtime_stringSplitMakeSegment` (1) : alloue chaque segment `String` et le
  stocke dans le tableau courant.
- `Int_method_toString` (1), `Bool_method_toString` (2),
  `Char_method_toString` (1), `FloatDouble_method_toString` (2) et
  `Any_toString` (1) : allouent des `String` de conversion.

Ces sites sont non encore couverts par des cartes racines consommables. Les
helpers multi-allocation effectifs (`Runtime_buildArgsArray`,
`Runtime_stringSplit`, `Runtime_stringToCharArray`, `FloatDouble_method_toString`)
restent prioritaires pour le passage futur aux cartes exactes : ils gardent des
références heap dans des registres ou sur la pile native entre deux appels à
`Runtime_alloc`. Le GC actif courant contourne cette limite par le scan
conservateur des registres spillés et de la pile, pas par consommation de ces
cartes. Les premières protections concrètes couvrent
seulement `Runtime_buildArgsArray`, qui spille `r15` sur la pile native autour de
l'appel allocant `Runtime_cStringToString` de la boucle et autour de l'allocation
finale de façade, puis `Runtime_stringToCharArray`, qui spille le owner `String`
source autour de l'allocation du tableau brut de caractères et `rbx` autour de
l'allocation de la façade `ArrayObject[Char]`, puis `Runtime_stringSplit` et
`Runtime_stringSplitMakeSegment`, qui spillent les owners `String` source et
séparateur, le tableau brut `ObjectArray[String]` destination et le owner source
de segment autour de leurs `Runtime_alloc` directs, puis
`FloatDouble_method_toString`, qui spille `r10` comme owner `String` de la
partie entière autour des deux allocations directes des chemins fractionnel et
sans fraction non nulle. Les helpers avec plusieurs sites alternatifs sur
branches exclusives, comme `Bool_method_toString`, restent à documenter mais ne
créent pas le même besoin de conservation entre deux allocations successives.

### Métadonnées De Racines Internes Des Helpers Runtime

Le runtime ASM émet maintenant dans `.data` des cartes candidates pour les
helpers runtime qui allouent plusieurs fois ou qui participent à un helper
multi-allocation :

- `nabla_gc_runtime_helper_allocs_<helper>` : index additif dont le premier mot
  est le nombre de sites `Runtime_alloc` directs couverts pour le helper, suivi
  des labels `nabla_gc_runtime_helper_alloc_<helper>_<index>`.
- `nabla_gc_runtime_helper_alloc_<helper>_<index>` : carte candidate minimale
  pour un site d'allocation interne au helper. Le premier mot est un nombre de
  racines candidates; les noms de registres ou slots natifs sont conservés sous
  forme de descripteurs/commentaires ASM pour les tests structuraux.

Ces cartes couvrent aujourd'hui `Runtime_buildArgsArray`,
`Runtime_stringToCharArray`, `Runtime_stringSplit`,
`Runtime_stringSplitMakeSegment` et `FloatDouble_method_toString`. Elles listent
des racines conservatrices comme `native_stack+8` pour le `r15` spillé qui tient
le tableau brut d'arguments pendant l'allocation finale de
`Runtime_buildArgsArray`, `native_stack+8` pour le owner `String` source ou le
`rbx` spillé de `Runtime_stringToCharArray`, `native_stack+8` /
`native_stack+16` pour les owners `String` source/séparateur spillés de
`Runtime_stringSplit` et pour `r10`/`rbx` spillés de
`Runtime_stringSplitMakeSegment`, et `native_stack+8` pour le `r10` spillé qui
tient la chaîne de partie entière dans `FloatDouble_method_toString`. Le slot
local `[rsp + 16]` de `FloatDouble_method_toString` conserve aussi cette chaîne
dans le helper, mais la racine concrète autour des safepoints est le `push r10`.
Dans `Runtime_stringToCharArray`,
`r10` reste un pointeur intérieur vers les bytes du `String` source et doit être
considéré comme un sujet à rattacher ou recalculer; le candidat racine testable
est désormais le owner spillé. Dans `Runtime_stringSplit`,
`r14`/`r15` restent des pointeurs intérieurs/recalculables non consommables vers
les bytes source/séparateur, et `Runtime_stringSplitMakeSegment` conserve `r14`
avec le même statut. Les cartes signalent aussi les dépendances `interior:*` qui
restent dangereuses pour un GC mobile ou actif.

Ces métadonnées sont runtime-inertes, non consommées par `Runtime_alloc` et non
précises : la collecte active actuelle ne s'appuie pas sur elles, mais sur le
scan conservateur pile/payload. Une entrée
`interior:*` n'est pas une racine consommable : elle documente au contraire un
pointeur intérieur qui devra être rattaché à son objet propriétaire, recalculé
après collection ou interdit comme safepoint. La prochaine étape consiste à
protéger automatiquement les registres/slots natifs listés ou à remplacer ces
cartes candidates par des cartes consommables dont le format et les points sûrs
sont spécifiés, afin de réduire la dépendance aux faux positifs conservateurs.

Ces codes doivent rester documentés au fur et à mesure qu'ils deviennent une
surface observable par l'utilisateur.

## Principes De Maintenance

- Centraliser toute nouvelle convention de représentation dans ce document.
- Ajouter un test de régression avant de changer une convention runtime.
- Garder la surface utilisateur orientée `Array[T]`, `Option[T]`, `Set[T]`,
  `String`, classes, méthodes et lambdas.
- Éviter d'exposer dans les diagnostics des noms internes lorsqu'une forme
  source plus claire existe.
