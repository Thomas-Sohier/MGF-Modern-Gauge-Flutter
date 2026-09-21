# Rapport d’architecture — MGF Gauge LVGL

## 1. Synthèse

**Avis : bonne base de prototype, mais pas encore une architecture embarquée validée pour la production.** Le rangement en `domain`, `app`, `infrastructure` et `ui` est pertinent. La prochaine étape doit renforcer les contrats entre ces couches, supprimer le travail graphique inutile et rendre le comportement mesurable — pas multiplier les abstractions.

Les cinq priorités :

1. **Compiler et mesurer sur ESP32-S3**, notamment la mémoire statique, les buffers RGB et le rendu vectoriel du démarrage.
2. **Rendre explicite la fraîcheur des données** : une erreur de source ne doit pas laisser des valeurs anciennes paraître fiables indéfiniment.
3. **Ne mettre à jour que ce qui change visuellement** : aujourd’hui, le fond du cadran est invalidé et les valeurs sont réécrites à chaque cycle.
4. **Extraire la logique applicative de LVGL**, avec une fonction de traitement déterministe et un adaptateur de présentation minimal.
5. **Fiabiliser les builds et les tests** : cache correctement invalidé, avertissements actifs, comparaison des images sans écraser les références.

La performance maximale utile signifie ici : latence bornée, image stable, mémoire maîtrisée et marge CPU. Elle ne signifie pas atteindre le plus grand nombre de FPS au prix de la lisibilité du code.

## 2. Périmètre et limites de l’audit

Analyse des fichiers présents dans l’arbre de travail, y compris la réorganisation locale non commitée. Les modifications existantes n’ont pas été altérées.

Fichiers principaux examinés : composition `main/app_main.c`, contrôleur, domaine, source simulée, initialisation matérielle, écran ambre, widgets de valeurs, polices, écran de démarrage, simulateur, configurations et tests.

- Test exécuté : `./test/run_tests.sh` → **succès**.
- Ce test couvre l’adaptateur `ecu_source_read`, pas le firmware ni le contrôleur.
- `idf.py` n’est pas disponible dans cet environnement : **aucune compilation cible ni mesure matérielle**.
- Aucun rendu de référence n’a été régénéré : le rapport ne modifie pas l’UI.
- Aucun outil/commande dédié de lint Markdown ou de formatage n’a été identifié dans les éléments inspectés ; il est recommandé d’en ajouter.

Les coûts mémoire ci-dessous sont des calculs de dimensionnement. Les gains de performance proposés sont des hypothèses à valider, pas des résultats de benchmark.

## 3. Architecture actuelle

```text
app_main — composition et démarrage
  ├── board_display — ESP-IDF, RGB, tactile, port LVGL
  ├── ui_fonts / boot_screen / amber_screen
  ├── fake_ecu — données et timer LVGL
  └── dashboard_controller — timer LVGL, snapshot, état
        ├── ecu_source — lecture par copie
        └── amber_screen_update
              ├── dessin du cadran
              ├── amber_value — labels superposés
              └── dash_icons — primitives vectorielles
```

### Points solides à conserver

- **Domaine indépendant** de LVGL et d’ESP-IDF : `ecu_data.h`, `ecu_source.h`.
- **Lecture par copie** de l’instantané : bonne base pour isoler une future acquisition asynchrone.
- **Composition visible** dans `app_main`, sans registre global de services.
- Types opaques pour les objets applicatifs et UI ; interfaces publiques relativement petites.
- Source et écran explicitement empruntés par le contrôleur.
- Exécution actuelle des deux timers dans le contexte LVGL : pas de concurrence entre eux dans ce scénario.
- Thème et widget de valeur partagés, géométrie ambre centralisée et essentiellement déclarative.
- Styles legacy exclus de la liste des sources firmware.
- Simulateur réutilisant les sources UI de la cible.

### Limites structurelles

Le découpage est réel, mais **la couche application reste liée à la technologie graphique** : `dashboard_controller.h` inclut l’écran ambre ; son implémentation utilise `lv_timer` et `lv_malloc`. Un test du comportement applicatif entraîne donc LVGL et une présentation particulière.

