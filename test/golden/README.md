# Goldens — dashboard MGF

Rendus de référence produits par le simulateur hôte `../../sim/` à partir de
la même UI LVGL que la cible ESP32-S3.

## Goldens standard (14)

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_amber.png` | **480×480 (rond)** | ambre, 26 barres radiales, séparateurs fins et rangée de 4 indicateurs (cf. `specs/image` photo 1) | `main/ui/screens/style_amber.c` + `main/ui/` |
| `dashboard_amber.png` | **480×480 (rond)** | tableau de bord complet : 11 pages, point de position, page **INJECTION** sélectionnée | `main/ui/navigation/dashboard_navigator.c` |
| `boot_amber.png` | **480×480 (rond)** | écran de démarrage ambre | `main/ui/screens/boot_screen.c` |
| `clock_amber.png` | **480×480 (rond)** | horloge ambre (style propre, hors kit) | `main/ui/screens/clock_screen.c` |
| `{music,navigation,faults}_amber.png` | **480×480 (rond)** | services et diagnostic ambre, kit commun ; musique et navigation avec des données de l'application compagnon | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `{injection,ignition}_amber.png` | **480×480 (rond)** | télémétrie : couronne segmentée, valeur héros, grille 2×2 | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `{temps,lambda,idle,admission}_amber.png` | **480×480 (rond)** | télémétrie sans jauge : filet fin, valeur héros, grille 2×2 | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `settings_amber.png` | **480×480 (rond)** | réglages (luminosité, page de démarrage, BLE ouvert) | `main/ui/screens/settings_screen.c` |

> Cible réelle : écran **rond ~52 mm**. Tous les écrans `*_amber` sont pensés pour ce
> format (canvas carré rempli au maximum).

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/ui/icons/dash_icons.c`.

## Scénarios réels et régressions

Convention : **`<écran>_<scénario>_amber.png`**. Le scénario est le 3e argument
de `gen_golden <style> <png> <scénario>` ; un scénario inconnu est refusé
(code de sortie 2).

| Scénario | Effet sur l'instantané / le rendu | Goldens |
|---|---|---|
| `standard` (défaut) | moteur nominal (eau 89 °C, huile 96 °C, 875 tr/min) | les 14 ci-dessus |
| `offline` | ECU déconnectée : toutes les mesures à `NAN`, la session est invalide | `rpm_offline`, `faults_offline`, `temps_offline`, `injection_offline`, `lambda_offline`, `ignition_offline`, `idle_offline`, `admission_offline`, `music_offline`, `navigation_offline`, `settings_offline` (BLE indisponible) |
| `missing` | connectée mais **eau et huile absentes** (`NAN`) → « -- » sans fausse valeur | `temps_missing` |
| `hot` | surchauffe : eau **110 °C**, huile **135 °C** → alerte thermique | `temps_hot`, `dashboard_hot` |
| `sensor_fault` | eau `NAN` **et** bit `ECU_FAULT_COOLANT_SENSOR` dans `faults_available` | `temps_sensor_fault` |
| `imperial` | `set_units(IMPERIAL)` sur les vues concernées (°F / mi) ; les autres styles restent en standard | `rpm_imperial`, `temps_imperial`, `admission_imperial` |
| `long` | titres/consignes français accentués de plus de 80 caractères → ellipsis (`LV_LABEL_LONG_DOT`) | `music_long`, `navigation_long` |
| `paused` | média reçu avec `state: "paused"` → pictogramme pause | `music_paused` |
| `unsynced` | horloge : `clock_screen_set_time(view, 0, 0, false)` **après** la mise à jour → « HEURE / NON SYNCHRONISEE » | `clock_unsynced` |
| `reconnected` | transitions exercées puis état final nominal : horloge `valid → invalid → valid`, RPM `online → offline → online` | `clock_reconnected`, `rpm_reconnected` |

`dashboard_hot` sélectionne **NAVIGATION** (index 2, page événementielle sans
callback ECU), pose un itinéraire réel puis appelle
`dashboard_navigator_update()` : le bandeau « ALERTE THERMIQUE » est appliqué
globalement et le golden vérifie qu'il **ne masque pas** la manœuvre.

## Tableau de bord — 11 pages (ordre de production)

`sim/main_sim.c` enregistre **exactement** les 11 pages, avec la vraie vue et
son callback de destruction pour chacune :

| # | Nom | Vue | Période de mise à jour |
|---|---|---|---|
| 0 | `HEURE` | `clock_screen` | 1000 ms |
| 1 | `MUSIQUE` | `music_screen` | événementiel |
| 2 | `NAVIGATION` | `navigation_screen` | événementiel |
| 3 | `RPM` | `amber_screen` | 40 ms |
| 4 | `DEFAUTS` | `faults_screen` | 1000 ms |
| 5 | `TEMPERATURES` | `temps_screen` | 150 ms |
| 6 | `INJECTION` | `injection_screen` | 80 ms |
| 7 | `LAMBDA` | `lambda_screen` | 80 ms |
| 8 | `ALLUMAGE` | `ignition_screen` | 80 ms |
| 9 | `RALENTI` | `idle_screen` | 150 ms |
| 10 | `ADMISSION` | `admission_screen` | 150 ms |

Le golden `dashboard_amber` actualise **les 11 pages** puis sélectionne
`INJECTION` (index 6). `dashboard_hot` sélectionne `NAVIGATION` (index 2).

## Données mock

Le simulateur injecte un instantané moteur complet et déterministe dans tous les
écrans. Les scénarios ci-dessus dérivent de cette vue nominale (champs à `NAN`,
surchauffe, défaut sonde, unités impériales, textes longs, média en pause,
transitions de reconnexion). Musique et navigation utilisent des charges utiles
JSON telles que l'application compagnon les envoie.

> Le `°` des unités est dessiné en vectoriel par `amber_readout` (le glyphe
> Michroma ressemble à un « o » bas).

## Régénérer

```bash
./build.sh                   # compile l'UI et régénère tous les PNG
# ou, sans cache (LVGL recompilé) :
sim/build_golden.sh
```

Un seul écran/scénario à la fois :

```bash
sim/build/gen_golden temps ../test/golden/temps_hot_amber.png hot
sim/build/gen_golden dashboard ../test/golden/dashboard_hot_amber.png hot
```
