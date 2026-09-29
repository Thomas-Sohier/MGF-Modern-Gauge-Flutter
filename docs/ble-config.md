# Lien Bluetooth (NimBLE) : réglages et application compagnon

La jauge embarque un lien BLE permanent, activé par défaut
(`MGF_ENABLE_BLE_CONFIG=1`, `CONFIG_BT_NIMBLE_ENABLED=y` dans
`sdkconfig.defaults`). Seuls le contrôleur BLE et l'hôte **NimBLE** d'ESP-IDF
sont utilisés : l'ESP32-S3 n'a pas de Bluetooth classique. Deux services GATT
cohabitent sur la même connexion :

| Service | UUID | Rôle |
|---|---|---|
| Réglages (protocole binaire v1) | `0111b043-1870-448e-2c4d-6a913172549a` | luminosité, page, thème, unités ; date UTC |
| Compagnon (JSON) | `7f3a0001-9c44-4e6b-8d2a-5b1f00000001` | application Android `rover-mems-ecu-companion` : musique, navigation, télécommande, heure |

Coût mémoire mesuré (ESP-IDF 5.4.2) : ~36 Ko de DIRAM statique, plus le tas
alloué par NimBLE au démarrage ; DIRAM ~33 % au total. `tools/check_iram.sh`
contrôle la marge.

```bash
idf.py build                                   # BLE inclus
idf.py -D MGF_ENABLE_BLE_CONFIG=0 build        # BLE retiré du code (garder
                                               # un sdkconfig sans BT pour
                                               # récupérer la mémoire)
```

> Un `sdkconfig` ou un `build/` générés avant l'activation du BLE ne sont pas
> réécrits par les defaults : supprimer `sdkconfig` (ou `idf.py fullclean`),
> et passer une fois `-D MGF_ENABLE_BLE_CONFIG=1` si `build/CMakeCache.txt`
> contient encore `MGF_ENABLE_BLE_CONFIG=0`.

Le simulateur hôte et les tests ne compilent pas NimBLE ; le décodage du
protocole compagnon (`domain/companion_protocol.c`, `domain/flat_json.c`) est
en C pur et testé sur hôte (`test/test_companion.c`).

## Politique de connexion et d'appairage

