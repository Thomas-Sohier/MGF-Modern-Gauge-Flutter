# CLAUDE.md — MGF Gauge LVGL

Portage LVGL de l'écran RPM principal du projet Flutter `MGF-Modern-Gauge-Flutter`
vers un microcontrôleur. Rendu **entièrement vectoriel** (aucune image de fond),
piloté par l'ECU MEMS de la MGF (K-line) ou une ECU simulée. Le style de référence est **« amber »**
(cadran automobile rond, monochrome ambre) — c'est celui qui cible le matériel.

## Matériel cible

**LILYGO T-RGB 2.1 Full Circle H597** (références : `specs/lilygo-t-rgb-2.1/`).

| | |
|---|---|
| MCU | ESP32-S3R8, Xtensa LX7 double cœur @ 240 MHz |
| Mémoire | 512 KB SRAM + 384 KB ROM, **16 MB Flash**, **8 MB PSRAM** |
| Écran | IPS **rond 480×480**, 2.1", **RGB** 565, driver **ST7701S** (init 9 bits via XL9535) |
| Tactile | capacitif I²C **CST820** (`0x15`) |
| Rétroéclairage | AW9364, 16 niveaux par impulsions sur GPIO46 |
| I²C partagé | GPIO8/GPIO48 : CST820, XL9535 (`0x20`), connecteur externe |
| Autres | microSD, USB-C, WiFi + BLE5. **Pas d'IMU ni de RTC intégrée** |

> Le bring-up est fait dans `main/infrastructure/board_display.c` (pas de BSP)
> à partir des composants esp_lcd et du brochage officiel LILYGO. L'XL9535 est
> porté par le composant TCA95xx 16-bit ; les tables ST7701S upstream sont dans
> `main/infrastructure/lilygo_st7701_init.h` (profil original, V2 via
> `-DMGF_LILYGO_T_RGB_V2=1`).

### Périphériques ajoutés

- **Téléphone Android** via BLE (NimBLE, toujours actif) : application
  compagnon `../rover-mems-ecu-companion` (musique, navigation Google Maps,
  télécommande, heure + fuseau). Reconnexion automatique au démarrage de la
  voiture, appairage limité à une fenêtre ouverte depuis les Réglages. Cf.
  `docs/ble-config.md`.
- **RTC DS3231 externe** (`0x68`, bus I²C partagé) : la carte n'a pas de RTC.
  Optionnelle, stocke de l'**UTC** (cf. `docs/rtc-ds3231.md`) ; l'horloge
  affiche l'heure légale avec le décalage reçu du téléphone.
