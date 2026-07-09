# Nabla Language Guide

Nabla est un langage compile vers du natif Linux x86_64. Sa syntaxe est
inspiree de Scala, avec typage statique, classes, fonctions, generiques simples,
collections standard et petites primitives d'entree/sortie.

Ce document decrit l'etat actuel du langage, pas une specification definitive.

## Programme Minimal

Un programme executable expose une fonction globale `main(): Int` ou, s'il veut lire les arguments de ligne de commande, `main(args: Array[String]): Int`.

```nabla
def main(): Int = {
    42
}
```

```nabla
import collections.array

def main(args: Array[String]): Int = {
    if args.size() > 0 && args.get(0) == "ok" { 0 } else { 1 }
}
```

La valeur retournee par `main` devient le code de sortie du programme. Dans la variante `main(args: Array[String])`, `args` contient les arguments utilisateur sans le nom de l'executable, donc `./prog alpha beta` fournit un tableau de taille 2 contenant `"alpha"` puis `"beta"`.

## Compilation

Depuis la racine du depot :

```bash
make
build/nablac examples/command_shell.nabla
./build/command_shell
```

Avec le `Makefile` :

```bash
make test SRC=examples/command_shell.nabla
make all-tests
```

Le compilateur expose aussi quelques modes utiles :

```bash
build/nablac --emit-ir tests/test_arithmetic.nabla
build/nablac --keep-temp tests/test_string_split.nabla
build/nablac --backend-ir tests/test_arithmetic.nabla
```

## Packages Et Imports

Un fichier peut commencer par une declaration `package` Scala-like. Elle donne
un nom logique au module et doit apparaitre avant les imports et declarations :

```nabla
package app.math

def fortyTwo(): Int = {
    42
}
```

`import app.math` charge toujours le fichier `app/math.nabla` selon l'ordre de
resolution existant : d'abord relatif au fichier courant, puis depuis la racine
du projet, puis depuis `stdlib/`. Si le fichier importé déclare un package, ce
nom doit correspondre exactement au chemin importé; sinon le compilateur émet un
diagnostic au point d'import.

```nabla
import app.math

def main(): Int = {
    fortyTwo()
}
```

La V0 reste volontairement compatible avec les imports actuels : les declarations
importees sont encore visibles par leur nom court dans le programme courant
quand ce nom est non ambigu. Deux modules peuvent exposer la meme fonction
top-level courte; importer un seul de ces modules reste valide, mais importer les
deux puis utiliser ce nom court produit un diagnostic d'ambiguite qui nomme les
modules concernes.

Les fonctions top-level importees peuvent aussi etre appelees ou referencees par
leur nom source pleinement qualifie, ce qui contourne l'ambiguite du nom court
quand plusieurs modules importes exposent la meme fonction :

```nabla
import app.alpha.Tools
import app.beta.Tools

def main(): Int = {
    val fn = app.alpha.Tools.value
    fn() + app.beta.Tools.value()
}
```

Les imports selectifs, alias, wildcards explicites, visibilites de package et
l'ambiguite fine des noms de types/classes sont reserves a une evolution
ulterieure.

## Commentaires

Les commentaires de ligne utilisent `//`.

```nabla
// Calcule le score final.
def score(value: Int): Int = {
    value + 1
}
```

## Types

Types de base disponibles :

- `Int`
- `Long`
- `Float`
- `Double`
- `Bool`
- `Char`
- `String`
- `Unit`

Types racines Scala-like :

- `Any` est le supertype de toutes les valeurs Nabla.
- `AnyVal` est le supertype des valeurs primitives builtin : `Unit`, `Bool`,
  `Int`, `Long`, `Float`, `Double` et `Char`.
- `AnyRef` est le supertype des references heap : `String`, tableaux,
  fonctions/closures et classes utilisateur.
- `Nothing` est le type bottom : il est assignable a tout type attendu, mais
  aucune valeur concrete de type `Nothing` ne peut etre construite.

`AnyVal` et `AnyRef` sont des types builtin abstraits. Ils structurent le
systeme de types mais ne sont pas des classes utilisateur instanciables.
`Nothing` sert aux expressions qui ne terminent pas normalement, notamment
`panic(message)` et son alias `error(message)`, qui quittent le programme avec
le statut runtime `250`.

Collections et types standard :

- `Array[T]`
- `Option[T]`
- `Set[T]`
- `Map[K, V]`
- `Sized`

Les noms comme `IntArray`, `LongArray`, `FloatArray`, `DoubleArray`,
`BoolArray`, `ObjectArray[T]`, `ArrayInt`, `ArrayLong`, `ArrayFloat`,
`ArrayDouble`, `ArrayBool` et `ArrayObject[T]` existent encore pour la
compatibilite, les tests bas niveau et l'implementation de la stdlib. Le code
applicatif devrait privilegier `Array[T]` et les compagnons publics.

Les chaines et caracteres sont actuellement byte-based/ASCII pour les operations
de longueur, indexation et decoupe.

