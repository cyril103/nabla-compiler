# Checklist RC 0.1

## Objectif

Transformer le perimetre 0.1 en checklist de release executable, sans ajouter de
fonctionnalite langage. Cette passe sert a figer les commandes, les criteres de
sortie et les derniers points de decision avant un tag `v0.1.0`.

## Portee

- Mettre a jour `docs/releases/0.1.md` avec une matrice RC concrete.
- Aligner `AGENTS.md` avec l'etat courant : diagnostics durcis, surface stdlib
  clarifiee et prochaine etape orientee release candidate.
- Aligner `docs/roadmap.md` avec le gel de fonctionnalites et la preparation du
  tag `v0.1.0`.
- Garder la PR documentation-only.

## Verification locale

Executer la meme matrice que la release doit exiger :

```bash
PATH=/opt/data/local/usr/bin:$PATH make all-tests
PATH=/opt/data/local/usr/bin:$PATH make examples
PATH=/opt/data/local/usr/bin:$PATH make tooling-tests
PATH=/opt/data/local/usr/bin:$PATH make stdlib-docs
git diff --exit-code docs/stdlib
g++ -std=c++17 -Wall -Wextra -Werror \
  src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp \
  src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o /tmp/nablac-werror
git diff --check
```

## Non-objectifs

- Pas de nouvelle API stdlib.
- Pas de changement de comportement compilateur/runtime.
- Pas de tag automatique : le tag `v0.1.0` reste une action explicite apres PR.