Ce n’est pas dramatique pour un seul écran. En revanche, l’arrivée d’une vraie source ECU rendra utile une séparation entre **traitement des données**, **cadencement** et **rendu**.

## 4. Constats prioritaires

| Priorité | Constat et preuve | Conséquence | Recommandation |
| --- | --- | --- | --- |
| P0 | `boot_screen.c` réserve un buffer statique ARGB8888 de 240 × 240 | Environ 225 Kio persistants, même après destruction de l’écran de boot ; placement mémoire cible à vérifier | Lire la map du firmware ; préférer des primitives directes pour ce logo simple, ou une allocation temporaire explicitement placée et libérée |
| P0 | Firmware non compilé dans l’environnement ; dépendances à plages ouvertes | Compatibilité des API et configuration vectorielle non démontrées | Fixer une combinaison IDF/composants, produire un build reproductible, puis valider sur carte |
| P1 | Sur échec de lecture, `dashboard_controller_tick` conserve l’image sans prévenir l’écran | Données anciennes potentiellement présentées comme actuelles, y compris le statut OBD | Propager état et fraîcheur à la présentation ; conserver la dernière valeur seulement avec indication explicite |
| P1 | `amber_screen_update` invalide systématiquement son objet de dessin plein écran | Travail de rendu potentiellement important même à données constantes | Comparer l’état visible ; isoler cadran, décor et valeurs ; mesurer ensuite les régions invalidées |
| P1 | `amber_value_widget_set` réécrit deux labels et recalcule leur placement à chaque appel | Formatage, opérations sur buffers de texte et layout répétés sans changement utile | Comparer le texte affiché ; centrer dans une zone stable ; éviter le layout forcé en régime permanent |
| P1 | Contrôleur directement dépendant de LVGL et de `amber_screen_t` | Tests et évolution de la logique compliqués | Extraire une étape applicative appelable sans LVGL ; garder le timer comme adaptateur |
| P1 | Cache de `build.sh` fondé sur la seule existence des `.o` LVGL | Modification de configuration, headers ou options potentiellement ignorée | Utiliser un graphe de build avec dépendances et configuration explicites |
| P1 | `build.sh` compile avec `-w` ; génération écrasant les goldens | Diagnostics masqués ; une nouvelle image ne prouve aucune non-régression | Activer les warnings du code projet ; distinguer génération candidate, comparaison et acceptation |
| P2 | Politique d’erreur matérielle mêlant `ESP_ERROR_CHECK`, retours et erreurs ignorées | Contrat de démarrage difficile à comprendre ; repli parfois inaccessible | Séparer échec fatal et fonction optionnelle ; documenter le nettoyage et la politique de reprise |
| P2 | README et CLAUDE conservent des chemins antérieurs au déplacement des fichiers | Navigation et compréhension ralenties | Synchroniser la documentation avec l’arborescence réelle |

P0 : à lever avant de conclure à la viabilité matérielle. P1 : nécessaire avant intégration d’une vraie source. P2 : consolidation de maintenance.

## 5. Architecture cible recommandée

### 5.1 Une séparation légère, pas un framework

```text
Acquisition ECU, tâche dédiée si nécessaire
  └── publication du dernier instantané cohérent
        └── ecu_source.read — copie rapide, non bloquante
              └── traitement applicatif — état, validité, fraîcheur
                    └── présentation ambre — conversion vers l’état visible
                          └── widgets LVGL — rendu différentiel
```

`app_main` reste le point de composition. Le timer LVGL déclenche le traitement et la présentation ; il ne doit jamais attendre une transaction OBD, réseau ou série.

Organisation possible, à atteindre progressivement :

```text
main/
  app_main.c
  domain/            modèle ECU et contrat de lecture
  app/               état dashboard, règles, traitement déterministe
  infrastructure/    carte, acquisition réelle, source simulée
  ui/
    screens/         boot et ambre
    widgets/         valeur et cadran si extraction utile
    icons/
    fonts/
    themes/
    adapters/        liaison timer LVGL → application → écran
sim/                 plateforme hôte et scénarios
test/                tests unitaires, intégration et références visuelles
```