Les types numeriques supportent `+`, `-`, `*`, `/`, `%` et les comparaisons.
`Char` participe a ces operations comme une valeur entiere byte-based: les
operations arithmetiques sur des `Char` produisent au minimum un `Int`, tandis
que les comparaisons produisent un `Bool`. Quand une operation ou un type attendu
combine plusieurs types numeriques, Nabla applique uniquement des promotions
implicites elargissantes dans l'ordre `Char -> Int -> Long -> Float -> Double`:
par exemple `Int * Double` produit un `Double`, `Long + Float` produit un
`Float`, un argument `Char` peut etre passe a un parametre ordinaire `Int` ou
`Double`, et un argument `Int` peut etre passe a un parametre ordinaire `Double`
quand l'appel n'a pas besoin de choisir entre plusieurs surcharges. Les
conversions retrecissantes restent explicites ou refusees. Ces promotions sont
des conversions de valeurs, pas du sous-typage: elles ne rendent pas un
`override def f(): Int` compatible avec un parent qui retourne `Double`. Le reste
de division `%` est disponible seulement quand le type promu reste entier
(`Int` ou `Long`). Les primitives exposent aussi les conversions elargissantes
directes disponibles, comme `Char.toInt()`, `Char.toLong()`, `Int.toFloat()`,
`Long.toDouble()` ou `Float.toDouble()`.

## Tuples

Nabla supporte pour l'instant les paires `Tuple2[A, B]`.

Le type `(A, B)` est un alias syntaxique de `Tuple2[A, B]`. Les expressions
`(a, b)` et `a -> b` construisent une valeur `Tuple2`.

```nabla
def label(value: Int): (String, Int) = {
    ("score", value)
}

def main(): Int = {
    val pair = label(42)
    if pair._1 == "score" && pair._2 == 42 {
        0
    } else {
        1
    }
}
```

Les paires exposent `_1`, `_2`, `swap()`, `toString()` et `hashCode()`. Les
tuples vides et les tuples d'arite superieure a 2 ne sont pas encore supportes.

## Variables

`val` declare une valeur non reassignee. `var` declare une variable mutable.
Une annotation de type locale peut etre ajoutee avant `=`; elle sert aussi de
type attendu pour les lambdas inferees et les references de fonctions
surchargees.

```nabla
def main(): Int = {
    val base = 40
    var value: Int = base
    value = value + 2
    value
}
```

## Fonctions

Les fonctions globales ont des parametres types et un type de retour explicite.

```nabla
def add(left: Int, right: Int): Int = {
    left + right
}

def main(): Int = {
    add(20, 22)
}
```

Les appels recursifs directs en position terminale sont optimises en boucle par
le backend. Cela couvre notamment les branches `if` qui retournent directement
la valeur de l'appel recursif.

Le dernier parametre d'une fonction ou methode peut etre repete avec `*`.
Dans le corps, ce parametre est vu comme un `Array[T]`.

```nabla
import collections.array

def join(words: String*): String = {
    words.mkString("-")
}

def main(): Int = {
    if join("a", "b", "c") == "a-b-c" { 42 } else { 1 }
}
```

Un tableau existant peut etre deplie dans un parametre repete avec `: _*` :

```nabla
import collections.array

def join(words: String*): String = {
    words.mkString("-")
}

def main(): Int = {
    val words = Array("a", "b", "c")
    if join(words: _*) == "a-b-c" { 42 } else { 1 }
}
```

Les fonctions globales peuvent etre surchargees par signature. Deux fonctions
peuvent partager le meme nom si la liste de types de leurs parametres les
distingue. L'appel est resolu statiquement depuis les types des arguments :

```nabla
def label(value: Int): Int = {
    value + 1
}

def label(value: String): Int = {
    value.length()
}

def main(): Int = {
    label(41) + label("x")
}
```

Une redéfinition avec exactement les memes types de parametres est refusee. Les
fonctions generiques peuvent participer a la resolution de surcharge : une
signature concrete exacte est prioritaire sur une signature generique inferable,
et une erreur d'ambiguite est levee si plusieurs generiques restent compatibles.

```nabla
def pick(value: Int): Int = {
    value + 1
}

def pick[T](value: T): Int = {
    10
}

def main(): Int = {
    pick(41) + pick("generic") - 10
}
```

Dans cet exemple, `pick(41)` utilise la signature concrete `Int`, tandis que
`pick("generic")` utilise la signature generique. Les references de fonctions
surchargees sont autorisees lorsqu'un type fonction est attendu, par exemple en
argument de fonction d'ordre superieur ou via une annotation locale comme
`val f: (Float) => Float = sqrt`. Les references peuvent aussi inferer une
signature generique depuis le type fonction attendu, avec la meme priorite pour
les signatures concretes. Sans type attendu explicite, elles restent refusees
pour eviter une ambiguite silencieuse.

Les methodes de classe peuvent aussi etre surchargees par signature exacte :

```nabla
class Box() {
    def pick(value: Int): Int = {
        value
    }

    def pick(value: String): Int = {
        value.length()
    }
}

def main(): Int = {
    val box = new Box()
    box.pick(40) + box.pick("hi")
}
```

La surcharge de methodes est resolue statiquement depuis le type du receveur et
les types des arguments. Une signature concrete exacte est prioritaire sur une
signature generique inferable, et plusieurs signatures generiques compatibles
produisent un diagnostic d'ambiguite. Une lambda inferee en argument peut
recevoir son type attendu si l'arite et les positions de lambdas identifient une
seule surcharge; pour une methode generique, les arguments deja lus peuvent
inferer les parametres de type avant de typer la lambda suivante. Si plusieurs
surcharges fonctionnelles restent possibles, un diagnostic d'ambiguite liste les
signatures candidates. Hors de ce cas, les conversions implicites sont limitees
aux promotions numeriques elargissantes (`Int -> Long -> Float -> Double`) et ne
servent pas a choisir une surcharge non exacte.

