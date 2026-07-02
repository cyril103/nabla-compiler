# Nabla Roadmap

Ce document capture l'etat courant du projet et les prochaines pistes de travail
pour garder un cap clair après le tag `v0.1.0`.

## Etat Actuel

- Backend natif Linux x86_64 via assembleur, avec backend IR utilise par defaut.
  Le compilateur signale explicitement une commande externe introuvable comme
  `nasm` ou `ld`.
- Typage statique avec fonctions, fonctions locales de bloc utilisables dans les
  contextes generiques de fonctions/methodes/classes/traits, methodes, classes,
  generiques simples, lambdas et paramètres par nom `=> T` abaissés en thunks
  zero-argument; les paramètres constructeur `val` et `var` génèrent des getters,
  et les champs `var` sont réassignables depuis les méthodes de leur classe.
- Les `def` sans liste de parametres declarent des proprietes calculees
  zero-argument reutilisant les fonctions/methodes existantes: `def pi: Double`
  est utilisable comme `pi`, `Config.base` appelle `Config.base()`,
  `value.head` appelle `value.head()`, et les membres abstraits de trait peuvent
  s'ecrire `def head: T`.
- Surcharge V1 des fonctions globales non generiques par signature exacte :
  plusieurs `def` peuvent partager un nom si leurs types de parametres
  different; les appels sont abaisses vers un symbole IR unique.
- Controle de flux : `if` expression, `else if`, `match`, `while`, `for`.
- Support de l'héritage objet avec `extends` + `with` (mixins), hiérarchie
  racine `Any` / `AnyVal` / `AnyRef`, `toString(): String` et
  `hashCode(): Int` redéfinissables sur les classes reference, détection de
  cycles / conflits et appel `super`.
  `trait` est supporte en V1 comme contrat nominal sans etat, avec methodes
  abstraites, methodes concretes par defaut, composition par `with`, obligation
  d'implementation dans les classes concretes et rejet des conflits de defaults
  sans `override` explicite. Les classes generiques peuvent implementer des
  traits, y compris des traits generiques instancies comme `Iterable[T]`, avec
  dispatch via le type du trait. La stdlib expose maintenant `Sized` et
  `Iterable[T]` comme contrats publics minimaux pour les collections.
  La conversion `toString()` est désormais disponible de façon uniforme pour les
  types primitifs usuels (`Int`, `Long`, `Float`, `Double`, `Bool`, `Char`) au niveau du
  backend runtime, y compris via un paramètre `Any`/`AnyVal` de fonction/méthode
  grâce au boxing heap minimal. La primitive globale `print(value)` accepte aussi
  `Any` et imprime le résultat de `value.toString()`, avec dispatch dynamique
  vers les overrides utilisateur. Les chaînes `String` portent aussi un tag runtime
  dédié afin de conserver `toString`, `hashCode` et `equals` par contenu quand
  elles sont manipulées via `Any`.
- `object Name { ... }` reste un namespace statique et supporte les compagnons
  de surface. `object Name with Trait { ... }` est maintenant un singleton
  runtime V0: valeur stable assignable aux traits, a `AnyRef` et a `Any`, avec
  validation class-like des méthodes abstraites, `override`, signatures et
  conflits de defaults. Cette V0 exclut encore champs, constructeurs,
  `extends`, arguments de type et initialisation dédiée; ces singletons ne sont
  pas instanciables avec `new` et ne peuvent pas servir de parents de classe.
- Standard library deja utile :
  - collections typées et facade `Array[T]`, dont `sorted(lessThan)` retourne
    une copie triee sans mutation et les facades primitives exposent aussi
    `sorted()` par ordre naturel; les conversions primitives vers tableaux
    generiques utilisent les noms idiomatiques `map[U]` et `flatMap[U]`
  - `collections.set` avec `Set[T]`, `add`, `remove`, `union`, `intersect`,
    `difference`
  - `collections.map` avec `Map[K, V]`, recherche optionnelle, mises a jour
    immutables et conversions cle/valeur
  - `collections.list` V0 experimentale avec `List[T]`, le singleton `Nil` de
    type `List[Nothing]`, `Cons[T]`, fabriques `List.empty` / `List.cons`,
    operations compagnon `fold`, `map`, `filter` et conversion depuis `Array[T]`
  - `Option[T]`
  - operations `String`
  - I/O console et fichiers texte
