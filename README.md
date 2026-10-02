# MemoMind — Liste de courses (shopping-list)

Application **appairée** MemoMind : liste de courses interactive affichée sur les lunettes sous forme de carrousel (5 lignes), synchronisée avec le téléphone.

## Composition

| Côté | Dossier | Artefact |
| --- | --- | --- |
| Lunettes (plugin natif C) | `glass/shopping-list/` | `builds/shopping-list.gmp` |
| Téléphone (plugin Web) | `phone/shopping-list/` | `builds/shopping-list-1.0.0.mmpkg` |

## Fonctionnement

- Le téléphone gère les articles (ajout, coché, suppression).
- Les lunettes affichent la liste en carrousel avec un effet de dégradé (la ligne centrale est en grand, les autres réduites/estompées).

## Build

Nécessite le SDK officiel MemoMind (`memomind-open/plugin-open-platform`). Déposer `glass/shopping-list/` dans `GlassSDK/examples/` et `phone/shopping-list/` dans `PhoneSDK/examples/`, puis lancer `python3 build.py`.

- Lunettes : `python3 build.py glass --force` → `.gmp`
- Téléphone : `python3 build.py web --force` → `.mmpkg`
