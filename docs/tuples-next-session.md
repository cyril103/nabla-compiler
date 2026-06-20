# Tuples - note de reprise

Cette note resume les idees discutees pour une prochaine session autour des
tuples en Nabla/Sala.

## Reference Scala 2

Scala 2 implemente les tuples comme des classes de stdlib, pas comme une
structure runtime speciale :

- `Tuple1` jusqu'a `Tuple22` existent dans la bibliotheque standard.
- `(A, B)` est du sucre syntaxique pour `Tuple2[A, B]`.
- `(a, b)` est du sucre syntaxique pour une creation de `Tuple2(a, b)`.
- `Tuple2` expose les deux elements avec les noms `_1` et `_2`.
- `Map("a" -> 1)` s'appuie sur `->`, qui construit une paire
  `Tuple2[String, Int]`.

Exemple conceptuel Scala 2 :

```scala
final case class Tuple2[+T1, +T2](_1: T1, _2: T2)
```

## Direction recommandee pour Nabla

Commencer petit et solide :

1. Ajouter `Tuple2[A, B]` dans la stdlib.
2. Ajouter la syntaxe type `(A, B)` comme alias de `Tuple2[A, B]`.
3. Ajouter la syntaxe expression `(a, b)` comme creation de `Tuple2`.
4. Permettre l'acces `pair._1` et `pair._2`.
5. Ajouter `toString`, `equals`, `hashCode` si possible.

Ne pas commencer par les tuples de grande arite ni par le pattern matching.
Ces sujets peuvent venir apres.

## Premiere iteration proposee

Surface utilisateur cible :

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

Version desucree approximative :

```nabla
def label(value: Int): Tuple2[String, Int] = {
    Tuple2("score", value)
}
```

## Implementation probable

### Stdlib

Ajouter un module, par exemple `stdlib/core/tuple.nabla` ou
`stdlib/core/tuples.nabla`, avec :

```nabla
object Tuple2 {
    def apply[A, B](_1: A, _2: B): Tuple2[A, B] = {
        new Tuple2[A, B](_1, _2)
    }
}

class Tuple2[A, B](_1: A, _2: B) {
    def swap(): Tuple2[B, A] = {
        Tuple2(_2, _1)
    }

    override def toString(): String = {
        "(" + _1.toString() + ", " + _2.toString() + ")"
    }
}
```

Verifier comment les champs de constructeur sont exposes aujourd'hui :

- si `pair._1` marche deja via field access, profiter de ce chemin ;
- sinon ajouter des getters/methodes ou ajuster la creation des champs.

### Parser/type syntax

Ajouter le parsing de types parenthesises avec virgule :

- `(A)` doit rester equivalent a `A` si cette syntaxe existe ;
- `(A, B)` devient `Tuple2[A, B]` ;
- eventuellement refuser `(A, B, C)` au debut avec un diagnostic clair.

### Parser/expression syntax

Ajouter le parsing d'expressions parenthesees avec virgule :

- `(expr)` reste une expression parenthesee ;
- `(left, right)` devient un appel/constructeur `Tuple2(left, right)` ;
- refuser les tuples de plus de deux elements au debut.

### Semantique

S'assurer que :

- `Tuple2[A, B]` infere correctement `A` et `B` depuis `(a, b)` ;
- les tuples fonctionnent comme type de retour ;
- les tuples peuvent etre stockes dans `Array` et `Set` si les types le
  permettent ;
- `toString`, `equals`, `hashCode` ne cassent pas les types generiques.

## Tests a prevoir

Tests positifs :

- `tests/test_tuple2_literal.nabla`
- `tests/test_tuple2_type_alias.nabla`
- `tests/test_tuple2_access.nabla`
- `tests/test_tuple2_return.nabla`
- `tests/test_tuple2_generic.nabla`
- `tests/test_tuple2_array_set.nabla` si les collections acceptent bien ces
  objets generiques.

Tests d'erreur :

- tuple expression vide `()` si `Unit` n'est pas represente ainsi ;
- tuple de trois elements si seule l'arite 2 est supportee ;
- type tuple de trois elements si seule l'arite 2 est supportee ;
- acces `_3` sur `Tuple2`.

## Suite apres Tuple2

Une fois `Tuple2` stable :

1. Ajouter `->` comme sucre pour construire un `Tuple2`.
2. Ajouter `Map("a" -> 1, "b" -> 2)`.
3. Ajouter `zip` / `zipWithIndex` retournant des tuples publics.
4. Ajouter `Tuple3` si le besoin devient concret.
5. Ajouter le pattern matching tuple plus tard :

```nabla
match pair {
    case (name, score) => ...
}
```

## Point d'attention

On a observe avec `Set.toArray()` que certaines APIs retournent encore
`ArrayObject[Int]` plutot que la facade primitive attendue par `Set(values: _*)`.
Pour les tuples, il faudra faire attention aux representations internes et aux
facades utilisateur, surtout si `Tuple2[Int, String]` circule dans `Array` ou
`Set`.