- I/O fichiers texte :
  - `readTextFile`
  - `writeTextFile`
  - `appendTextFile`
  - `deleteTextFile`
  - `renameTextFile`
  - `createDirectory`
  - `pathExists`
- Exemple principal `examples/command_shell.nabla` utilise maintenant `match`,
  les commandes fichier, et sert de vitrine pour l'ergonomie du langage,
  notamment `write` / `append` avec texte multi-mots.
- `examples/student_scores.nabla` sert d'exemple public vérifié pour
  `Array[T]`, `Array.fill[T]`, `Option.some[T]` / `Option.none[T]`, classes,
  lambdas et sortie console.
- `examples/workshop_set_inheritance.nabla` sert d'exemple public vérifié pour
  `Array[T]`, `Set[T]`, `Set.fromArray[T]`, opérations d'ensemble et héritage
  avec `override`; il couvre maintenant aussi les collections polymorphes de
  type parent avec dispatch runtime des overrides utilisateur.
- `examples/game_of_life.nabla` sert d'exemple interactif compilé par
  `make examples` pour `Array[T]` et `Array.fill[T]`, sans exposer
  `IntArray` ni les helpers `intArray...` dans son code applicatif.
- `examples/stdlib_collections_cookbook.nabla`,
  `examples/stdlib_list_cookbook.nabla` et
  `examples/stdlib_text_cookbook.nabla` servent de cookbooks non interactifs
  verifies pour la surface stdlib publique (`Array[T]`, `Set[T]`, `Map[K, V]`,
  `List[T]`, `Option[T]`, `Sized` et operations texte), sans construction directe des
  representations de tableaux internes.
- Support editeur minimal disponible dans `editor/vim` et `editor/vscode`; le
  support VS Code couvre la detection `*.nabla`, la coloration TextMate, la
  configuration langage et des snippets.
- Outillage de formatage minimal disponible: `make format` normalise les espaces
  de fin de ligne et le retour final des sources/docs suivis, et `make
  format-check` est integre a `make tooling-tests`.
- Suites `make all-tests`, `make examples` et `make tooling-tests` vertes au
  moment de cette mise a jour; la CI GitHub lance aussi les exemples publics.

## Priorités Post-0.1

### Fil conducteur produit/langage

La prochaine phase doit transformer Nabla de "langage qui compile beaucoup de
cas" en "langage ou l'utilisateur sait naturellement quelle API employer".
Le cap prioritaire est donc la coherence de surface, pas l'accumulation de
features.

- API publique cible : `Array[T]`, `Option[T]`, `Set[T]`, `Map[K, V]`,
  `List[T]` experimentale, `Sized`, `Iterable[T]`, `String`, classes, methodes,
  lambdas et modules standards documentes.
- Details a cacher ou marquer internes : `IntArray`, `LongArray`,
  `FloatArray`, `DoubleArray`, `BoolArray`, `ObjectArray[T]`,
  `ArrayObject[T]`, helpers `arrayBase...` et fonctions specialisees de pont.
- Runtime a formaliser en continu : valeurs raw `Float`/`Double`, objets heap,
  tableaux natifs, slots nuls, conventions d'erreur et limites memoire. Les
  constantes d'encodage `Int`/`Long`/`Bool` sont deja centralisees dans le code,
  et la capacité du heap peut être ajustée par exécutable avec
  `nablac --heap-size <octets>`.
- Typage a garder simple : sous-typage nominal pour les classes, generiques
  invariants par defaut, conversions explicites ou helpers stdlib.
- Documentation : la reference HTML doit rester une doc utilisateur claire,
  avec une separation visible entre API publique et helpers internes. Le
  generateur accepte `@status` pour afficher des badges de statut et
  `@example ... @end` pour rendre des exemples de code; les pages publiees
  utilisent une mise en page type reference Scala avec sidebar par
  types/fabriques/methodes, signatures ancrees et chemins CSS relatifs verifies.
  Le périmètre livré par Nabla 0.1 et la matrice de validation 0.1.x sont
  conservés dans `docs/releases/0.1.md`.

