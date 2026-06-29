# Integrer une nouvelle feature

Ce guide sert de point d'entree pour reprendre le depot proprement avant une
nouvelle evolution. Il complete `AGENTS.md`, `docs/roadmap.md` et
`docs/plans/README.md` sans remplacer la specification vivante.

## 1. Etat de depart

Avant d'ouvrir une branche feature, lire aussi `docs/roadmap.md` pour le cap
produit et ce guide pour la checklist d'integration.

```bash
git fetch origin --prune
git checkout master
git pull --ff-only origin master
git status --short --branch
```

Verifier aussi l'historique recent des PRs pour eviter de recreer une feature
deja livree. Si une branche locale suit une branche distante supprimee, ne la
conserver que si elle contient encore un delta utile; sinon la supprimer apres
avoir confirme que la PR correspondante est mergee.

## 2. Choisir le bon support documentaire

- `docs/plans/` ne contient que les plans actifs. Creer un plan court quand la
  feature demande plusieurs etapes ou touche parser/semantique/IR/backend.
- `docs/roadmap.md` decrit le cap produit et les priorites a moyen terme.
- `docs/internals.md` documente les invariants de compilation/runtime qui
  guideront les futures features.
- `docs/stdlib-api.md` classe la surface publique, les compatibilites et les
  details internes avant tout changement de stdlib.
- `AGENTS.md` reste le resume operatoire pour agents: etat courant, feuille de
  route, jalons et prochaine etape recommandee.

Ne pas restaurer un ancien plan supprime pour reprendre un sujet historique:
creer un delta-plan qui cite les tests, docs et PRs deja livres.

## 3. Definition of done feature

Une feature est prete a integrer quand elle a :

1. des tests positifs et negatifs cibles, avec `.expected`, `.stdout`,
   `.diagnostic` ou `.ir` quand necessaire;
2. au moins un test runtime lorsque le dispatch, le layout objet, le boxing, les
   collections ou les appels indirects peuvent diverger entre semantique et
   backend;
3. les docs utilisateur/internals/stdlib mises a jour selon la surface touchee;
4. `AGENTS.md` aligne dans le meme commit;
5. une validation locale adaptee, au minimum:

```bash
make all-tests
make examples
make tooling-tests
git diff --check
```

Ajouter `make stdlib-docs` et verifier le diff genere quand une API publique de
`stdlib/` ou un commentaire `///` change.

## 4. Hygiene avant PR

Avant de pousser :

```bash
git status --short --branch
git diff --check
git grep -n "TODO\|FIXME\|WIP" -- '*.md' 'src/*' 'stdlib/*' 'tests/*' || true
```

Relire les references Markdown ajoutees ou modifiees. Les liens vers des plans
supprimes ne doivent rester que dans un historique explicite; les documents de
reprise actifs doivent pointer vers `docs/roadmap.md`, `docs/internals.md`, les
tests ou l'historique de PR.

## 5. Apres merge squash

Apres une PR mergee et la branche distante supprimee :

```bash
git fetch origin --prune
git checkout master
git pull --ff-only origin master
```

Comparer le contenu avant de supprimer une branche locale si necessaire:

```bash
git diff --stat master..<branche>
```

Si le delta n'est qu'un ancien etat deja merge puis depasse par `master`, verifier
la PR GitHub correspondante avant de supprimer la branche locale.
