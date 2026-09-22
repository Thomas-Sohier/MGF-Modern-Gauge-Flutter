# Sources et provenance

Collecte effectuée le **2026-09-22** depuis le dépôt officiel LILYGO, commit `359c03f7cd8e9aaadd4bf0f3fbf790844fd95f98`.

## Liens principaux

- Produit : <https://www.lilygo.cc/products/t-rgb>
- Dépôt officiel : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB>
- README et tableau des variantes : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/README.md>
- Pilote et timings : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/src/LilyGo_RGBPanel.cpp>
- Brochage amont : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/src/utilities.h>
- Séquences ST7701S : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/src/RGBPanelInit.h>
- Schéma : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/schematic/T-RGB.pdf>
- Fichiers mécaniques : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/tree/main/dimensions>
- Exemple ESP-IDF officiel : <https://github.com/Xinyuan-LilyGO/LilyGo-Display-IDF>
- Ajout des profils V2 : <https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/commit/01f7cdbe75d147c8dc2b5b1b1e7e532d75a29088>
- ESP32-S3 : <https://www.espressif.com/en/products/socs/esp32-s3>

Les fichiers `reference/upstream/*.h` proviennent du dépôt LILYGO sous licence MIT. Les PDF et fichiers mécaniques sont des copies de référence fournisseur ; consulter le fournisseur pour leurs conditions de redistribution.

## Fichiers archivés et SHA-256

```text
510caa2f769094e2984e7c40d7fe1400949e189f1553b72d96516450fd428c97  reference/T-RGB-schematic.pdf
a209e8734d905c7f873e0189dc13387ae61e8cbcdddb01207627d34a8125d242  reference/datasheets/ST7701S-v1.1.pdf
adee87a692a2d6a2b2e9d4f2b30c5acd335df7f75e78d143059a5a7bffbfe9a4  reference/mechanical/T-RGB-FULL-3D-2.1-Inches.step
f6cddd426cc478ec36d2fcdda9930b2d4913fcdebbeb291318cd799625ad74e4  reference/mechanical/T-RGB-PCB.dxf
e91bbb8a5ae4d577f14c08f41dda1fb2f84c168d9c4dcfd23030122e489181a3  reference/upstream/RGBPanelInit.h
def76a8c20fa16c0f0381b08b384591d2fe077f90ee53fcd652b0cca001c18fd  reference/upstream/utilities.h
```

## Limites documentaires

- Aucune datasheet CST820 n’est fournie dans le dépôt officiel ; LILYGO emploie un pilote CST816/CSTXXX à l’adresse `0x15`.
- Le schéma archivé est daté du 23 septembre 2022 et semble viser le matériel original.
- La révision V2 n’a pas de correspondance SKU/marquage officiellement publiée.
- Les noms de couleurs RGB sont contradictoires dans les sources ; les listes ordonnées `data_gpio_nums` du pilote sont la référence opérationnelle.
