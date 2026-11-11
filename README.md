# Tableau de pointage multi-sport (ESP32-S3, 4 panneaux P4 64x32)

Pointages en direct NHL, MLB, NFL, NBA, NCAA football et PWHL / LPHF, classements,
horloge et météo, sur 4 panneaux HUB75 64x32 chaînés (256x32), avec une interface web
de réglage (`http://scoreboard.local`).

## Matériel et compilation

- ESP32-S3-DevKitC-1 N16R8 (16 Mo de flash, 8 Mo de PSRAM OPI)
- IDE Arduino, carte **ESP32S3 Dev Module**, core ESP32 3.3.4 :
  PSRAM « OPI PSRAM », Flash Size « 16MB », Partition Scheme « Minimal SPIFFS (1.9MB APP with OTA) »
- Bibliothèques : ArduinoJson 7.4.3, ElegantOTA 4.0.0, Adafruit GFX Library 1.13.0,
  ESP32 HUB75 LED MATRIX PANEL DMA Display 3.0.14

## Installer un tableau neuf

1. Télécharger `scoreboard-usb-complet.bin` depuis la dernière release.
2. Le flasher à l'adresse `0x0` (par exemple avec https://espressif.github.io/esptool-js/).
3. Au démarrage, se connecter au réseau WiFi **Scoreboard-Setup** (mot de passe `scoreboard123`)
   et choisir le WiFi de la maison dans la page qui s'ouvre.

## Mises à jour

Le tableau vérifie les releases de ce dépôt au démarrage puis toutes les 6 h. Quand une
nouvelle version existe, elle apparaît dans la web UI (**Paramètres › Système**) avec un
bouton **Installer**. Le téléchargement est vérifié (certificat TLS de GitHub et somme
SHA-256) avant d'être installé.

Publier une version :

1. Augmenter `FW_VERSION` dans [version.h](version.h) et commiter.
2. Étiqueter et pousser :
   ```bash
   git tag v1.2.0
   git push origin main v1.2.0
   ```
3. GitHub Actions compile le firmware et publie la release avec les deux fichiers `.bin`.

Tester d'abord la version sur un tableau par USB : un firmware qui plante au démarrage
ne peut plus se mettre à jour par le WiFi.
