# Nabla Roadmap

Ce document capture l'etat courant du projet et les prochaines pistes de travail
pour reprendre facilement apres une pause.

## Etat Actuel

- Backend natif Linux x86_64 via assembleur, avec backend IR utilise par defaut.
  Le compilateur signale explicitement une commande externe introuvable comme
  `nasm` ou `ld`.
- Typage statique avec fonctions, methodes, classes, generiques simples et lambdas.
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
  traits non generiques, avec dispatch via le type du trait. La stdlib expose
  maintenant le trait public minimal `Sized`, visible via les collections
  publiques qui exposent `size()`, `isEmpty()` et `nonEmpty()`.
  La conversion `toString()` est désormais disponible de façon uniforme pour les
  types primitifs usuels (`Int`, `Long`, `Float`, `Double`, `Bool`, `Char`) au niveau du
  backend runtime, y compris via un paramètre `Any`/`AnyVal` de fonction/méthode
  grâce au boxing heap minimal. Les chaînes `String` portent aussi un tag runtime
  dédié afin de conserver `toString`, `hashCode` et `equals` par contenu quand
  elles sont manipulées via `Any`.
- Standard library deja utile :
  - collections typées et facade `Array[T]`
  - `collections.set` avec `Set[T]`, `add`, `remove`, `union`, `intersect`,
    `difference`
  - `collections.map` avec `Map[K, V]`, recherche optionnelle, mises a jour
    immutables et conversions cle/valeur
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
- `examples/stdlib_collections_cookbook.nabla` et
  `examples/stdlib_text_cookbook.nabla` servent de cookbooks non interactifs
  verifies pour la surface stdlib publique (`Array[T]`, `Set[T]`, `Map[K, V]`,
  `Option[T]`, `Sized` et operations texte), sans construction directe des
  representations de tableaux internes.
- Support Vim minimal disponible dans `editor/vim`.
- Suites `make all-tests`, `make examples` et `make tooling-tests` vertes au
  moment de cette mise a jour; la CI GitHub lance aussi les exemples publics.

## Priorites Prochaine Session

### Fil conducteur produit/langage

La prochaine phase doit transformer Nabla de "langage qui compile beaucoup de
cas" en "langage ou l'utilisateur sait naturellement quelle API employer".
Le cap prioritaire est donc la coherence de surface, pas l'accumulation de
features.

- API publique cible : `Array[T]`, `Option[T]`, `Set[T]`, `Map[K, V]`,
  `Sized`, `String`, classes, methodes, lambdas et modules standards
  documentes.
- Details a cacher ou marquer internes : `IntArray`, `LongArray`,
  `FloatArray`, `DoubleArray`, `BoolArray`, `ObjectArray[T]`,
  `ArrayObject[T]`, helpers `arrayBase...` et fonctions specialisees de pont.
- Runtime a formaliser en continu : valeurs raw `Float`/`Double`, objets heap,
  tableaux natifs, slots nuls, conventions d'erreur et limites memoire. Les
  constantes d'encodage `Int`/`Long`/`Bool` sont deja centralisees dans le code.
- Typage a garder simple : sous-typage nominal pour les classes, generiques
  invariants par defaut, conversions explicites ou helpers stdlib.
- Documentation : la reference HTML doit devenir une doc utilisateur claire,
  avec une separation visible entre API publique et helpers internes. Le
  generateur accepte maintenant `@status` pour afficher des badges de statut;
  toutes les pages de reference actuellement publiees affichent les symboles
  recommandes, avec les alias de compatibilite distingues dans `Array`,
  `Option` et `Set`. Le perimetre cible de Nabla 0.1 est fixe dans
  `docs/releases/0.1.md`.

Actions recommandees :

1. Passer en gel de fonctionnalites pour preparer `v0.1.0`: relire
   `docs/releases/0.1.md`, executer la matrice RC complete sur `master` apres
   les derniers merges, puis ne plus accepter que corrections de bug,
   diagnostics, documentation et exemples avant le tag.
