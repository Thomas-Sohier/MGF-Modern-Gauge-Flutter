# MGF Gauge LVGL — ESP32-S3

Portage **de faisabilité** de l'écran RPM principal du projet Flutter
[`MGF-Modern-Gauge-Flutter`](../MGF-Modern-Gauge-Flutter) vers **LVGL 9 / ESP-IDF**,
pour tourner sur la carte **Waveshare ESP32-S3-Touch-LCD-2.1** (écran IPS **rond
480×480**, driver ST7701, tactile CST820).

Objectif : vérifier qu'un microcontrôleur (Xtensa LX7 bicœur, sans GPU) peut
rendre fidèlement et de façon fluide la jauge, sans passer par Flutter/Impeller.
L'écran cible est le style **ambre** (cadran rond monochrome) ; les données ECU
sont simulées.

> Détails matériel, système de conception et points de validation : voir
> [`CLAUDE.md`](CLAUDE.md).

## Rendu reproduit

Le style ambre cible le panneau rond :

- Demi-cercle supérieur segmenté (**26 barres**, espacées régulièrement) — arc RPM.
- Palette monochrome ambre, fond brun-noir et séparateurs fins.
- Disque de fond + valeur centrale (grand nombre + « RPM »).
- Quatre indicateurs vectoriels en bas : température LDR, batterie, huile et OBD.

Les variantes dual/cream legacy restent disponibles dans le simulateur pour
conserver les goldens historiques.

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
| WebSocket `EcuService` | `fake_ecu.c` (simulée) / `mems_ecu.c` (K-line MEMS réelle, cf. [`docs/kline-mems.md`](docs/kline-mems.md)) | source de données |

## Matériel cible

**Waveshare ESP32-S3-Touch-LCD-2.1** (fiche dans `specs/`) : ESP32-S3 (LX7 bicœur
240 MHz, 16 Mo Flash, 8 Mo PSRAM), écran IPS **rond 480×480** (driver **ST7701**,
interface RGB), tactile capacitif **CST820** (I²C), expander **TCA9554**.

Pas de BSP tout-en-un pour cette carte : le bring-up (ST7701 RGB + CST820 +
TCA9554 + port LVGL) est fait dans `main/infrastructure/board_display.c` à partir des composants
`esp_lcd` standard et du brochage officiel Waveshare.

## Build & flash

Nécessite **ESP-IDF ≥ 5.3** installé et sourcé (`. $IDF_PATH/export.sh`).

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Les dépendances (LVGL, esp_lvgl_port, esp_lcd_st7701, esp_lcd_touch_cst816s,
esp_io_expander_tca9554…) sont téléchargées au premier `build` dans
`managed_components/`.

> ⚠️ Le firmware n'a pas encore été compilé/validé sur matériel (ESP-IDF absent
> de l'environnement de portage). Points de validation listés dans `CLAUDE.md`
> (séquence d'init ST7701, timings RGB, tactile).

## Structure

```
main/
  app_main.c        # composition matériel/police/UI + démarrage du contrôleur
  board_display.c   # bring-up ST7701 RGB + CST820 + TCA9554 + port LVGL (S3)
  ui_theme.[ch]     # palette ambre partagée
  ui/widgets/       # widgets de valeurs/indicateurs ambre
  ecu_data.h        # modèle ecu_data_t uniquement
  domain/ecu_source.h # abstraction de lecture par copie
  fake_ecu.[ch]      # source de données simulée
  app/dashboard_controller.[ch] # orchestration source -> écran
  style_amber.*     # écran ambre (cible) — cadran rond vectoriel
  dash_icons.*      # icônes vectorielles (eau, batterie, huile, OBD)
  ui_fonts.*        # police Michroma via tiny_ttf
  # dual_arc_dial.* / rpm_screen.* / style_cream.* / gauge_icons.* : sim-only
```

## Simulateur hôte & golden

La même UI ambre (`style_amber.c`, ses widgets et `fake_ecu.c`) se compile sur
PC pour un rendu hors-écran. Les variantes dual/cream legacy sont générées dans
la même passe, ce qui permet d'itérer sans matériel et de produire les captures
de référence :

```bash
sim/build_golden.sh          # -> test/golden/{rpm_screen,rpm_amber,rpm_cream}.png
```

Trois variantes de style sont rendues (sombre de base, ambre, crème — d'après
`specs/image/`). Voir `sim/` (harnais + `lv_conf.h`) et `test/golden/` (images +
données mock). Les icônes ambre sont dans `main/ui/icons/dash_icons.c`; les icônes et
styles legacy sont dans `main/ui/icons/gauge_icons.c`, `main/ui/screens/style_cream.c`.

Le style ambre utilise **Michroma** rendue à la volée par tiny_ttf
(`main/ui/fonts/ui_fonts.c`, TTF dans `main/fonts/`, embarqué côté cible via
`EMBED_FILES`). Remplacer le `.ttf` suffit pour changer de fonte (ex. une fonte
plus « rétro »).

## Socle graphique 480×480

- `main/ui/ui_layout.h` : taille cible 480 px, repère logique 320 unités,
  échelle uniforme et centrage communs aux composants.
- Le cadran ambre utilise un rayon extérieur de 237 px à la résolution cible :
  marge nominale de **3 px** sur le disque, sans padding supplémentaire.
- Chaque écran ambre possède une racine carrée centrée ; cadran, icônes et
  valeurs utilisent le même repère. La racine et ses composants sont détruits
  ensemble via `amber_screen_destroy()`.
- La géométrie s'adapte à la taille disponible **à la création**. Recréer
  l'écran après un redimensionnement ; les polices restent calibrées pour
  480 px (pas de redimensionnement typographique automatique).
- `./test/run_tests.sh` vérifie aussi échelle, marge et centrage, sans LVGL.

## Statut

Code **non compilé dans l'environnement de génération** (ESP-IDF absent).
Écrit pour LVGL 9.x / ESP-IDF 5.3+. À valider par un premier `idf.py build`
sur un poste équipé du toolchain.
