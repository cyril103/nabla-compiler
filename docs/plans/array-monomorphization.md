# Array monomorphization and specialization plan

## Objectif

Conserver `Array[T]` comme seul type public tout en permettant au compilateur de
specialiser les fonctions generiques appelees avec des tableaux primitifs. Le
but est de reduire le boxing et les representations objet-backed sans exposer
`ArrayInt`, `ArrayObject[T]` ou les helpers runtime dans les signatures source.

## Principes

- `Array[T]` reste la facade nominale observable dans le code utilisateur, les
  diagnostics et la documentation.
- Les backends (`ArrayInt`, `ArrayLong`, `ArrayFloat`, `ArrayDouble`,
  `ArrayBool`, `ArrayObject[T]`) restent des choix de lowering.
- Une fonction generique doit d'abord compiler avec la representation generique
  actuelle; la specialisation est une optimisation semantiquement transparente.
- Les erreurs de typage doivent continuer a parler de `Array[T]`, meme si une
  instance specialisee existe dans l'IR ou l'assembleur.

## Representation cible

Ajouter une notion interne de representation de tableau apres resolution des
types :

- `Array[Int]` -> `ArrayRepr::Int`
- `Array[Long]` -> `ArrayRepr::Long`
- `Array[Float]` -> `ArrayRepr::Float`
- `Array[Double]` -> `ArrayRepr::Double`
- `Array[Bool]` -> `ArrayRepr::Bool`
- `Array[Char]`, `Array[String]`, `Array[Class]` et `Array[T]` non specialise ->
  `ArrayRepr::Object`

Cette information ne doit pas remplacer le type source canonique. Elle s'ajoute
comme annotation de lowering ou de symbole specialise.

## Slice 1: inventaire et garde-fous

1. Ajouter des tests qui prouvent que les diagnostics restent publics quand une
   fonction generique manipule `Array[T]` avec des types primitifs et reference.
2. Ajouter un test IR/ASM minimal qui capture le backend actuel sans exiger de
   specialisation nouvelle.
3. Identifier les points qui convertissent deja `Array[T]` en backend concret :
   resolution de type, compatibilite `Array` / `ArrayObject`, emission IR et
   emission ASM.

Critere d'acceptation : aucun changement de performance attendu; seulement une
cartographie testee et des assertions de non-regression UX.

## Slice 2: monomorphisation locale opt-in

Specialiser uniquement les fonctions generiques definies dans le meme programme
lorsque tous les arguments de type sont connus au site d'appel.

Exemple :

```nabla
def sumFirst[T](xs: Array[T]): T = {
    xs.get(0)
}

val n = sumFirst[Int](Array(1, 2, 3))
```

Le compilateur peut generer un symbole interne du type :

```text
sumFirst__T_Int(xs: Array[Int])
```

mais les diagnostics et traces utilisateur continuent a mentionner
`sumFirst[T](xs: Array[T])` / `Array[Int]`.

Contraintes :

- pas de specialisation implicite des fonctions exportees/importees au debut;
- pas de variance ni de conversion magique entre `Array[Int]` et `Array[Any]`;
- fallback generique obligatoire si la specialisation ne couvre pas le cas.

## Slice 3: propagation dans methodes et stdlib

Etendre la specialisation aux methodes generiques de classes/traits quand le
receveur et les arguments de type sont concretement connus.

Cibles utiles :

- `Array[T].map`, `filter`, `flatMap` quand le resultat peut rester primitif;
- helpers de `Set.fromArray`, `Map.fromArray`, `List.fromArray` lorsque l'entree
  est concretement primitive;
- callbacks simples ou references de fonction deja resolues.

Critere d'acceptation : tests `.nabla` comparant les resultats publics et tests
IR/ASM qui montrent l'appel au helper primitif attendu.

## Slice 4: cache et mangling des specialisations

Ajouter une table de specialisations par symbole generique :

```text
(original symbol, concrete type args, array repr args) -> specialized symbol
```

Le mangling doit etre deterministe, stable dans les tests et distinct pour :

- types primitifs;
- types reference nominaux;
- types parametres encore generiques;
- combinaisons de plusieurs `Array[...]`.

Eviter de dupliquer plusieurs fois la meme specialisation dans un meme module.

## Slice 5: imports et compilation multi-fichier

Ne reprendre cette tranche qu'apres stabilisation des slices locales.

Questions ouvertes :

- faut-il specialiser une fonction importee dans le module appelant ?
- faut-il emettre plusieurs versions dans le module source ?
- comment nommer/exporter les specialisations sans en faire une API ?
- quelle strategie quand deux modules demandent la meme specialisation ?

## Verification minimale par PR

Pour chaque tranche :

```bash
export PATH=/opt/data/local/usr/bin:$PATH
make nablac
make unit-tests PYTHON=python3
make all-tests </dev/null
make examples
make tooling-tests PYTHON=python3
make stdlib-docs PYTHON=python3
git diff --exit-code docs/stdlib
git diff --check
```

Ajouter au moins un test ciblé qui prouve que la surface publique reste
`Array[T]`, même quand l'implementation interne devient specialisee.
