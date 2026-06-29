# Nabla Compiler

Un compilateur de recherche écrit en C++17 qui traduit un sous-ensemble orienté objet inspiré de **Scala** directement en assembleur **x86_64 natif (ELF Linux)**. 

Le pipeline sépare désormais l'analyse syntaxique, l'analyse sémantique et la
génération de code. L'analyse sémantique valide notamment les classes,
constructeurs, appels de méthodes, types déclarés et affectations avant
l'émission de l'assembleur.

Les diagnostics indiquent le fichier, la ligne, la colonne et la phase concernée :

```text
tests/example.nabla:8:15: semantic error: méthode inconnue: A.missing
```

Le langage met l'accent sur une surface Scala-like simple, une hiérarchie de
types unifiée (`Any` / `AnyVal` / `AnyRef`), l'absence de machine virtuelle et
une exécution native proche du matériel.

---

## 🚀 Fonctionnalités Clés & Choix d'Architecture

* **Hiérarchie de types unifiée :** `Any` est le supertype de toutes les valeurs.
  `AnyVal` couvre les primitives builtin (`Int`, `Long`, `Bool`, `Float`,
  `Double`, `Char`, `Unit`) et `AnyRef` couvre les références heap (`String`,
  tableaux, closures et classes utilisateur).
* **Pointer Tagging :** `Int`, `Long` et `Bool` utilisent le bit de poids faible
  pour représenter les valeurs immédiates. Les constantes runtime communes sont
  centralisées dans `src/runtime_values.hpp`.
* **Modèle Objet & Pointeur `this` :** L'instanciation via `new` crée une
  disposition linéaire de slots de 8 octets. Le slot 0 porte un identifiant de
  classe runtime pour le dispatch dynamique simple des overrides utilisateur,
  puis viennent les champs. Lors d'un appel de méthode, `RDI` reçoit l'objet
  courant comme `this`.
* **Bump Allocator Interne :** Le runtime initialise par défaut un tas de 8 MiB
  avec `mmap`, configurable à la compilation via `--heap-size <octets>`, aligne
  les allocations sur 8 octets et vérifie les dépassements.
* **Système d'Import Résolutif :** Gestion des dépendances par graphe de fichiers
  avec détection des inclusions cycliques pour éviter la duplication de code généré.
* **Fonctions et Méthodes Paramétrées :** Les signatures sont validées
  statiquement. Les fonctions globales acceptent jusqu'à 6 paramètres et les
  méthodes jusqu'à 5 paramètres, `RDI` étant réservé à `this`.

---

## 📂 Structure du Dépôt

```text
nabla-compiler/
├── src/
│   ├── main.cpp                 # Pipeline principal et invocation NASM/ld
│   ├── lexer.hpp                # Tokenisation
│   ├── parser.cpp/.hpp          # Syntaxe, imports, collecte des declarations
│   ├── semantic_analyzer.cpp    # Validation des types, classes et signatures
│   ├── ast.cpp/.hpp             # Nœuds AST et lowering vers IR
│   ├── ir.cpp/.hpp              # IR textuelle et builder
│   ├── ir_codegen.cpp/.hpp      # Génération x86_64 depuis l'IR
│   └── runtime_asm.cpp/.hpp     # Runtime assembleur partagé
├── stdlib/             # Bibliothèque standard Nabla
├── examples/           # Exemples publics et scénarios applicatifs
├── docs/               # Guide langage, API stdlib, internals, roadmap et workflow feature
├── tests/
│   ├── *.nabla          # Tests positifs et négatifs
│   ├── *.expected       # Codes de sortie attendus
│   ├── *.stdout         # Sorties console attendues optionnelles
│   ├── *.diagnostic     # Diagnostics attendus optionnels
│   └── *.ir             # Snapshots IR optionnels
├── Makefile           # Automatisation de la compilation du projet
└── .gitignore         # Fichiers et binaires temporaires à ignorer
```

## Documentation du langage

Une première documentation utilisateur du langage est disponible ici :

- [docs/language.md](docs/language.md)
- [docs/stdlib-api.md](docs/stdlib-api.md)
- [docs/roadmap.md](docs/roadmap.md)
- [docs/internals.md](docs/internals.md)
- [docs/feature-integration.md](docs/feature-integration.md)
- [docs/releases/0.1.md](docs/releases/0.1.md)

