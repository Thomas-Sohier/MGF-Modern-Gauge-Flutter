# Goldens — dashboard MGF

Rendus de référence produits par le simulateur hôte `../../sim/` à partir de
la même UI LVGL que la cible ESP32-S3.

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_screen.png` | 1024×600 | de base (sombre, double arc segmenté) | `main/ui/widgets/dual_arc_dial.c`, `main/ui/screens/rpm_screen.c` |
| `rpm_amber.png` | **480×480 (rond)** | ambre, 26 barres radiales, séparateurs fins et rangée de 4 indicateurs (cf. `specs/image` photo 1) | `main/ui/screens/style_amber.c` + `main/ui/` |
| `rpm_cream.png` | 1024×600 | crème analogique, graduations + zone rouge (cf. `specs/image` photo 2) | `main/ui/screens/style_cream.c` |
| `boot_amber.png` | **480×480 (rond)** | écran de démarrage ambre | `main/ui/screens/boot_screen.c` |
| `{clock,music,navigation,faults}_amber.png` | **480×480 (rond)** | services et diagnostic ambre | `main/ui/screens/*_screen.c` |
| `{temps,injection,lambda,ignition,idle,admission}_amber.png` | **480×480 (rond)** | télémétrie moteur ambre | `main/ui/screens/*_screen.c` |

> Cible réelle : écran **rond ~52 mm**. Tous les écrans `*_amber` sont pensés pour ce
> format (canvas carré rempli au maximum). `rpm_screen`/`rpm_cream` sont encore
> en paysage 1024×600 et restent à porter en rond.

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/ui/icons/dash_icons.c` pour l'ambre, `main/ui/icons/gauge_icons.c` pour les variantes legacy.

## Données mock

Le simulateur injecte un instantané moteur complet et déterministe dans tous les
écrans. Musique et navigation utilisent encore un scénario de démonstration,
leurs modèles n'étant pas fournis par la source MEMS.

> La police Michroma embarquée fournit le glyphe `°`, rendu comme un petit
> anneau. Le fallback Montserrat peut afficher un rendu différent.

## Régénérer

```bash
./build.sh                   # compile l'UI et régénère tous les PNG
# ou, avec des sources LVGL existantes :
LVGL_DIR=/chemin/vers/lvgl sim/build_golden.sh
```
