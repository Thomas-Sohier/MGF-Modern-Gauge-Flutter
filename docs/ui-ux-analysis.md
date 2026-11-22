# Analyse & Plan de Rework UI/UX — MGF Gauge LVGL (Ambre Rétro-Futuriste)

> **Cible matérielle** : LILYGO T-RGB 2.1 Full Circle H597 — ESP32-S3R8 (Xtensa LX7 dual-core @ 240 MHz, 8 Mo PSRAM octale, écran IPS rond 480×480 RGB565 ST7701S, tactile CST820, rétroéclairage AW9364 16 niveaux).
> **Véhicule cible** : MG F (1995-1999, ECU Rover MEMS 1.9 K-line, cabriolet, console centrale à bout de bras).
> **Direction artistique** : Instrumentation automobile ambre monochrome rétro-futuriste (esprit VFD / plasma / électroluminescent youngtimer des années 80-90), 100 % vectorielle, lisibilité maximale en < 0,5 s.

---

## 1. Synthèse de l'existant (Points forts à préserver absolument)

1. **Architecture et performances** :
   - Séparation stricte `domain/` (pur C testé sur hôte) / `app/` / `ui/` / `infrastructure/`.
   - Géométrie logique dans un repère `320×320` mise à l'échelle vers `480×480` (`ui_layout_t`, `scale = 1.5`).
   - Lissage exponentiel du régime moteur (`value_smoothing_step`, $\tau = 100\text{ ms}$) et invalidation partielle par secteur angulaire (`amber_invalidate_sector`).
   - Bascule de palette Normale / Inversée à chaud en $O(N)$ (`ui_theme_apply_inverted`) sans recréer les écrans et strictement réversible au pixel près (`check_toggle` dans `build.sh`).
2. **Grammaire visuelle (`amber_kit`)** :
   - Palette strictement limitée à 4 rôles (`AMBER_PALETTE_BG` `#170F08`, `AMBER_PALETTE_DIM` `#6B451B`, `AMBER_PALETTE_SEPARATOR` `#A85C00`, `AMBER_PALETTE_BRIGHT` `#FF8C00`).
   - Alignement optique des textes sur la hauteur de capitale (`font_cap('H')`), anneau vectoriel pour le symbole `°`, grille 2×2 ouverte aux intersections.

---

## 2. Diagnostic UI/UX & Axes d'Amélioration

