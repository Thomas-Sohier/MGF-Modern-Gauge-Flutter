# Golden — écran RPM

Rendus de référence de l'écran RPM, produits par le simulateur hôte `../../sim/`
à partir de la **même** UI que la cible ESP32-P4. Trois variantes de style :

| Fichier | Format | Style | Source UI |
|---|---|---|---|
| `rpm_screen.png` | 1024×600 | de base (sombre, double arc segmenté) | `main/dual_arc_dial.c`, `main/rpm_screen.c` |
| `rpm_amber.png` | **720×720 (rond)** | ambre, arc segmenté épais + aiguille, rangée de 4 indicateurs (centre plus bas, bordures haut + séparateurs internes seulement) (cf. `specs/image` photo 1) | `main/style_amber.c` |
| `rpm_cream.png` | 1024×600 | crème analogique, graduations + zone rouge (cf. `specs/image` photo 2) | `main/style_cream.c` |

> Cible réelle : écran **rond ~52 mm**. `rpm_amber` est déjà pensé pour ce
> format (canvas carré rempli au maximum). `rpm_screen`/`rpm_cream` sont encore
> en paysage 1024×600 et restent à porter en rond.

Icônes des indicateurs dessinées en vectoriel (pas de police d'icônes) :
`main/gauge_icons.c`.

## Données mock

| Canal | `rpm_screen` | `rpm_amber` | `rpm_cream` |
|---|---|---|---|
| RPM | 7200 (zone rouge) | 800 (ralenti) | 800 (ralenti) |
| Papillon | 85 % | 6 % | — |
| Liquide (LDR) | 92 | 89 | 87 |
| Batterie | 14.0 V | 14.2 V | 14.0 V |
| Huile | 98 | 91 | — |
| OBD | connecté | connecté | — |

Sur `rpm_screen`, le régime est volontairement en zone rouge pour valider la
coloration de danger ; sur `rpm_amber`/`rpm_cream`, au ralenti pour coller aux
photos de référence.

> `°` n'est pas inclus dans les polices Montserrat LVGL par défaut : les
> températures sont affichées « 89 C » plutôt que « 89 °C ».

## Régénérer

```bash
sim/build_golden.sh          # clone LVGL v9.2.2 si besoin, compile, réécrit ce PNG
# ou, avec des sources LVGL existantes :
LVGL_DIR=/chemin/vers/lvgl sim/build_golden.sh
```
