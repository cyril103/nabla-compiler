# Constructor `var` fields

## Objectif

Ajouter une forme Scala-like de champ constructeur mutable :

```nabla
class Node[T](val head: T, var next: List[T]) {
    def relink(value: List[T]): Unit = {
        next = value
    }
}
```

La V1 vise les structures internes performantes de stdlib, par exemple des
builders de listes qui relient des cellules avant exposition publique.

## Portee V1

- Accepter `var name: Type` dans les constructeurs de classes.
- Stocker le champ exactement comme un champ constructeur existant.
- Generer un getter zero-argument comme pour `val`, afin que `var next` puisse
  satisfaire `def next(): T` / `def next: T` quand la signature correspond.
- Autoriser l'affectation au champ mutable depuis les methodes de la classe avec
  la syntaxe `name = expr`.
- Rejeter l'affectation aux champs `val`, les types incompatibles et les champs
  inexistants.

## Non-objectifs V1

- Pas encore de setter public `obj.field = value`.
- Pas encore de methode synthetique `field_=` publique.
- Pas de visibilite `private[...]`, covariance, annotations type-system ou
  fences memoire comme dans l'implementation Scala de `::`.

## Taches

1. Parser/metadonnees : reconnaitre `KW_VAR` dans les listes de champs typées et
   ajouter `isMutable` a `FieldInfo`.
2. Accesseurs : generer le getter synthetique pour `val` et `var`.
3. Affectation : quand une affectation simple apparait dans une methode de
   classe et ne cible pas une variable locale, resoudre un champ visible de la
   classe courante, verifier `isMutable` et abaisser vers une ecriture de slot.
4. Tests : positifs `Int`, generique, `List[T] next`; negatifs `val`, mauvais
   type, champ inconnu, hors classe.
5. Docs : mettre a jour `docs/language.md`, `docs/internals.md`,
   `docs/roadmap.md` et `AGENTS.md`.
6. Validation : ciblés, `make all-tests`, `make examples`, `make tooling-tests`,
   build strict et `git diff --check`.
