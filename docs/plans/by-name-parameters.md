# Parametres par nom `=> T`

## Objectif

Ajouter une V0 Scala-like des parametres par nom afin que des APIs comme
`Array.fill[T](n: Int)(elem: => T)` puissent reevaluer `elem` a chaque usage,
en s'appuyant sur les thunks zero-argument `() => T` deja disponibles.

## Semantique V0

- `name: => T` est autorise uniquement en position parametre de fonction/methode/lambda locale.
- Le parametre est represente en interne comme un thunk `Fn()->T`, avec un marqueur `byName` pour l'affichage et les usages implicites.
- A l'appel, une expression ordinaire fournie a un parametre par nom est enveloppee automatiquement dans une lambda zero-argument.
- Dans le corps, une reference nue a un parametre par nom est reevaluee via un appel implicite du thunk.
- V0 herite des regles actuelles de capture des closures; les captures mutables Scala-fideles seront traitees separement si necessaire.

## Tests RED

- `twice(next())` retourne `3`, prouvant que l'argument est evalue deux fois.
- Un parametre par nom `Unit` peut servir a executer un bloc/action plusieurs fois.
- Un appel curryfie style `fill(n)(elem)` construit des valeurs distinctes.
- `=> T` hors position parametre reste invalide.

## Implementation

1. Etendre les structures de parametre (`FunctionDefNode::Parameter`, `ParameterInfo`, symboles parser) avec `byName`.
2. Parser `name: => Type` dans les definitions de fonctions/methodes/local def et lambdas typees.
3. Canonicaliser le type stocke vers `Fn()->Type` tout en gardant `byName`.
4. Lors de la resolution d'identifiant, transformer une reference nue a un symbole `byName` en appel indirect zero-argument.
5. Lors du parsing d'arguments avec type attendu, envelopper automatiquement l'expression dans une lambda zero-argument quand le parametre attendu est par nom.
6. Mettre a jour diagnostics/docs/roadmap/AGENTS.

## Verification

- Tests cibles nouveaux et regressions existantes autour des fonctions valeurs/lambdas.
- `make all-tests`
- `make examples`
- `make tooling-tests`
- `git diff --check`
