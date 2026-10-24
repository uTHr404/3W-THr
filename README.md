# Worldwide Weather Watcher (WWW)

Station météo embarquée sur carte STM32 NUCLEO-L476RG, réalisée dans le cadre du projet CESI.

## Équipe

- Nom de l'équipe : **
- Membres : *BOUZABAL TAHAR\MOUDJIB ABDELMOUAIN HOUAMEL\FERIEL ABERSI\ZIAD SMAILI*
- Encadrante : maryam-cesi

## Présentation du projet

Le Worldwide Weather Watcher est une station météo autonome. Elle mesure la température, l'humidité et la luminosité, enregistre les mesures sur une carte SD avec la date et l'heure, et affiche l'état du système grâce à une LED RGB et un écran LCD.

Le système fonctionne selon quatre modes :

- **Standard** : démarrage normal (sans bouton pressé), mesures toutes les 10 minutes par défaut (paramètre LOG_INTERVAL) et enregistrement horodaté sur la carte SD.
- **Configuration** : démarrage avec le bouton rouge pressé, réglage des paramètres via la console série ; l'acquisition est désactivée et le système repasse en mode standard après 30 minutes sans activité.
- **Économique** : accessible depuis le mode standard par un appui de 5 secondes sur le bouton vert ; le GPS n'est lu qu'une fois sur deux et l'intervalle entre mesures est doublé pour économiser la batterie.
- **Maintenance** : accessible depuis le mode standard ou économique par un appui de 5 secondes sur le bouton rouge ; les données sont consultables en direct sur le port série et la carte SD peut être retirée sans risque.

La LED RGB indique le mode en cours : verte (standard), jaune (configuration), bleue (économique), orange (maintenance). Une LED qui clignote en rouge et une autre couleur signale une erreur (horloge RTC, GPS, capteur ou carte SD).

## Matériel utilisé

- Carte STM32 NUCLEO-L476RG
- Capteur de température et d'humidité DHT11
- Capteur de luminosité BH1750 (I2C)
- Écran LCD 16x2 en I2C
- Horloge temps réel (RTC)
- Module carte micro SD (SPI)
- LED RGB
- Boutons poussoirs rouge et vert
- Module GPS (désactivé dans la version actuelle)

Le schéma de câblage se trouve dans le dépôt : `schema_cablage_WWW.xml` (à ouvrir avec draw.io) et `schema.branchement.org.drawio(2).png`.

## Structure du dépôt

- `Core/` : code source du projet : main.c, un fichier par composant (www_bh1750, www_dht11, www_gps, www_lcd, www_led, www_rtc, www_sd) et les modules de logique (www_app, www_config, www_console, www_measure, www_mode, www_util)
- `Drivers/` : pilotes STM32 (HAL, CMSIS)
- `FATFS/` et `Middlewares/Third_Party/FatFs/` : gestion du système de fichiers de la carte SD
- `cmake/`, `CMakeLists.txt`, `CMakePresets.json` : configuration de la compilation
- `build.sh` : script de compilation et de flash
- `firste_project.ioc` : configuration STM32CubeMX

## Compilation et flash

### Prérequis

- Toolchain ARM GCC (arm-none-eabi-gcc)
- CMake et Ninja
- st-flash (outils stlink) pour programmer la carte
- Git

### Étapes

1. Cloner le dépôt :

```bash
git clone https://github.com/uTHr404/3W-THr.git
cd 3W-THr
```

2. Lancer le script de compilation et de flash, avec la carte branchée en USB :

```bash
chmod +x build.sh
./build.sh
```

Le script compile le projet avec CMake puis flashe la carte NUCLEO-L476RG.

Le dossier `build/` est généré à la compilation et n'est pas versionné (il est exclu par le fichier `.gitignore`).

## Utilisation

1. Brancher la carte en USB et insérer la carte micro SD.
2. Ouvrir la console série (COM3 sous Windows, 115200 bauds) pour suivre les messages et accéder au mode Configuration.
3. Changer de mode avec les boutons : démarrer avec le bouton rouge pressé pour la configuration, appuyer 5 secondes sur le vert pour l'économique, 5 secondes sur le rouge pour la maintenance ou pour revenir au mode précédent.
4. Lire la couleur de la LED RGB pour connaître le mode ou l'erreur en cours.

## Documentation

- Documentation technique : *lien à ajouter*
- Fiche d'utilisation : *lien à ajouter*

## Auteur

Projet réalisé par *à compléter* (uTHr404), CESI.
