# Extension future : K-line et horloge RTC sauvegardée

Cette architecture permet d’ajouter simultanément une interface automobile K-line et une horloge conservant l’heure pendant les coupures d’alimentation, au prix de l’abandon du lecteur microSD.

> Cette proposition reste à valider sur la carte réelle et ne fait pas encore partie de l’implémentation.

## Architecture proposée

```text
LILYGO T-RGB ESP32-S3
├── UART matériel
│   ├── GPIO40 (ancienne ligne microSD CMD)  → K-line TX
│   └── GPIO38 (ancienne ligne microSD DAT0) ← K-line RX
│       └── transceiver automobile L9637D, MC33290 ou équivalent
│
└── I²C partagé — SDA GPIO8 / SCL GPIO48
    ├── tactile CST820          adresse 0x15
    ├── expander XL9535         adresse 0x20
    └── module RTC DS3231       adresse 0x68
        └── pile de sauvegarde
```

GPIO39, ancien clock microSD, resterait potentiellement disponible, sous réserve de vérification sur la révision matérielle utilisée.

## Répartition des ressources

| Fonction | Ressource | Conséquence |
|---|---|---|
| K-line TX | GPIO40 / UART | microSD désactivée |
| K-line RX | GPIO38 / UART | microSD désactivée |
| RTC SDA | GPIO8 / I²C partagé | aucune nouvelle broche nécessaire |
| RTC SCL | GPIO48 / I²C partagé | aucune nouvelle broche nécessaire |
| RTC INT/SQW | non connecté initialement | évite de consommer un GPIO |
| microSD CLK | GPIO39 libéré | réserve éventuelle |

Le firmware ne devra jamais initialiser SDMMC lorsque GPIO38 et GPIO40 sont utilisés par l’UART K-line.

## Interface K-line obligatoire

La K-line automobile peut atteindre la tension batterie du véhicule. Elle ne doit **jamais** être raccordée directement à l’ESP32-S3.

Un transceiver automobile est nécessaire, par exemple :

- L9637D ;
- MC33290 ;
- composant équivalent qualifié pour ISO 9141 / ISO 14230.

Le circuit final devra prévoir au minimum :

- adaptation entre logique ESP32 3,3 V et transceiver ;
- protection contre inversion de polarité, surtensions et transitoires automobiles ;
- masse commune maîtrisée avec le véhicule ;
- découplage proche du transceiver ;
- connectique OBD/K-line adaptée.

Le choix exact du composant doit être vérifié pour garantir que ses niveaux logiques d’entrée et de sortie sont compatibles avec 3,3 V. L’expander I²C XL9535 ne doit pas servir à générer ou recevoir le signal K-line : utiliser un UART matériel.

## RTC recommandé

### DS3231

Le DS3231 est recommandé pour sa bonne précision et sa compensation en température :

- adresse I²C `0x68`, sans conflit avec le CST820 ou le XL9535 ;
- alimentation principale compatible avec un système 3,3 V selon le module choisi ;
- maintien de l’heure sur pile lorsque la carte est hors tension ;
- précision typique de l’ordre de ±2 ppm selon version et conditions ;
- lecture possible au démarrage sans connexion Wi-Fi.

L’entrée/sortie `INT/SQW` peut rester non connectée. Elle ne devient utile que pour une alarme ou un réveil matériel.

### Attention aux modules à pile

De nombreux modules DS3231 bon marché comprennent un circuit de recharge destiné à une pile **LIR2032 rechargeable**. Une **CR2032 classique n’est pas rechargeable** et ne doit pas être utilisée si ce circuit reste actif.

Choisir l’une des solutions suivantes :

1. module explicitement prévu pour CR2032 sans recharge ;
2. suppression/désactivation documentée du circuit de recharge ;
3. module et accumulateur LIR2032 compatibles.

Vérifier également que les résistances pull-up I²C du module sont reliées à **3,3 V**, jamais à 5 V. Plusieurs jeux de pull-up en parallèle peuvent charger excessivement le bus ; conserver une résistance équivalente raisonnable.

## Sauvegarde persistante : choix NVS

La microSD n’est pas nécessaire pour conserver l’état de l’application. Le choix retenu est la **NVS (Non-Volatile Storage) d’ESP-IDF**, stockée dans les 16 Mo de flash embarquée.

La NVS servira notamment à conserver :

- préférences utilisateur, unités, thème et luminosité ;
- seuils d’alerte et paramètres K-line ;
- calibration éventuelle ;
- dernier écran ou état utile au redémarrage ;
- données de bonding Bluetooth, gérées par la pile ESP-IDF.

Règles d’écriture :

- écrire uniquement après une modification réelle ;
- regrouper les changements et attendre 2 à 5 secondes après la dernière action utilisateur ;
- ne jamais enregistrer à chaque trame d’affichage ou mesure ECU ;
- ne pas réécrire une valeur identique ;
- versionner la structure des paramètres pour permettre les migrations futures.

Avec des réglages modifiés occasionnellement et le wear leveling de la NVS, l’endurance de la flash est très largement suffisante pour la durée de vie prévue de l’appareil. LittleFS ne sera envisagé que si de vrais fichiers volumineux deviennent nécessaires.

## Comportement logiciel envisagé

Au démarrage :

1. initialiser le bus I²C et l’expander ;
2. détecter le CST820 (`0x15`), le XL9535 (`0x20`) et le RTC (`0x68`) ;
3. lire l’heure du DS3231 ;
4. vérifier le drapeau de perte d’oscillation du RTC avant de faire confiance à l’heure ;
5. initialiser l’UART sur GPIO40/GPIO38 ;
6. démarrer le protocole K-line uniquement après stabilisation de l’affichage.

Une synchronisation occasionnelle par Wi-Fi ou téléphone pourra corriger le RTC. L’heure corrigée sera ensuite réécrite dans le DS3231.

## Validation matérielle nécessaire

- Confirmer la révision originale ou V2 de la T-RGB.
- Vérifier que GPIO38 et GPIO40 sont utilisables en UART lorsque SDMMC est désactivé.
- Vérifier qu’aucun pull-up ou circuit microSD ne déforme les signaux UART.
- Mesurer les niveaux RX/TX entre l’ESP32 et le transceiver K-line.
- Tester le bus I²C avec écran, tactile et RTC actifs simultanément à 400 kHz.
- Tester le maintien de l’heure après une coupure prolongée.
- Tester les perturbations du bus et du RTC pendant les échanges K-line.
- Effectuer les essais K-line avec une alimentation protégée avant connexion à un véhicule.

## Décision d’architecture

Cette solution est viable si la microSD n’est pas requise :

- **écran et tactile conservés** ;
- **K-line sur UART matériel** ;
- **heure sauvegardée sur RTC I²C** ;
- **états et configuration sauvegardés en NVS dans la flash interne** ;
- **bonding Bluetooth conservé via la NVS d’ESP-IDF** ;
- **aucun GPIO supplémentaire nécessaire pour le RTC ou la sauvegarde** ;
- GPIO39 potentiellement conservé en réserve.
