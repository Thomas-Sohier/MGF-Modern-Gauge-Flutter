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
NimBLE et ne dépendent donc pas de `bt`.

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
  paramètres passe par `settings_store_*`, et le callback RTC passe par
  `rtc_set`; le service ne possède donc ni NVS ni bus I²C.
- Le DS3231 ne stocke pas de fuseau horaire. Le champ UTC/local est transmis au
  callback pour une politique future, mais l'intégration actuelle écrit la
  valeur civile fournie telle quelle.

Le support n'a pas encore été validé sur carte réelle ; il faut tester le
pairage, la reconnexion d'un bond et les révisions NimBLE/ESP-IDF utilisées par
la carte.
