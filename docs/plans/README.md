# Plans actifs

Ce répertoire est réservé aux plans de travail encore utiles pour la suite.

Les plans d'implémentation des jalons déjà livrés ont été retirés de l'arbre
actif; leur résultat est résumé dans [`AGENTS.md`](../../AGENTS.md),
[`docs/roadmap.md`](../roadmap.md), les tests et l'historique des PRs. Pour les
jalons inclus dans `v0.1.0`, [`docs/releases/0.1.md`](../releases/0.1.md)
conserve aussi le périmètre livré et la matrice de validation 0.1.x.

Le cap post-0.1 courant est décrit dans [`docs/roadmap.md`](../roadmap.md) :
durcissement héritage/runtime, nettoyage de la surface stdlib et maintien de
[`docs/internals.md`](../internals.md). Le chantier actif de stratégie mémoire
runtime est suivi dans [`runtime-memory-management.md`](runtime-memory-management.md);
après formalisation du heap monotone et des mitigations de pression heap, son
delta courant est la fondation d'un GC traçant simple non compactant; les
compteurs `heapUsed()` / `heapCapacity()` sont disponibles comme observation
sans collecte, l'inventaire des familles heap et des racines backend est
documenté dans `../internals.md`, les premières métadonnées de racines de frame
sont émises dans l'assembleur, et les descripteurs champs/captures plus les
cartes de points d'appel `Runtime_alloc` restent à stabiliser avant tout parcours
GC.
La checklist opérationnelle pour intégrer une nouvelle feature est dans
[`docs/feature-integration.md`](../feature-integration.md).

Quand un nouveau chantier reprend un sujet ancien (`List[T]`, `object`, `def`
local, propriétés sans parenthèses, etc.), créer un nouveau plan court centré sur
le delta restant plutôt que de restaurer un plan historique terminé.