`Unit` sert aux fonctions a effet.

```nabla
import io

def greet(name: String): Unit = {
    println("hello " + name)
}
```

## Controle De Flux

`if` est une expression : quand les deux branches produisent le meme type, ce
type devient celui du `if`. Quand les branches produisent deux types differents,
le `if` est type `Unit`, ce qui permet de l'utiliser naturellement pour des
effets de bord. Les chaines `else if` sont acceptees, mais une branche finale
`else` reste obligatoire.

Les conditions de `if` et `while` doivent être de type `Bool`; le compteur de
`for` doit être de type `Int`. Les opérandes de `&&`, `||` et `!` sont aussi
validés comme `Bool`, avec diagnostics sur l'expression fautive.

```nabla
def max(left: Int, right: Int): Int = {
    if left > right {
        left
    } else if left == right {
        left
    } else {
        right
    }
}
```

`while` sert aux boucles imperatives.

```nabla
def sumTo(limit: Int): Int = {
    var value = 0
    var total = 0
    while (value <= limit) {
        total = total + value
        value = value + 1
    }
    total
}
```

`match` compare une expression a des motifs avec `==`:
- des motifs litteraux `Int`, `Long`, `Float`, `Double`, `Bool`, `String` et
  `Char`;
- un motif nommé (ex. `valeur`) qui capture la valeur courante;
- un motif de constructeur V0, par exemple `Point(x, y)`, `Cons(head, tail)` ou
  le singleton runtime `Nil`;
- un motif `_` final.
Le motif peut aussi être suivi d'une garde booléenne avec `if`.
Les variables capturées par un motif nommé ou un motif de constructeur sont
locales à la branche et ne sortent pas du `match`.

```nabla
def commandCode(command: String): Int = {
    match command {
        "add" => 1
        "quit" => 2
        _ => 0
    }
}
```

On peut aussi filtrer une branche avec une garde booléenne :

```nabla
def describe(value: Int): String = {
    match value {
        1 => "petit un"
        current if current > 1 => "grand"
        _ => "autre"
    }
}
```

Les motifs de constructeur V0 testent le type runtime exact du constructeur puis
lient les champs du constructeur dans l'ordre. Les sous-motifs imbriqués,
alternatives et extracteurs ne sont pas encore supportés; chaque argument est un
identifiant de binding ou `_`.

```nabla
import collections.list

def sum(values: List[Int]): Int = {
    match values {
        Nil => 0
        Cons(head, tail) => head + sum(tail)
        _ => 0
    }
}
```

Toutes les branches d'un `match` doivent produire le meme type. Une branche peut
aussi utiliser un bloc :

```nabla
match value {
    1 => {
        val label = "one"
        label.length()
    }
    _ => 0
}
```

## Classes

Une classe declare ses champs dans le constructeur primaire. Par défaut, ces
champs sont visibles dans les méthodes de la classe. Un paramètre constructeur
préfixé par `val` expose aussi un accesseur public zéro-argument du même nom.
Un paramètre préfixé par `var` expose le même getter et autorise la réaffectation
du champ depuis les méthodes de la classe.

```nabla
class Student(val name: String, val score: Int) {
}

def main(): Int = {
    val ada = new Student("Ada", 19)
    ada.score() + 23
}
```

`this` est disponible implicitement dans les methodes. L'accesseur généré par
`val score: Int` ou `var score: Int` se consomme comme une méthode, par exemple
`ada.score()` ; l'accès propriété sans parenthèses (`ada.score`) appelle aussi
ce getter zéro-argument quand la résolution le permet. La V1 de `var` constructeur
n'expose pas encore de setter public `ada.score = ...` : l'affectation supportée
est interne à la classe.

```nabla
class Counter(var value: Int) {
    def add(delta: Int): Int = {
        value = value + delta
        value
    }
}
```

## Objets Statiques

Un `object` déclare un namespace statique inspiré des objets compagnons Scala.
Sans clause `with`, un objet ne possède pas d'identité runtime, de champs, ni de
valeur singleton manipulable : seules des fonctions statiques sont autorisées
dans son bloc.

```nabla
object MathTools {
    def abs(value: Int): Int = {
        if value < 0 { -value } else { value }
    }
}

def main(): Int = {
    MathTools.abs(-42)
}
```

Un objet peut partager son nom avec une classe pour former un compagnon de
surface :

```nabla
class Box[T](value: T) {
    def get(): T = { value }
}

object Box {
    def of[T](value: T): Box[T] = {
        new Box[T](value)
    }
}
```

Si un objet expose une methode `apply`, `Name(...)` est un raccourci pour
`Name.apply(...)`. C'est notamment utilise par `Array(1, 2, 3)`.

Un `object` qui compose au moins un trait avec `with` devient en revanche un
singleton runtime V0. Il est utilisable comme valeur, assignable a ses traits,
a `AnyRef` et a `Any`, et ses methodes redispatchent comme celles d'une classe.
Il ne supporte pas `extends`, les champs, les constructeurs, les arguments de
type, ni l'initialisation dédiée. Un singleton runtime ne s'instancie pas avec
`new` et ne peut pas servir de parent de classe.

```nabla
trait Named {
    def name(): String
}

object RuntimeName with Named {
    override def name(): String = {
        "runtime"
    }
}

def main(): Int = {
    val named: Named = RuntimeName
    if named.name() == "runtime" { 42 } else { 1 }
}
```