Le dossier `adapters` n’est justifié que s’il contient une vraie responsabilité. Un fichier de liaison bien nommé peut suffire initialement. De même, pas besoin d’un composant CMake par petit fichier : commencer par une bibliothèque logique testable sans LVGL et une liste explicite des sources partagées.

### 5.2 Contrats à rendre explicites

**Source ECU**

- Lecture par copie, rapide et non bloquante.
- Garantie d’un snapshot cohérent si plusieurs tâches interviennent.
- Durée de validité du contexte et politique d’échec documentées.
- Horodatage monotone de réception, ou métadonnées équivalentes.
- Distinction entre transport connecté, donnée disponible, donnée valide et donnée fraîche.

Le booléen actuel suffit au mock, pas à qualifier une donnée automobile réelle. Ajouter les métadonnées réellement nécessaires ; éviter une collection d’états spéculatifs.

**Application**

- Une étape de traitement reçoit un instantané/résultat de lecture et le temps courant.
- Elle détermine les transitions : démarrage, connecté, déconnecté, erreur, périmé selon les besoins retenus.
- Elle ne connaît ni objet LVGL, ni couleurs, ni géométrie.
- Elle ne fait pas d’I/O bloquante et reste testable avec une horloge simulée.

**Présentation**

- Convertit les valeurs métier en textes, nombre de segments et indicateurs de qualité.
- Compare l’état visible précédent avec le nouveau.
- Garde les règles de précision d’affichage dans un seul endroit.
- Seule cette couche et ses widgets manipulent LVGL.

Une interface de présentation par callback peut être utile ; elle n’est pas obligatoire si l’adaptateur appelle simplement le traitement puis l’écran. Éviter une table de fonctions pour chaque objet.

### 5.3 Concurrence et durée de vie

Aujourd’hui, la copie dans `fake_ecu_read` est sûre parce que la simulation et sa consommation sont exécutées dans le même contexte LVGL. **Une copie de structure n’est pas intrinsèquement thread-safe** avec un producteur concurrent.

Pour la future acquisition : une boîte aux lettres contenant le dernier snapshot, protégée brièvement, ou une queue FreeRTOS de longueur 1 avec remplacement, constitue un point de départ simple. Ne pas accumuler les anciens régimes si seul le dernier est utile à l’affichage.

Règles conseillées :

- Une seule tâche propriétaire des objets UI.
- Acquisition indépendante de LVGL.
- Aucun verrou de transport gardé pendant un rendu.
- Lectures des états/compteurs du contrôleur soumises au même contrat de synchronisation.
- Arrêt : désactiver les callbacks consommateurs avant de détruire leurs dépendances.

## 6. Performance : optimiser dans le bon ordre

### 6.1 Premier levier : éviter les mises à jour inutiles

Le cycle applicatif actuel est de **40 ms, soit 25 Hz**. À chaque succès, quatre valeurs numériques sont formatées et cinq widgets sont mis à jour, chacun possédant deux labels.

Le cadran n’a pourtant que 27 états de remplissage possibles : de 0 à 26 barres éclairées. Une variation de RPM ne change pas systématiquement ce remplissage. Les températures et la batterie varient encore moins souvent à la précision affichée.

Ordre recommandé :

1. Comparer les chaînes affichées avant toute écriture LVGL.
2. Ne redessiner le cadran que si son nombre de barres change.
3. Séparer les séparateurs statiques du composant dynamique.
4. Éviter les appels répétés à `lv_obj_update_layout` grâce à des zones de texte stables.
5. Seulement si les mesures le justifient, invalider les bornes des segments modifiés plutôt que tout le cadran.

Attention : LVGL peut déjà court-circuiter certaines opérations, et le clipping influence le coût réel. Les appels redondants sont observables dans le code ; leur gain de suppression doit être mesuré.

Le fait de séparer les objets ne garantit pas que le décor ne sera jamais redessiné : il peut croiser une région invalidée. Il réduit surtout les invalidations globales et rend le comportement plus prévisible.

### 6.2 Budget mémoire et bande passante