Actions recommandees :

1. Basculer en phase post-`v0.1.0`: le tag 0.1 existe, donc les prochains
   changements doivent clarifier ou durcir le comportement observable avant
   d'élargir la surface publique.
2. Durcir l'héritage/runtime : conflits champs/méthodes visibles, `super`
   statique, règles d'égalité et hash dans les hiérarchies complexes, avec des
   régressions couvrant aussi `Array[Parent]` et `Set.fromArray[Parent]`.
3. Continuer le nettoyage de la référence stdlib générée en affinant les
   descriptions utilisateur et en évitant d'ajouter de nouveaux helpers
   documentés sans classification préalable.
4. Maintenir le check CI pour `make stdlib-docs` et la référence HTML générée.
5. Maintenir le check CI `make examples` pour garantir que les exemples publics
   restent compilables et que leurs sorties attendues ne dérivent pas.
6. Maintenir `make unit-tests` comme filet rapide du front-end en couvrant
   directement lexer, parser et analyse sémantique sans passer par NASM/ld.
7. Maintenir et enrichir `docs/internals.md`, la specification vivante pour
   types, runtime, IR et conventions de stdlib.
8. Utiliser `docs/stdlib-api.md` pour distinguer API publique, compatibilite
   temporaire et helpers internes avant d'ajouter de nouveaux symboles.
9. Revoir la doc stdlib pour masquer ou signaler les helpers internes.
10. Stabiliser `Array[T]`, `Option[T]` et `Set[T]` comme surfaces utilisateur
   principales.
11. Continuer a garder les exemples publics sur les facades idiomatiques
   (`Array[T]`, `Array.fill[T]`, `Set.fromArray[T]`,
   `Option.some[T]` / `Option.none[T]`) quand elles existent.
   Les cookbooks doivent rester concis et ne pas laisser entendre que tous les
   retours de tableaux masquent deja completement `ArrayObject[T]`.
12. Garder les diagnostics de compatibilite orientes vers les compagnons
   recommandes (`Array.fill`, `Array.range`, `Set.fromArray`,
   `Option.some`, `Option.none`, etc.).
13. Exploiter les champs constructeur `var` pour préparer des builders internes
    de collections, notamment une construction de `List[T]` plus efficace avant
    exposition publique.
14. Poursuivre la surcharge par signature apres `v0.1.0` : la base couvre maintenant les
    fonctions globales, y compris les generiques inferables et les references
    typees, ainsi que les methodes de classe concretes ou generiques par
    signature exacte, y compris l'inference de lambdas apres arguments generiques
    deja resolus; commencer la migration publique de la stdlib vers les noms
    surcharges idiomatiques et garder les diagnostics d'ambiguite riches si la
    resolution devient moins stricte que l'exact match.
15. Reporter `Result[T]`, variance avancee et GC tant que cette surface n'est
   pas propre.
16. Pour chaque nouvelle feature, suivre `docs/feature-integration.md` afin de
   verifier l'etat de depart, le plan actif, les tests, les docs et l'hygiene
   avant PR.

## Jalons Récents Pris En Compte

Ces jalons sont déjà intégrés et ne doivent plus être traités comme des notes de
reprise séparées :

- `v0.1.0` est tagué ; l'ancienne checklist de préparation du tag a été
  transformée en notes de release et matrice de validation 0.1.x dans
  `docs/releases/0.1.md`.
- Les diagnostics d'héritage, `override`, `super`, conflits champ/méthode et
  noms legacy de stdlib sont couverts par des tests négatifs exacts.
- `Any.toString`, `Any.hashCode`, `Any.equals`, `String` taggé et `print(value)`
  via `Any.toString()` sont stabilisés pour les cas couverts.
- Le runtime heap est configurable avec `nablac --heap-size <octets>`.
- `Sized` et `Iterable[T]` sont exposés comme traits publics minimaux de
  collections.
