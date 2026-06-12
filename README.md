# Nabla Compiler

Un compilateur de recherche écrit en C++17 qui traduit un sous-ensemble orienté objet inspiré de **Scala** directement en assembleur **x86_64 natif (ELF Linux)**. 

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
