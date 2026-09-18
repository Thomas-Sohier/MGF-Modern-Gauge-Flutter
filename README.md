# MGF Gauge LVGL — ESP32-P4

Portage **de faisabilité** de l'écran RPM principal du projet Flutter
[`MGF-Modern-Gauge-Flutter`](../MGF-Modern-Gauge-Flutter) vers **LVGL 9 / ESP-IDF**,
pour tourner sur un **ESP32-P4** avec écran MIPI-DSI.

Objectif : vérifier qu'un microcontrôleur (RISC-V bicœur, sans GPU) peut rendre
fidèlement et de façon fluide la jauge segmentée à double arc, sans passer par
Flutter/Impeller. Seul l'écran RPM est reproduit ; les données ECU sont
simulées (balayage ralenti → ligne rouge → ralenti).

## Rendu reproduit

- Demi-cercle supérieur segmenté (**20 segments**, gap 3°) — arc primaire RPM.
- **Zone de danger** rouge au-delà de 7000 tr/min (segments éteints atténués).
- Arc **papillon** continu intérieur (angle d'accélérateur).
- Disque de fond + valeur centrale (grand nombre + « RPM »).
- **Indicateurs de métriques en arc** en bas : OBD, RPM, LDR, BATT, HUILE,
  la métrique primaire (RPM) mise en évidence.

Palette et géométrie reprises à l'identique du thème sombre Flutter.

## Correspondance Flutter → LVGL

| Flutter (Dart) | Ici (C / LVGL) | Rôle |
|---|---|---|
| `DualArcDial` (`CustomPainter`) | `dual_arc_dial.c` (event `LV_EVENT_DRAW_MAIN`) | dessin des deux arcs en une passe |
| `ArcGeometry` | fonctions `segment_*` de `dual_arc_dial.c` | angles, seuil de danger, progression |
| `hasGaugeValueChanged` | `gauge_value_changed()` | invalidation seulement si changement visible |
| `GaugeLayout` + `_ArcFlowDelegate` | `rpm_screen.c` (placement `cos/sin`) | disposition en arc des indicateurs |
| `MetricPrimaryDisplay` / `MetricIndicator` | labels de `rpm_screen.c` | valeur centrale + indicateurs |
| `AppColors` / `GaugeTheme` (dark) | macros `MGF_COL_*` de `gauge_theme.h` | palette |
| `EcuInfos` / `DialData` | `ecu_data_t` (`ecu_data.h`) | instantané ECU |
| WebSocket `EcuService` | `fake_ecu.c` | source de données (ici simulée) |

## Matériel cible

Par défaut : **ESP32-P4-Function-EV-Board** (panneau EK79007, 1024×600, MIPI-DSI),
via le BSP `espressif/esp32_p4_function_ev_board` qui gère l'écran, le
rétroéclairage et le port LVGL.

Autre carte P4 (Waveshare, M5Stack Tab5…) : remplacer la dépendance BSP dans
`main/idf_component.yml` — l'API `bsp_display_*` reste identique.

## Build & flash

Nécessite **ESP-IDF ≥ 5.3** installé et sourcé (`. $IDF_PATH/export.sh`).

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Les dépendances (BSP, LVGL, esp_lvgl_port) sont téléchargées automatiquement
au premier `build` dans `managed_components/`.

## Structure

```
main/
  app_main.c        # init BSP/LVGL, timers de rafraîchissement
  gauge_theme.h     # palette + géométrie (constantes du thème Flutter)
  ecu_data.h        # struct ecu_data_t
  fake_ecu.c        # source de données simulée
  dual_arc_dial.*   # widget jauge double arc (dessin custom)
  rpm_screen.*      # composition de l'écran RPM
```

## Simulateur hôte & golden

La même UI (`main/dual_arc_dial.c`, `main/rpm_screen.c`, `main/fake_ecu.c` ne
dépendent que de LVGL) se compile sur PC pour un rendu hors-écran. Utile pour
itérer sans matériel et produire une capture de référence :

```bash
sim/build_golden.sh          # -> test/golden/{rpm_screen,rpm_amber,rpm_cream}.png
```

Trois variantes de style sont rendues (sombre de base, ambre, crème — d'après
`specs/image/`). Voir `sim/` (harnais + `lv_conf.h`) et `test/golden/` (images +
données mock). Icônes vectorielles : `main/gauge_icons.c` ; styles :
`main/style_amber.c`, `main/style_cream.c`.

Le style ambre utilise une police **monospace JetBrains Mono** rendue à la volée
par tiny_ttf (`main/ui_fonts.c`, TTF dans `main/fonts/`, embarqué côté cible via
`EMBED_FILES`). Remplacer le `.ttf` suffit pour changer de fonte (ex. une fonte
plus « rétro »).

## Statut

Code **non compilé dans l'environnement de génération** (ESP-IDF absent).
Écrit pour LVGL 9.x / ESP-IDF 5.3+. À valider par un premier `idf.py build`
sur un poste équipé du toolchain.
