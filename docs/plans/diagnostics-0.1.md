# Diagnostics utilisateur 0.1

## Objectif

Durcir une petite tranche de diagnostics avant Nabla 0.1, sans ajouter de
nouvelle fonctionnalite langage. La priorite est que les erreurs autour de
l'heritage, `override` et `super` indiquent le contexte utile pour corriger le
programme.

## Scope initial

1. `override` avec signature incompatible : afficher les signatures heritees
   candidates pour le meme nom de methode.
2. `override` sans methode heritee du meme nom : afficher les methodes heritees
   visibles pour eviter un diagnostic opaque.
3. `super` invalide dans une classe sans parent explicite : inclure le nom de la
   classe et la correction attendue (`extends Parent(...)`).

## TDD

1. Mettre a jour/ajouter des `.diagnostic` attendus qui decrivent le message
   cible.
2. Lancer les tests cibles et verifier qu'ils echouent en RED avec les anciens
   messages.
3. Modifier le compilateur au minimum pour produire les details attendus.
4. Relancer les tests cibles, puis `make all-tests`, `make examples`,
   `make tooling-tests`, `git diff --check` et une compilation `-Werror`.

## Hors scope

- Pas de nouvelle regle d'heritage.
- Pas de changement de resolution de surcharge.
- Pas de nouvelle linearisation de traits/mixins.
- Pas de refonte globale de l'infrastructure de diagnostics.
