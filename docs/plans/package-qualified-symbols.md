# Package-Qualified Symbols

Plan actif court pour la suite du chantier package/namespace.

## Etat livre

- `package a.b` est accepte comme premiere declaration optionnelle et valide
  les imports par chemin.
- Les fonctions top-level importees sont stockees sous un nom interne qualifie;
  le nom court reste utilisable quand il est unique, et un nom court ambigu
  produit un diagnostic.
- Les appels et references de fonctions pleinement qualifies
  (`pkg.module.function()` / `pkg.module.function`) resolvent le symbole importe
  exact, y compris les surcharges d'un meme module package.
- Les types/classes/objets top-level importes sont stockes sous un nom interne
  qualifie quand le fichier declare un package. Les annotations de type et
  constructions `new` acceptent `pkg.module.Type`; le nom court reste utilisable
  quand il est unique et devient ambigu quand plusieurs imports exposent le meme
  type.
- La stdlib source declare maintenant un `package` pour chaque module sous
  `stdlib/`. Les imports existants (`import collections.array`) continuent
  d'exposer les noms courts uniques, tandis que les annotations peuvent nommer
  explicitement les types publics (`collections.int_array.ArrayInt`,
  `core.option.Option[T]`, etc.).

## Tests couverts

- Declaration de package, mismatch package/import, et `package` non premier.
- Fonctions homonymes importees: nom court ambigu, import unique par nom court,
  appels qualifies, references qualifiees et surcharges qualifiees.
- Classes homonymes importees: annotation et `new` pleinement qualifies,
  import unique par nom court, et diagnostic d'ambiguite par nom court.
- Surface stdlib packagee: declarations `package ...` sur tous les modules,
  aliases Array/Set compatibles avec les noms internes qualifies, et regression
  `test_stdlib_package_qualified_surface.nabla`.

## Differe

- Imports selectifs, alias et wildcards explicites.
- Motifs de constructeur pleinement qualifies dans `match`.
- Packages multi-fichiers, visibilites package, re-export et ergonomie complete
  des diagnostics pour tous les symboles.
- Nettoyage public de la stdlib: reexports/prelude, choix definitif des noms de
  package publics, et reecriture plus large des APIs obsoletes.