La reference HTML de la bibliotheque standard se genere depuis les commentaires
`///` places devant les declarations publiques :

```bash
make stdlib-docs
```

Le point d'entree genere est `docs/stdlib/index.html`.

## Support éditeur

Un support Vim minimal est disponible dans [editor/vim](editor/vim). Pour
l'installer localement :

```bash
mkdir -p ~/.vim/ftdetect ~/.vim/syntax
cp editor/vim/ftdetect/nabla.vim ~/.vim/ftdetect/
cp editor/vim/syntax/nabla.vim ~/.vim/syntax/
```

Les fichiers `*.nabla` seront ensuite detectes avec `filetype=nabla` et une
coloration syntaxique de base.

## 🛠️ Compilation et usage

Le projet se compile avec GNU C++17 et le `Makefile` expose des cibles pratiques :

- `make` : compile le compilateur `nablac`
- `make test` : exécute `nablac` sur la source par défaut (`tests/test_import.nabla`), affiche le code ASM généré et lance ensuite le binaire produit
- `make debug` : exécute `nablac --keep-asm` sur la source par défaut, conserve le fichier assembleur `<basename>_tmp.asm`, puis lance le binaire
- `make all-tests` : exécute la suite de tests langage complète
- `make tooling-tests` : vérifie les diagnostics d'outillage du compilateur, par exemple `nasm` absent du `PATH`
- `make examples` : compile les exemples publics et vérifie leurs oracles quand
  ils existent
- `make stdlib-docs` : régénère la référence HTML de la stdlib
- `make clean` : supprime `build/`

Le compilateur peut aussi afficher sa représentation intermédiaire textuelle :

```bash
build/nablac --emit-ir tests/test_function_parameters.nabla
```

```text
function add(%a: Int, %b: Int) -> Int
  %0 = + %a, %b
  return %0
```

L'IR couvre le langage testé actuel : fonctions, variables, contrôle de flux,
`match`, objets, héritage, appels de méthodes, lambdas/closures, tableaux
natifs, génériques monomorphisés et appels vers la bibliothèque standard.

La génération assembleur utilise le backend IR par défaut :

```bash
build/nablac tests/test_arithmetic.nabla
```

L'option historique `--backend-ir` est conservée pour compatibilité, mais elle
ne sélectionne plus un chemin séparé.

### Personnaliser le fichier source

Le `Makefile` supporte une variable `SRC` pour choisir le fichier Nabla à compiler :

```bash
make debug SRC=tests/test_import.nabla
make test SRC=tests/test_import.nabla
```

Le binaire exécuté est déterminé automatiquement à partir du nom de fichier `SRC`.

### Fonctions et méthodes avec paramètres

```nabla
def add(a: Int, b: Int): Int = {
    a + b
}

class Calculator(base: Int) {
    def add(value: Int): Int = {
        base + value
    }
}

def main(): Int = {
    add(20, (new Calculator(2)).add(20))
}
```

### Fichier ASM généré

Lorsque `--keep-asm` est utilisé (via `make debug`), le compilateur conserve le fichier assembleur généré. Par exemple pour `tests/test_import.nabla`, le fichier créé est :

```text
test_import_tmp.asm
```

### Tests automatiques

Le projet dispose désormais d’une cible `make all-tests` qui exécute tous les fichiers `tests/*.nabla` avec `nablac --keep-temp`.

```bash
make all-tests
```

Chaque test normal possède un fichier voisin `<nom>.expected` contenant le code de
sortie attendu. La cible compile puis exécute le programme et compare son code de
sortie à cette valeur. Les tests d’erreur dont le nom contient `error` ou `fail`
doivent échouer pendant la compilation. Lorsqu'un fichier voisin
`<nom>.diagnostic` existe, le message d'erreur normalisé doit également
correspondre exactement. Lorsqu'un fichier voisin `<nom>.ir` existe, la sortie
de `nablac --emit-ir` est elle aussi comparée exactement au snapshot. Lorsqu'un
fichier voisin `<nom>.ir-backend.expected` existe, le même test est aussi compilé
avec `nablac --backend-ir` puis exécuté.

La cible affiche `PASS` ou `FAIL` pour chaque fichier de test et renvoie `1` si un test donne un résultat inattendu.
