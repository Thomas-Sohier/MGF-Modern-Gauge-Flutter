# Goldens — dashboard MGF

Rendus de référence produits par le simulateur hôte `../../sim/` à partir de
la même UI LVGL que la cible ESP32-S3.

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_amber.png` | **480×480 (rond)** | ambre, 26 barres radiales, séparateurs fins et rangée de 4 indicateurs (cf. `specs/image` photo 1) | `main/ui/screens/style_amber.c` + `main/ui/` |
| `boot_amber.png` | **480×480 (rond)** | écran de démarrage ambre | `main/ui/screens/boot_screen.c` |
| `clock_amber.png` | **480×480 (rond)** | horloge ambre (style propre, hors kit) | `main/ui/screens/clock_screen.c` |
| `{music,navigation,faults}_amber.png` | **480×480 (rond)** | services et diagnostic ambre, kit commun ; musique et navigation avec des données de l'application compagnon (`offline` = téléphone déconnecté) | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `{injection,ignition}_amber.png` | **480×480 (rond)** | télémétrie : couronne segmentée, valeur héros, grille 2×2 | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `{temps,lambda,idle,admission}_amber.png` | **480×480 (rond)** | télémétrie sans jauge : filet fin, valeur héros, grille 2×2 | `main/ui/screens/*_screen.c` + `main/ui/widgets/amber_kit.c` |
| `settings_amber.png` | **480×480 (rond)** | réglages (luminosité, page de démarrage, BLE ouvert) ; `offline` = image sans BLE | `main/ui/screens/settings_screen.c` |

> Cible réelle : écran **rond ~52 mm**. Tous les écrans `*_amber` sont pensés pour ce
> format (canvas carré rempli au maximum).

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/ui/icons/dash_icons.c`.

## Données mock

Le simulateur injecte un instantané moteur complet et déterministe dans tous les
écrans. Musique et navigation utilisent encore un scénario de démonstration,
leurs modèles n'étant pas fournis par la source MEMS.

> Le `°` des unités est dessiné en vectoriel par `amber_readout` (le glyphe
> Michroma ressemble à un « o » bas).
>
> État ECU déconnectée (hors goldens) : `sim/build/gen_golden <écran> out.png offline`.

## Régénérer

```bash
./build.sh                   # compile l'UI et régénère tous les PNG
# ou, sans cache (LVGL recompilé) :
sim/build_golden.sh
```