| Élément | Calcul | Volume indicatif |
| --- | --- | --- |
| Une image RGB565 480 × 480 | 480 × 480 × 2 | 460 800 octets, 450 Kio |
| Deux framebuffers RGB | 2 × 460 800 | 900 Kio |
| Deux buffers LVGL plein écran, s’ils sont distincts | 2 × 460 800 | 900 Kio supplémentaires |
| Buffer logo ARGB8888 | 240 × 240 × 4 | 230 400 octets, 225 Kio |
| Un bloc bounce de 10 lignes RGB565 | 480 × 10 × 2 | 9 600 octets ; nombre effectif à vérifier |

`board_display.c` configure deux framebuffers RGB et demande du double buffering LVGL plein écran. **Ne pas additionner automatiquement ces demandes comme des allocations distinctes** : le port RGB peut réutiliser des buffers selon sa version et son mode anti-tearing. Examiner son implémentation résolue et mesurer les heaps après chaque étape.

Une écriture complète à 25 Hz représente déjà **11,52 Mo/s** de pixels RGB565, hors lecture de scanout, copies, blending, accès aux glyphes et contention. Le scanout RGB continue même si l’UI ne change pas ; limiter les invalidations économise surtout le travail CPU et les accès mémoire liés au rendu.

Avec les timings configurés, la fréquence théorique de scanout est proche de `16 000 000 / (518 × 518) ≈ 59,6 Hz`. Ce n’est ni le débit réel de rendu ni une validation des timings du panneau.

Priorité : map mémoire, heap interne libre, plus grand bloc libre, PSRAM utilisée, piles et stabilité RGB sous charge. **8 Mo de PSRAM n’annulent pas les contraintes de SRAM interne et de bande passante.**

### 6.3 Boot : coût élevé pour une géométrie simple

Le logo de `boot_screen.c` est une collection de segments droits, mais mobilise un canvas ARGB8888 et l’API de chemins vectoriels.

Préférence : dessiner ces segments directement avec les primitives déjà utilisées ailleurs. Cela préserverait une source entièrement vectorielle, avec moins de mémoire persistante et moins de dépendances graphiques.

Autres points :

- Les `#define LV_USE_*` locaux avant les includes ne remplacent pas une configuration cohérente de toute la bibliothèque LVGL.
- La prise en charge effective du rendu des chemins vectoriels doit être validée dans le build cible, pas seulement l’exposition de son API.
- Le buffer statique est partagé entre les instances : assumer/documenter un seul écran de boot ou changer sa propriété.
- Le rétroéclairage est activé avant le premier rendu dans `app_main` : le différer jusqu’à une image valide peut éviter un flash initial.

### 6.4 Polices et faux gras

`ui_fonts.c` crée trois fontes tiny_ttf globales. Le faux gras double les objets texte et leurs opérations de rendu.

Ne pas supprimer ce choix esthétique à l’aveugle. Comparer :

- tiny_ttf actuel, avec observation des caches et des allocations ;
- fonte LVGL précompilée avec le sous-ensemble des glyphes nécessaires ;
- préchauffage des glyphes au démarrage si tiny_ttf est conservé.

Une fonte précompilée peut réduire les coûts à l’exécution, mais doit être acceptée au regard de l’exigence vectorielle et de la fidélité visuelle. Ce n’est pas une recommandation de remplacer le cadran par une image.

L’initialisation doit signaler un succès complet ou un mode de repli : actuellement, un échec partiel peut laisser certaines fontes nulles et `ui_font_xl` empêche ensuite une nouvelle tentative. Documenter également leur durée de vie globale.

### 6.5 Cadencement et mesures

Trois fréquences coexistent : acquisition simulée 25 Hz, contrôleur 25 Hz, rafraîchissement LVGL configuré à 30 ms, soit environ **33,3 Hz**. Deux timers de même période ne constituent pas un contrat d’ordre acquisition → présentation.

La simulation avance avec un pas fixe de 40 ms, pas le temps réellement écoulé : sous charge, elle ralentit par rapport à l’horloge. Choisir explicitement simulation déterministe ou simulation temps réel.

Mesures à collecter avant toute optimisation avancée :