- **K-line MEMS** : GPIO40 = TX, GPIO38 = RX, UART1 (reprend microSD CMD/DAT0 ;
  **SDMMC n'est ni initialisé ni possédé**). Garde-fous dans
  `main/infrastructure/kline_board_config.h`. Désactivée par défaut
  (`MGF_USE_MEMS_KLINE=0` → ECU simulée). Variante par défaut **MEMS 1.9**
  (MGF 1995-1999, réveil 5 bauds) ; MEMS 3 (MGF 2000+/TF) non supporté.
  Transceiver automobile externe et protections obligatoires : jamais de 12 V
  sur un GPIO.

> ⚠️ Build ESP-IDF 5.4.2 validé par l'utilisateur, **pas encore testé sur la
> carte** (cf. `docs/lilygo-roadmap.md`). ESP-IDF 5.4.2 est installé hors
> du PATH : `. ~/.cache/mgf-gauge-esp-idf/esp-idf-v5.4.2/export.sh` avant
> `idf.py` / `tools/check_iram.sh`.

## Build & tests

**Simulateur hôte (recommandé pour itérer)** — compile la même UI que la cible
contre LVGL 9.x sur PC, rend hors-écran et exporte les golden PNG :

```bash
./build.sh          # compil UI + régénère test/golden/*_amber.png
./build.sh clean    # repart de zéro (efface le cache)
bash test/run_tests.sh  # tests unitaires hôte (domaine, MEMS, K-line, NVS, RTC…)
```

LVGL est cloné (v9.2.2) et compilé **une seule fois** en cache
(`sim/build/lvgl_obj/`) ; les rebuilds ne recompilent que l'UI (~3 s).
`sim/build_golden.sh` = `./build.sh clean`. Tous les écrans sont rendus en
**480×480 avec masque circulaire** (`write_png` dans `sim/main_sim.c`).
La CI (`.github/workflows/ci.yml`) lance `test/run_tests.sh`, puis `./build.sh`
et refuse tout écart de golden (`git diff -- test/golden`).

**Cible ESP-IDF** :

```bash
idf.py set-target esp32s3 && idf.py build          # firmware (BLE NimBLE inclus)
tools/check_iram.sh                                # marge DIRAM (la région IRAM 16 KiB est toujours ~pleine, c'est normal)
```

Options CMake : `MGF_USE_MEMS_KLINE`, `MGF_MEMS_VARIANT`, `MGF_KLINE_*`,
`MGF_ENABLE_BLE_CONFIG`, `MGF_RUNTIME_DIAGNOSTICS`, `MGF_LILYGO_T_RGB_V2`.

## Organisation

```
main/
  app_main.c          # composition matériel/police/UI ; redémarre si le boot échoue
  domain/             # pur C, testé sur hôte : ecu_data, MEMS (protocole,
                      # lecteurs 1.6/1.9, session), réglages, protocole BLE, RTC
  app/                # dashboard_controller, settings_coordinator/runtime
  infrastructure/     # ESP-IDF : board_display, K-line UART, mems_ecu, fake_ecu,
                      # NVS, DS3231, BLE NimBLE, diagnostics
  ui/navigation/      # dashboard_navigator (11 pages, swipe cyclique, tap
                      # tiers gauche/droit, maintien 1 s -> réglages)
  ui/screens/         # style_amber (★ RPM), boot, clock, music, navigation,
                      # faults, temps, injection, lambda, ignition, idle,
                      # admission, settings (surimpression)
  ui/widgets/ icons/ themes/ fonts/   # briques ambre partagées, Michroma tiny_ttf
sim/                  # harnais de rendu hôte + lv_conf.h
test/                 # tests hôte (run_tests.sh, mocks ESP-IDF) + golden/
tools/check_iram.sh   # contrôle de marge IRAM après build IDF
specs/                # image de réf + pack matériel LILYGO
docs/                 # K-line MEMS, NVS, BLE, RTC, roadmap LILYGO
```

## Données ECU

- `ecu_data_t` est copié en bloc source → contrôleur → pages.
- **Mesure indisponible = `NAN`** (ECU déconnectée, champ absent de MEMS comme
  `oil_temp`) ; les écrans affichent `--`, jamais un faux 0. Utiliser
  `ecu_data_unavailable()` pour un instantané vide.
- Défauts : `fault_flags` (`ECU_FAULT_*`) valable seulement si `faults_available`.
- La session MEMS tolère `MEMS_SESSION_MAX_POLL_FAILURES` trames perdues
  consécutives avant de se déclarer déconnectée.

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
- **Lissage** : le RPM affiché suit la valeur ECU (~5 Hz) via
  `value_smoothing_step` (τ 100 ms) pour éviter un bargraph en escalier.
- **API** : `amber_screen_create(parent)` / `amber_screen_update(scr, ecu_data_t*)` /
  `amber_screen_set_units(scr, units)` / `amber_screen_destroy(scr)`.

### Autres écrans — kit commun (`main/ui/widgets/amber_kit.c`)

Tous les écrans hors RPM partagent une même grammaire, fournie par le kit :
- **Couronne segmentée 220°** ouverte en bas (36 segments, même écart
  angulaire que le RPM) **uniquement sur injection et allumage** ; remplissage
  depuis un **repère** (correction 100 %, PMH). Les autres écrans
  (températures, richesse, ralenti, admission, diagnostic) n'ont pas de
  jauge, seulement le **filet fin** gradué ; la navigation n'a aucun cadre.
  Exceptions hors kit :
  l'**horloge** garde son style propre, la **musique** un anneau continu de
  progression (pas de segments).
- **Titre** en capitales espacées encadré de filets pointés, **valeur héros**
  (56 px) + **unité réduite** posée sur la ligne de base, **légende**.
- **Grille 2×2** ouverte (séparateurs non jointifs, esprit RPM), valeurs
  30 px + unités, **ligne d'état** dans l'ouverture basse.
- Couleurs : valeur = ambre vif, légendes/unités/filets = séparateur
  (lisible), éteint/indisponible = ambre sombre.
- `amber_page_*` construit l'écran type ; `amber_readout_*` = valeur + unité,
  avec un **`°` vectoriel** (unité commençant par « ° ») ; `amber_kit_place`
  centre un texte sur la **hauteur de capitale** (alignement optique).
- `sim/build/gen_golden <écran> out.png offline` rend l'état ECU déconnectée.

### Réglages (`main/ui/screens/settings_screen.c`)

- Surimpression opaque ouverte par un **maintien ~1 s** (doigt immobile) sur
  n'importe quelle page ; fermée par **OK**. Luminosité (16 crans AW9364,
  jamais 0), **page de démarrage** (fixe ou « dernière vue », défaut) et
  fenêtre d'**appairage** Bluetooth de 5 min (`domain/ble_window`, refermée
  dès qu'un téléphone est lié ; « INDISPONIBLE » si le BLE est absent).