### Axe A — Écran Principal Compte-Tours (`main/ui/screens/style_amber.c`)
1. **Absence d'échelle interne et de Zone Rouge (Redline Rover K-Series ~6800 tr/min)** :
   - Les 26 segments radiaux (0..8000 tr/min) sont visuellement identiques. En vision périphérique, on ne distingue pas les paliers (2000, 4000, 6000) ni l'entrée en zone rouge (> 6500 tr/min, segments 21..25).
   - **Solution** :
     - Ajouter un **arc d'échelle fin interne** (`DIAL_R_IN - 4.0f`) avec des graduations radiales (`amber_draw_tick`) aux quarts/huitièmes d'échelle (comme sur l'image de référence `specs/image/amber.png`), en couleur `SEPARATOR`.
     - **Zone rouge monochrome rétro-futuriste** sur les 5 derniers segments (`i >= 21`, soit $\ge 6460\text{ tr/min}$) : ajouter un **filet de garde extérieur/intérieur** ou un **crénelage double** sur ces 5 segments pour signer visuellement la zone critique sans ajouter de couleur hors palette.
2. **Tremblement horizontal des chiffres Michroma** :
   - Michroma n'a pas de chiffres à chasse fixe (`1` est très étroit, `8` est large). Le recentrage dynamique du couple `valeur + unité` fait vibrer horizontalement l'unité (`°C`, `V`, `ms`, `%`) à chaque rafraîchissement.
3. **Encadrement d'alerte sur les cellules Eau / Batterie** :
   - Quand la température d'eau est en alerte ($\ge 105\text{ °C}$) ou la tension batterie hors plage ($< 11.8\text{ V}$ ou $> 15.0\text{ V}$), la cellule correspondante du compte-tours doit attirer l'œil immédiatement par un **cadre chanfreiné ambre vif** (dans l'esprit des cellules en défaut de `faults_screen.c`).

### Axe B — Dynamisme des Écrans du Kit (`lambda`, `admission`, `temps`, `dashboard_navigator`)
1. **Sous-utilisation de la couronne segmentée (`AMBER_RIM_SEGMENTS`)** :
   - Actuellement, 5 écrans de télémétrie sur 7 utilisent un simple filet statique (`AMBER_RIM_HAIRLINE`), ce qui les rend difficiles à différencier d'un coup d'œil en roulant.
   - **Solutions** :
     - **RICHESSE (`lambda_screen.c`)** : passer en `AMBER_RIM_SEGMENTS` (36 segments) en mode **centre-zéro sur la stœchiométrie (`14.7 AFR`)** sur la plage `11.7 .. 17.7 AFR` (`marker = 0.5f`). L'oscillation de la sonde lambda devient immédiatement lisible comme sur un analyseur AFR de tableau de bord.
     - **ADMISSION (`admission_screen.c`)** : passer en `AMBER_RIM_SEGMENTS` (36 segments) comme **vacuomètre / jauge de charge collecteur (MAP)** de `0` à `105 kPa`, avec repère (`marker`) à la pression atmosphérique (`100 kPa`).
2. **Bandeau `ALERTE THERMIQUE` (`dashboard_navigator.c`) trop étroit** :
   - Largeur actuelle `192.0f` (en repère 320) : les lettres `A` et `E` de `ALERTE THERMIQUE` touchent les bords arrondis de la pilule (`dashboard_hot_amber.png`).
   - **Solution** : Élargir `THERMAL_ALERT_W` à `210.0f` et `THERMAL_ALERT_H` à `24.0f` pour offrir une marge interne nette et lisible.

### Axe C — Musique, Navigation, Horloge & Optimisations ESP32-S3 (`music`, `navigation`, `clock`)
1. **Fallback Musique éblouissant de nuit & Coût CPU (`music_screen.c`)** :
   - `draw_cover_fallback()` dessine un dôme plein `#FF8C00` via ~275 appels à `amber_draw_line()` par frame. De nuit, cet aplat massif éblouit l'habitacle ; en mode normal, il tranche brutalement avec l'esthétique vectorielle sombre.
   - **Solution** : Remplacer ce dôme plein par un **visuel vectoriel rétro-futuriste sur fond sombre** : sillons concentriques d'un disque / optique laser (arcs `DIM` et `SEPARATOR`) entourant un médaillon central ambre vif avec la double-croche, en moins de 15 primitives vectorielles.
   - **Information temporelle** : Afficher `position / durée` (ex. `02:22 / 07:46`) quand la position est connue, au lieu de la seule durée totale figée `07:46`.
   - **Optimisation d'invalidation ESP32-S3** : Dans `refresh_progress()`, ne plus appeler `amber_page_invalidate(&scr->page)` chaque seconde lorsque seule la progression de l'anneau (`scr->ring`) évolue (évite de recomposer l'image RGB565 à travers les conteneurs clippés chaque seconde).
2. **Collision du pictogramme Rond-Point avec le titre (`navigation_screen.c`)** :
   - Dans `navigation_long_amber.png`, la flèche de sortie du rond-point monte jusqu'à `y = 58` (avec épaisseur de chaussée `17 px`, soit bord haut à `y = 49.5`), entrant en collision avec le titre `NAVIGATION` (`y = 47`).
   - **Solution** : Abaisser légèrement le centre du rond-point (`y = 92` -> `y = 96`), réduire le rayon (`20` -> `18`) et raccourcir l'extrémité de sortie (`exit_b = {190.0f, 66.0f}`) pour dégager un espace franc sous le titre.
3. **Hiérarchie des aiguilles de l'Horloge (`clock_screen.c`)** :
   - L'aiguille des heures (`0.11f * radius`) et celle des minutes (`0.105f * radius`) ont quasiment la même épaisseur, ce qui nuit à la lecture instantanée, et n'ont aucun détail rétro-futuriste.
   - **Solution** : Affiner l'aiguille des minutes (`0.078f * radius`) et ajouter une **fente intérieure sombre (aiguilles squelettes / ajourées)** sur la base des aiguilles pour un rendu horloger automobile plus fin et lisible.

---

## 3. Découpage en Sous-Agents (Exécution Parallèle Encadrée)

Pour garantir **zéro conflit de fichiers** et respecter toutes les règles du projet (`ui_theme_amber_*` uniquement pour la réversibilité de la palette inversée, `clang-format`, tests unitaires et génération des goldens), le travail d'exploration/implémentation est découpé en **3 sous-agents indépendants** utilisant `opencode-go/deepseek-v4.1-flash` :

| Sous-agent | Périmètre strict de fichiers | Objectif |
|---|---|---|
| **Agent 1 — Compte-Tours Rétro-Futuriste** | `main/ui/screens/style_amber.c` | Arc d'échelle interne gradué, signature visuelle de Zone Rouge (segments 21..25), encadrement chanfreiné sur cellule Eau/Batterie en alerte. |
| **Agent 2 — Couronnes Kit & Bandeau Alerte** | `main/ui/screens/lambda_screen.c`, `main/ui/screens/admission_screen.c`, `main/ui/navigation/dashboard_navigator.c` | Couronne 36 segments centre-zéro (14.7 AFR) sur Richesse, couronne charge MAP (0..105 kPa) sur Admission, élargissement et aération du bandeau `ALERTE THERMIQUE`. |
| **Agent 3 — Musique, Navigation & Horloge** | `main/ui/screens/music_screen.c`, `main/ui/screens/navigation_screen.c`, `main/ui/screens/clock_screen.c` | Fallback vinyle/optique vectoriel sombre + `pos / durée` + suppression de l'invalidation superflue du canvas pochette sur Musique ; correction de la collision du rond-point sur Navigation ; aiguilles différenciées et ajourées sur Horloge. |

Une fois que les 3 sous-agents auront terminé leurs modifications sur leurs fichiers respectifs, nous lancerons `./build.sh`, `bash test/run_tests.sh`, `tools/format.sh` et nous inspecterons visuellement tous les PNG générés dans `test/golden/`.
