# Array surface follow-ups

## Objectif

Terminer la stabilisation utilisateur apres la livraison de la facade publique
`Array[T]` : les APIs recommandees doivent exposer `Array[T]`, tandis que
`ArrayObject[T]`, `ArrayInt` et les autres backends restent limites aux modules
internes, aux tests bas niveau et aux notes de compatibilite.

## Audit initial

Occurrences acceptees :

- `docs/internals.md` et `docs/plans/runtime-memory-management.md` peuvent nommer
  les backends, car ils decrivent les representations runtime et GC.
- `stdlib/collections/*_array.nabla`, `object_array.nabla`, `set.nabla` et
  `map.nabla` peuvent utiliser `ArrayObject[T]` comme stockage interne.
- Les tests explicitement bas niveau ou de compatibilite peuvent construire
  `ArrayInt`, `ArrayObject[T]` ou importer `collections.int_array` /
  `collections.object_array`.

Occurrences a corriger dans cette tranche :

- Les pages utilisateur doivent eviter de presenter `String.toCharArray`,
  `String.split`, `strings.words`, `Set.toArray`, `Map.keys`, `Map.values` et
  `Map.toArray` comme retournant `ArrayObject[...]`.
- Les signatures effectives publiques de ces helpers doivent accepter ou retourner
  `Array[T]` quand la facade est suffisante.
- Les exemples applicatifs doivent continuer a utiliser `Array[T]` et les
  fabriques haut niveau.

## Sous-etapes prevues

1. Remplacer les commentaires de docs qui expliquent une fuite actuelle par une
   regle de facade publique.
2. Aligner les signatures stdlib publiques restantes sur `Array[T]` quand le
   corps peut conserver `ArrayObject[T]` comme representation.
3. Ajouter une regression qui annote explicitement les retours avec `Array[...]`.
4. Regenerer `docs/stdlib/` et valider la matrice pertinente.