2. Continuer le nettoyage de la référence stdlib générée en affinant les
   descriptions utilisateur et en évitant d'ajouter de nouveaux helpers
   documentés sans classification préalable.
3. Maintenir le check CI pour `make stdlib-docs` et la référence HTML générée.
4. Maintenir le check CI `make examples` pour garantir que les exemples publics
   restent compilables et que leurs sorties attendues ne dérivent pas.
5. Maintenir et enrichir `docs/internals.md`, la specification vivante pour
   types, runtime, IR et conventions de stdlib.
6. Utiliser `docs/stdlib-api.md` pour distinguer API publique, compatibilite
   temporaire et helpers internes avant d'ajouter de nouveaux symboles.
7. Revoir la doc stdlib pour masquer ou signaler les helpers internes.
8. Stabiliser `Array[T]`, `Option[T]` et `Set[T]` comme surfaces utilisateur
   principales.
9. Continuer a garder les exemples publics sur les facades idiomatiques
   (`Array[T]`, `Array.fill[T]`, `Set.fromArray[T]`,
   `Option.some[T]` / `Option.none[T]`) quand elles existent.
   Les cookbooks doivent rester concis et ne pas laisser entendre que tous les
   retours de tableaux masquent deja completement `ArrayObject[T]`.
10. Garder les diagnostics de compatibilite orientes vers les compagnons
   recommandes (`Array.fill`, `Array.rangeUntil`, `Set.fromArray`,
   `Option.some`, `Option.none`, etc.).
11. Poursuivre la surcharge par signature apres `v0.1.0` : la base couvre maintenant les
    fonctions globales, y compris les generiques inferables et les references
    typees, ainsi que les methodes de classe concretes ou generiques par
    signature exacte, y compris l'inference de lambdas apres arguments generiques
    deja resolus; commencer la migration publique de la stdlib vers les noms
    surcharges idiomatiques et garder les diagnostics d'ambiguite riches si la
    resolution devient moins stricte que l'exact match.
12. Reporter `Result[T]`, variance avancee et GC tant que cette surface n'est
   pas propre.

### Revue de code (16/06/2026, corrigée)

- Corrigé — Représentation des `Bool` correcte et explicite :
  - `src/runtime_values.hpp` centralise les valeurs runtime `false = 1` et
    `true = 3`.
  - `src/ast.cpp` émet maintenant les constantes booléennes IR directement avec
    ces valeurs taggées.
  - `src/ir_codegen.cpp` valide que les constantes IR `Bool` sont déjà taggées
    avant de les écrire en assembleur, au lieu de les faire passer par
    l'encodage `Int`.
  - `tests/test_bool_runtime_encoding_regression.nabla` couvre constantes,
    comparaisons, retours de fonctions, opérateurs logiques et `BoolArray`.
- Corrigé — Vérification incomplète de `override` :
  - Le flag `override` est parsé et stocké (`src/parser.cpp:1252`) puis seules les règles de présence/supériorité sont contrôlées en semantique (`src/semantic_analyzer.cpp:205`).
  - La validation compare maintenant arité, paramètres, retour et paramètres génériques de méthode, avec substitutions des types hérités.
- Corrigé — Initialisation des tableaux natifs par défaut à valeur fixe `1` :
  - `src/ir_codegen.cpp:746` remplit maintenant chaque slot des tableaux natifs selon le type d’élément.
  - Le backend initialise désormais `FloatArray`, `DoubleArray` et `ObjectArray[T]` avec `0`, et conserve `1` pour les zéros/faux taggés de `Int`, `Long` et `Bool`.
- Corrigé — Fallbacks silencieux vers `Int` quand un type est inconnu :
  - `src/parser.cpp:1071` utilise maintenant un marqueur `<unresolved>` pour les identifiants non résolus.
  - `src/ir_codegen.cpp:211` échoue maintenant explicitement si un type IR est vide ou introuvable.
