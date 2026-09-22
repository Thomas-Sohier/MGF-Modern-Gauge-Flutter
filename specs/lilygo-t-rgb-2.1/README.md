# LILYGO T-RGB ESP32-S3 — écran rond 2,1 pouces

Dossier de référence matériel pour porter le projet sur la **LILYGO T-RGB 2.1" Full Circle** (SKU H597), écran rond 480×480 piloté par ST7701S et tactile CST820.

> **Validation matérielle requise : identifier la révision de la carte.** Le portage
> sélectionne l'original par défaut et permet de construire V2 avec
> `-DMGF_LILYGO_T_RGB_V2=1`. Les deux profils restent incompatibles (timings, ordre
> des lignes RGB et séquence ST7701S), sans méthode officielle d'identification visuelle.

## Contenu

- [`hardware.md`](hardware.md) — caractéristiques, variantes, alimentation et périphériques.
- [`pinout.md`](pinout.md) — brochage complet et affectations de l’expander.
- [`esp-idf-bringup.md`](esp-idf-bringup.md) — paramètres LCD/tactile et checklist de portage ESP-IDF.
- [`future-kline-rtc.md`](future-kline-rtc.md) — architecture future K-line + RTC sauvegardée en réutilisant les broches microSD.
- [`sources.md`](sources.md) — sources, provenance, version et sommes SHA-256.
- [`reference/`](reference/) — schéma, datasheet ST7701S, fichiers mécaniques et extraits du pilote officiel.

## Profil cible présumé

| Élément | Valeur |
|---|---|
| Produit | LILYGO T-RGB 2.1" Full Circle, H597 |
| SoC | ESP32-S3R8, double cœur LX7, jusqu’à 240 MHz |
| Flash / PSRAM | 16 Mo QSPI / 8 Mo OPI intégrée |
| LCD | IPS rond 2,1", 480×480, ST7701S |
| Interface LCD | RGB666 câblée physiquement, exploitée en RGB565 16 bits par l’ESP32-S3 |
| Tactile | CST820 capacitif, I²C `0x15`, interruption GPIO1 |
| Stockage persistant | NVS en flash pour états, configuration et bonding Bluetooth |
| microSD | présente mais destinée à être désactivée pour libérer la K-line |
| USB | USB-C natif ESP32-S3, USB Serial/JTAG |

## État du portage

`main/infrastructure/board_display.c` cible maintenant directement la T-RGB :

- XL9535/XL9555 compatible à `0x20`, `PWR_EN`, resets et série ST7701S 9-bit via expander ;
- GPIO RGB, synchro, rétroéclairage AW9364, I²C et tactile selon le brochage H597 ;
- profil original 8 MHz ou V2 10 MHz, avec porches, ordre RGB et tables exactes ;
- CST820 à `0x15` via le pilote CST816S compatible et LVGL RGB565 en PSRAM ;
- rétroéclairage discret AW9364 via `board_display_backlight_set_brightness(0..16)`.

Les broches K-line provisoires GPIO17/GPIO18 sont utilisées par le bus LCD LILYGO et
nécessitent une réaffectation avant l’activation de cette source sur matériel.
