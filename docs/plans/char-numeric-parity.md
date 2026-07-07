# Parité numérique de `Char`

Objectif: donner à `Char` les mêmes opérations arithmétiques/comparaisons que `Int`, tout en conservant `Char` comme type distinct pour l'égalité stricte, les tableaux de caractères et l'affichage.

## Étapes

1. Étendre le rang de promotion numérique à `Char -> Int -> Long -> Float -> Double`.
2. Faire produire `Int` aux opérations arithmétiques entre `Char`/entiers, et `Bool` aux comparaisons.
3. Ajouter les conversions explicites `Char.toInt()`, `Char.toLong()`, `Char.toFloat()` et `Char.toDouble()` et réutiliser ces conversions pour les promotions implicites.
4. Couvrir les promotions dans les contextes de type attendu, appels, constructeurs, retours et écritures de tableaux natifs.
5. Mettre à jour la documentation utilisateur puis retirer ce plan avant la PR.
