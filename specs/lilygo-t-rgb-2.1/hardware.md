# Caractéristiques matérielles

## Variantes T-RGB

| Référence | Format | LCD | Tactile |
|---|---|---|---|
| H583 | 2,1" Half Circle | ST7701S, 480×480 | FT3267 (`0x38`) |
| **H597** | **2,1" Full Circle** | **ST7701S, 480×480** | **CST820 (`0x15`)** |
| H604 | 2,8" Full Circle | ST7701S, 480×480 | GT911 (`0x5D`, parfois `0x14`) |

Le présent dossier cible H597. Les variantes 2,1" ont la même définition mais une vitre et un contrôleur tactile différents.

Le pilote officiel définit aussi `HALF_CIRCLE_V2` et `FULL_CIRCLE_V2`. Cette révision emploie une autre séquence ST7701S, d’autres timings et un autre ordre RGB. Aucun SKU ni marquage permettant de reconnaître V2 n’est documenté.

## Calcul et mémoire

- ESP32-S3R8 : deux cœurs Xtensa LX7 jusqu’à 240 MHz.
- 512 Ko de SRAM interne, 384 Ko de ROM.
- 8 Mo de PSRAM OPI intégrée au module/SoC R8.
- 16 Mo de flash QSPI externe W25Q128, QIO 80 MHz dans la configuration officielle.
- Wi-Fi 2,4 GHz 802.11 b/g/n et Bluetooth 5 LE ; pas de Bluetooth Classic.

La page commerciale inverse parfois flash et PSRAM. Le README, le schéma, le fichier PlatformIO et la référence S3R8 confirment **16 Mo de flash et 8 Mo de PSRAM**.

## Affichage

- Dalle IPS ronde 2,1 pouces, 480×480.
- Contrôleur ST7701S.
- Bus parallèle RGB666 physiquement raccordé (18 lignes), mais pilote ESP32 en RGB565 (16 lignes).
- Initialisation par protocole série 9 bits (bit commande/donnée + octet) au travers de l’expander I²C.
- Deux profils officiels : original à 8 MHz et V2 à 10 MHz.
- Rétroéclairage piloté par AW9364DNR, 16 niveaux par comptage d’impulsions sur GPIO46. Ce n’est pas un PWM classique ; maintenir la ligne basse environ 3 ms éteint le rétroéclairage.

Un framebuffer RGB565 complet vaut `480 × 480 × 2 = 460 800` octets. Deux buffers occupent 921 600 octets et doivent résider en PSRAM.

## Tactile cible H597

- CST820, compatible avec les pilotes de famille CST816/CSTXXX.
- Adresse I²C `0x15`.
- SDA GPIO8, SCL GPIO48, fréquence maximale pratique 400 kHz.
- Interruption GPIO1.
- Reset sur XL9535 IO1.
- L’interruption CST820 n’est pas maintenue pendant tout le contact : le pilote officiel interroge les registres pour suivre un glissement continu.
- Orientation, miroir et coordonnées restent à valider sur le matériel réel.

## Alimentation

D’après le schéma officiel :

- USB-C relié au contrôleur USB natif de l’ESP32-S3 (GPIO19 D−, GPIO20 D+).
- Connecteur batterie Li-ion/LiPo 1 cellule, 3,7–4,2 V.
- Chargeur TP4065, résistance PROG 2 kΩ.
- Régulateur 3,3 V RT9080.
- Chemin d’alimentation SI2307/1N5819.
- `PWR_EN` sur XL9535 IO2 ; doit être activé pour le fonctionnement sur batterie.
- Mesure batterie sur GPIO4 via diviseur 100 kΩ / 100 kΩ : tension batterie ≈ 2 × tension ADC.
- La mesure n’est pas fiable pendant la charge USB.

## Interfaces et contraintes

- microSD en SDMMC 1 bit : CLK GPIO39, CMD GPIO40, DAT0 GPIO38 ; contrôle via XL9535 IO7.
- Connecteur 4 broches type Grove : 3,3 V, GND, I²C SDA/SCL.
- I²C partagé par tactile, expander et connecteur externe.
- BOOT GPIO0 et bouton RESET/CHIP_PU.
- UART0 peut apparaître sur GPIO43/GPIO44 quand USB CDC est désactivé, mais ces lignes sont aussi raccordées au RGB666 : ne pas les considérer libres.
- Aucun IMU, RTC, codec audio ou microphone n’est documenté.
- Selon la FAQ LILYGO, **aucun GPIO n’est libre pour une extension générale**.
