# Plan De Gestion Mémoire Runtime

**Objectif :** transformer le heap bump-allocated actuel de Nabla en chantier de gestion mémoire explicite et sûr, sans exposer de `delete` dangereux dans la surface normale du langage.

**Architecture :** garder pour l'instant `new` sur le bump allocator `mmap` existant, documenter que les allocations vivent jusqu'à la fin du processus, puis ajouter des capacités runtime incrémentales derrière des sémantiques claires. La surface utilisateur doit rester orientée références sûres (`AnyRef`, `Option[T]`, tableaux/collections); le contrôle manuel doit rester réservé à une future API d'arène ou à un module `unsafe` si un besoin bas niveau concret l'impose.

**Pile technique :** compilateur Nabla C++17 frontend/backend, runtime assembleur x86-64, stdlib/docs/tests Nabla.

---

## État Du Plan

- Phase 1 est couverte par la PR qui introduit ce plan : les docs décrivent le heap monotone actuel, l'absence de `delete` mémoire public, `Option[T]` comme modèle d'absence et les options futures.
- Phase 2 est couverte : le dépassement heap a désormais une régression sous `--heap-size 4096`, un diagnostic stderr stable, le code de sortie 255, des garde-fous contre les wraps arithmétiques de taille d'allocation déjà observés sur les tableaux natifs, et des mitigations utilisateur documentées.
- Phase 3 choisit le GC traçant simple comme direction par défaut : c'est le modèle qui préserve le mieux la surface Scala-like actuelle (`AnyRef`, `Option[T]`, `Array[T]`, closures) sans introduire de `delete` public ni d'obligations de portée pour le code utilisateur.
- Le delta actif passe à la fondation GC : `heapUsed()` / `heapCapacity()` exposent maintenant des points d'observation runtime sans collecte; l'inventaire des familles heap et des racines backend est documenté, les premières métadonnées de racines de frame sont émises sous forme de descripteurs testables, les descripteurs de champs/captures heap sont présents pour les classes/closures, les cartes de points d'appel `Runtime_alloc` générés par le code utilisateur sont disponibles, l'inventaire des allocations internes aux helpers runtime est outillé, et les helpers runtime multi-allocation émettent des cartes candidates inertes. La suite consiste à protéger/spiller les registres et slots natifs transitoires avant tout parcours GC.

## Non-objectifs Pour La Surface Normale

- Ne pas ajouter de `delete value` façon C++ pour les instances ordinaires.
- Ne pas exposer d'adresses brutes, d'arithmétique de pointeurs ou de `null` comme modèle d'absence par défaut.
- Ne pas figer le layout objet ni les détails de vtable comme ABI publique tant que `AnyRef`, les génériques, les modules et la surface stdlib évoluent encore.

## Phase 1: Formaliser Les Sémantiques Actuelles — Couvert

**Objectif :** rendre le contrat runtime actuel clair et régressable.

**Fichiers couverts :**
- `docs/language.md`
- `docs/internals.md`
- `docs/roadmap.md`
- `AGENTS.md`

**Résultat :**
1. `new`, chaînes, tableaux, closures, valeurs boxées allouées sur le heap et singletons runtime produisent des références heap ou des références runtime stables, mais aucun `delete` source n'existe.
2. L'absence de valeur doit passer par `Option[T]`, pas par un `null` public.
3. `deleteTextFile` / `deleteFile` restent explicitement limités à la suppression de fichiers.
4. L'allocateur courant est monotone : les allocations sont récupérées seulement à la fin du processus.
5. La libération manuelle reste reportée; la phase 3 retient un GC traçant simple comme direction sûre par défaut, avec arènes ou couche `unsafe` seulement pour de futurs besoins spécialisés.

## Phase 2: Améliorer L'Observabilité De La Pression Heap

**Objectif :** rendre les dépassements mémoire plus faciles à comprendre avant de changer la sémantique d'allocation.

**Tâches couvertes :**
1. Régression runtime de dépassement heap avec `--heap-size 4096`.
2. Diagnostic observable stable : stderr `Nabla runtime error: heap exhausted` et code de sortie 255.
3. Test d'outillage existant prouvant que l'assembleur généré reçoit bien la capacité demandée.
4. Mitigations usuelles documentées : augmenter `--heap-size`, éviter les concaténations ou répétitions de chaînes non bornées, réutiliser des tableaux mutables quand c'est possible.

