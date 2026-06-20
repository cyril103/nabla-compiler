# Nabla Language Guide

Nabla est un langage compile vers du natif Linux x86_64. Sa syntaxe est
inspiree de Scala, avec typage statique, classes, fonctions, generiques simples,
collections standard et petites primitives d'entree/sortie.

Ce document decrit l'etat actuel du langage, pas une specification definitive.

## Programme Minimal

Un programme executable expose une fonction globale `main(): Int`.

```nabla
def main(): Int = {
    42
}
```

La valeur retournee par `main` devient le code de sortie du programme.

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

`AnyVal` et `AnyRef` sont des types builtin abstraits. Ils structurent le
systeme de types mais ne sont pas des classes utilisateur instanciables.

Collections et types standard :

- `IntArray`, `LongArray`, `FloatArray`, `DoubleArray`, `BoolArray`
- `ObjectArray[T]`
- `Array[T]`
- `ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble`, `ArrayBool`
- `ArrayObject[T]`
- `Option[T]`

Les chaines et caracteres sont actuellement byte-based/ASCII pour les operations
de longueur, indexation et decoupe.

Les types numeriques supportent `+`, `-`, `*`, `/` et les comparaisons. Le
reste de division `%` est disponible pour `Int` et `Long`.

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
signatures candidates. Les conversions implicites restent volontairement limitees
a la resolution exacte actuelle.

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
- un motif `_` final.
Le motif peut aussi être suivi d'une garde booléenne avec `if`.
La variable nommée est locale à la branche et ne sort pas du `match`.

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

Une classe declare ses champs dans le constructeur primaire.

```nabla
class Student(nameValue: String, scoreValue: Int) {
    def name(): String = {
        nameValue
    }

    def score(): Int = {
        scoreValue
    }
}

def main(): Int = {
    val ada = new Student("Ada", 19)
    ada.score() + 23
}
```

`this` est disponible implicitement dans les methodes.

## Objets Statiques

Un `object` déclare un namespace statique inspiré des objets compagnons Scala.
Dans l'implémentation actuelle, un objet ne possède pas encore d'identité
runtime, de champs, ni de valeur singleton manipulable : seules des fonctions
statiques sont autorisées dans son bloc.

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

Quand une classe n'indique pas de parent, la racine implicite ajoutee est
`AnyRef`. `AnyRef` herite de `Any`, et `AnyVal` herite aussi de `Any` pour les
types primitifs builtin. La hierarchie de surface est donc :

```text
Any
├── AnyVal  // Unit, Bool, Int, Long, Float, Double, Char
└── AnyRef  // String, tableaux, fonctions/closures, classes utilisateur
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

Les types fonction s'ecrivent avec une syntaxe parenthesee.

```nabla
def apply(value: Int, f: (Int) => Int): Int = {
    f(value)
}

def main(): Int = {
    apply(41, value => value + 1)
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

## Chaines

Operations disponibles sur `String` :

- `length(): Int`
- `charAt(index: Int): Char`
- `toCharArray(): ArrayObject[Char]`
- `toInt(): Int`
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

## Tableaux Et Collections

Le module `collections.array` donne acces a la facade generique `Array[T]` et
aux fonctions communes. C'est l'API recommandee pour le code utilisateur :
les types plus bas niveau comme `IntArray`, `ArrayInt`, `ObjectArray[T]` et
`ArrayObject[T]` sont surtout des details d'implementation de la bibliotheque.

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
    val oneBased = Array.rangeUntil(1, 4)
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
- `Array.tabulate[T](size, f)` pour creer un tableau en calculant chaque
  element depuis son index ;
- `Array.range(size)` pour obtenir `[0, 1, ..., size - 1]` en `Array[Int]` ;
- `Array.rangeUntil(start, until)` pour obtenir `[start, ..., until - 1]` en
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
`Array.fill`, `Array.tabulate`, `Array.range` et `Array.rangeUntil` sont les
noms a privilegier dans le code utilisateur.

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
sont specialises vers des facades primitives. Les autres types passent par
`ArrayObject[T]`.

Le module `collections.set` fournit une structure `Set[T]` immutable basée sur
un tableau interne, avec déduplication par `==` et table de hachage interne
(`hashCode()`) pour des vérifications d’appartenance rapides en moyenne. Si une
classe redéfinit `hashCode()`, cette méthode est utilisée même dans un
`Set[Parent]` contenant des instances de sous-types.

```nabla
import collections.set
import collections.array

def main(): Int = {
    val values = new Array[Int](4)
    values.set(0, 1)
    values.set(1, 3)
    values.set(2, 1)
    values.set(3, 2)

    val deduped = Set.fromArray[Int](values)
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

Les noms de compatibilite `SetEmpty`, `SetFromArray`, ainsi que les noms bas
niveau `setEmpty`, `setFromArray`, `ObjectArray[T]` et `ArrayObject[T]`,
restent disponibles pour la stdlib et le code existant, mais `Set.empty`,
`Set(...)`, `Set.apply`, `Set.fromArray` et `Array[T]` sont les noms
recommandés dans le code utilisateur.

## Option

Le module `core.option` fournit `Option[T]`.

```nabla
import core.option

def main(): Int = {
    val some = Option.some[String]("hello")
    val none = Option.none[String]("fallback")

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
- `Option.none[T](default): Option[T]`
- `isEmpty()`
- `nonEmpty()`
- `getOrElse(default)`
- `map[U](default, f)`
- `flatMap[U](default, f)`
- `foreach(f)`
- `filter(predicate)`
- `orElse(fallback)`

Les noms de compatibilite `optionSome[T](value)` et
`optionNone[T](default)` restent disponibles pour le code existant.

## Entree Et Sortie

Le module `io` expose les helpers recommandes pour les entrees/sorties :

- `println(value: String): Unit`
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

Les primitives globales `print(value: String)`, `readLine(): String`,
`readFile(path: String): String`, `writeFile(path: String, content: String): Int`,
`appendFile(path: String, content: String): Int`, `deleteFile(path: String): Bool`,
`renameFile(path: String, to: String): Bool`, `createDir(path: String): Bool`
et `fileExists(path: String): Bool`
existent aussi, mais le module `io` est l'interface recommandee.

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
- La memoire utilise un heap `mmap` de 8 MiB avec bump allocator, sans
  liberation ni GC.
- Les objets utilisateur stockent un identifiant de classe runtime dans leur
  header et les appels de méthodes utilisateur sont virtuels par défaut quand
  la valeur est manipulée via un type parent, y compris pour un parent générique
  instancié, une méthode générique spécialisée et les méthodes de base
  `Any.toString` / `Any.hashCode` / `Any.equals`. `super` reste un appel
  statique. Les vraies vtables ne sont pas encore formalisées.
- Les `object` sont pour l'instant des namespaces statiques, pas des singletons
  runtime avec champs ou identité.
- La monomorphisation des classes generiques est encore limitee aux cas
  supportes par la suite actuelle.
- Les fonctions generiques ne sont pas encore des valeurs polymorphes.

## Exemples

Voir :

- `examples/command_shell.nabla`
- `examples/game_of_life.nabla`
- `examples/student_scores.nabla`
- `examples/interactive_confirm.nabla`
