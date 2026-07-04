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
après formalisation du heap et des mitigations de pression heap, son delta
courant introduit une première collecte GC conservative active, traçante et non
compactante. `Runtime_alloc` utilise un header caché de 16 octets, réutilise
`heap_free_list`, découpe les blocs libres surdimensionnés, appelle `Runtime_gc`
avant overflow et retente l'allocation;
`Runtime_gc` scanne conservativement la pile native jusqu'à `gc_stack_top`, puis
les payloads heap marqués jusqu'à fixpoint, avant de sweep les blocs non marqués
vers la free-list. Les compteurs `heapUsed()` / `heapCapacity()` restent des
observations high-water/capacité, pas une mesure de mémoire vivante; les
compteurs GC `gcCollections()`, `gcLastFreedBytes()`,
`gcLastLargestFreeBlock()`, `heapFreeBytes()` et `heapLargestFreeBlock()`
exposent le nombre de collectes, le dernier sweep et l'état courant de la
free-list pour les tests et diagnostics. La suite de stress GC couvre désormais
les temporaires imbriqués, helpers de chaînes, `Array[T]`, tableaux d'objets,
`Map[K, V]` et `Set[T]` sous heaps serrés afin de garder ces chemins exercés dans
`make tooling-tests`.

L'inventaire des familles heap et des racines backend reste documenté dans
`../internals.md`; les métadonnées de racines de frame, les descripteurs
champs/captures, les cartes de points d'appel `Runtime_alloc`, l'inventaire des
allocations internes aux helpers runtime et les cartes candidates de racines
internes aux helpers runtime sont toujours émis comme métadonnées inertes. Elles
ne sont pas encore consommées par `Runtime_alloc` ou `Runtime_gc`; la suite du
plan consiste à réduire les faux positifs conservateurs en consommant
progressivement ces cartes exactes et en raffinant `heapUsed()` si nécessaire.
La checklist opérationnelle pour intégrer une nouvelle feature est dans
[`docs/feature-integration.md`](../feature-integration.md).

Quand un nouveau chantier reprend un sujet ancien (`List[T]`, `object`, `def`
local, propriétés sans parenthèses, etc.), créer un nouveau plan court centré sur
le delta restant plutôt que de restaurer un plan historique terminé.