Les méthodes d'un singleton runtime suivent les règles de validation de classe :
les méthodes abstraites des traits doivent être implémentées, `override` est
obligatoire pour les membres hérités, les signatures doivent correspondre, et
les conflits de méthodes concrètes héritées doivent être résolus explicitement.

## Héritage

Une classe peut heriter d'une classe base via une clause `extends` optionnelle,
et composer sa hiérarchie via des mixins avec `with`.

```nabla
class Entity(nameValue: String) {
    def name(): String = {
        nameValue
    }
}

class Person extends Entity(nameValue: String, ageValue: Int) {
    def age(): Int = {
        ageValue
    }
}

def main(): Int = {
    val person = new Person("Ada", 19)
    if person.name() == "Ada" {
        person.age()
    } else {
        0
    }
}

class CanWork() {
    def canWork(): Bool = {
        true
    }
}

class Worker extends Person with CanWork(nameValue: String, ageValue: Int) {
    def title(): String = {
        "eng"
    }
}
```

Dans une signature héritée comme `extends Entity(nameValue: String, ageValue:
Int)`, le préfixe doit reprendre les champs du parent direct dans l'ordre de son
layout. Les champs restants deviennent les champs propres de la classe enfant.
Cette forme rend le constructeur public explicite sans répéter un appel parent
séparé.

La resolution des methodes suit une recherche dans la classe courante, puis dans
le parent puis les mixins dans l'ordre d'énonciation (`with`). Les conflits
de méthodes sont détectés également quand une méthode ambiguë provient d'une
chaîne d'héritage transitive.
Quand une même méthode existe dans plusieurs parents/mixins sans redéfinition
dans la classe courante, une erreur de conflit d'héritage est levée.

Dans une méthode, `super` permet d'appeler un membre de la classe parente directe.

```nabla
class Base() {
    def label(): String = {
        "base"
    }
}

class Child extends Base() {
    def label(): String = {
        "child"
    }

    def baseLabel(): String = {
        super.label()
    }
}
```

## Traits

`trait` declare un contrat nominal sans etat. Un trait peut contenir des
signatures abstraites sans corps et des methodes concretes par defaut.

```nabla
trait Named {
    def name(): String

    def label(): String = {
        "name:" + this.name()
    }
}

class Person(val name: String) with Named {
}
```

Un accesseur `val` généré peut satisfaire une méthode abstraite de trait de même
signature, sans écrire `override`. Une redéfinition manuelle d'une méthode
concrète héritée continue en revanche à exiger `override`.

Les traits se composent avec `with`, sur une classe ou sur un autre trait :

```nabla
trait Sized {
    def size(): Int

    def nonEmpty(): Bool = {
        this.size() > 0
    }
}

trait NamedSized with Sized {
    def name(): String
}
```

Une classe generique peut composer un trait non generique. Le type du trait
reste nominal et les appels via une valeur du trait redispatchent vers
l'implementation de la classe concrete :

```nabla
class Box[T](value: T) with Sized {
    override def size(): Int = {
        1
    }
}
```

Une classe concrete doit implementer toutes les methodes abstraites heritees
depuis ses traits. L'implementation ou le remplacement d'une methode de trait
doit etre marquee avec `override`, comme pour les methodes de classe heritees.
Si deux traits apportent une methode concrete de meme signature, la classe doit
fournir son propre `override`; Nabla ne fait pas de linearisation implicite.

Limites de la version actuelle :

- un trait ne peut pas etre instancie avec `new`;
- un trait ne declare ni constructeur ni champ d'instance;
- `super` est interdit dans un trait;
- les traits n'ont pas d'etat runtime singleton.

Quand une classe n'indique pas de parent, la racine implicite ajoutee est
`AnyRef`. `AnyRef` herite de `Any`, et `AnyVal` herite aussi de `Any` pour les
types primitifs builtin. La hierarchie de surface est donc :

```text
Any
├── AnyVal  // Unit, Bool, Int, Long, Float, Double, Char
├── AnyRef  // String, tableaux, fonctions/closures, classes utilisateur
└── Nothing // bottom type, sous-type assignable a tous les types
```

`Any` apporte des methodes de base disponibles sur toute classe reference :
`toString(): String`, `hashCode(): Int` et `equals(other: Any): Bool`. Ces
methodes peuvent etre redefinies dans les sous-classes. Les appels a
`toString()`, `hashCode()` et `equals(...)` redispatchent vers l'override runtime
quand la valeur est manipulée via `Any`, un type parent ou un paramètre générique
spécialisé. Pour les objets, `==` et `!=` s'appuient sur `equals(...)`; le
fallback de `Any.equals` conserve l'égalité par identité. Les primitives
disposent aussi de chemins specialises comme `Int.toString()` ou
`Double.toString()`. Quand une primitive est passée à un paramètre `Any` ou
`AnyVal` de fonction/méthode, le compilateur insère un boxing runtime minimal :
`value.toString()` conserve alors le rendu spécialisé (`true`, `Z`,
`1.500000`, etc.).

## Generiques

Les fonctions et classes peuvent etre parametrees par type.

```nabla
def identity[T](value: T): T = {
    value
}

class Pair[T](firstValue: T, secondValue: T) {
    def first(): T = {
        firstValue
    }

    def second(): T = {
        secondValue
    }
}

def main(): Int = {
    val pair = new Pair[Int](40, 2)
    identity(pair.first() + pair.second())
}
```

