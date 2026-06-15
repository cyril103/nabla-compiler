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

```nabla
def main(): Int = {
    val base = 40
    var value = base
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

`match` compare une expression a des motifs litteraux avec `==`. Les motifs
supportes sont les litteraux `Int`, `Long`, `Float`, `Double`, `Bool`, `String`
et `Char`, plus la branche finale obligatoire `_`.

```nabla
def commandCode(command: String): Int = {
    match command {
        "add" => 1
        "quit" => 2
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
aux fonctions communes.

```nabla
import collections.array

def main(): Int = {
    val values = arrayFill[Int](3, 7)
    val doubled = values.map(value => value * 2)
    if doubled.mkString(",") == "14,14,14" {
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

`collections.int_array` expose aussi `arrayIntRange(size)` pour produire
`0..size-1` et `arrayIntRangeUntil(start, until)` pour produire une plage
`start..until-1`.

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

## Option

Le module `core.option` fournit `Option[T]`.

```nabla
import core.option

def main(): Int = {
    val some = optionSome("hello")
    val none = optionNone("fallback")

    if some.getOrElse("x") == "hello" &&
       none.getOrElse("fallback") == "fallback" {
        42
    } else {
        1
    }
}
```

Operations courantes :

- `isEmpty()`
- `nonEmpty()`
- `getOrElse(default)`
- `map[U](default, f)`
- `flatMap[U](default, f)`
- `foreach(f)`
- `filter(predicate)`
- `orElse(fallback)`

## Entree Et Sortie

Le module `io` expose les helpers recommandes pour les entrees/sorties :

- `println(value: String): Unit`
- `input(): String`
- `readTextFile(path: String): String`
- `writeTextFile(path: String, content: String): Int`
- `appendTextFile(path: String, content: String): Int`
- `deleteTextFile(path: String): Bool`
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
`appendFile(path: String, content: String): Int`, `deleteFile(path: String): Bool`
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
- `writeTextFile` et `appendTextFile` retournent le nombre d'octets ecrits.
- Une valeur negative indique une erreur de syscall Linux. Nabla n'a pas encore
  de type `Result` ni d'abstraction `errno`.
- Les fichiers tres volumineux peuvent epuiser le tas statique du runtime.
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
- La memoire utilise un bump allocator sans liberation ni GC.
- Les vtables ont encore un emplacement reserve mais pas une strategie objet
  complete d'heritage dynamique.
- La monomorphisation des classes generiques est encore limitee aux cas
  supportes par la suite actuelle.
- Les fonctions generiques ne sont pas encore des valeurs polymorphes.

## Exemples

Voir :

- `examples/command_shell.nabla`
- `examples/game_of_life.nabla`
- `examples/student_scores.nabla`
- `examples/interactive_confirm.nabla`
