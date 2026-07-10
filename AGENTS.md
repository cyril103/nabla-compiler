# AGENTS.md

Guide de reprise rapide pour les agents qui contribuent au compilateur Nabla.
Ce fichier doit rester court: il donne les règles opérationnelles, les pointeurs
vers les documents de référence et le cap courant. L'historique détaillé vit dans
les commits et les PR GitHub, pas dans ce fichier.

## Règle de maintenance

- Mettre à jour ce fichier seulement quand un changement modifie durablement le
  cap, la surface actuelle, les commandes de validation ou les règles de reprise.
- Ne pas ajouter de journal chronologique de jalons: utiliser l'historique Git,
  les PR, les tests et les release notes pour retrouver les changements livrés.
- Pour une nouvelle feature, suivre `docs/feature-integration.md`: branche
  propre, vérification que la feature n'existe pas déjà, plan actif si utile,
  tests/docs, validation locale et hygiène Markdown avant PR.
- Garder `docs/plans/` réservé aux plans actifs. Supprimer ou archiver dans les
  docs appropriées les plans déjà livrés.
- Régénérer la référence HTML avec `make stdlib-docs` quand une API publique de
  `stdlib/`, un commentaire `///`, une directive `@signature` ou une directive
  `@symbol` change, puis inclure le résultat `docs/stdlib/` dans le commit.
- Ne pas marquer une étape comme terminée sans test automatisé ou validation
  explicite correspondante.

## Vision

Nabla est un langage inspiré de Scala compilé directement vers de l'assembleur
x86-64 ELF Linux. Le projet privilégie une syntaxe concise, un typage statique,
un modèle objet simple, une stdlib lisible et un runtime minimal mais documenté.

Pipeline cible:

```text
Source Nabla
  -> Lexer
  -> Parser / AST
  -> Analyse sémantique
  -> Représentation intermédiaire
  -> Génération x86-64
  -> NASM + ld
```

## Documents sources de vérité

- `README.md`: installation, commandes de base et exemples rapides.
- `docs/language.md`: langage utilisateur et diagnostics publics.
- `docs/internals.md`: conventions d'implémentation, runtime, ABI, GC et IR.
- `docs/stdlib-api.md`: classification public / compatibilité / interne de la
  stdlib.
- `docs/roadmap.md`: état courant synthétique et priorités post-0.1.
- `docs/plans/README.md`: politique des plans actifs.
- `docs/plans/runtime-memory-management.md`: plan actif mémoire/GC.
- `docs/releases/0.1.md`: périmètre et validation du tag `v0.1.0`.
- Historique Git / PR GitHub: journal détaillé des changements livrés.

## Fil conducteur

Le compilateur a dépassé le stade du prototype. Les prochaines évolutions
utiles doivent surtout améliorer la cohérence utilisateur et réduire l'exposition
des détails internes.

Principes:

- préférer une API publique uniforme: `Array[T]`, `Option[T]`, `Set[T]`,
  `Map[K, V]`, `Sized`, `Iterator[T]`, `Iterable[T]`, `String`, classes,
  méthodes et lambdas;
- masquer progressivement `IntArray`, `LongArray`, `ObjectArray[T]`,
  `ArrayObject[T]`, les helpers `arrayBase...` et les fonctions spécialisées;
- garder les spécialisations runtime comme optimisations internes;
- formaliser les conventions runtime avant de les exploiter plus largement:
  tagging de `Int`/`Long`/`Bool`, valeurs raw `Float`/`Double`, objets heap,
  tableaux natifs, slots nuls et erreurs runtime;
- remplacer les fallbacks implicites par des diagnostics explicites;
- produire des diagnostics orientés utilisateur, sans exposer de noms internes
  quand une forme source claire existe;
- considérer la documentation HTML de la stdlib comme une surface produit;
- conserver un typage simple: sous-typage nominal, génériques invariants par
  défaut, conversions explicites ou fonctions stdlib plutôt que magie implicite.

## État actuel synthétique

Le détail exhaustif est dans `docs/language.md`, `docs/internals.md`, les tests et
la roadmap. À la date de ce fichier, le compilateur couvre notamment:

- parsing et analyse sémantique des imports, classes, traits/mixins, objets,
  fonctions, méthodes, blocs, `val`/`var`, `if`/`else if`, `match` avec motifs
  de constructeur V0, boucles, lambdas et fonctions locales;
- imports source relatifs, racine projet et `stdlib/`, packages déclaratifs V0
  validés contre le chemin importé, protection contre les cycles et diagnostic
  d'ambiguïté pour fonctions top-level homonymes importées par nom court;
- classes avec champs constructeur `val`/`var`, getters synthétiques,
  réassignation interne des `var`, héritage nominal, mixins, `super`, dispatch
  dynamique et vérification `override`;
- fonctions globales et méthodes surchargées par signature exacte
  alpha-normalisée; priorité aux variantes concrètes, inférence générique,
  références typées et diagnostics d'ambiguïté;
- fonctions locales `def` abaissées en symboles cachés, avec récursion locale,
  appels directs et réutilisation des paramètres génériques englobants; captures
  implicites et fonctions locales explicitement génériques restent reportées;
- propriétés calculées `def name: T = expr` utilisables sans parenthèses;
- types fonction canoniques `Fn(...)->...`, lambdas sans capture, closures avec
  capture par valeur, fonctions retournant des fonctions et formes curryfiées;