L'inference des arguments de type fonctionne pour les appels de fonctions dans
les cas simples :

```nabla
identity(42)
```

## Fonctions Valeurs Et Lambdas

Les types fonction s'ecrivent avec une syntaxe parenthesee. La liste de
parametres peut etre vide pour exprimer un thunk zero-argument `() => T`.

```nabla
def apply(value: Int, f: (Int) => Int): Int = {
    f(value)
}

def main(): Int = {
    apply(41, value => value + 1)
}
```

```nabla
def eval(thunk: () => Int): Int = {
    thunk()
}

def main(): Int = {
    val thunk: () => Int = () => {
        42
    }
    eval(thunk)
}
```

Les lambdas peuvent capturer des valeurs locales.

```nabla
def main(): Int = {
    val offset = 2
    val f = (value: Int) => value + offset
    f(40)
}
```

Les champs de classe de type fonction peuvent aussi etre appeles depuis les
methodes de la classe.

Les paramètres de fonction peuvent être déclarés par nom avec une syntaxe
Scala-like `name: => T`. Le compilateur les abaisse vers un thunk zéro-argument
`name: () => T`: l'appelant fournit une expression normale, et chaque usage de
`name` dans le corps réévalue cette expression.

```nabla
class Counter(var value: Int) {
    def next(): Int = {
        value = value + 1
        value
    }
}

def twice(x: => Int): Int = {
    x + x
}

def main(): Int = {
    val counter = new Counter(0)
    twice(counter.next()) // 1 + 2 == 3
}
```

La forme `=> T` est réservée aux positions paramètre; pour stocker explicitement
un thunk dans une valeur ou un champ, utilisez `() => T`.

## Chaines

Operations disponibles sur `String` :

- `length(): Int`
- `charAt(index: Int): Char`
- `toCharArray(): ArrayObject[Char]`
- `toInt(): Int`
- `toFloat(): Float`
- `toDouble(): Double`
- `substring(from: Int, until: Int): String`
- `repeat(count: Int): String`
- `trim(): String`
- `split(separator: String): ArrayObject[String]`
- `indexOf(needle: String): Int`
- `contains(needle: String): Bool`
- `startsWith(prefix: String): Bool`
- `endsWith(suffix: String): Bool`
- `isEmpty(): Bool`
- `nonEmpty(): Bool`
- `+`, `==`, `!=`

`toCharArray()` et `split(...)` exposent actuellement `ArrayObject[...]`. C'est
la representation actuelle des tableaux de types non primitifs ; le code
utilisateur devrait surtout utiliser les operations communes (`size`, `get`,
`mkString`, `foreach`, etc.) et garder `Array[T]` pour construire ses propres
tableaux.

`toInt()`, `toFloat()` et `toDouble()` échouent avec le code runtime `253` si la
chaine ne contient pas un nombre valide. Les formes globales `parseInt(value)`,
`parseFloat(value)` et `parseDouble(value)` sont des alias équivalents. Les
conversions `Float`/`Double` acceptent un signe optionnel, des chiffres avant
ou après un unique point décimal (`"12"`, `"12.5"`, `".5"`, `"5."`), sans
espaces ni exposant (`"1e2"` est rejeté).

```nabla
import collections.array

def main(): Int = {
    val command = "  set 3 4  ".trim().split(" ")
    if command.get(0) == "set" &&
       command.get(1).toInt() == 3 &&
       "na".repeat(3) == "nanana" {
        42
    } else {
        1
    }
}
```

`split("")` decoupe une chaine en caracteres. Les segments vides sont conserves :

```nabla
"a,,b,".split(",").mkString("|") // "a||b|"
```

### Pression Heap Des Chaines

Les operations `+`, `repeat`, `substring`, `trim`, `split` et `toCharArray`
allouent de nouvelles valeurs sur le heap runtime. Le collecteur conservateur
non compactant peut récupérer des chaînes devenues inaccessibles, mais les faux
positifs et la fragmentation restent possibles; pour les traitements
volumineux, préférer une accumulation bornée, factoriser les transformations
dans quelques tableaux réutilisés quand c'est possible, et augmenter
explicitement la capacité avec `nablac --heap-size <octets>` si le volume attendu
est connu. Un dépassement persistant après collecte termine le programme avec le
diagnostic `Nabla runtime error: heap exhausted` sur stderr et le code de sortie
255.

## Tableaux Et Collections

Le module `collections.array` donne acces a la facade generique `Array[T]` et
aux fonctions communes. C'est l'API recommandee pour le code utilisateur :
les types plus bas niveau comme `IntArray`, `ArrayInt`, `ObjectArray[T]` et
`ArrayObject[T]` sont surtout des details d'implementation de la bibliotheque.
Les fabriques comme `Array.fill`, `Array.tabulate`, `Array.range` et les
operations qui retournent un nouveau tableau allouent sur le même heap runtime :
préférer réutiliser un tableau mutable via `set` quand le volume est grand et
que l'algorithme le permet.

