# RTC DS3231 optionnelle (architecture LILYGO)

Le support DS3231 est ajouté comme une fonction optionnelle pour la LILYGO
T-RGB H597 : adresse I²C `0x68`, SDA GPIO8 et SCL GPIO48. `INT/SQW` n'est pas
utilisée.

## Contrat logiciel

- `main/domain/rtc.h` expose `rtc_probe`, `rtc_read`, `rtc_set` et les dates
  validées (`2000..2099`, calendrier grégorien).
- `main/domain/rtc_time.c` contient le codec BCD et la validation sans
  dépendance ESP-IDF ; `test/test_rtc.c` l'exerce sur l'hôte.
- `main/infrastructure/rtc_ds3231.c` traduit ce contrat vers les registres
  DS3231.
- Le bit OSF (`STATUS 0x0F`, bit 7) et les champs BCD/date invalides refusent
  une lecture. `rtc_set` écrit l'heure puis efface OSF.

Une RTC absente, inaccessible, arrêtée ou non réglée ne fait pas échouer le
firmware. L'écran horloge utilise l'heure système en repli et conserve une
valeur cohérente si aucune source n'est valide.

## Bus I²C partagé

Le driver RTC ne fait jamais `i2c_driver_install()` et ne détruit jamais le
bus. `board_display_i2c_bus()` fournit une vue du bus déjà initialisé par le
bring-up écran via `shared_i2c_bus_t`. Les transactions passent par le driver
ESP-IDF existant, qui sérialise les transactions legacy ; aucun second
propriétaire du port n'est créé.

Cette branche contient encore le bring-up **Waveshare** (SDA/SCL GPIO15/7).
L'application refuse donc explicitement ce bus pour le DS3231 et journalise que
la RTC attend le bus LILYGO GPIO8/48. La branche `feat/lilygo-display` doit :

1. initialiser une seule fois le bus GPIO8/48 pour tactile, expander et
   connecteur externe ;
2. implémenter `board_display_i2c_bus()` avec le même contrat, ou fournir son
   adaptateur de transaction équivalent ;
3. conserver l'adresse tactile `0x15` et expander `0x20`, puis laisser
   l'application sonder `0x68` sans considérer son absence comme fatale.

Le support RTC dépend donc de cette adaptation du display branch ; il ne doit
pas être activé en reliant directement un second pilote I²C aux GPIO.

## Matériel

Vérifier que le module DS3231 est compatible avec son élément de sauvegarde :
les modules prévus pour LIR2032 ne doivent pas recevoir une CR2032 non
rechargeable. Vérifier aussi que les pull-ups sont vers 3,3 V.

Commandes hôte :

```sh
./test/run_tests.sh
./build.sh
```

La compilation ESP-IDF et la validation sur LILYGO restent à faire avec la
révision matérielle et le profil d'écran confirmés.
