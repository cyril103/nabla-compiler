# randInt()

## Objectif

Ajouter une commodite publique `randInt(): Int` pour les petits programmes et les exemples qui veulent une valeur entiere pseudo-aleatoire sans gerer explicitement `RandomState`.

## Semantique V0

- `randInt()` retourne un `Int` positif tire depuis une seed temporelle runtime.
- La fonction est volontairement non deterministe et sans etat explicite; le chemin reproductible reste `randomSeed(...)` + `randomInt(state)`.
- `randInt()` doit etre utilisable comme expression par nom dans `Array.fill[T](n)(elem)`, afin que chaque case reevalue l'appel.

## Plan TDD

1. Ajouter un test runtime qui appelle `Array.fill[Int](n)(randInt())` et verifie que le tableau est rempli avec des entiers positifs et que plusieurs cases sont reevaluees.
2. Ajouter `randInt()` dans `stdlib/util.nabla` comme wrapper public de `randomInt(randomSeedNow()).value()`.
3. Documenter la surface publique dans `docs/stdlib-api.md` et regenerer `docs/stdlib`.
4. Mettre a jour `AGENTS.md`, puis valider avec tests cibles, exemples/outillage et revue docs/code.
