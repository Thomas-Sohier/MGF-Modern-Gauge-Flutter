# Golden — écran RPM

Rendus de référence de l'écran RPM, produits par le simulateur hôte `../../sim/`
à partir de la même UI LVGL que la cible ESP32-S3. Quatre variantes de style :

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_screen.png` | 1024×600 | de base (sombre, double arc segmenté) | `main/ui/widgets/dual_arc_dial.c`, `main/ui/screens/rpm_screen.c` |
| `rpm_amber.png` | **480×480 (rond)** | ambre, 26 barres radiales, séparateurs fins et rangée de 4 indicateurs (cf. `specs/image` photo 1) | `main/ui/screens/style_amber.c` + `main/ui/` |
| `rpm_cream.png` | 1024×600 | crème analogique, graduations + zone rouge (cf. `specs/image` photo 2) | `main/ui/screens/style_cream.c` |
| `boot_amber.png` | **480×480 (rond)** | écran de démarrage ambre | `main/ui/screens/boot_screen.c` |

> Cible réelle : écran **rond ~52 mm**. `rpm_amber` est déjà pensé pour ce
> format (canvas carré rempli au maximum). `rpm_screen`/`rpm_cream` sont encore
> en paysage 1024×600 et restent à porter en rond.

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/ui/icons/dash_icons.c` pour l'ambre, `main/ui/icons/gauge_icons.c` pour les variantes legacy.

## Données mock

| Canal | `rpm_screen` | `rpm_amber` | `rpm_cream` | `boot_amber` |
|---|---|---|---|---|
| RPM | 7200 (zone rouge) | 800 (ralenti) | 800 (ralenti) | — |
| Papillon | 85 % | 6 % | — | — |
| Liquide (LDR) | 92 | 89 | 87 | — |
| Batterie | 14.0 V | 14.2 V | 14.0 V | — |
| Huile | 98 | 91 | — | — |
| OBD | connecté | connecté | — | — |

Sur `rpm_screen`, le régime est volontairement en zone rouge pour valider la
coloration de danger ; sur `rpm_amber`/`rpm_cream`, au ralenti pour coller aux
photos de référence.

> La police Michroma embarquée fournit le glyphe `°`, rendu comme un petit
> anneau. Le fallback Montserrat peut afficher un rendu différent.

## Régénérer

```bash
sim/build_golden.sh          # clone LVGL v9.2.2 si besoin, compile, réécrit les quatre PNG
# ou, avec des sources LVGL existantes :
LVGL_DIR=/chemin/vers/lvgl sim/build_golden.sh
```