- Durées du traitement, de la préparation UI, du rendu et du flush.
- Latence réception ECU → présentation, avec distinction entre flush et image réellement affichée.
- P50/P95/P99 et maximum, pas uniquement FPS moyens.
- Surface invalidée et nombre de rendus à données constantes.
- Heap minimum, fragmentation, croissance des caches et marge de pile.
- Stabilité avec acquisition active et activités réseau/flash si elles font partie du produit.

Point de départ proposé : préserver 25 Hz avec une marge significative dans les 40 ms ; n’augmenter la fréquence qu’après mesure. Pas de fixed-point généralisé, de double cœur dédié au rendu ou de caches bitmap massifs sans preuve de besoin.

## 7. Fiabilité et lisibilité humaine

### 7.1 Ne jamais confondre dernière valeur et valeur actuelle

`DASHBOARD_STATE_ERROR` est conservé dans le contrôleur mais n’est pas envoyé à l’écran. Une série d’échecs laisse donc l’UI inchangée.

Définir une politique visible : dernière valeur accompagnée d’un indicateur de péremption, ou remplacement par `--` selon la métrique. Les seuils de fraîcheur doivent venir des besoins ECU, pas d’une constante choisie arbitrairement dans le widget.

Valider aussi `NaN`, infinis et valeurs hors plage avant le calcul de segments et le formatage. Le clamp graphique du RPM ne constitue pas une validation métier et ne garantit pas un traitement approprié de `NaN`.

### 7.2 Propriété des objets

L’écran ambre crée ses objets directement sous le parent fourni, contrairement au boot qui possède une racine dédiée. Cela disperse la destruction et rend la suppression externe du parent dangereuse si les wrappers sont ensuite réutilisés ou détruits.

Recommandation : racine privée pour les objets de chaque écran, contrat explicite de destruction des wrappers et arrêt des timers avant suppression. La suppression automatique de l’arbre LVGL ne libère pas automatiquement les structures C externes.

L’absence d’arrêt global en fin de `app_main` n’est pas en soi une fuite : l’application est destinée à vivre jusqu’au redémarrage. En revanche, les chemins d’échec partiel et une future recréation d’écran doivent être testables.

### 7.3 Politique d’erreur matérielle

`board_display_start` semble offrir un retour d’échec, mais plusieurs étapes appellent `ESP_ERROR_CHECK`, qui interrompt le programme en cas d’erreur. Le tactile est partiellement traité comme optionnel, alors que sa création d’IO utilise aussi ce mécanisme fatal.

Décider explicitement :

- Écran essentiel : arrêt contrôlé ou redémarrage documenté.
- Tactile facultatif : avertissement et poursuite sans tactile, si le produit le permet.
- Initialisation partielle : libération des ressources acquises si une reprise est prévue.
- Vérification des retours de configuration expander, GPIO et ajout du tactile selon leur criticité.

### 7.4 Conventions de code

- Garder les fonctions courtes et les responsabilités explicites ; éviter le morcellement en dizaines de helpers triviaux.
- Unifier indentation, placement des accolades et langue des commentaires.
- Nommer selon le rôle : l’objet `canvas` du cadran ambre est un objet custom de dessin, pas un `lv_canvas` avec framebuffer.
- Centraliser la transformation du repère 320 si plusieurs widgets en dépendent, sans créer un moteur de layout généraliste.
- Retirer constantes inutilisées et commentaires périmés, notamment `bw`, `bh`, `cut` dans le logo.
- Documenter unités, validité, thread d’appel et propriété dans les headers.
- Garder les allocations au démarrage lorsque c’est raisonnable ; viser un fonctionnement stable sans churn évitable, plutôt qu’interdire dogmatiquement tout `malloc`.

Le thème, les icônes et les primitives de dessin sont déjà suffisamment séparés pour ce projet. Leur créer des interfaces abstraites supplémentaires apporterait peu.

## 8. Builds, tests et reproductibilité

### 8.1 Build

Le simulateur clone LVGL `v9.2.2`, alors que le firmware accepte `^9.2`. Les configurations diffèrent aussi : 32 bits sur hôte, RGB565 sur cible. Les images hôte ne prouvent donc ni la fidélité exacte des couleurs matérielles ni le budget mémoire cible.

