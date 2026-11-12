// ============================================================
//  version.h - version du firmware et depot GitHub des mises a jour
// ============================================================
// FW_VERSION : a augmenter AVANT chaque nouvelle release. La compilation
// GitHub Actions refuse une etiquette (tag) qui ne correspond pas : pousser
// le tag "v1.2.0" exige FW_VERSION "1.2.0" ici.
// FW_REPO : "proprietaire/depot" sur GitHub. Le tableau y lit la derniere
// release publiee (les brouillons et les pre-releases sont ignores).
#pragma once
#define FW_VERSION "1.1.2"
#define FW_REPO    "julz24/MLB_Scoreboard"
#define FW_ASSET   "scoreboard.bin"   // nom du fichier .bin attache a chaque release