```nabla
import collections.array

def main(): Int = {
    val scores = new Array[Int](3)
    scores.set(0, 10)
    scores.set(1, 20)
    scores.set(2, 30)

    val weights = new Array[Double](2)
    weights.set(0, 1.5)
    weights.set(1, 2.5)

    val values = Array.fill[Int](3, 7)
    val doubled = values.map(value => value * 2)
    val literal = Array(1, 2, 3)
    val empty = Array.empty[Int]()
    val squares = Array.tabulate[Int](4, index => index * index)
    val indexes = Array.range(3)
    val oneBased = Array.range(1, 4)
    if scores.mkString(",") == "10,20,30" &&
       weights.toString() == "[1.500000, 2.500000]" &&
       doubled.mkString(",") == "14,14,14" &&
       literal.toString() == "[1, 2, 3]" &&
       empty.isEmpty() &&
       squares.toString() == "[0, 1, 4, 9]" &&
       indexes.toString() == "[0, 1, 2]" &&
       oneBased.toString() == "[1, 2, 3]" {
        42
    } else {
        1
    }
}
```

Pour creer un tableau de n'importe quel type `T`, l'API recommandee est donc :

- `new Array[T](size)` pour creer un tableau modifiable de taille fixe ;
- `Array(value1, value2, ...)` pour creer un tableau depuis des elements ;
- `Array.empty[T]()` pour creer un tableau vide ;
- `Array.fill[T](size, value)` pour creer un tableau deja rempli ;
- `Array.fill[T](n)(elem)` pour creer un tableau en reevaluant `elem` par nom
  pour chaque case ;
- `Array.tabulate[T](size, f)` pour creer un tableau en calculant chaque
  element depuis son index ;
- `Array.range(size)` pour obtenir `[0, 1, ..., size - 1]` en `Array[Int]` ;
- `Array.range(start, until)` pour obtenir `[start, ..., until - 1]` en
  `Array[Int]`.

Exemple avec un type utilisateur :

```nabla
import collections.array

class Student(name: String) {
    override def toString(): String = {
        name
    }
}

def main(): Int = {
    val students = new Array[Student](2)
    students.set(0, new Student("ada"))
    students.set(1, new Student("linus"))
    if students.toString() == "[ada, linus]" {
        42
    } else {
        1
    }
}
```

Operations courantes :

- `size()` / `length()`
- `isEmpty()` / `nonEmpty()`
- `get(index)`
- `set(index, value)`
- `map`
- `filter`
- `fold`
- `flatMap`
- `foreach`
- `mkString(separator)` pour les tableaux de `Int`, `Long`, `Bool` et `String`

Les anciens noms `ArrayFill[T](...)`, `ArrayRange(size)` et les noms bas niveau
comme `arrayFill[Int](...)`, `arrayIntRange(size)`,
`arrayIntRangeUntil(start, until)`, `new IntArray(size)` ou
`new ObjectArray[T](size)` restent disponibles pour compatibilite et pour la
stdlib, mais `Array[T]`, `Array(...)`, `Array.apply`, `Array.empty`,
`Array.fill`, `Array.tabulate` et les surcharges `Array.range` sont les noms a
privilegier dans le code utilisateur. `Array.rangeUntil(start, until)` reste
disponible comme alias de compatibilite.

`intRangeUntil(start, until)` produit une plage `Int` paresseuse. Ses operations
`foreach`, `fold`, `map` et `max` evitent de construire un `IntArray`
intermediaire pour les parcours simples.

```nabla
import collections.int_array

def main(): Int = {
    intRangeUntil(1, 5)
        .map(value => value * value)
        .max()
        .getOrElse(0)
}
```

`Array[Int]`, `Array[Long]`, `Array[Float]`, `Array[Double]` et `Array[Bool]`
sont specialises en interne vers des facades primitives. Les autres types
passent aujourd'hui par `ArrayObject[T]`. Ces noms de representation peuvent
apparaitre dans certains retours ou diagnostics, mais ne sont pas l'API
idiomatique.

Le module `collections.set` fournit une structure `Set[T]` immutable basée sur
un tableau interne, avec déduplication par `==` et table de hachage interne
(`hashCode()`) pour des vérifications d’appartenance rapides en moyenne. Si une
classe redéfinit `hashCode()`, cette méthode est utilisée même dans un
`Set[Parent]` contenant des instances de sous-types.

```nabla
import collections.set
import collections.array

def main(): Int = {
    val values = Array(1, 3, 1, 2)
    val deduped = Set(values: _*)
    val literal = Set(5, 5, 8)
    val numbers = Set.empty[Int]().add(3).add(1).add(3).add(2)
    val more = Set.empty[Int]().add(2).add(4).add(1)
    if numbers.union(more).toString() == "[3, 1, 2, 4]" &&
       deduped.toString() == "[1, 3, 2]" &&
       literal.toString() == "[5, 8]" &&
       numbers.intersect(more).toString() == "[1, 2]" &&
       numbers.difference(more).toString() == "[3]" {
        42
    } else {
        1
    }
}
```

Operations utiles :

- `Set(value1, value2, ...): Set[T]`
- `Set.apply[T](values: T*): Set[T]`
- `Set.empty[T](): Set[T]`
- `Set.fromArray[T](values: Array[T]): Set[T]`
- `size()`
- `isEmpty()` / `nonEmpty()`
- `contains(value: T): Bool`
- `add(value: T): Set[T]`
- `remove(value: T): Set[T]`
- `union(other: Set[T]): Set[T]`
- `intersect(other: Set[T]): Set[T]`
- `difference(other: Set[T]): Set[T]`
- `clear(): Set[T]`
- `toArray(): ArrayObject[T]`
- `toString(): String`

