# String dispatch through Any

## Contexte

`String` est assignable a `Any` / `AnyRef`, mais les chemins runtime `Any.toString`,
`Any.hashCode` et `Any.equals` ne distinguent pas encore les objets `String` bruts
quand le receveur statique est efface vers `Any`.

Aujourd'hui, les chaines runtime utilisent le meme header nul que plusieurs objets
internes. `Any.toString` tombe donc sur le fallback d'identite pour une chaine
passee via `Any`, au lieu de rendre la chaine elle-meme.

## Objectif

Durcir le contrat de dispatch racine pour le type reference builtin `String` :

- `value.toString()` via un parametre `Any` retourne la chaine d'origine ;
- `value.hashCode()` via `Any` reutilise le hash de `String` ;
- `value.equals(other)` via `Any` compare le contenu lorsque les deux valeurs sont
  des chaines ;
- les autres objets heap conservent le fallback actuel.

## Plan TDD

1. Ajouter un test runtime positif qui passe des `String` via `Any` et verifie
   `toString`, `hashCode` et `equals`.
2. Observer l'echec avant correctif pour confirmer la regression.
3. Ajouter un tag runtime explicite pour les objets `String`.
4. L'appliquer aux litteraux et aux allocations runtime qui produisent des
   chaines.
5. Brancher `Any.toString`, `Any.hashCode` et `Any.equals` sur les helpers string
   quand le tag est celui de `String`.
6. Documenter la convention dans `docs/internals.md` et `AGENTS.md`.
7. Verifier avec le test cible, puis les suites completes.