- paramètres by-name `param: => T` avec thunking distinct des `Fn()->T` ordinaires;
- types primitifs `Int`, `Long`, `Bool`, `Float`, `Double`, `Char`, `String`,
  `Unit`, `Nothing`, racines `Any` / `AnyVal` / `AnyRef`, conversions numériques
  de base, boxing et dispatch des méthodes racines (`toString`, `hashCode`,
  `equals`);
- tableaux natifs spécialisés et facades `Array[T]` / `ArrayObject[T]`, avec API
  Scala-like documentée progressivement; la facade générique s'intègre aussi à
  `IterableOps[T, Array, ArrayObject[T]]` pour les opérations expérimentales
  communes;
- stdlib structurée autour de `core`, `collections`, `math`, `strings`, `io` et
  `util`, avec documentation HTML générée;
- backend IR par défaut, génération x86-64, runtime Linux, heap configurable,
  GC conservateur non compactant, métriques de heap/GC et métadonnées GC inertes;
- CLI `nablac`, runner de tests, exemples, tests front-end C++ et outillage de
  formatage sans dépendance externe.

## Invariants d'architecture

- Le backend assembleur doit rester déterministe et testable avec les fixtures
  existantes.
- Les noms internes générés ne doivent pas devenir une API source publique.
- Les helpers stdlib internes doivent être cachés ou marqués comme compatibilité
  quand ils restent importables.
- Les changements de sémantique doivent être couverts par des tests `.nabla` ou
  unitaires C++ ciblés; les changements runtime doivent aussi avoir au moins une
  preuve exécutable.
- Les diagnostics attendus sont des contrats: mettre à jour les `.diagnostic`
  seulement quand le changement de message est intentionnel.
- Les plans de `docs/plans/` décrivent le travail actif, pas l'historique livré.

## Commandes de validation

Préfixer `PATH` par `/opt/data/local/usr/bin` si le NASM local est nécessaire.

```bash
export PATH=/opt/data/local/usr/bin:$PATH
make nablac
make unit-tests PYTHON=python3
make all-tests </dev/null
make examples
make tooling-tests
make stdlib-docs
git diff --exit-code docs/stdlib
git diff --check
```

Selon le changement, ajouter:

```bash
make CXXFLAGS='-std=c++17 -Wall -Wextra -Werror' unit-tests PYTHON=python3
make format-check PYTHON=python3
python3 tests/test_format_sources.py
```

Pour les changements docs-only, le minimum acceptable est généralement:

```bash
make tooling-tests PYTHON=python3
make stdlib-docs
git diff --exit-code docs/stdlib
git diff --check
```

mais exécuter la matrice complète dès qu'un exemple, une spec utilisateur ou une
surface stdlib peut être impacté.

## Feuille de route condensée

La roadmap détaillée est `docs/roadmap.md`. Priorités opérationnelles:

### P0 - Hygiène de surface et reprise

- Garder `AGENTS.md` court et non chronologique.
- Maintenir `docs/feature-integration.md` comme checklist de reprise.
- Garder `docs/plans/` limité aux plans réellement actifs.
- Vérifier les liens Markdown et la cohérence README / roadmap / plans avant les
  PR de documentation.

### P1 - Diagnostics et ergonomie source

- Continuer à durcir les diagnostics sémantiques quand un fallback implicite ou
  un nom interne fuit vers l'utilisateur.
- Prioriser les messages exacts pour héritage, surcharge, génériques, stdlib
  legacy et erreurs runtime déclenchées depuis le source.
- Garder les tests négatifs sous forme `.diagnostic` exacte.

### P1 - Stdlib publique

- Stabiliser la surface recommandée dans `docs/stdlib-api.md`.
- Migrer exemples et docs vers les facades publiques.
- Masquer ou déclasser les helpers de compatibilité non idiomatiques.
- Conserver les surcharges spécialisées comme optimisations internes tant que le
  comportement public reste uniforme.

### P2 - Runtime, mémoire et GC

- Continuer le chantier actif `docs/plans/runtime-memory-management.md`.
- Documenter clairement ce qui est actif: GC conservateur non compactant,
  métriques d'observation, free-list et métadonnées GC encore partiellement
  inertes.
- Éviter toute nouvelle surface source `delete` / pointeur brut sans plan validé.

### P2 - Type system et objets

- Garder les génériques invariants par défaut.
- Introduire variance, `Result[T]` ou autres structures lourdes seulement après
  stabilisation des collections et options.
- Renforcer les tests de dispatch dynamique via parents, traits, collections et
  `Any`.

### P3 - Outillage

- Garder l'outillage sans dépendance inutile et compatible `PYTHON=python3`.
- Étendre les tests rapides front-end quand ils réduisent le coût de feedback.
- Maintenir la génération docs stdlib idempotente en CI.

## Prochaine étape recommandée

Si aucune demande plus précise n'est donnée:

1. partir de `master` propre;
2. lire `docs/feature-integration.md`, `docs/roadmap.md` et les plans actifs;
3. vérifier par recherche/tests que la feature envisagée n'est pas déjà livrée;
4. choisir une petite PR orientée utilisateur: diagnostic, exemple public,
   nettoyage stdlib ou tranche mémoire/GC déjà prévue;
5. finir par tests, revue indépendante, commit et PR GitHub.
