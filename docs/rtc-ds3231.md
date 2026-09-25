# RTC DS3231 optionnelle (LILYGO)

Le DS3231 utilise l'adresse I²C `0x68`, SDA GPIO8 et SCL GPIO48, sur le bus
partagé avec le tactile et l'expander de la LILYGO T-RGB H597. `INT/SQW` n'est
pas utilisée. Son absence ne bloque pas le démarrage.

## Contrat logiciel

- `main/domain/rtc.h` expose le contrat synchrone `rtc_probe`, `rtc_read` et
  `rtc_set`. Les dates valides couvrent 2000–2099 et sont toujours en **UTC**.
- `main/domain/rtc_time.c` valide les dates et réalise les conversions sans
  dépendance ESP-IDF ; les tests hôte couvrent ce contrat.
- `main/infrastructure/rtc_ds3231.c` traduit ce contrat en transactions de
  registres. Le bit OSF, des bits réservés incorrects ou une date invalide
  rendent la lecture indisponible. Une écriture valide efface OSF.
- `main/infrastructure/rtc_worker.c` possède les accès RTC après le démarrage :
  lectures et écritures I²C s'exécutent dans sa tâche, **jamais dans LVGL**.
  La RTC est empruntée ; le worker doit être arrêté avant `rtc_destroy` et
  avant la destruction du bus écran.

## Affichage et synchronisation

Le worker initialise l'heure système à partir d'une RTC valide, puis la
resynchronise périodiquement. Les erreurs de lecture n'arrêtent pas une
horloge système déjà synchronisée. L'écran horloge lit uniquement cette
heure système ; sans heure valide, les aiguilles sont masquées et le message
« HEURE / NON SYNCHRONISEE » apparaît. Aucune heure arbitraire n'est affichée.

Le DS3231 et l'heure système restent en UTC. L'affichage applique le décalage
`utc_offset_minutes` persisté, reçu du téléphone (fuseau et heure d'été).
Sans nouveau décalage fourni par le téléphone, la jauge ne calcule pas
elle-même les transitions saisonnières.

Les commandes téléphone mettent l'heure système à jour immédiatement et
soumettent une copie de la date UTC au worker. La file est bornée et la
commande la plus récente remplace celle en attente. Une erreur d'écriture
RTC est journalisée et retentée hors de la tâche UI.

Pour la caractéristique BLE `datetime`, seules les dates `UTC` sont acceptées ;
`LOCAL` est refusé. Un succès signifie **commande acceptée dans la file**,
pas écriture matérielle déjà terminée. Aucun accusé de persistance différé
n'est défini par ce protocole. Sans worker RTC disponible, cette caractéristique
refuse l'écriture ; la synchronisation système par l'application compagnon
reste utilisable sans DS3231.

## Bus partagé

`board_display_i2c_bus()` fournit une vue du bus initialisé par le bring-up
LILYGO. Le driver RTC ne crée ni ne détruit ce bus. Le driver I²C maître ESP-IDF
sérialise les transactions ; aucun deuxième propriétaire des GPIO n'est créé.
Le tactile conserve l'adresse `0x15`, l'expander `0x20`.

## Matériel et validation

Vérifier que le module est compatible avec sa pile : un circuit de charge
prévu pour LIR2032 ne doit pas charger une CR2032 non rechargeable. Les
pull-ups doivent être reliées au 3,3 V.

```sh
bash test/run_tests.sh
./build.sh
tools/format.sh check
```

La validation sur carte reste nécessaire : démarrage sans RTC, OSF actif,
perte du périphérique après synchronisation, commandes téléphone successives,
contention avec le tactile et comportement sous coupure d'alimentation.
