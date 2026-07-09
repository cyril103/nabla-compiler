# Package-Qualified Symbols

Plan actif court pour le delta package en cours.

## Objectif

- Enregistrer les declarations top-level avec leur module/package declare et leur
  nom source court.
- Permettre a deux modules importes de definir le meme nom top-level court sans
  collision interne immediate, au moins pour les fonctions globales.
- Conserver la visibilite historique par nom court quand un seul module expose
  ce nom.
- Emettre un diagnostic exact et oriente source quand deux imports exposent le
  meme nom court et que le programme l'utilise sans qualification.

## Tests

- Module package unique avec fonction homonyme: compilation/execution OK.
- Deux imports avec meme fonction courte: diagnostic d'ambiguite exact.
- Tests package existants: declaration, mismatch, package non premier.

## Differe

- References source pleinement qualifiees (`a.b.name`), imports selectifs, alias
  et wildcards explicites.
- Ambiguite des noms de types/classes: la fondation de metadata est ajoutee,
  mais la resolution de type reste majoritairement indexee par nom court et sera
  durcie dans une tranche separee pour eviter une reecriture trop large.
