# Interface K-line / Rover MEMS

Lecture des données OBD réelles de la MG F depuis son calculateur **Rover MEMS
1.6** (ROSCO), en remplacement du simulateur `fake_ecu`. Portage C de la partie
décodage du projet Go de référence [`andrewdjackson/rosco`](https://github.com/andrewdjackson/rosco)
(ex-`readmems`), documenté par [MEMS FCR](https://memsfcr.co.uk/rover-service-communications-protocol-rosco/).

## Chaîne de couches

```
K-line (12 V, 1 fil)
  └─ transceiver externe (ex. ST L9637D)  ── TX/RX 3.3 V ──┐
                                                            ▼
  infrastructure/kline_uart_esp32.c   UART 9600 8N1 + réveil 5 bauds → kline_transport_t
  domain/ecu_reader.h                 interface (connect / send_and_receive / disconnect)
  domain/mems_reader.c                impl COMMUNE (handshake + échange à écho)
  domain/mems19_reader.c              décorateur 1.9 (réveil puis délégation)
  domain/mems_session.c               cycle de poll 0x80 (+ 0x7D 1/3) (via ecu_reader_t)
  domain/mems_protocol.c              octets de trame → mems_data_t (contrôles d'intégrité)
  domain/mems_mapping.c               mems_data_t → ecu_data_t (modèle d'affichage)
  infrastructure/mems_ecu.c           tâche FreeRTOS + snapshot mutex → ecu_source_t
  app/dashboard_controller.c          lit une copie non bloquante → écran ambre
```

Deux abstractions clés :

- **`ecu_reader_t`** (`domain/ecu_reader.h`) — interface bas niveau du dialogue
  ECU, calquée sur `ECUReader` du projet Go. Implémentée une seule fois
  (`mems_reader`, commune 1.6/1.9) et décorée pour la 1.9 (`mems19_reader`).
  `mems_session` pilote cette interface sans connaître la variante.
- **`ecu_source_t`** (`domain/ecu_source.h`) — contrat de lecture par copie du
  snapshot, identique à celui de `fake_ecu` : la source MEMS est donc
  **interchangeable** dans `app_main` sans toucher au contrôleur ni à l'UI.

Correspondance avec le projet Go de référence : `ecu_reader_t` ↔ `ECUReader`,
`mems_reader` ↔ `MEMSReader`, `mems19_reader` ↔ `MEMS19Reader`, `mems_session`
↔ `ECUReaderInstance`.

## MEMS 1.6 vs 1.9

Les deux calculateurs partagent **le même jeu de commandes et les mêmes trames**
`0x80`/`0x7D` : d'où une **implémentation commune** (`mems_reader`). Seule
différence : MEMS 1.9 exige un **réveil « slow init » 5 bauds** sur la K-line
avant le handshake standard (émission de l'adresse ECU `0x16` bit à bit,
~200 ms/bit). Ce réveil n'est pas un `if` dans le code commun mais un
**décorateur** (`mems19_reader`) qui, sur `connect`, déclenche le réveil puis
délègue tout le reste à l'impl commune.

La variante se choisit via `mems_variant_t` (`infrastructure/mems_ecu.h`) ;
`mems_ecu_create` construit la chaîne de lecteurs en conséquence :

- `MEMS_VARIANT_1_6` — `mems_reader` seul.
- `MEMS_VARIANT_1_9` — `mems_reader` enveloppé dans `mems19_reader`.

Le réveil est une primitive **matérielle** : le décorateur l'invoque via
`transport.wake_up`, et l'UART ESP32 le réalise en pilotant le GPIO TX à la main
(`kline_uart_esp32.c::kline_wake_up`), puis rend le brochage au pilote UART.

## Protocole (résumé)

- **Liaison** : UART 9600 bps, 8 bits, sans parité, 1 stop, half-duplex. L'ECU
  ré-émet (« echo ») chaque octet de commande en tête de sa réponse.
- **Réveil (1.9 uniquement)** : slow init 5 bauds, adresse ECU `0x16`.
- **Handshake** : `CA` → `75` → `F4` (heartbeat) → `D0` (renvoie l'ID ECU,
  ex. `D0 99 00 03 03`).
- **Polling** : commande `0x80` (trame 29 o : RPM, températures, batterie,
  potentiomètre papillon, codes défaut…) puis `0x7D` (trame 33 o : angle
  papillon, lambda, boucle de régulation…).
- **Conversions clés** : RPM = 16 bits big-endian ; température = brut − 55 °C ;
  batterie = brut ÷ 10 ; potentiomètre papillon = brut × 0,02 V.
- **Température brute `0xFF`** = capteur absent / non supporté (fréquent pour
  carburant et ambiant) : décodée en `NAN` (affichée « -- »), jamais en 200 °C
  ni en alerte thermique. S'applique à l'eau, l'air d'admission, l'ambiant et
  le carburant.

### Contrôles d'intégrité

MEMS n'a **pas de checksum**. Une trame n'est acceptée que si :

- sa longueur est exacte (29 o pour `0x80`, 33 o pour `0x7D`) ;
- l'octet [0] est l'écho de la commande ;
- l'octet [1] est la taille annoncée (`0x1C` pour `0x80`, `0x20` pour `0x7D`).
  C'est ce contrôle qui détecte un **décalage d'un octet**, typiquement l'écho
  local du transceiver non absorbé alors que `MGF_KLINE_LOCAL_ECHO=0`
  (« `80 80 1C …` ») ;
- le régime est plausible (≤ `MEMS_RPM_MAX_PLAUSIBLE` = 10000 tr/min ; 0 reste
  valide, moteur arrêté). Aucune autre borne n'est appliquée.

Une trame rejetée compte comme une **trame perdue** pour la session.

Tables complètes des octets : `domain/mems_protocol.c`.

## Câblage LILYGO T-RGB 2.1

La carte LILYGO **n'a pas** de transceiver K-line. La K-line du connecteur
véhicule doit passer par un transceiver automobile externe (L9637D, MC33290 ou
équivalent qualifié) entre la ligne 12 V et deux niveaux logiques 3,3 V. Ne
jamais raccorder la K-line directement à l'ESP32 ; prévoir les protections
automobiles indiquées dans [`specs/lilygo-t-rgb-2.1/future-kline-rtc.md`](../specs/lilygo-t-rgb-2.1/future-kline-rtc.md).

Le profil LILYGO réutilise les anciennes broches microSD :

| Signal | GPIO (défaut) |
|---|---|
| UART TX → transceiver | `MGF_KLINE_TX_GPIO` = **40** (microSD CMD) |
| UART RX ← transceiver | `MGF_KLINE_RX_GPIO` = **38** (microSD DAT0) |
| UART | `MGF_KLINE_UART_NUM` = **1** |

**SDMMC est explicitement désactivé et aucune initialisation/prise de
possession SDMMC ne doit être ajoutée** : GPIO38 et GPIO40 appartiennent au
transport K-line dans ce profil. GPIO39 (microSD CLK), les lignes RGB de
l'écran, l'I²C GPIO8/GPIO48, l'IRQ tactile et les autres ressources réservées
sont refusées par des contrôles de compilation dans
`infrastructure/kline_board_config.h`. Ce header (sans include ESP-IDF) est la
**source unique** des ressources réservées : ses prédicats
`MGF_KLINE_GPIO_IS_*` / `MGF_KLINE_PINS_VALID` servent à la fois aux `#error`
et à la validation à l'exécution de `kline_uart_create`.

Si le montage boucle le TX sur le RX (fil unique), passer
`-DMGF_KLINE_LOCAL_ECHO=1` pour que le transport rejette l'écho local ; la
couche session ne voit alors que l'écho renvoyé par l'ECU.

## Activation et configuration de compilation

La source ECU reste le simulateur par défaut (`MGF_USE_MEMS_KLINE=0`). Activer
explicitement le matériel réel et, si nécessaire, modifier les ressources :

```bash
idf.py -D MGF_USE_MEMS_KLINE=1 \
       -D MGF_KLINE_TX_GPIO=40 -D MGF_KLINE_RX_GPIO=38 build
```

Les valeurs sont des variables CMake persistantes du composant. Les options
supportées sont `MGF_USE_MEMS_KLINE`, `MGF_KLINE_UART_NUM`,
`MGF_KLINE_TX_GPIO`, `MGF_KLINE_RX_GPIO`, `MGF_KLINE_LOCAL_ECHO` et
`MGF_MEMS_VARIANT`. Le header vérifie au préprocesseur les conflits avec les
broches LCD/I²C et les ressources réservées de la T-RGB.

Variante ECU par `MGF_MEMS_VARIANT` : `MEMS_VARIANT_1_9` par défaut (MGF
1995-1999, réveil 5 bauds), `MEMS_VARIANT_1_6` pour un handshake direct.
Les MGF 2000+ et MG TF utilisent le **MEMS 3**, protocole différent non
supporté par ce firmware.

À la connexion, `mems_ecu` lance une tâche FreeRTOS qui :
1. tente le handshake, retente toutes `reconnect_delay_ms` en cas d'échec ;
2. une fois connectée, exécute un cycle de poll **périodique** toutes
   `poll_period_ms` (100 ms, `xTaskDelayUntil` : la durée des échanges est
   absorbée dans la période ; un cycle qui déborde recale la référence au lieu
   de rattraper en rafale). Chaque cycle interroge `0x80` (régime, ~30 ms à
   9600 bps) ; `0x7D` (grandeurs lentes, ~35 ms) n'est interrogée qu'**un cycle
   sur 3** (`MEMS_SESSION_DEFAULT_7D_INTERVAL`, réglable par
   `mems_session_set_7d_interval`). Entre deux lectures, les champs `0x7D`
   gardent leur dernière valeur valide ; une `0x7D` perdue compte comme trame
   perdue et reste due au cycle suivant. Après (re)connexion, l'instantané est
   oublié : le premier cycle lit toujours `0x80` **et** `0x7D`, aucun champ
   d'une session précédente n'est republié ;
3. publie le dernier instantané derrière un mutex ; une trame perdue isolée
   conserve le dernier instantané, et seuls `MEMS_SESSION_MAX_POLL_FAILURES` (3)
   échecs consécutifs repassent en reconnexion (valeurs affichées « -- »).
   Les octets tardifs sont purgés avant chaque commande pour ne pas décaler
   les trames suivantes. Un instantané connecté âgé de 1,5 s ou plus reste
   présenté comme indisponible (`domain/ecu_freshness.h`).

## Limites connues

- **Température d'huile** : MEMS 1.6/1.9 n'a pas ce capteur → `ecu_data_t.oil_temp`
  vaut `NAN` et la cellule « huile » affiche `--`. À remplacer par un capteur
  externe ou par une donnée MEMS réelle (temp. air, pression collecteur…).
- **% de gaz** : MEMS ne fournit pas de pourcentage ; estimé linéairement depuis
  la tension du potentiomètre papillon (0,6 V fermé → 4,6 V plein gaz).
- **Non validé sur matériel** : ESP-IDF absent de l'environnement de build. Le
  décodage et l'enchaînement réveil/handshake sont en revanche testés sur hôte
  (`test/test_mems.c`) contre les trames de référence du projet Go.
- **Slow init 5 bauds (1.9)** : le timing (200 ms/bit) est bit-bangé via le GPIO
  TX, sur échéances absolues : sommeil FreeRTOS jusqu'à ~1 tick avant
  l'échéance puis attente active (`esp_rom_delay_us`), donc indépendant de
  `CONFIG_FREERTOS_HZ` (testé sur hôte à 1 kHz et 100 Hz). Une erreur GPIO en
  cours de bit-bang remet la ligne au repos et ré-attache le TX à l'UART ; à vérifier à l’oscilloscope sur la carte, et selon le transceiver
  (inversion éventuelle des niveaux). Les octets de synchronisation attendus
  (`55 76 83`) sont maintenant vérifiés avec un timeout de 1 s ; une absence
  d’ECU ou une réponse invalide laisse la source déconnectée.
- **Init 0x7C** : la variante d'init alternative (`0x7C`/`0xE9`) présente dans le
  projet Go n'est pas portée (chemin non utilisé par leur `Connect`).
