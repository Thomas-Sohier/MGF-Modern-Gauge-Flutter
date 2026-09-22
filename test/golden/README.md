# Goldens — dashboard MGF

Rendus de référence produits par le simulateur hôte `../../sim/` à partir de
la même UI LVGL que la cible ESP32-S3.

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_amber.png` | **480×480 (rond)** | ambre, 26 barres radiales, séparateurs fins et rangée de 4 indicateurs (cf. `specs/image` photo 1) | `main/ui/screens/style_amber.c` + `main/ui/` |
| `boot_amber.png` | **480×480 (rond)** | écran de démarrage ambre | `main/ui/screens/boot_screen.c` |
| `{clock,music,navigation,faults}_amber.png` | **480×480 (rond)** | services et diagnostic ambre | `main/ui/screens/*_screen.c` |
| `{temps,injection,lambda,ignition,idle,admission}_amber.png` | **480×480 (rond)** | télémétrie moteur ambre | `main/ui/screens/*_screen.c` |

> Cible réelle : écran **rond ~52 mm**. Tous les écrans `*_amber` sont pensés pour ce
> format (canvas carré rempli au maximum).

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/ui/icons/dash_icons.c`.

## Données mock

Le simulateur injecte un instantané moteur complet et déterministe dans tous les
écrans. Musique et navigation utilisent encore un scénario de démonstration,
leurs modèles n'étant pas fournis par la source MEMS.

> La police Michroma embarquée fournit le glyphe `°`, rendu comme un petit
> anneau. Le fallback Montserrat peut afficher un rendu différent.

## Régénérer

```bash
./build.sh                   # compile l'UI et régénère tous les PNG
# ou, sans cache (LVGL recompilé) :
sim/build_golden.sh
```
