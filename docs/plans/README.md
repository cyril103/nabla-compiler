# Plans actifs

Ce répertoire est réservé aux plans de travail encore utiles pour la suite.

Les plans d'implémentation des jalons déjà livrés ont été retirés de l'arbre
actif; leur résultat se retrouve dans [`docs/roadmap.md`](../roadmap.md), les
tests, les release notes et l'historique des PRs. Pour les jalons inclus dans
`v0.1.0`, [`docs/releases/0.1.md`](../releases/0.1.md) reste la synthèse de
référence.

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
`gcLastLargestFreeBlock()`, `gcLastMarkedBlocks()`, `gcLastFreedBlocks()`,
`gcLastStackWords()`, `gcLastHeapWords()`, `gcLastStackCandidateWords()`,
`gcLastHeapCandidateWords()`, `gcLastStackInteriorCandidateWords()`,
`gcLastHeapInteriorCandidateWords()`, `gcLastAllocSafepointMapFound()`,
`gcLastAllocSafepointMapMissed()`, `gcLastAllocSafepointRootSlots()`,
`gcLastAllocSafepointRootBytes()`, `heapAllocatedBytes()`, `heapFreeBytes()`,
`heapFreeBlockCount()` et `heapLargestFreeBlock()` exposent
le nombre de collectes, le dernier sweep, le marquage, le volume de scan
conservateur, le bruit candidat pile/heap, les candidats intérieurs au payload,
le lookup observationnel du return PC d'allocation vers une carte exacte non
consommée, le payload encore alloué et l'état courant de la free-list pour
les tests et diagnostics. La suite de stress GC couvre désormais
les temporaires imbriqués, helpers de chaînes, `Array[T]`, tableaux d'objets,
`Map[K, V]` et `Set[T]` sous heaps serrés afin de garder ces chemins exercés dans
`make tooling-tests`.

L'inventaire des familles heap et des racines backend reste documenté dans
`../internals.md`; les métadonnées de racines de frame, les descripteurs
champs/captures, l'index `nabla_gc_static_roots` des singletons runtime et
littéraux `String` statiques, les cartes de points d'appel `Runtime_alloc`,
l'inventaire des allocations internes aux helpers runtime et les cartes
candidates de racines internes aux helpers runtime sont toujours émis comme
métadonnées inertes. Les appels `Runtime_alloc` utilisateur portent aussi des
commentaires ASM `gc alloc safepoint map ... exact-frame-offsets-consumed` qui les relient aux
cartes correspondantes, un label
`nabla_gc_alloc_return_<fonction>_<index>` immédiatement après l'appel, et un
index `nabla_gc_alloc_safepoints_<fonction>` qui associe chaque return PC à sa
carte, plus l'index global `nabla_gc_alloc_safepoint_tables`; `Runtime_gc`
parcourt désormais ces index pour exposer found/missed, lit le header count de
la carte trouvée pour exposer slots/octets déclarés, puis consomme les offsets
`rbp - offset` de cette carte afin de marquer les slots de frame exacts avant le
scan conservateur de fallback. Les cartes utilisateur deviennent donc
partiellement consommées; les cartes helpers runtime, layouts et autres
métadonnées restent inertes tant qu'elles ne sont pas branchées au marqueur.
La suite du plan consiste à réduire les faux positifs conservateurs en
consommant progressivement les autres métadonnées exactes et en raffinant
`heapUsed()` si nécessaire.

Le chantier actif des collections higher-kinded est suivi dans
[`higher-kinded-collection-ops.md`](higher-kinded-collection-ops.md): le support
`CC[_]`, les builders/factories expérimentaux et l'intégration `IterableOps` de
`List[T]` / `Set[T]` / la facade generique `Array[T]` sont couverts; les
operations communes supplémentaires, les constructeurs d'arité 2 pour `Map` et
la variance restent des deltas séparés.

Le plan [`array-monomorphization.md`](array-monomorphization.md) décrit la suite
possible pour spécialiser `Array[T]` en interne sans élargir la surface publique
au-delà de la facade unique.

Les plans package/namespaces et parité numérique de `Char` ont été retirés de
l'arbre actif parce que leurs tranches livrées sont maintenant résumées dans la
roadmap, les tests et les PRs. Les imports sélectifs/alias/wildcards, packages
multi-fichiers, reexports/prelude et motifs de constructeur qualifiés restent des
pistes futures à reprendre par un nouveau plan court si elles redeviennent
prioritaires.

La checklist opérationnelle pour intégrer une nouvelle feature est dans
[`docs/feature-integration.md`](../feature-integration.md).

Quand un nouveau chantier reprend un sujet ancien (`List[T]`, `object`, `def`
local, propriétés sans parenthèses, etc.), créer un nouveau plan court centré sur
le delta restant plutôt que de restaurer un plan historique terminé.
