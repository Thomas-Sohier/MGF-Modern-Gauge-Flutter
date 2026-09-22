# LILYGO T-RGB — travail restant et marge IRAM

Ce document conserve la liste des travaux à reprendre après le premier build ESP-IDF réussi et avant la finalisation sur véhicule.

## État actuel

- Cible : LILYGO T-RGB 2.1" Full Circle H597, ESP32-S3R8.
- ESP-IDF 5.4.2 : build validé.
- Écran ST7701S : profils original et V2 implémentés.
- Tactile CST820, rétroéclairage AW9364 et expander XL9535 : implémentés.
- Paramètres NVS et restauration de la dernière page : implémentés.
- RTC DS3231 : support optionnel implémenté.
- K-line sur GPIO40/GPIO38 : support logiciel implémenté, désactivé par défaut.
- Service BLE NimBLE : optionnel, build validé avec `MGF_ENABLE_BLE_CONFIG=1`.
- Validation sur carte réelle : non effectuée.

## Point de vigilance IRAM

> **Mesure ESP-IDF 5.4.2 (2026-09-22)** : la région IRAM de 16 384 octets est
> à 16 383 octets **avec et sans BLE**. Sur ESP32-S3, l'éditeur de liens
> remplit toujours cette région puis place le reste du code IRAM en DIRAM
> (`.text` DIRAM : 58 843 o). Ce 99,99 % n'est donc pas une limite. Le budget
> réel est la DIRAM : 75 395 / 341 760 o (22 %) pour le firmware normal,
> 111 647 o (33 %) pour l'image BLE. `tools/check_iram.sh` contrôle
> désormais la marge DIRAM (128 Kio minimum par défaut).

Les pistes ci-dessous restent utiles si la DIRAM venait à manquer ou si une
erreur `iram0_0_seg overflowed` apparaissait réellement au linkage.

Erreur typique :

