# Impression via Any.toString

## Objectif

Rendre `print(value)` plus idiomatique : l'appel accepte une valeur de n'importe
quel type Nabla et imprime le résultat de `value.toString()` au lieu d'exiger que
l'utilisateur convertisse explicitement en `String`.

## Contraintes

- Conserver `print` comme primitive globale renvoyant `Unit`.
- Ne pas dupliquer la logique de rendu dans `Runtime_print` : le lowering doit
  réutiliser `Any.toString()` et donc le dispatch dynamique existant.
- Préserver le rendu spécialisé des primitives, y compris `Bool`, `Char`,
  `Float` et `Double`, en boxant les valeurs builtin lorsque le chemin `Any` le
  demande.
- Préserver les overrides utilisateur de `toString()` via les appels dynamiques
  sur `Any`.
- Garder `println` du module `io` aligné sur `print`.

## Plan TDD

1. Ajouter un test runtime `print` avec `Int`, `Bool`, `Char`, classe utilisateur
   et valeur typée `Any`; vérifier la sortie console.
2. Ajouter un test `io.println(value: Any)` pour prouver que le helper stdlib
   suit la même règle.
3. Assouplir la validation sémantique de `print` : seulement l'arité est
   vérifiée.
4. Abaisser `print(value)` en `Any.toString(value)` puis `Runtime_print(String)`.
5. Mettre à jour la documentation langage/interne et régénérer la référence
   HTML si la signature stdlib `println` change.