Les noms bas niveau `setEmpty`, `setFromArray`, `ObjectArray[T]` et
`ArrayObject[T]` restent visibles dans certains diagnostics et modules importes.
Le code utilisateur devrait preferer `Set.empty[T]()` et
`Set.fromArray[T](values)`.

`toArray()` retourne encore `ArrayObject[T]`, qui est la representation actuelle
du tableau generique d'objets. Pour construire ou passer des tableaux dans le
code applicatif, preferer la facade `Array[T]`.

## Map

Le module `collections.map` fournit `Map[K, V]`, un dictionnaire immutable. Les
cles sont comparees avec `==` et indexees avec `hashCode()`, comme pour
`Set[T]`.

```nabla
import collections.map

def main(): Int = {
    val scores = Map("ada" -> 42, "linus" -> 39)
    val updated = scores.updated("linus", 41)
    if updated.contains("ada") &&
       updated.getOrElse("linus", 0) == 41 &&
       updated.size() == 2 {
        42
    } else {
        1
    }
}
```

Operations utiles :

- `Map(value1 -> value2, ...): Map[K, V]`
- `Map.apply[K, V](entries: Tuple2[K, V]*): Map[K, V]`
- `Map.empty[K, V](): Map[K, V]`
- `Map.fromArray[K, V](entries: Array[Tuple2[K, V]]): Map[K, V]`
- `size()`
- `isEmpty()` / `nonEmpty()`
- `contains(key)` / `containsKey(key)`
- `getOption(key): Option[V]`
- `getOrElse(key, default): V`
- `updated(key, value): Map[K, V]`
- `removed(key): Map[K, V]`
- `clear(): Map[K, V]`
- `keys()`, `values()`, `toArray()`
- `foreachEntry(f)`
- `mapValues[U](default, f)`
- `filterKeys(predicate)`
- `mkString(separator)` / `toString()`

Les tableaux retournes par `keys()`, `values()` et `toArray()` utilisent
actuellement `ArrayObject[...]` pour les entrees generiques. C'est un detail de
representation ; les exemples et le code applicatif devraient utiliser
`Map(...)`, `Map.empty`, `Map.fromArray` et `Array[T]` comme surface publique.

## Option

Le module `core.option` fournit `Option[T]`.

```nabla
import core.option

def main(): Int = {
    val some = Option.some[String]("hello")
    val none = Option.none[String]()

    if some.getOrElse("x") == "hello" &&
       none.getOrElse("fallback") == "fallback" {
        42
    } else {
        1
    }
}
```

Operations courantes :

- `Option.some[T](value): Option[T]`
- `Option.none[T](): Option[T]`
- `isEmpty()`
- `nonEmpty()`
- `getOrElse(default)`
- `map[U](default, f)`
- `flatMap[U](default, f)`
- `foreach(f)`
- `filter(predicate)`
- `orElse(fallback)`

Le nom de compatibilite `optionSome[T](value)` reste disponible pour le code
existant, mais les nouvelles absences doivent utiliser `Option.none[T]()`.

## Entree Et Sortie

Le module `io` expose les helpers recommandes pour les entrees/sorties :

- `println(value: Any): Unit` — convertit la valeur avec `toString()` avant l'affichage
- `input(): String`
- `readTextFile(path: String): String`
- `writeTextFile(path: String, content: String): Int`
- `appendTextFile(path: String, content: String): Int`
- `deleteTextFile(path: String): Bool`
- `renameTextFile(from: String, to: String): Bool`
- `createDirectory(path: String): Bool`
- `pathExists(path: String): Bool`

```nabla
import io
import strings

def main(): Int = {
    println("Enter a command:")
    val parts = words(input())
    println("tokens: " + parts.size().toString())
    42
}
```

Les primitives globales `print(value: Any)`, `readLine(): String`,
`readFile(path: String): String`, `writeFile(path: String, content: String): Int`,
`appendFile(path: String, content: String): Int`, `deleteFile(path: String): Bool`,
`renameFile(path: String, to: String): Bool`, `createDir(path: String): Bool`
et `fileExists(path: String): Bool`
existent aussi, mais le module `io` est l'interface recommandee. `print` abaisse
son argument vers `Any.toString()` puis imprime la chaine obtenue : les primitives
conservent leur rendu specialise et les classes utilisateur passent par leur
override de `toString()` quand il existe.

Lecture et ecriture de fichiers texte :

```nabla
import io

def main(): Int = {
    val written = writeTextFile("build/message.txt", "hello")
    val appended = appendTextFile("build/message.txt", "\nagain")
    val loaded = readTextFile("build/message.txt")
    val deleted = deleteTextFile("build/message.txt")

    if written == 5 && appended == 6 && pathExists("build/message.txt") &&
       loaded == "hello\nagain" && deleted && !pathExists("build/message.txt") {
        42
    } else {
        1
    }
}
```

Conventions actuelles :

- `readTextFile` lit le fichier texte complet dans le tas Nabla.
- Si un fichier ne peut pas etre ouvert ou lu, `readTextFile` retourne une
  chaine vide.
- `pathExists` et `fileExists` permettent de distinguer un fichier absent d'un
  fichier vide.
- `writeTextFile` cree ou tronque le fichier.
- `appendTextFile` cree le fichier si besoin puis ajoute le contenu a la fin.
- `deleteTextFile` supprime un fichier et retourne `false` si la suppression
  echoue.
- `renameTextFile` renomme `from` vers `to` et renvoie `true` en cas de
  reussite.