À prévoir :

- Version IDF de référence et résolution des composants figée/reproductible.
- Cache dépendant des sources, headers, configuration, compilateur et options.
- Une définition partagée des sources réutilisées entre build rapide, golden et cible.
- Warnings actifs pour le projet ; gestion séparée des avertissements tiers.
- Commandes distinctes `check`, `format`, `lint`, test unitaire, comparaison visuelle et build firmware.

Un CMake hôte avec Ninja est un candidat naturel puisque le firmware utilise déjà CMake ; le but est d’éliminer le cache artisanal, pas de changer d’outil pour le principe.

### 8.2 Tests manquants

| Niveau | Couverture actuelle | Cible recommandée |
| --- | --- | --- |
| Domaine | Nullité, propagation d’échec, copie des champs | Validité, bornes, fraîcheur et propriétés du snapshot |
| Application | Pas de test du contrôleur dans la suite inspectée | Erreur, reprise, déconnexion, péremption, compteur saturant, transitions avec horloge contrôlée |
| Présentation | Images de quelques états fixes | Seuils de segments, précision, textes invalides, statut périmé, aucune modification si état visible identique |
| Cycle de vie | Pas d’exercice répété dans le harnais | Création/destruction, ordre d’arrêt, échecs partiels, sanitizers sur hôte |
| Visuel | Génération de PNG | Comparaison candidate/référence, diff exploitable, acceptation explicite |
| Matériel | Non validé | Build IDF, démarrage, rendu prolongé, charge concurrente, marge mémoire et absence de tearing |

Le simulateur compile le contrôleur et la source simulée, mais `main_sim.c` injecte directement des données mock dans l’écran : **leur compilation n’est pas un test de leur exécution**.

Conserver les goldens approuvés immuables durant `check`. Générer les candidats dans un répertoire séparé ; choisir une tolérance de comparaison justifiée et conserver un environnement de rendu reproductible.

## 9. Plan d’action recommandé

### Étape A — Valider les fondations

- Compiler une combinaison IDF/composants fixée.
- Vérifier la configuration vectorielle et la map mémoire, en premier le boot.
- Valider init ST7701, timings RGB, tactile et placement des buffers.
- Définir les contrats de fraîcheur, concurrence et échec.

**Critère de sortie :** firmware compilable et démarrant sur carte, mémoire connue, aucun affichage indéfiniment fiable en cas de source perdue.

### Étape B — Réduire le travail inutile

- Comparaison des textes et des segments.
- Layout des valeurs stabilisé.
- Séparation décor/cadran si elle améliore les régions invalidées.
- Mesures CPU, mémoire et latence avant/après.

**Critère de sortie :** à snapshot visuellement identique, aucun update UI applicatif inutile ; à données changeantes, période de 40 ms tenue avec marge mesurée.

### Étape C — Rendre la logique indépendante

- Extraire une étape applicative déterministe sans LVGL.
- Garder un adaptateur de timer très mince.
- Tester états, erreurs, fraîcheur et reprise.
- Brancher la future acquisition via snapshot cohérent non bloquant.

**Critère de sortie :** logique applicative testable sans initialiser LVGL, aucune opération de transport dans son thread.

### Étape D — Pérenniser

- Build hôte à dépendances correctes.
- Séparation génération/comparaison/acceptation des références.
- Warnings, formatage, analyse statique et CI cible.
- Documentation conforme au code et conventions de durée de vie.

**Critère de sortie :** un développeur peut comprendre les flux, exécuter les vérifications et détecter une régression sans connaître l’historique du projet.

## Conclusion

**Conserver l’architecture légère actuelle, mais rendre ses frontières effectives.** Le projet n’a pas besoin d’une réécriture ni d’une architecture abstraite imposante.

Les meilleurs leviers sont concrets : maîtriser la mémoire du boot, qualifier les données, éviter les mises à jour identiques, retirer LVGL de la logique testable et fiabiliser les outils de validation. Ils améliorent ensemble propreté, performance et lisibilité — sans sacrifier le rendu vectoriel du cadran.
