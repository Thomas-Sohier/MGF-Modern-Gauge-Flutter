# Guide de bring-up ESP-IDF

Ce fichier décrit le contrat matériel à implémenter ; il ne remplace pas un test sur la carte réelle.

## Configuration recommandée

- Cible `esp32s3`, CPU 240 MHz.
- Flash 16 Mo, QIO 80 MHz.
- PSRAM OPI 8 Mo, 80 MHz.
- RGB565, framebuffers en PSRAM, alignement PSRAM 64 octets.
- ISR RGB en IRAM et bounce buffer pour limiter les underruns.
- USB Serial/JTAG natif ; si le démarrage attend un terminal USB sur batterie, désactiver USB CDC au boot.

## Timings LCD

### Profil original

```text
pclk_hz              8 000 000
h_res / v_res        480 / 480
hsync pulse/back/front  1 / 30 / 50
vsync pulse/back/front  1 / 30 / 20
pclk_active_neg      true
```

### Profil V2

```text
pclk_hz              10 000 000
h_res / v_res        480 / 480
hsync pulse/back/front  2 / 34 / 20
vsync pulse/back/front  2 / 20 / 50
pclk_active_neg      true
```

Utiliser pour chaque profil la liste `data_gpio_nums` correspondante dans [`pinout.md`](pinout.md).

## Initialisation ST7701S

Les tables byte-for-byte sont conservées dans [`reference/upstream/RGBPanelInit.h`](reference/upstream/RGBPanelInit.h) :

- `st7701_2_1_inches` pour le matériel original ;
- `st7701_2_1_inches_rev2` pour V2.

Le transfert officiel :

1. active `PWR_EN` (XL9535 IO2) ;
2. maintient LCD reset (IO6) bas environ 20 ms ;
3. relâche reset puis attend environ 10 ms ;
4. transmet les commandes en série 9 bits via XL9535 IO3/IO4/IO5 ;
5. attend les délais demandés après `Sleep Out` et `Display On` ;
6. configure ensuite le périphérique RGB ESP-IDF.

La séquence originale finit notamment par `MADCTL 0x08`, `COLMOD 0x66`, `Sleep Out`, délai, `Display On`, délai. La V2 commence par `COLMOD 0x50`, utilise des registres gamma/alimentation différents et termine par inversion désactivée (`0x20`). Ne pas utiliser la séquence générique du composant ST7701.

L’amont passe temporairement l’I²C à 1 MHz pour accélérer les nombreux basculements de l’expander, puis revient à 400 kHz pour le tactile. Pour un premier bring-up, 400 kHz est plus conservateur mais plus lent.

## Tactile CST820

- Bus I²C GPIO8/GPIO48 à 400 kHz.
- Adresse `0x15`.
- Reset via XL9535 IO1, IRQ GPIO1.
- Le composant ESP-IDF `esp_lcd_touch_cst816s` peut servir de pilote compatible ; valider en pratique l’identifiant, les coordonnées et les gestes.
- Ne pas dépendre uniquement d’un IRQ maintenu pour suivre le doigt : le CST820 nécessite des lectures répétées.

## Rétroéclairage

GPIO46 pilote un AW9364DNR par impulsions :

- niveau bas pendant environ 3 ms : extinction/réinitialisation ;
- impulsions : sélection d’un des 16 niveaux ;
- protéger la séquence contre les interruptions si la durée des impulsions est critique.

Un simple `gpio_set_level(..., 1)` peut allumer l’écran mais ne fournit pas une gestion fiable de la luminosité.

## Ordre de validation sur carte

1. Vérifier le SKU H597, le marquage PCB et photographier la révision.
2. Démarrer sans LCD : USB, logs, flash, PSRAM et scan I²C (`0x20`, `0x15`).
3. Activer `PWR_EN`, tester reset LCD et rétroéclairage à faible luminosité.
4. Essayer le profil original (init, ordre RGB, timings 8 MHz).
5. Si écran noir, couleurs/fausses lignes ou synchro instable, essayer le profil V2 complet, sans mélanger les paramètres.
6. Afficher des aplats rouge/vert/bleu, damier et mire de bord pour valider ordre, polarité et porches.
7. Tester tactile aux quatre coins et au centre ; régler swap/mirror dans le pilote LVGL.
8. Tester double buffering et charge UI ; surveiller les underruns RGB et la bande passante PSRAM.
9. Tester démarrage USB puis batterie, extinction, luminosité et mesure batterie.
10. Tester microSD seulement après stabilisation écran/tactile.

## Risques à traiter dans le portage actuel

- `board_display.c` est entièrement câblé pour Waveshare et doit être remplacé ou séparé derrière une configuration de carte.
- Le projet dépend actuellement de TCA9554 et d’un SPI 3 fils direct ; la T-RGB demande XL9535 et une série 9 bits via expander.
- Les broches K-line GPIO17/GPIO18 entrent en conflit avec le LCD.
- Le pilote officiel contient deux révisions non auto-détectables.
- Le schéma date de 2022 et peut ne pas refléter V2.
- Le CST820 n’a pas de datasheet officielle jointe au dépôt LILYGO ; la compatibilité CST816 est empirique/officielle au niveau logiciel.