- L'écran ne fait que remonter des actions : `app_main.c` les applique via
  `settings_runtime_*` et resynchronise l'affichage dans le timer réglages.
- `startup_page` et `utc_offset_minutes` (schéma NVS **v2**, migration depuis
  v1) ne sont **pas** dans le protocole BLE de réglages v1 : les valeurs
  locales sont conservées lors d'une écriture BLE (`app_settings_merge_ble_v1`).

### Application compagnon (`infrastructure/companion_gatt.c`)

- Service GATT `7f3a0001-…` repris de l'ancien boîtier Go ; JSON décodé dans
  la tâche NimBLE par `domain/companion_protocol.c` (C pur, testé), déposé
  dans une boîte aux lettres, appliqué par `companion_timer_tick` (LVGL).
- Musique/Navigation ne dépendent plus de l'ECU : `music_screen_set_media`,
  `navigation_screen_set_route`, `*_set_link`. Flèche de manœuvre déduite du
  texte ; pochette, icône PNG et alertes ignorées. Lien à sens unique.
- `sim/build/gen_golden music|navigation out.png offline` rend l'état
  « téléphone déconnecté ».

### Gotchas

- **Objets décoratifs non cliquables** : `lv_obj_create` rend un objet
  cliquable, il capte alors les appuis destinés au navigateur (tap latéral,
  maintien). Retirer `LV_OBJ_FLAG_CLICKABLE` des conteneurs/canvas (fait dans
  `amber_ui_root_create`/`amber_ui_canvas_create`) ; seuls les vrais contrôles
  (boutons des réglages) restent cliquables.
- **Polices en px fixes** (`ui_fonts.c`) : tiny_ttf crée les fontes à taille fixe,
  elles doivent correspondre à la **résolution de rendu** (480). Si tu changes la
  résolution du sim, re-cale `UI_FONT_*_PX` en proportion.
- **Symbole `°`** : Michroma le rend comme un petit « o » bas (« 89oC ») — c'est
  le vrai glyphe U+00B0 de la police. Passer par `amber_readout` (unité « °C »)
  qui dessine un anneau en exposant.
- **Changer de police** : déposer un `.ttf` dans `main/fonts/` et mettre à jour
  `TTF_PATH` (build.sh, fallback `main_sim.c`), `EMBED_FILES`
  (main/CMakeLists.txt) et les symboles `_binary_*` dans `app_main.c`.
- **Allocateur LVGL** : la cible utilise `CONFIG_LV_USE_CLIB_MALLOC` (heap
  ESP-IDF, PSRAM en secours), comme le sim. Ne pas revenir au pool fixe 64 Ko.
- **Nouvelle source dans le sim** : l'ajouter à `UI_SRCS` de `build.sh` (et à
  `main/CMakeLists.txt` pour la cible).
- **Toujours régénérer les golden** après une modif d'UI (`./build.sh`) et
  vérifier visuellement les PNG concernés de `test/golden/`.

## Correspondance Flutter → LVGL

Voir `README.md`. Le style ambre est une refonte spécifique au cadran rond, pas
une transposition 1:1 du Flutter.
