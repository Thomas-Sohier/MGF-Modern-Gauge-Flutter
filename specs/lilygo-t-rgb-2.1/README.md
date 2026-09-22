# LILYGO T-RGB ESP32-S3 — écran rond 2,1 pouces

Dossier de référence matériel pour porter le projet sur la **LILYGO T-RGB 2.1" Full Circle** (SKU H597), écran rond 480×480 piloté par ST7701S et tactile CST820.

> **Point bloquant avant le portage : identifier la révision de la carte.** Le dépôt LILYGO contient une configuration originale et une configuration `V2` incompatibles (timings, ordre des lignes RGB et séquence ST7701S), sans méthode officielle d’identification visuelle. Relever le marquage PCB et tester les deux profils si nécessaire.

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

## Impact sur le projet actuel

La carte n’est **pas compatible broche à broche** avec la Waveshare actuellement codée dans `main/infrastructure/board_display.c`. Il faudra notamment remplacer :

- le TCA9554 par un XL9535/XL9555 compatible à l’adresse `0x20` ;
- tous les GPIO RGB, synchro, rétroéclairage, I²C et tactile ;
- le pixel clock 16 MHz et les porches provisoires par le profil LILYGO 8 MHz ou V2 10 MHz ;
- le SPI 3 fils direct par le protocole série 9 bits transmis via l’expander ;
- la séquence ST7701 générique par la table exacte LILYGO ;
- la commande tout-ou-rien du rétroéclairage par le protocole à impulsions AW9364.

Les broches K-line provisoires GPIO17/GPIO18 sont utilisées par le bus LCD LILYGO et devront être réaffectées. LILYGO indique par ailleurs qu’aucun GPIO n’est réellement libre.
