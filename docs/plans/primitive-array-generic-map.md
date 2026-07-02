# Surcharges generiques `Array.map[U]` pour les tableaux primitifs

## Objectif

Rendre l'API utilisateur des facades `ArrayInt`, `ArrayLong`, `ArrayFloat`,
`ArrayDouble` et `ArrayBool` plus idiomatique en exposant une surcharge
`map[U](...)` quand le resultat n'est pas du meme type primitif.

Aujourd'hui, ces conversions passent par le nom de compatibilite
`mapObject[U](...)`, qui expose le detail de representation `ArrayObject[U]`.
Le changement doit garder `mapObject` disponible pour compatibilite, mais les
exemples et nouveaux tests doivent utiliser `map[U]`.

## Portee

- Ajouter des tests runtime pour `map[U]` sur les cinq facades primitives.
- Prouver que la surcharge specialisee existante `map(Int => Int): ArrayInt`
  continue d'etre choisie pour le mapping same-type.
- Ajouter les overloads `map[U](...)` en reutilisant les helpers
  `*ArrayMapObject` existants.
- Migrer au moins un exemple public depuis `mapObject` vers `map`.
- Mettre a jour la documentation de surface stdlib et le journal `AGENTS.md`.

## Hors portee

- Supprimer `mapObject`.
- Changer les types internes retournes par les helpers de representation.
- Modifier l'overload resolution du compilateur, sauf si les tests montrent une
  ambiguite reelle.

## Verification

1. RED: le nouveau test `test_generic_array_primitive_map_method.nabla` echoue
   avant les overloads.
2. GREEN: tests cibles sur les nouveaux overloads et regression `mapObject`.
3. Validation complete: `make all-tests`, `make examples`, `make tooling-tests`,
   `git diff --check`.
