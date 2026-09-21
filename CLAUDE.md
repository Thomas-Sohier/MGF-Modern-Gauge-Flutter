# CLAUDE.md — MGF Gauge LVGL

Portage LVGL de l'écran RPM principal du projet Flutter `MGF-Modern-Gauge-Flutter`
vers un microcontrôleur. Rendu **entièrement vectoriel** (aucune image de fond),
piloté par des données ECU simulées. Le style de référence est **« amber »**
(cadran automobile rond, monochrome ambre) — c'est celui qui cible le matériel.

## Matériel cible

**Waveshare ESP32-S3-Touch-LCD-2.1** (fiche : `specs/ESP32-S3 ….html`).

| | |
|---|---|
| MCU | ESP32-S3, Xtensa LX7 double cœur @ 240 MHz |
| Mémoire | 512 KB SRAM + 384 KB ROM, **16 MB Flash**, **8 MB PSRAM** |
| Écran | IPS **rond 480×480**, 262K couleurs, 2.1" |
| Interface écran | **RGB**, driver **ST7701** |
| Tactile | capacitif I²C, **CST820** (avec interruption) |
| Autres | IMU QMI8658, RTC PCF85063, slot TF, USB-C (CH343P), WiFi + BLE5 |
| Variantes | `-2.1` (dalle plate, SKU 28169) / `-2.1B` (2.5D incurvée, 30697) |

> Le bring-up matériel S3 est fait dans `main/infrastructure/board_display.c` (pas de BSP
> tout-en-un pour cette carte) à partir des composants esp_lcd standard et du
> **brochage officiel Waveshare** (repris en tête du fichier). `idf_component.yml`,
> `sdkconfig.defaults`, `main/CMakeLists.txt` et `app_main.c` ciblent le S3.
>
> ⚠️ **Non compilé/validé sur matériel** (ESP-IDF absent de l'environnement de
> génération). Points à vérifier sur la carte réelle :
> - **Séquence d'init ST7701** : `init_cmds = NULL` utilise la séquence par
>   défaut du composant ; si l'écran reste noir/brouillé, coller la séquence
>   spécifique du panneau Waveshare (`st7701_lcd_init_cmd_t[]`).
> - **Timings RGB** (porches/pulses, pclk 16 MHz) : à ajuster si l'image roule.
> - **CST820** piloté par `esp_lcd_touch_cst816s` (compatible) — vérifier
>   l'orientation (swap/mirror) et l'adresse I²C.
> - **API I²C legacy** (`driver/i2c.h`) : si l'IDF impose le nouveau pilote
>   `i2c_master`, adapter `i2c_bus_init()`.

## Build & rendu

**Simulateur hôte (recommandé pour itérer)** — compile la même UI que la cible
contre LVGL 9.x sur PC, rend hors-écran et exporte les golden PNG :

```bash
./build.sh          # compil UI + régénère test/golden/{rpm_screen,rpm_amber,rpm_cream}.png
./build.sh clean    # repart de zéro (efface le cache)
```

LVGL est cloné (v9.2.2) et compilé **une seule fois** en cache
(`sim/build/lvgl_obj/`) ; les rebuilds ne recompilent que l'UI (~3 s).
`sim/build_golden.sh` fait la même chose sans cache (recompile tout LVGL).

L'ambre est rendu en **480×480 avec masque circulaire** (voir `write_png` dans
`sim/main_sim.c`) pour coller au panneau rond réel ; dual/cream restent en
1024×600 (legacy P4, non masqués).

**Cible ESP-IDF** (non buildable dans cet environnement, ESP-IDF absent) :

```bash
idf.py set-target esp32s3
idf.py build
```

## Organisation

```
main/
  app_main.c        # composition matériel/police/UI + démarrage du contrôleur
  gauge_theme.h     # palette + géométrie (thème sombre Flutter, legacy)
  ecu_data.h        # struct ecu_data_t (instantané ECU)
  domain/ecu_source.h # abstraction de lecture par copie
  fake_ecu.[ch]     # source de données simulée + adaptateur ECU
  app/dashboard_controller.[ch] # orchestration source -> écran
  ui_theme.[ch]     # palette ambre partagée
  ui/widgets/       # widgets de valeurs/indicateurs ambre
  dual_arc_dial.*   # widget jauge double arc (dessin custom) — style de base
  rpm_screen.*      # écran RPM de base
  style_amber.*     # ★ écran ambre (cible) — cadran rond vectoriel
  style_cream.*     # variante crème (legacy)
  dash_icons.*      # icônes vectorielles (eau, batterie, huile, moteur, OBD)
  gauge_icons.*     # anciennes icônes (utilisées par rpm_screen/cream)
  ui_fonts.*        # polices tiny_ttf (Michroma) rendues à la volée
  fonts/Michroma-Regular.ttf
sim/                # harnais de rendu hôte + lv_conf.h
test/golden/        # captures de référence (PNG)
specs/              # image de réf (amber/cream) + fiche matériel
```

## Écran ambre — système de conception (`main/ui/screens/style_amber.c`)

- **Repère de référence 320 px** : toute la géométrie est en coordonnées 320,
  mises à l'échelle `k = min(w,h)/320` et centrées (`xform_of`, `PX`/`PY`).
  Fonctionne à n'importe quelle taille de canvas (480 réel, autres en sim).
- **Compte-tours** : 26 barres radiales (secteurs annulaires `lv_draw_arc`,
  bords droits), demi-cercle **exact 180°** (extrémités à plat), **espacement
  angulaire constant** `SEG_GAP_DEG`. Barres allumées ∝ régime
  (`RPM_MIN`..`RPM_MAX`). Pas d'aiguille (les barres suffisent).
- **Palette** : fond `#1B1712`, ambre `#FFB51B`, graduations éteintes `#756345`,
  séparateurs `#B47A12`. Aucun blanc/bleu/rouge/vert.
- **Séparateurs fins** avec marge à chaque intersection (grille jamais fermée) ;
  cellules d'extrémité (eau/OBD) relevées pour suivre la courbe du cadran.
- **Texte** : police **Michroma** (OFL, esprit Microgramma — substitut libre à la
  police propriétaire). Une seule graisse -> **faux-gras** par calque dupliqué
  décalé (`amber_value_widget`, écart `BOLD_XL`/`BOLD_SM` selon la taille).
- **API** : `amber_screen_create(parent)` / `amber_screen_update(scr, ecu_data_t*)` /
  `amber_screen_destroy(scr)`.

### Gotchas

- **Polices en px fixes** (`ui_fonts.c`) : tiny_ttf crée les fontes à taille fixe,
  elles doivent correspondre à la **résolution de rendu** (480). Si tu changes la
  résolution du sim, re-cale `UI_FONT_*_PX` en proportion.
- **Symbole `°`** : Michroma le rend comme un petit anneau (« 89oC ») — c'est le
  vrai glyphe U+00B0 de la police. Le dessiner en vectoriel si un vrai exposant
  est voulu.
- **Changer de police** : déposer un `.ttf` dans `main/fonts/` et mettre à jour
  `TTF_PATH` (build.sh, build_golden.sh, fallback `main_sim.c`), `EMBED_FILES`
  (main/CMakeLists.txt) et les symboles `_binary_*` dans `app_main.c`.
- **Toujours régénérer les golden** après une modif d'UI (`./build.sh`) et
  vérifier visuellement `test/golden/rpm_amber.png`.

## Correspondance Flutter → LVGL

Voir `README.md` (table `DualArcDial`→`dual_arc_dial.c`, etc.). Le style ambre est
une refonte spécifique au cadran rond, pas une transposition 1:1 du Flutter.
