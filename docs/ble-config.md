# Configuration BLE optionnelle

Le service de configuration BLE est désactivé par défaut. Il utilise uniquement
le contrôleur BLE et l'hôte **NimBLE** d'ESP-IDF (aucun profil Bluetooth
Classic). Pour l'activer sur le firmware ESP-IDF :

```bash
idf.py -D MGF_ENABLE_BLE_CONFIG=1 set-target esp32s3
# activer CONFIG_BT_ENABLED et CONFIG_BT_NIMBLE_ENABLED dans menuconfig
idf.py -D MGF_ENABLE_BLE_CONFIG=1 build
```

Le simulateur hôte et les builds firmware par défaut ne compilent pas le fichier
NimBLE et ne dépendent donc pas de `bt`. Avec NimBLE, l'ouverture de la
publicité au boot est contrôlée séparément par
`MGF_BLE_CONFIG_OPEN_ON_BOOT=1` (désactivée par défaut) ; les écritures restent
limitées aux clients chiffrés et bondés.

Dans `sdkconfig.defaults`, la politique fixe également
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` et `CONFIG_BT_NIMBLE_MAX_BONDS=3`.

## Politique d'ouverture

Le service peut être initialisé mais reste **fermé et non annonçant par défaut**.
`ble_config_service_open()` doit être appelé après une action locale explicite
(mode maintenance) ; `ble_config_service_close()` arrête les annonces et la
connexion en cours. L'option `MGF_BLE_CONFIG_OPEN_ON_BOOT=1` est réservée à une
image de banc/service contrôlée.

> TODO produit : le déclencheur local de ce mode (séquence tactile, bouton ou
> autre procédure) n'est pas décidé. Aucune ouverture automatique supplémentaire
> n'est inventée ici.

## GATT et protocole v1

Service custom : `0111b043-1870-448e-2c4d-6a913172549a`.

| Caractéristique | UUID | Accès |
|---|---|---|
| settings | `0211b043-1870-448e-2c4d-6a913172549a` | read, encrypted write |
| datetime | `0311b043-1870-448e-2c4d-6a913172549a` | encrypted write |

Les octets sont little-endian et chaque payload commence par la version `1`.

- `settings` : 5 octets `[version, brightness, page, theme, units]`.
- `datetime` : 10 octets `[version, basis, year_lo, year_hi, month, day,
  weekday, hour, minute, second]`. `basis` vaut `0` pour UTC et `1` pour
  local. L'année est limitée à 2000..2099 et la date complète est validée
  avant l'appel du callback.

Le parseur est indépendant d'ESP-IDF et testé sur l'hôte. Les valeurs
inconnues, versions inattendues, longueurs incorrectes et dates invalides sont
refusées avant tout callback ou écriture.

## Sécurité et stockage

- Les propriétés GATT demandent le chiffrement pour les écritures et le
  callback vérifie en plus `encrypted && bonded` avant de parser ou d'appliquer
  une valeur.
- NimBLE est configuré avec bonding, Secure Connections et échange des clés
  d'identité/chiffrement. Une demande de sécurité est lancée à la connexion ;
  le client doit donc accepter le pairage et conserver le bond.
- Le matériel n'a pas d'écran/clavier utilisable pour confirmer un code : le
  pairage est **Just Works**, sans MITM. Un attaquant présent pendant le
  pairage initial peut donc usurper le premier client. Il faut supprimer les
  bonds non reconnus côté téléphone et re-flasher/effacer les données BLE selon
  la procédure de maintenance si nécessaire.
- La lecture de `settings` reste lisible sans chiffrement ; elle ne contient
  pas de secret. Les écritures sont la surface protégée.
- Les clés de bond sont conservées par les callbacks de stockage NimBLE/ESP-IDF
  dans sa zone NVS. Le service ne l'ouvre ni ne l'efface. Le callback de
  paramètres valide seulement la valeur ; le service la place dans son
  hand-off protégé et le timer LVGL la consomme via
  `ble_config_service_take_settings_update`, puis le runtime l'applique et la
  persiste. Le callback RTC passe par `rtc_set`; le service ne possède donc ni
  NVS applicative ni bus I²C. Une écriture de réglages peut ainsi être acceptée
  avant le commit NVS ; une panne avant le prochain tick laisse l'ancienne
  valeur persistée.
- `ble_config_service_forget_bonds()` supprime explicitement tous les bonds,
  uniquement service fermé et sans connexion active. La fermeture peut donc
  devoir être suivie d'un tick de déconnexion avant cet appel. Une demande de
  nouveau pairage d'un appareil déjà connu déclenche aussi le renouvellement de
  son bond ; aucune éviction automatique d'un autre appareil n'est faite.
  L'ouverture suivante impose alors un nouveau pairage.
- Le DS3231 stocke exclusivement UTC. Le champ `basis` reste explicite sur le
  protocole pour éviter toute ambiguïté ; `UTC` est validé puis synchronisé,
  tandis que `LOCAL` est refusé à la frontière application/RTC tant qu'aucun
  décalage de fuseau ni règle d'heure d'été n'est configuré. Une heure locale
  ne doit jamais être écrite telle quelle dans le DS3231.

Le support n'a pas encore été validé sur carte réelle ; il faut tester le
pairage, la reconnexion d'un bond et les révisions NimBLE/ESP-IDF utilisées par
la carte.
