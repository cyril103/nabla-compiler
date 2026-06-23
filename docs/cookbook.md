# Nabla Cookbook

Les exemples executables sous `examples/` restent la source la plus directe pour
voir les API publiques en contexte. Les deux cookbooks suivants sont
non-interactifs et verifies par `make examples` avec sortie console attendue.

- `examples/stdlib_collections_cookbook.nabla` montre `Array[T]`, `Set[T]`,
  `Map[K, V]`, `Option[T]` et `Sized` pour des transformations de collections
  courantes.
- `examples/stdlib_text_cookbook.nabla` montre `String.trim`, `String.split`,
  les methodes communes de tableaux, puis `Set`, `Map` et `Option` pour un petit
  traitement de texte deterministe.

Ces exemples evitent de construire directement les representations internes
comme `ObjectArray[T]`, `ArrayObject[T]` ou `ArrayInt`. Certains retours de la
stdlib, notamment `String.split`, restent toutefois des valeurs de tableau de la
representation actuelle; le code applicatif doit les manipuler via leurs
methodes publiques (`size`, `get`, `map`, `filter`, `fold`, `mkString`, etc.).
