# Plan De Gestion Mémoire Runtime

**Objectif :** transformer le heap bump-allocated actuel de Nabla en chantier de gestion mémoire explicite et sûr, sans exposer de `delete` dangereux dans la surface normale du langage.

**Architecture :** garder pour l'instant `new` sur le bump allocator `mmap` existant, documenter que les allocations vivent jusqu'à la fin du processus, puis ajouter des capacités runtime incrémentales derrière des sémantiques claires. La surface utilisateur doit rester orientée références sûres (`AnyRef`, `Option[T]`, tableaux/collections); le contrôle manuel doit rester réservé à une future API d'arène ou à un module `unsafe` si un besoin bas niveau concret l'impose.

**Pile technique :** compilateur Nabla C++17 frontend/backend, runtime assembleur x86-64, stdlib/docs/tests Nabla.

---

## État Du Plan

- Phase 1 est couverte par la PR qui introduit ce plan : les docs décrivent le heap monotone actuel, l'absence de `delete` mémoire public, `Option[T]` comme modèle d'absence et les options futures.
- Le delta actif commence donc en phase 2 : rendre la pression heap et les dépassements plus observables avant de choisir un mécanisme de libération.

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
5. La libération manuelle est reportée en faveur d'un choix explicite entre GC, arènes ou couche `unsafe`.

## Phase 2: Améliorer L'Observabilité De La Pression Heap

**Objectif :** rendre les dépassements mémoire plus faciles à comprendre avant de changer la sémantique d'allocation.

**Tâches potentielles :**
1. Ajouter ou durcir une régression runtime de dépassement heap avec un `--heap-size` volontairement petit.
2. Stabiliser et documenter le code de sortie et le message observable lors d'un dépassement heap.
3. Garder un test d'outillage prouvant que l'assembleur généré reçoit bien la capacité demandée.
4. Documenter les mitigations usuelles : augmenter `--heap-size`, éviter la concaténation de chaînes/listes non bornée, réutiliser des tableaux quand c'est possible.

**Fichiers probables :**
- `tests/` runtime ou tooling tests
- `src/runtime_asm.cpp`
- `docs/language.md`
- `docs/internals.md`

## Phase 3: Choisir Le Premier Modèle De Récupération Sûr

**Objectif :** choisir un seul mécanisme de récupération après avoir stabilisé le contrat courant et les diagnostics de pression heap.

**Candidat A : portées d'arène**
- Ajouter des arènes explicites ou régions pour allocations temporaires.
- Sûr seulement si les valeurs allouées dans l'arène ne peuvent pas s'échapper, ou si l'API est marquée `unsafe`.
- Utile pour parseurs, buffers, traitements temporaires et batchs.

**Candidat B : GC traçant simple**
- Plus naturel pour du code utilisateur Scala-like.
- Exige énumération des racines, conventions stack/globales, métadonnées de parcours objets/tableaux/chaînes et interactions avec closures et valeurs boxées.
- Évite d'exposer `delete`; le code utilisateur garde des références ordinaires.

**Candidat C : mémoire manuelle unsafe**
- Uniquement derrière un module comme `unsafe.memory`.
- Doit documenter que use-after-free, double-free et aliasing sont à la charge du programmeur.
- À reporter tant que FFI ou buffers bas niveau ne créent pas un vrai besoin.

## Phase 4: Esquisses D'API Futures, Pas Des Engagements

Helpers d'identité sûrs, si besoin :

```nabla
def sameRef(left: AnyRef, right: AnyRef): Bool

def identityHashCode(value: AnyRef): Int
```

API d'arène, si ce modèle est choisi :

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
