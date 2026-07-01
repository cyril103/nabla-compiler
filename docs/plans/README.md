# Plans actifs

Ce répertoire est réservé aux plans de travail encore utiles pour la suite.

Les plans d'implémentation des jalons déjà livrés ont été retirés de l'arbre
actif; leur résultat est résumé dans [`AGENTS.md`](../../AGENTS.md),
[`docs/roadmap.md`](../roadmap.md), les tests et l'historique des PRs. Pour les
jalons inclus dans `v0.1.0`, [`docs/releases/0.1.md`](../releases/0.1.md)
conserve aussi le périmètre livré et la matrice de validation 0.1.x.

- [`rand-int-convenience.md`](rand-int-convenience.md) — ajouter `randInt()`
  comme commodite sans etat explicite, notamment pour `Array.fill[T](n)(elem)`.

Le cap post-0.1 courant est décrit dans [`docs/roadmap.md`](../roadmap.md) :
durcissement héritage/runtime, nettoyage de la surface stdlib et maintien de
[`docs/internals.md`](../internals.md). La checklist opérationnelle pour
intégrer une nouvelle feature est dans
[`docs/feature-integration.md`](../feature-integration.md).

Quand un nouveau chantier reprend un sujet ancien (`List[T]`, `object`, `def`
local, propriétés sans parenthèses, etc.), créer un nouveau plan court centré sur
le delta restant plutôt que de restaurer un plan historique terminé.