```text
region `iram0_0_seg' overflowed
```

La consommation doit être contrôlée après chaque modification avec :

```bash
idf.py size
idf.py size-components
idf.py size-files
```

Le fichier `build/mgf_gauge_lvgl.map` permet ensuite d’identifier précisément les symboles placés dans les sections IRAM.

## Solutions IRAM, de la plus simple à la plus complexe

### 1. Vérifier qu’il s’agit réellement d’un problème

Avant toute optimisation :

1. conserver la version ESP-IDF et les dépendances verrouillées ;
2. refaire un build propre ;
3. comparer `idf.py size`, `size-components` et le fichier `.map` ;
4. vérifier quelle section déborde réellement.

La région de 16 Ko peut être remplie presque exactement par le script de linkage. Une ligne à 99,99 % n’implique pas forcément que toute la mémoire exécutable interne est épuisée.

### 2. Supprimer les annotations IRAM inutiles dans notre code

Rechercher :

```bash
rg "IRAM_ATTR|DRAM_ATTR" main
```

Pour chaque fonction annotée `IRAM_ATTR`, vérifier qu’elle est réellement appelée pendant une interruption ou lorsque le cache flash est indisponible. Une fonction applicative normale doit rester en flash.

Ne jamais retirer aveuglément `IRAM_ATTR` d’un gestionnaire d’interruption matériel.

### 3. Désactiver les options IRAM-safe non indispensables

Examiner les options ESP-IDF qui copient des pilotes en IRAM. La principale option connue du projet est :

```text
CONFIG_LCD_RGB_ISR_IRAM_SAFE=y
```

La désactiver peut libérer de l’IRAM, mais l’écran peut devenir plus sensible aux accès flash, notamment pendant une écriture NVS ou certaines opérations radio. Cette option ne doit être désactivée qu’après mesure et test matériel prolongé.

Faire de même pour les éventuelles options IRAM-safe UART, GPIO, timer ou Bluetooth si elles apparaissent dans le `sdkconfig`.

### 4. Réduire les fonctions appelées depuis les interruptions

Une fonction IRAM entraîne parfois avec elle ses fonctions appelées et certaines données nécessaires. Pour réduire cette chaîne :

- garder les ISR très courtes ;
- pousser seulement un événement ou une notification depuis l’ISR ;
- effectuer le traitement lourd dans une tâche FreeRTOS normale ;
- éviter logs, formatage, allocations et logique métier dans les ISR.

### 5. Désactiver les fonctionnalités non nécessaires dans ESP-IDF/LVGL

Réduire la configuration aux fonctions réellement utilisées :

- protocoles ou fonctions Bluetooth inutiles ;
- pilotes non utilisés ;
- fonctions LVGL inutiles ;
- instrumentation et logs de debug de production.

Cette étape réduit surtout flash et RAM générale, mais peut aussi supprimer des dépendances IRAM.

### 6. Utiliser une configuration distincte avec et sans BLE

**Appliqué** : `sdkconfig.defaults` désactive Bluetooth pour le firmware
normal, et l'image de configuration ajoute `sdkconfig.ble` dans un dossier de
build séparé (cf. `docs/ble-config.md`). La marge IRAM reste à surveiller sur
l'image BLE avec `tools/check_iram.sh`.

Si le BLE augmente trop les contraintes mémoire :

- firmware de configuration avec BLE activé ;
- firmware normal sans BLE ; ou
- activation du BLE seulement lors d’un mode de configuration dédié, avec initialisation/désinitialisation à l’exécution si l’architecture ESP-IDF le permet.

La compilation conditionnelle existe déjà via :

```bash
idf.py -DMGF_ENABLE_BLE_CONFIG=1 build
```

### 7. Déplacer explicitement du code hors IRAM

Après analyse du fichier `.map`, certaines fonctions non critiques peuvent être replacées en flash. Cette opération exige une bonne compréhension des contraintes de cache et des interruptions. Une erreur peut provoquer un crash uniquement dans certaines conditions, par exemple pendant une écriture flash.

### 8. Modifier le placement mémoire ou le script de linkage

En dernier recours :

- modifier les règles de placement des sections ;
- rééquilibrer IRAM et DRAM lorsque le SoC et ESP-IDF le permettent ;
- créer des fragments de linker ESP-IDF ciblés ;
- déplacer des composants ou symboles précis.

C’est la solution la plus complexe et la plus fragile face aux mises à jour ESP-IDF. Elle ne doit être envisagée qu’après analyse exacte des symboles responsables.

## Travail restant avant validation produit

### Bring-up de la carte

1. Photographier et identifier la révision PCB.
2. Flasher d’abord le profil ST7701S original.
3. Essayer le profil V2 complet si l’écran reste noir ou incorrect.
4. Valider les aplats rouge, vert et bleu.
5. Valider pixel clock, polarités, porches et stabilité de l’image.
6. Tester le tactile au centre et aux quatre coins.
7. Régler `swap_xy`, `mirror_x` et `mirror_y` si nécessaire.
8. Tester les 16 niveaux du rétroéclairage AW9364.
9. Tester le démarrage sur USB puis sur batterie.

### Stabilité et mémoire

- surveiller les underruns du périphérique RGB ;
- mesurer la RAM et la PSRAM libres après démarrage ;
- mesurer les stacks des tâches LVGL, ECU, BLE et système ;
- tester les écritures NVS pendant le rafraîchissement de l’écran ;
- tester le BLE pendant une charge graphique élevée ;
- effectuer un test longue durée de plusieurs heures ;
- ajouter un suivi automatique de la taille IRAM dans la CI.

### Paramètres et NVS

- [x] ajouter une interface applicative de modification des réglages via le
  runtime settings (les setters publics sont prêts ; pas encore de page tactile
  dédiée) ;
- [x] appliquer à l’exécution les changements BLE de luminosité, unités et page ;
- [x] éviter les accès concurrents entre la tâche NimBLE et la tâche LVGL : BLE
  ne fait qu'alimenter un snapshot protégé, consommé par le timer LVGL ;
- implémenter la migration lors d’un futur changement de schéma NVS ;
- tester coupure d’alimentation pendant une sauvegarde ;
- tester la conservation du bonding Bluetooth et des paramètres après mise à jour.

### Bluetooth

- tester avec un téléphone réel ;
- créer ou documenter une application cliente ;
- choisir la politique de pairing définitive ;
- décider si Just Works est suffisant ou si une authentification MITM est requise ;
- valider suppression et renouvellement des appareils bondés ;
- définir un mode permettant d’ouvrir volontairement la configuration ;
- vérifier la consommation électrique quand BLE est actif.

### RTC DS3231

- choisir un module 3,3 V sans recharge dangereuse d’une CR2032 ;
- vérifier les résistances pull-up I²C ;
- [x] détecter le drapeau oscillator-stop et ne l'acquitter qu'après une écriture valide ;
- [x] définir le DS3231 comme stockage UTC ; les valeurs `LOCAL` BLE sont
  refusées tant qu'aucun fuseau/DST n'est configuré ;
- gérer ultérieurement la conversion d'affichage, le fuseau et l'heure été/hiver ;
- [x] synchroniser l'heure UTC depuis BLE ; vérifier sa conservation après
  coupure sur carte.

### K-line

- choisir le transceiver automobile : L9637D, MC33290 ou équivalent ;
- concevoir l’adaptation logique 3,3 V ;
- ajouter protections surtension, inversion et transitoires automobiles ;
- vérifier que le circuit microSD résiduel ne perturbe pas GPIO38/GPIO40 ;
- activer `MGF_USE_MEMS_KLINE=1` seulement avec le transceiver présent ;
- tester les variantes MEMS 1.6 et 1.9 ;
- vérifier le slow init 5 bauds et l’écho local ;
- conserver le fonctionnement dégradé lorsque l’ECU est absent.

### Alimentation automobile

- concevoir une alimentation protégée depuis le 12 V véhicule ;
- prévoir fusible, TVS, inversion de polarité et load dump ;
- tester les chutes de tension pendant le démarrage moteur ;
- décider du comportement à la coupure du contact ;
- tester l’arrêt propre et la dernière sauvegarde NVS ;
- vérifier la température dans le boîtier final.

## Critères avant installation dans un véhicule

- build ESP-IDF reproductible et dépendances verrouillées ;
- profil d’écran confirmé ;
- affichage et tactile stables pendant plusieurs heures ;
- aucune corruption NVS après coupures répétées ;
- RTC fiable après coupure prolongée ;
- BLE sécurisé selon la politique retenue ;
- interface K-line isolée et protégée ;
- alimentation validée contre les transitoires automobiles ;
- aucune régression de taille IRAM ;
- boîtier, connectique et fixation mécaniquement sûrs.