- La jauge **annonce en permanence** (UUID 128 bits du service compagnon dans
  l'annonce, nom « MGF Gauge » dans la réponse de scan) dès qu'aucune
  connexion n'est active : le téléphone appairé se reconnecte ainsi seul au
  démarrage de la voiture. Une seule connexion à la fois
  (`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`), rôle périphérique uniquement.
- À la connexion, la jauge demande la sécurité. Un téléphone **appairé**
  chiffre le lien avec sa clé ; tout lien non chiffré avec une clé stockée
  (inconnu, clé perdue, simple scanner) est **coupé après 10 s**, ou dès
  l'échec du chiffrement.
- **Fenêtre d'appairage** : c'est le seul moment où un nouveau bond peut être
  créé. On l'ouvre depuis l'écran Réglages (maintien ~1 s sur une page) →
  Bluetooth → **APPAIRER**, pour 5 minutes (`domain/ble_window.c`). Hors
  fenêtre, `ble_hs_cfg.sm_bonding = 0` : un appairage ne produit aucune clé
  stockée, le lien est refusé et aucun bond existant n'est évincé. La fenêtre
  se referme dès qu'un téléphone appairé est lié, à l'échéance, ou par un
  second appui. La fermeture ne coupe pas le téléphone déjà lié.
- Un téléphone qui a « oublié » la jauge alors qu'elle garde sa clé
  (`REPEAT_PAIRING`) ne peut se ré-appairer que pendant la fenêtre.
- Bonds conservés en NVS (`CONFIG_BT_NIMBLE_NVS_PERSIST=y`,
  `CONFIG_BT_NIMBLE_MAX_BONDS=3`) : sans persistance, le téléphone devrait se
  ré-appairer à chaque démarrage. (Avant ce changement, l'option était absente
  et les bonds ne survivaient pas à un redémarrage.)
- Toutes les écritures des deux services exigent un lien **chiffré et
  appairé** ; la lecture des réglages reste libre (aucun secret).
- `MGF_BLE_CONFIG_OPEN_ON_BOOT=1` ouvre la fenêtre au démarrage, sans
  échéance : réservé à une image de banc.
- Pairage **Just Works** (ni écran de code ni clavier) : un attaquant présent
  pendant la fenêtre pourrait s'appairer à la place du téléphone. La fenêtre
  courte et déclenchée localement limite ce risque.
- `ble_config_service_forget_bonds()` supprime tous les bonds (fenêtre fermée,
  aucune connexion) ; chaque téléphone devra se ré-appairer.

### Diagnostic d'un plantage à la connexion sur carte réelle

Le lien n'ayant pas encore été validé sur matériel, un plantage au moment où
l'application appuie sur « connecter » doit d'abord être **localisé**. Depuis
le moniteur série (`idf.py monitor`), relever la trace `ble_config` /
`companion` au moment de la connexion et la lire dans cet ordre :

| Trace attendue | Si elle s'arrête avant… | Conclusion |
|---|---|---|
| `host synced (addr type N)` | rien | pile démarrée, annonce prête |
| `connect: handle=N` puis `security initiated` | si `status≠0` → `connect failed (status=N), resuming advertising` | la connexion n'a jamais abouti (radio/transport) → c'est le côté téléphone, pas un crash ESP32 |
| `pairing opened` ou `encryption established (bonded phone)` | si `onEncryption failed` → `link security failed (status=…), closing link` | l'appairage/chiffrement échoue (clé perdue des deux côtés ou fenêtre fermée) → sur un appareil vu comme non-bondé, les écritures sont rejetées avec une erreur GATT `0x05`/`0x06`, sans plantage du firmware mais avec un abandon côté Android |
| `characteristic read` / `write accepted` (service compagnon/réglages) | rien | flux nominal |
| `disconnect reason=…` | — | la table des codes se trouve dans `components/bt/host/nimble/nimble/nimble/host/docs` (installation IDF) ; `0x13` = coupure initiée depuis le téléphone (appairage annulé, típique), `0x08`/`0x3E` ferme le lien sans crash |

Deux cas n'étant pas un crash firmware sont déjà couverts côté Android
(`HeadUnitGattClient`) : le cache GATT rafraîchi par réflexion quand une
caractéristique manque après une mise à jour du firmware, et le
`createBond()` quand Android n'a pas initié l'appairage. Un « plantage »
ressenti peut donc être un simple abandon de connexion (l'app trouve la
jauge, la découvre, mais le lien saute avant la première écriture) : la
trace distingue immédiatement les deux cas.

En revanche, si **aucun log ESP32 n'apparaît du tout** au moment du
« plantage », il est côté application Android (remonter le `AppLogHub` de
l'app). Un `Guru Meditation` sur `nimble_host_task` ou un dépassement de pile
de la tâche `bt` du contrôleur pointent plutôt la pile hôte que la logique
applicative — d'où la marge volontaire sur les tailles de tâches BT.

## Service compagnon (`infrastructure/companion_gatt.c`)

Contrat repris de l'ancien boîtier Linux/Go (cf. README et ADR 0007 de
l'application). Le téléphone écrit du JSON UTF-8 (≤ 512 octets) ; la tâche
NimBLE le décode et dépose le dernier état dans une boîte aux lettres que le
timer LVGL (`companion_timer_tick`, 200 ms) consomme. Aucun appel LVGL dans
la tâche NimBLE.

| Car. | Contenu | Usage sur la jauge |
|---|---|---|
| `…0002` | `{title, artist, album, state, position_ms, duration_ms, art_id}` | écran Musique ; position extrapolée localement pendant la lecture |
| `…0003`/`…0004` | pochette (contrôle JSON / morceaux binaires) | réassemblée en PSRAM puis décodée (tjpgd) en image ambre monochrome par `companion_art_worker`, hors LVGL |
| `…0005` | `{active, instruction, distance, eta, maneuver_icon_id}` | écran Navigation ; flèche déduite du texte (FR/EN), consigne découpée en manœuvre + voie, distance en valeur + unité |
| `…0006`/`…0007` | icône de manœuvre PNG | acceptée, ignorée |
| `…0008` | alerte `{app, title, text, posted_at}` | acceptée, ignorée |
| `…0009` | `{type:"nav_key", key}` | `next`/`right` et `previous`/`left` changent de page ; `ok`/`back` ferment les réglages |
| `…000b` | `{epoch_ms, tz_offset_min}` | RTC DS3231 + horloge système en UTC ; décalage (fuseau + été) persisté et appliqué à l'horloge |

Les textes sont normalisés pour Michroma : capitales ASCII, accents repliés
(« Arrivée » → « ARRIVEE »), ponctuation typographique simplifiée, emoji
retirés. Un JSON invalide est refusé avec une erreur ATT (l'application
réessaie au plus trois fois). Le lien est à sens unique : la jauge n'envoie
rien au téléphone.

Pochette : `…0003` valide `art_id`, `total_bytes` (≤ 256 Kio) et
`chunk_count` (≤ 2048) avant d'allouer un tampon PSRAM. Les chunks `…0004`
(index big-endian 2 octets + données) sont assemblés séquentiellement ;
index hors bornes, doublon, chunk manquant/avant le 0, longueur incohérente
ou dépassement abandonnent le transfert. Une pochette complète est publiée
puis décodée hors LVGL (`companion_art_worker`, tjpgd via `LV_USE_TJPGD`) en
image ambre monochrome ; l'ancienne image est remplacée proprement et le
cadrage « cover » remplit un large rectangle rogné par la racine ronde. Une
nouvelle connexion purge tout transfert en cours.

Perte du lien : l'écran Musique affiche « TELEPHONE / NON CONNECTE » et le
guidage est effacé.

## Service de réglages (protocole binaire v1)

| Caractéristique | UUID | Accès |
|---|---|---|
| settings | `0211b043-1870-448e-2c4d-6a913172549a` | read, encrypted write |
| datetime | `0311b043-1870-448e-2c4d-6a913172549a` | encrypted write |

Les octets sont little-endian et chaque payload commence par la version `1`.

- `settings` : 5 octets `[version, brightness, page, theme, units]`. La page
  de démarrage et le décalage horaire ne sont **pas** dans ce format : une
  écriture BLE conserve les valeurs locales (`app_settings_merge_ble_v1`).
- `datetime` : 10 octets `[version, basis, year_lo, year_hi, month, day,
  weekday, hour, minute, second]`. `basis` vaut `0` pour UTC et `1` pour
  local ; `LOCAL` est refusé à la frontière RTC (le DS3231 stocke de l'UTC).

Le parseur est indépendant d'ESP-IDF et testé sur l'hôte. Les valeurs
inconnues, versions inattendues, longueurs incorrectes et dates invalides sont
refusées avant tout callback ou écriture. Les réglages acceptés sont appliqués
par le timer LVGL ; après l'anti-rebond de 3 s, un worker effectue la persistance
NVS hors de la tâche UI. Le coordinateur ne considère le snapshot sauvegardé
qu'après confirmation de l'écriture effective.

Pour `datetime`, un succès signifie que la commande UTC a été acceptée dans
la file du worker RTC, pas que le DS3231 a déjà été écrit. L'heure système est
mise à jour immédiatement ; les erreurs et reprises I²C sont journalisées par
le worker. Le protocole ne fournit pas d'accusé de persistance RTC différé.

Le lien n'a pas encore été validé sur carte réelle : pairage, reconnexion d'un
bond après coupure, écritures longues (JSON > MTU) et comportement avec
l'application restent à tester.
