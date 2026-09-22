# Brochage

## Signaux directs ESP32-S3

| Fonction | GPIO | Remarque |
|---|---:|---|
| LCD backlight | 46 | AW9364, commande par impulsions |
| LCD HSYNC | 47 | |
| LCD VSYNC | 41 | |
| LCD DE | 45 | |
| LCD PCLK | 42 | front descendant actif |
| I²C SDA | 8 | tactile + XL9535 + connecteur externe |
| I²C SCL | 48 | tactile + XL9535 + connecteur externe |
| Touch IRQ | 1 | actif bas selon le pilote officiel |
| microSD CLK | 39 | SDMMC 1 bit |
| microSD CMD | 40 | SDMMC 1 bit |
| microSD DAT0 | 38 | SDMMC 1 bit |
| Batterie ADC | 4 | diviseur par 2 |
| BOOT | 0 | téléchargement manuel avec RESET |
| USB D− / D+ | 19 / 20 | USB natif Serial/JTAG |

## Bus LCD physique RGB666

| Ligne dalle | GPIO | Ligne dalle | GPIO | Ligne dalle | GPIO |
|---|---:|---|---:|---|---:|
| DB0 | 44 | DB6 | 14 | DB12 | 43 |
| DB1 | 21 | DB7 | 13 | DB13 | 7 |
| DB2 | 18 | DB8 | 12 | DB14 | 6 |
| DB3 | 17 | DB9 | 11 | DB15 | 5 |
| DB4 | 16 | DB10 | 10 | DB16 | 3 |
| DB5 | 15 | DB11 | 9 | DB17 | 2 |

Les commentaires de couleur du code officiel sont incohérents entre fichiers. Pour ESP-IDF, utiliser les listes ordonnées ci-dessous telles quelles plutôt que de déduire l’ordre à partir des noms R/G/B.

### `data_gpio_nums[D0..D15]` — carte originale

```text
7, 6, 5, 3, 2,
14, 13, 12, 11, 10, 9,
21, 18, 17, 16, 15
```

GPIO43 et GPIO44, lignes supplémentaires du bus RGB666, sont omis en RGB565.

### `data_gpio_nums[D0..D15]` — carte V2

```text
43, 7, 6, 5, 3,
14, 13, 12, 11, 10, 9,
44, 21, 18, 17, 16
```

GPIO2 et GPIO15 sont omis dans ce profil.

## Expander I²C XL9535/XL9555

Adresse : `0x20`.

| Ligne expander | Fonction |
|---|---|
| IO1 | reset tactile |
| IO2 | `PWR_EN` carte/écran |
| IO3 | CS série ST7701S |
| IO4 | SDA/MOSI série ST7701S |
| IO5 | CLK série ST7701S |
| IO6 | reset LCD |
| IO7 | activation/contrôle microSD |

Les macros amont `BOARD_TFT_CS=3`, `MOSI=4`, `SCLK=5`, `RST=6` désignent en pratique les **numéros IO de l’expander**, pas des GPIO ESP32 directs. L’initialisation ST7701S est un transfert logiciel 9 bits via cet expander.

## Conflits connus avec le projet

- GPIO17 et GPIO18, envisagés pour la K-line dans le projet, sont des données LCD.
- GPIO4 est utilisé par la mesure batterie.
- GPIO8/GPIO48 doivent rester en I²C pour le tactile et l’expander.
- GPIO43/GPIO44 ne sont pas des UART librement disponibles malgré leur rôle UART0 possible.
- Le portage doit considérer tous les GPIO comme occupés et prévoir un expander ou un contrôleur externe pour toute nouvelle E/S.
