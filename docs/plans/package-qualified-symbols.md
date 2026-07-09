# Package-Qualified Symbols

Plan actif court pour le delta package en cours.

## Objectif courant

- Permettre les appels source pleinement qualifies vers les fonctions top-level
  importees: `pkg.module.function(...)` doit selectionner le symbole interne
  correspondant, meme si le nom court est ambigu entre plusieurs imports.
- Conserver la visibilite historique par nom court quand un seul module expose
  ce nom.
- Conserver le diagnostic exact d'ambiguite quand un nom court est expose par
  plusieurs modules et utilise sans qualification.
- Ne pas ajouter encore imports selectifs, alias, wildcards, packages multi-fichiers
  ou resolution qualifiee des types/classes.

## Tests

- Deux modules packages exposent la meme fonction courte; les deux appels
  pleinement qualifies s'executent et retournent des valeurs distinctes.
- Le meme programme utilisant le nom court ambigu continue de produire le
  diagnostic d'ambiguite exact.
- Une reference de fonction qualifiee peut etre affectee a un type fonction quand
  le symbole est unique.
- Les appels qualifies resolvent aussi les surcharges d'un meme module package
  par arite puis par types d'arguments.
- Tests package existants: declaration, mismatch, package non premier, import
  unique avec homonyme.

## Differe

- Imports selectifs, alias et wildcards explicites.
- Ambiguite/resolution qualifiee des noms de types/classes: la fondation de
  metadata existe, mais la resolution de type reste majoritairement indexee par
  nom court et sera durcie dans une tranche separee pour eviter une reecriture
  trop large.