- `createDirectory` cree un repertoire et renvoie `true` en cas de succes.
- `writeTextFile` et `appendTextFile` retournent le nombre d'octets ecrits.
- Une valeur negative indique une erreur de syscall Linux. Nabla n'a pas encore
  de type `Result` ni d'abstraction `errno`.
- Les fichiers tres volumineux peuvent epuiser le heap runtime actuel.
- Les dossiers parents ne sont pas crees automatiquement.

## Module `strings`

`strings.words(text)` decoupe une ligne en mots separes par des espaces et
ignore les segments vides. C'est l'outil recommande pour parser des commandes
simples.

```nabla
import strings

def main(): Int = {
    val parts = words("  add   2   40  ")
    if parts.mkString("|") == "add|2|40" {
        42
    } else {
        1
    }
}
```

## Imports

Un import utilise un chemin de module.

```nabla
import io
import strings
import collections.array
import core.option
```

La resolution cherche le fichier relatif au fichier courant, a la racine du
projet, puis dans `stdlib/`.

## Limites Actuelles

- Les chaines sont byte-based/ASCII, pas encore Unicode.
- La memoire utilise par défaut un heap `mmap` de 8 MiB. Chaque allocation a un
  header runtime caché, puis un payload stable retourné au programme; Nabla
  n'expose pas de `delete` mémoire ni de `free` source. Pour représenter
  l'absence de valeur, utiliser `Option[T]` plutôt qu'un `null` public.
  `nablac --heap-size <octets>` permet de compiler un exécutable avec une autre
  capacité de heap (minimum 4096 octets, maximum représentable par `Int`). Le
  runtime active une première collecte traçante conservative non compactante en
  cas de pression heap: il scanne la pile native et les payloads heap marqués,
  puis réutilise les blocs sweepés via une free-list. Les grands blocs libres
  sont découpés quand la queue restante peut former un bloc réutilisable, ce qui
  réduit la fragmentation immédiate; les faux positifs restent possibles et
  aucune compaction n'est effectuée. Les primitives d'observation
  `heapUsed(): Int` et `heapCapacity(): Int` retournent respectivement le
  high-water mark du pointeur bump et la capacité compilée de l'exécutable; elles
  ne représentent pas encore la mémoire vivante après sweep. Les compteurs
  runtime `gcCollections(): Int`, `gcLastFreedBytes(): Int`,
  `gcLastLargestFreeBlock(): Int`, `gcLastMarkedBlocks(): Int`,
  `gcLastFreedBlocks(): Int`, `gcLastStackWords(): Int`,
  `gcLastHeapWords(): Int`, `gcLastStackCandidateWords(): Int`,
  `gcLastHeapCandidateWords(): Int`,
  `gcLastStackInteriorCandidateWords(): Int`,
  `gcLastHeapInteriorCandidateWords(): Int`,
  `gcLastAllocSafepointMapFound(): Int`,
  `gcLastAllocSafepointMapMissed(): Int`,
  `gcLastAllocSafepointRootSlots(): Int`,
  `gcLastAllocSafepointRootBytes(): Int`, `heapAllocatedBytes(): Int`,
  `heapFreeBytes(): Int`, `heapFreeBlockCount(): Int` et
  `heapLargestFreeBlock(): Int` exposent l'observabilité GC pour les tests et
  diagnostics: nombre de collectes, octets/libre le plus grand bloc de la
  dernière collecte, blocs marqués/libérés, volume de scan conservateur pile/heap,
  mots candidats qui ressemblent à des pointeurs heap, sous-ensemble de
  candidats intérieurs au payload, résolution observationnelle du return PC
  d'allocation utilisateur vers une carte exacte non consommée, nombre de slots
  déclarés dans l'en-tête de cette carte et octets correspondants (`slots * 8`)
  sans lire les offsets de racines, puis état courant de la free-list et payload
  encore alloué. En cas de
  dépassement persistant après collecte, le runtime écrit `Nabla runtime error:
  heap exhausted` sur stderr et termine avec le code 255. Les sections `Chaines`
  et `Tableaux Et Collections` listent les mitigations usuelles pour réduire la
  pression heap sans changer le modèle mémoire.
- Les objets utilisateur stockent un pointeur de vtable backend dans leur
  header et les appels de méthodes utilisateur sont virtuels par défaut quand
  la valeur est manipulée via un type parent, y compris pour un parent générique
  instancié, une méthode générique spécialisée, un trait comme `Iterable[T]` ou
  `Sized`, et les méthodes de base `Any.toString` / `Any.hashCode` /
  `Any.equals`. `super` reste un appel statique. Cette vtable est une convention
  interne du backend, pas une ABI publique stable.
- Les `object` sans `with` restent des namespaces statiques. Les `object` avec
  `with Trait` sont des singletons runtime V0, mais sans champs, constructeur,
  `extends`, arguments de type ou initialisation dédiée.
- La monomorphisation des classes generiques est encore limitee aux cas
  supportes par la suite actuelle.
- Les fonctions generiques ne sont pas encore des valeurs polymorphes.

## Exemples

Voir :

- `examples/command_shell.nabla`
- `examples/game_of_life.nabla`
- `examples/student_scores.nabla`
- `examples/stdlib_collections_cookbook.nabla`
- `examples/stdlib_text_cookbook.nabla`
- `examples/interactive_confirm.nabla`

Voir aussi `docs/cookbook.md` pour des exemples courts centres sur la stdlib
publique.