**Reste hors phase 2 :**
- Ajouter plus tard, si utile, des métriques de pression heap plus riches que le simple dépassement.

## Phase 3: Choisir Le Premier Modèle De Récupération Sûr — Décidé

**Objectif :** choisir un seul mécanisme de récupération après avoir stabilisé le contrat courant et les diagnostics de pression heap.

**Décision : GC traçant simple, non compactant, comme direction par défaut.**

Raisons :
1. Le code Nabla courant manipule des références ordinaires (`AnyRef`, classes, `String`, `Array[T]`, closures, collections) sans annotations de durée de vie; un GC conserve ce modèle utilisateur.
2. Les arènes sont utiles pour des traitements temporaires, mais elles deviennent sûres seulement avec une analyse d'échappement ou une API explicitement `unsafe`; elles ne doivent donc pas être la première récupération générale.
3. La mémoire manuelle `unsafe` reste reportée tant qu'il n'existe pas de besoin FFI/buffer bas niveau concret, parce qu'elle exposerait double-free, use-after-free et aliasing au programmeur.
4. Un GC non compactant permet d'éviter de déplacer les références avant que le layout objet, les vtables et les conventions de racines soient stabilisés.

**Contraintes pour la première fondation GC :**
- Ne pas activer une collecte tant que les racines ne sont pas énumérées de façon fiable.
- Ne pas changer la surface source : pas de `delete`, pas de pointeurs bruts, pas de `null` public.
- Préférer des métadonnées runtime additives au-dessus des headers actuels plutôt qu'une ABI publique figée.
- Garder le bump allocator actuel comme fallback et comme point d'observation pendant la transition.

**Delta actif : fondation GC sans collecte active.**
1. Points d'observation couverts : `heapUsed()` et `heapCapacity()` lisent l'état
   du bump allocator sans changer la sémantique utilisateur.
2. Inventaire heap couvert dans `docs/internals.md` : `String`, tableaux natifs,
   `ArrayObject[T]`, instances utilisateur, closures, valeurs boxées,
   singletons runtime et valeurs immédiates.
3. Inventaire des racines backend couvert dans `docs/internals.md` : slots de
   frame `StackFrame`, paramètres, temporaires IR, `Store`/`var`, registres
   transitoires autour de `Runtime_alloc`, racines statiques et état runtime des
   helpers assembleur.
4. Métadonnées de racines de frame couvertes : le backend émet
   `nabla_gc_frame_roots_<fonction>` dans `.data` avec le nombre de slots
   référence-capables et leurs offsets `rbp` positifs. Ces descripteurs sont
   testables mais non consommés par le runtime.
5. Descripteurs de champs/captures heap couverts pour les classes et closures :
   `nabla_gc_object_layout_<classe>` liste les champs référence-capables et
   `nabla_gc_closure_layout_<fonction>_<result>` liste les captures
   référence-capables d'une allocation de closure. Ces descripteurs restent
   additifs et non consommés.
6. Cartes de points d'appel `Runtime_alloc` couvertes pour les allocations IR du
   code utilisateur : `nabla_gc_alloc_calls_<fonction>` indexe les cartes
   `nabla_gc_alloc_call_<fonction>_<index>`, qui listent les slots de frame
   référence-capables déjà produits dans le parcours IR linéaire avant le point
   d'allocation. Ces cartes restent additives, non consommées et non encore
   dominance-aware.
7. Inventaire des allocations internes aux helpers runtime couvert :
   `tests/test_gc_runtime_helper_alloc_inventory.py` ancre les appels
   `Runtime_alloc` directs de `src/runtime_asm.cpp` dans `docs/internals.md` et
   force une mise à jour explicite quand un helper runtime gagne ou perd une
   allocation.