- Les exemples publics principaux privilégient désormais `Array[T]`,
  `Array.fill[T]`, `Set.fromArray[T]`, `Option.some` / `Option.none` et les
  méthodes publiques, plutôt que les représentations internes; les conversions
  de facades primitives vers tableaux generiques utilisent `map[U]` /
  `flatMap[U]` plutot que `mapObject[U]` / `flatMapObject[U]`.
- Les objets runtime V0 sont couverts par des tests de valeur singleton, dispatch
  via trait, assignabilité `Any` / `AnyRef`, diagnostic de namespace statique
  utilisé comme valeur et diagnostics d'héritage class-like.
- Les diagnostics de types pour `if`, `while`, `for`, `!`, `&&` et `||`
  pointent l'expression fautive et couvrent les côtés gauche/droit des
  opérateurs booleens ainsi que le compteur de boucle.
- Une première cible `make unit-tests` compile un harness C++ front-end et
  vérifie directement lexer, parser, diagnostics sémantiques, surcharge,
  inférence générique et typage contextuel de lambdas sans assemblage.
- Une première cible `make format` sans dépendance externe normalise les espaces
  de fin de ligne et retours finaux des sources/docs; `make format-check` est couvert par
  `tooling-tests`.
- Le dispatch dynamique repose désormais sur des vtables backend internes:
  objet slot 0 vers table de fonctions, slots par propriétaire statique +
  méthode résolue, couverture de `Any`, traits `Sized` / `Iterable[T]`, parents
  génériques instanciés, méthodes génériques spécialisées, defaults de traits,
  `Set[Parent]` et clefs `Map[Parent, V]`.
- Les proprietes `def` sans parametres sont supportees pour les fonctions
  globales, methodes de classes/objets, overrides et membres abstraits de trait,
  avec acces externe `Object.member` / `value.member` et compatibilite conservee
  avec `Object.member()` / `value.member()`.
- Les fonctions locales `def` avec parametres sont supportees dans les blocs via
  des fonctions cachees non exposees comme API publique; elles couvrent l'appel
  direct, la recursion directe, les references comme valeurs fonction pour les
  helpers declares hors contexte generique, les appels vers des helpers locaux deja declares et les
  contextes generiques de fonctions/methodes/classes/traits. Les captures
  implicites et fonctions locales generiques restent explicitement reportees.
- `docs/plans/` est reserve aux plans actifs: les plans historiques des jalons
  deja livres ont ete retires, et les prochains chantiers doivent creer un plan
  court centre sur le delta restant, puis suivre la checklist
  `docs/feature-integration.md` avant PR.
- `docs/internals.md` documente les conventions internes courantes pour les
  proprietes `def` zero-argument et les fonctions locales de bloc, afin de ne
  pas les confondre avec des valeurs memoisees ou des lambdas source.

## Pistes Plus Larges

- Introduire un type `Result[T]` ou une convention d'erreurs plus riche pour
  l'I/O et le parsing.
- Agrandir le heap runtime au-delà de la valeur par défaut de 8 MiB est possible
  à la compilation avec `nablac --heap-size <octets>`; le chantier long terme
  reste la stratégie mémoire complète (libération/GC éventuel).
- Refactoriser les primitives runtime I/O pour eviter la duplication autour des
  chemins C.
- Formaliser les prochaines limites de closures et d'evaluation paresseuse
  (captures explicites, cout d'allocation des thunks) sans brouiller la
  distinction entre paramètres par nom `=> T` et types fonction ordinaires
  `() => T`.
- Ajouter un support editeur supplementaire :
  - Treesitter plus tard si le langage se stabilise.
- Ajouter plus d'exemples applicatifs :
  - outil de notes avec fichiers
  - mini transformateur de texte
  - Game of Life avec sauvegarde/chargement de grille.

## Regle De Reprise

Avant de commencer une nouvelle feature, suivre la checklist complète de
`docs/feature-integration.md`. Le minimum de reprise reste :

```bash
git status --short
make all-tests
```

Puis travailler en petits commits, comme pendant cette session.
