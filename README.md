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

Le langage met l'accent sur une unification forte des types (tout est objet, y compris les entiers), l'absence de runtime lourd ou de machine virtuelle, et une exécution ultra-rapide au plus proche du matériel.

---

## 🚀 Fonctionnalités Clés & Choix d'Architecture

* **Unification du Type `Int` :** Contrairement aux langages classiques où les types primitifs sont gérés à part, `Int` dans Nabla est une véritable classe possédant ses propres méthodes (`+`, `-`, `*`, `/`, `.toString()`).
* **Pointer Tagging (Bit de poids faible) :** Pour éviter d'allouer les entiers sur le tas à chaque opération, Nabla utilise le *pointer tagging*. Le bit de poids faible détermine la nature de la donnée :
    * `bit 0 == 1` : C'est un entier immédiat taggué. Sa valeur réelle en mémoire est $V_{real} = (V \times 2) + 1$. Le runtime corrige le tag à la volée en assembleur lors des calculs.
    * `bit 0 == 0` : C'est un pointeur direct vers un objet aligné sur le tas.
* **Modèle Objet & Pointeur `this` :** L'instanciation via `new` crée une disposition linéaire d'éléments de 8 octets (Offset 0: Pointeur de VTable, Offsets suivants: Attributs). Lors de l'appel d'une méthode, le registre `RDI` reçoit secrètement l'adresse de l'objet faisant office de contexte `this`.
* **Bump Allocator Interne :** L'allocation des objets se fait sur un tas statique virtuel (`global_heap`) géré par un pointeur de tas (`heap_pointer`) incrémenté de manière synchrone en assembleur.
* **Système d'Import Résolutif :** Gestion des dépendances par graphe de fichiers avec détection des inclusions cycliques pour éviter la duplication de code généré.
* **Fonctions et Méthodes Paramétrées :** Les signatures sont validées
  statiquement. Les fonctions globales acceptent jusqu'à 6 paramètres et les
  méthodes jusqu'à 5 paramètres, `RDI` étant réservé à `this`.

---

## 📂 Structure du Dépôt

```text
nabla-compiler/
├── src/
│   ├── main.cpp       # Pipeline principal du compilateur (I/O, génération binaire)
│   ├── lexer.hpp      # Analyseur lexical (Tokenisation de la syntaxe)
│   ├── parser.hpp     # Analyseur syntaxique avec précédence et résolution d'imports
│   └── ast.hpp        # Nœuds de l'AST et logique de génération d'assembleur x86_64
├── tests/
│   ├── test_import.nabla  # Script Nabla principal testant les fonctionnalités objets
│   └── utils/
│       └── Math.nabla     # Sous-module Nabla importé dynamiquement
├── Makefile           # Automatisation de la compilation du projet
└── .gitignore         # Fichiers et binaires temporaires à ignorer

## 🛠️ Compilation et usage

Le projet se compile avec GNU C++17 et le `Makefile` expose des cibles pratiques :

- `make` : compile le compilateur `nablac`
- `make test` : exécute `nablac` sur la source par défaut (`tests/test_import.nabla`), affiche le code ASM généré et lance ensuite le binaire produit
- `make debug` : exécute `nablac --keep-asm` sur la source par défaut, conserve le fichier assembleur `<basename>_tmp.asm`, puis lance le binaire
- `make clean` : supprime `nablac` et le binaire généré par le test

Le compilateur peut aussi afficher sa représentation intermédiaire textuelle :

```bash
build/nablac --emit-ir tests/test_function_parameters.nabla
```

```text
function add(%a: Int, %b: Int) -> Int
  %0 = + %a, %b
  return %0
```

Cette première IR couvre les fonctions globales, entiers, variables,
affectations, opérations binaires, appels de fonctions globales, `if`, `while`,
`for`, instanciations d'objets, accès aux champs, appels de méthodes et
`Int.toString`. La génération assembleur reste pour le moment directe depuis
l'AST.

La génération assembleur utilise désormais le backend IR par défaut :

```bash
build/nablac tests/test_arithmetic.nabla
```

Ce backend couvre désormais la suite positive actuelle, incluant fonctions,
variables, contrôle de flux, imports, objets, champs, appels de méthodes et
`Int.toString`.

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