8. Cartes candidates de racines internes aux helpers runtime couvertes :
   `nabla_gc_runtime_helper_allocs_<helper>` indexe les cartes
   `nabla_gc_runtime_helper_alloc_<helper>_<index>` pour
   `Runtime_buildArgsArray`, `Runtime_stringToCharArray`,
   `Runtime_stringSplit`, `Runtime_stringSplitMakeSegment` et
   `FloatDouble_method_toString`. Ces cartes listent des registres ou slots
   natifs conservateurs sous forme de descripteurs/commentaires ASM, y compris
   des entrées `interior:*` pour les pointeurs de bytes qui ne sont pas encore des
   racines consommables, restent non consommées par `Runtime_alloc` et sont
   vérifiées par `tests/test_gc_runtime_helper_root_maps.py`. Elles constituent
   la première tranche inerte de cartes racines internes aux helpers runtime
   assembleur, pas encore une protection consommable.
9. Première protection native concrète couverte : `Runtime_buildArgsArray`
   spille manuellement `r15`, qui tient le tableau brut d'arguments, autour de
   l'appel allocant `Runtime_cStringToString` dans la boucle et autour du
   `Runtime_alloc` final qui construit la façade `ArrayObject[String]`. La carte
   candidate de ce second site décrit `native_stack+8`, reste inerte et n'est
   pas consommée par `Runtime_alloc`; `tests/test_gc_runtime_helper_root_spills.py`
   ancre les commentaires et l'ordre `push` / `call` / `pop`.
10. Seconde protection native concrète couverte : `Runtime_stringToCharArray`
   spille le owner `String` source autour du `Runtime_alloc` du tableau brut de
   caractères, puis `rbx` autour du `Runtime_alloc` final de façade
   `ArrayObject[Char]`. Les deux cartes candidates décrivent `native_stack+8`;
   `r10` reste un pointeur intérieur/recalculable et les cartes restent inertes.
11. Troisième protection native concrète couverte : `Runtime_stringSplit`
   spille les owners `String` source/séparateur autour des allocations de
   tableaux bruts, puis `rbx` autour de l'allocation finale de façade
   `ArrayObject[String]`. Les cartes candidates décrivent `native_stack+8` et
   `native_stack+16` selon l'ordre des `push`; `r14`/`r15` restent des pointeurs
   intérieurs/recalculables non consommables.
12. Quatrième protection native concrète couverte :
   `Runtime_stringSplitMakeSegment` conserve les pushes d'état `r8`/`r9`/`rdx`
   et ajoute les spills racines `rbx` puis `r10` autour du `Runtime_alloc` de
   segment. A l'entrée de `Runtime_alloc`, `native_stack+8` décrit `r10` et
   `native_stack+16` décrit `rbx`; `r14` reste intérieur/recalculable et les
   cartes restent inertes.
13. Cinquième protection native concrète couverte :
   `FloatDouble_method_toString` spille `r10`, owner `String` de la partie
   entière produite par `Int_method_toString`, autour des deux `Runtime_alloc`
   directs des chemins fractionnel et sans fraction non nulle. Les deux cartes
   candidates décrivent désormais `native_stack+8`; `[rsp + 16]` reste un slot
   local du helper, mais la racine concrètement protégée au safepoint est le
   `push r10`. Les cartes restent inertes et non consommées par `Runtime_alloc`.
14. Ajouter ensuite la protection/spill automatique des registres transitoires et
   slots natifs autour de `Runtime_alloc`, puis remplacer ou stabiliser ces
   cartes candidates en cartes consommables avant tout parcours GC.
15. Introduire la collecte seulement dans une PR ultérieure, derrière des régressions runtime dédiées.

## Phase 4: Esquisses D'API Futures, Pas Des Engagements

Helpers d'identité sûrs, si besoin :

```nabla
def sameRef(left: AnyRef, right: AnyRef): Bool

def identityHashCode(value: AnyRef): Int
```

API d'arène spécialisée, si un besoin `unsafe` ou bas niveau apparaît plus tard :

```nabla
import runtime.arena

val arena = Arena.create()
val value = arena.newBox[Int](42)
arena.reset()
```

API unsafe, seulement si acceptée explicitement plus tard :

```nabla
import unsafe.memory

val ptr = unsafe.memory.allocate(64)
unsafe.memory.free(ptr)
```

Ces esquisses restent non contraignantes tant que les tests et contraintes runtime ne sont pas conçus.