- P3 — Corrigé : la référence HTML de la stdlib est vérifiée par la CI :
  - `.github/workflows/ci.yml` lance maintenant `make stdlib-docs` puis
    `git diff --exit-code docs/stdlib` pour empêcher une documentation générée
    désynchronisée.
- P3 — Corrigé : les exemples publics sont vérifiés par la CI :
  - `.github/workflows/ci.yml` lance maintenant `make examples` entre la suite
    générale et les tests d'outillage.

Actions suggérées pour la suite :

1. Finaliser la sémantique d'héritage.
   - Valider la résolution des champs hérités et les conflits de noms entre
     champs/méthodes.
   - Ajouter une erreur explicite quand un membre est ambigu.
2. Consolider le système `super`.
   - Confirmer les cas `super` dans chaînes de mixins / héritage.
   - Définir règles et tests pour les masquages explicites.
3. Améliorer l’ergonomie héritage/collisions de types.
   - Corrigé : `extends Parent(field: Type, extra: Type)` expose une signature
     constructeur héritée lisible ; le préfixe initialise le parent direct et
     le suffixe déclare les champs propres.
   - Rendre la résolution des champs/méthodes héritées plus prédictible dans les
     exemples concrets.
4. Revenir sur le chantier `match` avancé.
   - Finaliser les motifs nommés et les gardes (`motif if condition`).
   - Ajouter des diagnostics propres pour les erreurs de portée/typage des gardes.
5. Réduire la friction entre héritage et collections.
   - Corrigé : `Array[Person]` peut alimenter `Set.fromArray[Person]` avec des
     instances `Student`, `Instructor`, `Volunteer` sans dupliquer les
     spécialisations IR.
   - Corrigé : les appels de méthodes utilisateur redispatchent vers l'override
     runtime quand la valeur est manipulée via un type parent, y compris pour un
     parent générique instancié et une méthode générique spécialisée, tout en
     gardant `super` statique.
   - Corrigé : `Any.toString()`, `Any.hashCode()` et `Any.equals(...)`
     redispatchent vers les overrides utilisateur, ce qui stabilise `==` / `!=`
     et l'index hashé de `Set[Parent]` avec des instances de sous-types.
   - Reste à clarifier : stratégie complète de vtables et éventuelles règles
     d'égalité plus strictes pour les hiérarchies complexes.
   - Documenter des motifs d’exemple pour utiliser ce cas facilement.
6. Ajouter les objets statiques façon Scala.
   - Corrigé : `object Name { def ... }` est supporté comme namespace statique
     abaissé vers des fonctions globales qualifiées (`Name.method(...)`).
   - Corrigé : un `object` peut partager son nom avec une `class` pour servir de
     compagnon de surface (`Box.of(...)`).
   - Reste à clarifier : vrais singletons runtime, champs d'objet et stratégie
     d'initialisation.
7. Ajouter des tests “mélange” héritage + autres fonctionnalités.
   - Cas de régression couvrant `super`, champs hérités, shadowing contrôlé
     et conflit translatif.

## Pistes Plus Larges

- Introduire un type `Result[T]` ou une convention d'erreurs plus riche pour
  l'I/O et le parsing.
- Agrandir ou rendre configurable le heap runtime actuellement initialise a
  8 MiB.
- Refactoriser les primitives runtime I/O pour eviter la duplication autour des
  chemins C.
- Ajouter un support editeur supplementaire :
  - VS Code/TextMate grammar
  - Treesitter plus tard si le langage se stabilise.
- Ajouter plus d'exemples applicatifs :
  - outil de notes avec fichiers
  - mini transformateur de texte
  - Game of Life avec sauvegarde/chargement de grille.

## Regle De Reprise

Avant de commencer une nouvelle feature :

```bash
git status --short
make all-tests
```

Puis travailler en petits commits, comme pendant cette session.
