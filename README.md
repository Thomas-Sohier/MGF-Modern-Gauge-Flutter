# MGF Gauge LVGL — ESP32-P4

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

**Waveshare ESP32-S3-Touch-LCD-2.1** (fiche dans `specs/`) : ESP32-S3 (LX7 bicœur
240 MHz, 16 Mo Flash, 8 Mo PSRAM), écran IPS **rond 480×480** (driver **ST7701**,
interface RGB), tactile capacitif **CST820** (I²C), expander **TCA9554**.

Pas de BSP tout-en-un pour cette carte : le bring-up (ST7701 RGB + CST820 +
TCA9554 + port LVGL) est fait dans `main/board_display.c` à partir des composants
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
  app_main.c        # init écran/LVGL + timer de rafraîchissement (écran ambre)
  board_display.c   # bring-up ST7701 RGB + CST820 + TCA9554 + port LVGL (S3)
  ecu_data.h        # struct ecu_data_t
  fake_ecu.c        # source de données simulée
  style_amber.*     # écran ambre (cible) — cadran rond vectoriel
  dash_icons.*      # icônes vectorielles (eau, batterie, huile, OBD)
  ui_fonts.*        # police Michroma via tiny_ttf
  # dual_arc_dial.* / rpm_screen.* / style_cream.* / gauge_icons.* : sim-only
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
