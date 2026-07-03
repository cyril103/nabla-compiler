# Plan De Gestion Mémoire Runtime

**Objectif :** transformer le heap bump-allocated actuel de Nabla en chantier de gestion mémoire explicite et sûr, sans exposer de `delete` dangereux dans la surface normale du langage.

**Architecture :** garder pour l'instant `new` sur le bump allocator `mmap` existant, documenter que les allocations vivent jusqu'à la fin du processus, puis ajouter des capacités runtime incrémentales derrière des sémantiques claires. La surface utilisateur doit rester orientée références sûres (`AnyRef`, `Option[T]`, tableaux/collections); le contrôle manuel doit rester réservé à une future API d'arène ou à un module `unsafe` si un besoin bas niveau concret l'impose.

**Pile technique :** compilateur Nabla C++17 frontend/backend, runtime assembleur x86-64, stdlib/docs/tests Nabla.

---

## État Du Plan

- Phase 1 est couverte par la PR qui introduit ce plan : les docs décrivent le heap monotone actuel, l'absence de `delete` mémoire public, `Option[T]` comme modèle d'absence et les options futures.
- Phase 2 est couverte : le dépassement heap a désormais une régression sous `--heap-size 4096`, un diagnostic stderr stable, le code de sortie 255, des garde-fous contre les wraps arithmétiques de taille d'allocation déjà observés sur les tableaux natifs, et des mitigations utilisateur documentées.
- Phase 3 choisit le GC traçant simple comme direction par défaut : c'est le modèle qui préserve le mieux la surface Scala-like actuelle (`AnyRef`, `Option[T]`, `Array[T]`, closures) sans introduire de `delete` public ni d'obligations de portée pour le code utilisateur.
- Le delta actif passe à la fondation GC : `heapUsed()` / `heapCapacity()` exposent maintenant des points d'observation runtime sans collecte; l'inventaire des familles heap et des racines backend est documenté, et la suite consiste à produire des métadonnées/descripteurs testables avant tout parcours GC.

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
4. Ajouter ensuite des métadonnées ou tables de descripteurs assez petites pour tester le parcours sans modifier la sémantique utilisateur.
5. Introduire la collecte seulement dans une PR ultérieure, derrière des régressions runtime dédiées.

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
