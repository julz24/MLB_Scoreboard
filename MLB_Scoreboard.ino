/* ============================================================
   MLB SCOREBOARD v4 - ESP32-S3-DevKitC-1 N16R8
   4 panneaux P4 64x32 (256x32). Rotation PAR SPORT :
   NHL, MLB, NFL, NBA, NCAA football et PWHL / LPHF.
   Chaque sport actif a son panneau (match du favori epingle, sinon
   rotation des matchs, sinon classement / "aucun match") ; le favori
   NHL en direct prend 2 panneaux ; la meteo occupe le dernier panneau.
   Panneaux restants : matchs supplementaires puis classements.
   Plus de sports que de panneaux : une fenetre de sports glisse.
   Redessin seulement au changement (anti-scintillement).
   Web UI Material (sports, favoris, luminosite, on/off, meteo, langue)
   + OTA + mDNS.
   ------------------------------------------------------------
   Dependances : ESP32-HUB75-MatrixPanel-I2S-DMA, ArduinoJson v7,
                 ElegantOTA (mode WebServer sync).
   Fichiers du croquis (tous dans le meme dossier) :
     MLB_Scoreboard.ino            includes, structures, reglages NVS, globals, logs
     t1_dessin.ino                 dessin des panneaux, classements, meteo, animation
     t2_rendu_rotation.ino         repartition des sports sur les panneaux, rotations
     t3_fetch_matchs.ino           telechargement HTTPS commun, matchs MLB / NFL / NHL
     t4_fetch_meteo_classements.ino meteo, classements MLB / NHL / NFL
     t5_fetch_autres_sports.ino    avantage numerique LNH, PWHL, NBA, NCAA
     t6_web_wifi.ino               web UI, WiFi, portail captif
     t7_main.ino                   setup(), netTask(), loop()
     t8_mise_a_jour.ino            mise a jour du firmware depuis les releases GitHub
     version.h, sport_util.h, web_ui.h, logos_*.h
   L'IDE Arduino colle les onglets t1..t7 a la suite de ce fichier (ordre
   alphabetique) : l'ordre des declarations est donc celui des numeros.
   Partition recommandee : "Minimal SPIFFS (1.9MB APP with OTA)"
   ============================================================ */

#include <WiFi.h>
#include <DNSServer.h>          // portail captif maison
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_netif.h>          // DNS public sans figer l'adresse IP (ensurePublicDns)
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <Update.h>             // mise a jour depuis GitHub (t8_mise_a_jour.ino)
#include <mbedtls/sha256.h>     // controle SHA-256 du firmware telecharge
#include "version.h"            // FW_VERSION, FW_REPO
#include <Preferences.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>    // petite police 3x5 (Adafruit GFX) - "Entracte" NHL
#include "logos_mlb.h"
#include "logos_nhl.h"
#include "logos_nhl_20.h"
#include "logos_mlb_leagues.h"
#include "logos_nhl_shield.h"
#include "logos_alt_nhl.h"
#include "logos_nfl.h"
#include "logos_nfl_shield.h"
#include "logos_pwhl.h"        // logos PWHL 20x20 (cle = code d'equipe HockeyTech)
#include "logos_nba.h"         // logos NBA 20x20 (cle = abreviation ESPN)
#include "logos_ncaa.h"        // logos NCAA football 20x20 (cle = abreviation ESPN)
#include "logos_leagues_boot.h" // logos des ligues 28 px de haut - ecran de chargement
#include "sport_util.h"        // templates communs aux sports (anyLive, buildRotAbbr)

// Position/mise en page communes aux panneaux "1 match, 1 panneau 64px"
// (NFL, MLB, et NHL compact) - deplacees ici (au lieu de rester pres de
// drawPanelNFL plus bas) car le preprocesseur C ne voit pas en avant : les
// utiliser plus haut dans le fichier avant leur #define cause une erreur de
// compilation ("was not declared in this scope"), vu et corrige apres un
// vrai echec de compilation.
#define NFL_COL_RIGHT_X 44
#define NFL_LOGO_Y       1
#define NFL_BOT_Y       24

// ====================== WIFI ======================
// Identifiants par defaut (1er flash). Une fois configures via le portail,
// ils sont stockes en NVS (cles "wssid"/"wpass") et survivent au reflash.
const char* WIFI_SSID = "";   // vide : le WiFi se configure par le portail au 1er demarrage
const char* WIFI_PASS = "";
const char* HOSTNAME  = "scoreboard";        // -> http://scoreboard.local
const char* AP_NAME   = "Scoreboard-Setup";  // reseau de config si pas de connexion
const char* AP_PASS   = "scoreboard123";     // min 8 caracteres (ou "" pour ouvert)
#define WIFI_CONNECT_TIMEOUT_S 18            // secondes avant de basculer en portail

DNSServer dnsServer;
bool   apMode = false;        // true = portail de config actif
String staSsid, staPass;

// ====================== GEOMETRIE ======================
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 4
const int W = PANEL_RES_X * PANEL_CHAIN;  // 256
const int H = PANEL_RES_Y;                // 32

// ---- Pinout HUB75 -> ESP32-S3 (valide au test) ----
#define R1_PIN 4
#define G1_PIN 5
#define B1_PIN 6
#define R2_PIN 7
#define G2_PIN 15
#define B2_PIN 16
#define A_PIN  18
#define B_PIN  8
#define C_PIN  3
#define D_PIN  46
#define E_PIN  -1
#define LAT_PIN 47
#define OE_PIN  21
#define CLK_PIN 2

MatrixPanel_I2S_DMA* dma = nullptr;
WebServer server(80);
Preferences prefs;

// ====================== STRUCTURE MATCH ======================
#define MAX_GAMES 20
struct Game {
  int  awayId, homeId;
  int  awayScore, homeScore;
  uint8_t state;        // 0 = pas commence, 1 = live, 2 = final
  uint8_t inning;
  bool topInning;
  bool b1, b2, b3;      // coureurs sur 1er/2e/3e
  uint8_t outs;
  char timeStr[6];      // "19:05" si pas commence
  uint8_t allStar;      // 0 = normal ; 1 = away est AL / home est NL (match des etoiles) ; 2 = away NL / home AL
  uint8_t delayState;   // 0=normal 1=Delayed Start 2=Delayed(en cours) 3=Suspended 4=Postponed
};

// ====================== STRUCTURE MATCH ESPN (NFL / NBA / NCAA) ======================
// Les trois sports viennent de la meme API ESPN (scoreboard) et ont exactement les
// memes champs : une seule structure. Identification par ABREVIATION
// (team.abbreviation) : NFL 3 caracteres, NBA jusqu'a 4 (UTAH), NCAA jusqu'a 5.
#define MAX_GAMES_NFL  20
#define MAX_GAMES_NBA  20
#define MAX_GAMES_NCAA 40
struct GameESPN {
  char awayAbbr[6], homeAbbr[6];
  int  awayScore, homeScore;
  uint8_t state;        // 0 = pas commence, 1 = live, 2 = final
  uint8_t period;       // 1-4 ; 5 = prolongation, 6 = 2e prolongation...
  char clockStr[8];     // displayClock ESPN ("12:45" ; "45.3" sous la minute en NBA)
  bool halftime;
  bool wasOT;
  char timeStr[6];      // heure locale si pas commence
  uint8_t poss;         // possession du ballon (NFL) : 0 = inconnue, 1 = visiteur, 2 = local
};
typedef GameESPN GameNFL;
typedef GameESPN GameNBA;
typedef GameESPN GameNCAA;   // football universitaire ; conferenceId ESPN : ACC=1, Big 12=4, Big Ten=5, SEC=8, Pac-12=9
struct RankNCAA { char abbr[6]; char rec[8]; };

// ====================== STRUCTURE MATCH NHL ======================
// Identification par ABREVIATION (pas par ID numerique) : l'API renvoie
// directement "abbrev" et c'est stable/sans ambiguite, contrairement aux
// IDs numeriques (ex: rebrand Utah).

#define MAX_GAMES_NHL 20
struct GameNHL {
  uint32_t id;           // id du match NHL (ex. 2026020286) : sert a retrouver l'avantage numerique et a detecter les buts
  char awayAbbr[4], homeAbbr[4];
  int  awayScore, homeScore;
  uint8_t state;        // 0 = pas commence, 1 = live, 2 = final
  uint8_t period;       // 1,2,3 ; 4 = prolongation ; 5 = tirs de barrage
  char clockStr[6];      // "12:34" si live
  bool intermission;
  bool wasOT, wasSO;     // periode de fin, pour "Fin OP"/"Fin TB"
  char timeStr[6];       // heure locale si pas commence
  // Chrono "live" : l'API ne donne qu'une valeur ponctuelle (toutes les 30 s).
  // On memorise le temps restant + l'instant du fetch pour faire descendre le
  // chrono localement, seconde par seconde, entre deux fetchs (tickNhlClock).
  bool clockRunning;     // clock.running de l'API : true = chrono en marche
  int  clockSecs;        // secondes restantes AU MOMENT du fetch (-1 = inconnu)
  unsigned long clockSyncMs;   // millis() au moment du fetch
};


// ====================== STRUCTURE MATCH PWHL ======================
// API HockeyTech/LeagueStat (celle qui alimente thepwhl.com), vue "scorebar".
// Identification par CODE d'equipe (HomeCode/VisitorCode : MTL, TOR, NY...),
// meme principe que la NHL/NFL. Pas de chrono local (pas de "clock running"
// dans cette API) et pas de classement pour l'instant.
#define MAX_GAMES_PWHL 12
struct GamePWHL {
  char awayAbbr[4], homeAbbr[4];
  int  awayScore, homeScore;
  uint8_t state;        // 0 = pas commence, 1 = live, 2 = final
  uint8_t period;       // 1,2,3 ; 4 = prolongation ; 5 = tirs de barrage
  bool intermission;
  bool wasOT, wasSO;
  char timeStr[6];      // heure locale si pas commence
};

// ====================== REGLAGES (NVS) ======================
uint8_t  cfgBright      = 80;     // 0-255
uint16_t cfgRotationSec = 7;      // duree d'un match a droite
int      cfgFavTeam     = 0;      // aucun favori MLB par defaut (choisi dans la web UI)
bool     cfgLiveOnlyMLB = false;  // MLB : rotation = matchs live seulement
bool     cfgLiveOnlyNHL = false;  // NHL : idem, independant du MLB
bool     cfgScreenOn    = true;
bool     cfgForceWeather = true;  // horloge/meteo sur le dernier panneau (meme defaut que loadCfg)
bool     cfgSportNFL     = false; // NFL active (off par defaut : hors-saison une bonne partie de l'annee)
char     cfgFavTeamNFL[4] = "";   // abreviation NFL favorite ; "" = aucune
bool     cfgLiveOnlyNFL  = true;  // NFL : matchs en direct seulement (defaut)
bool     cfgWildCardNHL  = false; // idem pour la NHL (course aux series) - independant du MLB
bool     cfgWildCard     = false; // true = classement meneurs/meilleurs 2e (wild card) au lieu du classement standard
bool     cfgShowStandMLB = true;  // afficher le classement MLB dans la rotation (off = cacher en hors-saison)
bool     cfgShowStandNHL = true;  // idem NHL, independant
bool     cfgShowStandNFL = true;  // idem NFL, independant
uint8_t  cfgLang         = 0;     // 0 = FR, 1 = EN
bool     cfgSportMLB     = true;  // MLB actif
bool     cfgSportNHL     = true;  // NHL actif
char     cfgFavTeamNHL[4] = "";    // abreviation NHL favorite ; "" = aucun (choisi dans la web UI)
bool     cfgAltLogoNHL   = false; // true = utilise logos_alt_nhl.h au lieu de logos_nhl.h (quand disponible)
bool     cfgGoalAnimNHL  = true;  // animation plein ecran quand le favori LNH marque
bool     cfgGoalAnimNFL  = true;  // animation plein ecran quand le favori NFL marque (>= 3 pts)
// Les indicateurs AN/PP (LNH) et possession (NFL) sont toujours actifs (plus de reglage).

// ----- Sports ajoutes : PWHL / LPHF, NBA, NCAA football -----
bool     cfgWideNHL      = false; // "Affichage avec chronometre" : tout match NHL en direct au format 2 panneaux (comme le favori)
bool     cfgRankCFP      = false; // NCAA : false = sondage AP, true = classement CFP
bool     cfgSportPWHL    = false; // PWHL active (desactivee par defaut)
char     cfgFavTeamPWHL[4] = "";  // code PWHL favori (MTL, TOR...) ; "" = aucun
bool     cfgLiveOnlyPWHL = false; // PWHL : matchs en direct seulement
bool     cfgSportNBA     = false; // NBA active (desactivee par defaut)
char     cfgFavTeamNBA[5] = "";   // abreviation ESPN favorite (LAL, GS, UTAH...) ; "" = aucune
bool     cfgLiveOnlyNBA  = false; // NBA : matchs en direct seulement
bool     cfgShowStandNBA = true;  // afficher le classement NBA dans la rotation
bool     cfgSportNCAA     = false; // NCAA football active (desactivee par defaut)
bool     cfgNcaaConf[5]   = {true, true, true, true, true}; // ACC, Big 12, Big Ten, SEC, Pac-12
// Favoris NCAA : 0 ACC, 1 Big 12, 2 Big Ten, 3 SEC, 4 Pac-12, 5 Top 25 (abreviation ESPN ; "" = aucun)
char     cfgFavNCAA[6][6] = {"", "", "", "", "", ""};
bool     cfgLiveOnlyNCAA  = false;
bool     cfgShowStandNCAA = true; // afficher le Top 6 (et activer le favori "Top 25")

// ----- Position de la meteo (plus aucune ville/cle ecrite en dur) -----
// Par defaut : AUTOMATIQUE = geolocalisation de l'adresse IP publique de la
// connexion Internet ou le tableau est branche (pas du code, pas du nom du WiFi).
// Si une ville est saisie dans la web UI, elle remplace l'automatique
// (geocodage Open-Meteo, sans cle) et est memorisee en NVS.
char     cfgWxCity[40]   = "";    // ville choisie ; "" = automatique
float    wxLat = NAN, wxLon = NAN;   // position utilisee (NAN = pas encore resolue)
char     wxPlace[64]     = "";    // nom affiche dans la web UI : "Montreal, Quebec (CA)"
bool     wxLocIsAuto     = true;  // true = position venant de l'IP, false = ville choisie
volatile bool wxCityDirty   = false;  // ville modifiee dans la web UI -> re-resoudre
volatile bool wxSavePending = false;  // netTask a resolu une ville : loop() sauve en NVS (un seul ecrivain)
uint8_t  wxStat = 0;              // 0 ok, 1 recherche, 2 ville introuvable, 3 geoloc IP impossible, 4 erreur reseau
unsigned long wxGeoAt = 0;        // millis() de la derniere geolocalisation IP (refaite toutes les 24 h)

// Helper de traduction : renvoie la chaine FR ou EN selon la langue choisie.
const char* T(const char* fr, const char* en) { return cfgLang == 0 ? fr : en; }

void loadCfg() {
  prefs.begin("scoreboard", true);
  cfgBright       = prefs.getUChar("bright", 80);
  cfgRotationSec  = prefs.getUShort("rot", 7);
  cfgFavTeam      = prefs.getInt("fav", 0);
  cfgLiveOnlyMLB  = prefs.getBool("liveMlb", false);
  cfgLiveOnlyNHL  = prefs.getBool("liveNhl", false);
  cfgScreenOn     = prefs.getBool("screen", true);
  cfgForceWeather = prefs.getBool("wxforce", true);
  cfgWildCard     = prefs.getBool("wildcard", false);
  cfgWildCardNHL  = prefs.getBool("wcNhl", false);
  cfgShowStandMLB = prefs.getBool("showStMlb", true);
  cfgShowStandNHL = prefs.getBool("showStNhl", true);
  cfgShowStandNFL = prefs.getBool("showStNfl", true);
  cfgSportNFL     = prefs.getBool("spNfl", false);
  cfgLiveOnlyNFL  = prefs.getBool("liveNfl", true);
  { String fk = prefs.getString("favNfl", ""); fk.toCharArray(cfgFavTeamNFL, sizeof(cfgFavTeamNFL)); }
  cfgLang         = prefs.getUChar("lang", 0);
  cfgSportMLB     = prefs.getBool("spMlb", true);
  cfgSportNHL     = prefs.getBool("spNhl", true);
  cfgAltLogoNHL   = prefs.getBool("altLogoNhl", false);
  cfgWideNHL      = prefs.getBool("wideNhl", false);
  cfgGoalAnimNHL  = prefs.getBool("goalAnim", true);              // ancienne cle commune : garde le choix existant
  cfgGoalAnimNFL  = prefs.getBool("tdAnim", cfgGoalAnimNHL);
  cfgRankCFP      = prefs.getBool("rkCfp", false);
  cfgSportPWHL    = prefs.getBool("spPwhl", false);
  cfgLiveOnlyPWHL = prefs.getBool("livePwhl", false);
  { String fw = prefs.getString("favPwhl", ""); fw.toCharArray(cfgFavTeamPWHL, sizeof(cfgFavTeamPWHL)); }
  cfgSportNBA     = prefs.getBool("spNba", false);
  cfgLiveOnlyNBA  = prefs.getBool("liveNba", false);
  cfgShowStandNBA = prefs.getBool("showStNba", true);
  { String fb = prefs.getString("favNba", ""); fb.toCharArray(cfgFavTeamNBA, sizeof(cfgFavTeamNBA)); }
  cfgSportNCAA     = prefs.getBool("spNcaa", false);
  cfgLiveOnlyNCAA  = prefs.getBool("liveNcaa", false);
  cfgShowStandNCAA = prefs.getBool("showStNcaa", true);
  cfgNcaaConf[0] = prefs.getBool("cfAcc", true);
  cfgNcaaConf[1] = prefs.getBool("cfB12", true);
  cfgNcaaConf[2] = prefs.getBool("cfB10", true);
  cfgNcaaConf[3] = prefs.getBool("cfSec", true);
  cfgNcaaConf[4] = prefs.getBool("cfP12", true);
  for (int k = 0; k < 6; k++) { char key[8]; snprintf(key, sizeof(key), "fnc%d", k); String fn = prefs.getString(key, ""); fn.toCharArray(cfgFavNCAA[k], 6); }
  { String fn = prefs.getString("favNhl", ""); fn.toCharArray(cfgFavTeamNHL, sizeof(cfgFavTeamNHL)); }
  { String wc = prefs.getString("wxCity", ""); wc.toCharArray(cfgWxCity, sizeof(cfgWxCity)); }
  if (cfgWxCity[0]) {   // ville choisie : on reprend sa position memorisee (aucun appel reseau au boot)
    float la = prefs.getFloat("wxLat", NAN), lo = prefs.getFloat("wxLon", NAN);
    if (!isnan(la) && !isnan(lo)) {
      wxLat = la; wxLon = lo; wxLocIsAuto = false;
      String wp = prefs.getString("wxPlace", ""); wp.toCharArray(wxPlace, sizeof(wxPlace));
    }
  }
  prefs.end();
}
void saveCfg() {
  prefs.begin("scoreboard", false);
  prefs.putUChar("bright", cfgBright);
  prefs.putUShort("rot", cfgRotationSec);
  prefs.putInt("fav", cfgFavTeam);
  prefs.putBool("liveMlb", cfgLiveOnlyMLB);
  prefs.putBool("liveNhl", cfgLiveOnlyNHL);
  prefs.putBool("screen", cfgScreenOn);
  prefs.putBool("wxforce", cfgForceWeather);
  prefs.putBool("wildcard", cfgWildCard);
  prefs.putBool("wcNhl", cfgWildCardNHL);
  prefs.putBool("showStMlb", cfgShowStandMLB);
  prefs.putBool("showStNhl", cfgShowStandNHL);
  prefs.putBool("showStNfl", cfgShowStandNFL);
  prefs.putBool("spNfl", cfgSportNFL);
  prefs.putBool("liveNfl", cfgLiveOnlyNFL);
  prefs.putString("favNfl", String(cfgFavTeamNFL));
  prefs.putUChar("lang", cfgLang);
  prefs.putBool("spMlb", cfgSportMLB);
  prefs.putBool("spNhl", cfgSportNHL);
  prefs.putBool("altLogoNhl", cfgAltLogoNHL);
  prefs.putBool("wideNhl", cfgWideNHL);
  prefs.putBool("goalAnim", cfgGoalAnimNHL);
  prefs.putBool("tdAnim", cfgGoalAnimNFL);
  prefs.putBool("rkCfp", cfgRankCFP);
  prefs.putBool("spPwhl", cfgSportPWHL);
  prefs.putBool("livePwhl", cfgLiveOnlyPWHL);
  prefs.putString("favPwhl", String(cfgFavTeamPWHL));
  prefs.putBool("spNba", cfgSportNBA);
  prefs.putBool("liveNba", cfgLiveOnlyNBA);
  prefs.putBool("showStNba", cfgShowStandNBA);
  prefs.putString("favNba", String(cfgFavTeamNBA));
  prefs.putBool("spNcaa", cfgSportNCAA);
  prefs.putBool("liveNcaa", cfgLiveOnlyNCAA);
  prefs.putBool("showStNcaa", cfgShowStandNCAA);
  prefs.putBool("cfAcc", cfgNcaaConf[0]);
  prefs.putBool("cfB12", cfgNcaaConf[1]);
  prefs.putBool("cfB10", cfgNcaaConf[2]);
  prefs.putBool("cfSec", cfgNcaaConf[3]);
  prefs.putBool("cfP12", cfgNcaaConf[4]);
  for (int k = 0; k < 6; k++) { char key[8]; snprintf(key, sizeof(key), "fnc%d", k); prefs.putString(key, String(cfgFavNCAA[k])); }
  prefs.putString("favNhl", String(cfgFavTeamNHL));
  prefs.putString("wxCity", String(cfgWxCity));
  if (!wxLocIsAuto && !isnan(wxLat) && !isnan(wxLon)) {   // position de la ville choisie
    prefs.putFloat("wxLat", wxLat);
    prefs.putFloat("wxLon", wxLon);
    prefs.putString("wxPlace", String(wxPlace));
  }
  prefs.end();
}

// Sauvegarde DIFFEREE : chaque /api/set arme seulement un delai ; loop() ecrit la NVS une
// seule fois, CFG_SAVE_DELAY_MS apres le DERNIER changement. Avant, glisser le curseur de
// luminosite reecrivait la cinquantaine de cles a chaque pas (ecriture flash bloquante).
#define CFG_SAVE_DELAY_MS 2500UL
volatile bool cfgSavePending = false;
unsigned long cfgSaveAt = 0;
// Ecrit tout de suite une sauvegarde en attente (avant un redemarrage ou une OTA).
void flushCfg() {
  if (cfgSavePending) { cfgSavePending = false; saveCfg(); }
}

// ====================== MATCHS (globals) ======================
Game games[MAX_GAMES];
int  gameCount = 0;
int  favIdx = -1;             // index du match favori (panneau 1)
int  rotList[MAX_GAMES];      // indices des matchs en rotation
int  rotCount = 0;
int  rotBase = 0;                 // position de depart de la rotation des matchs
int  wxPanels[3];                 // px des panneaux montrant la meteo (rempli par render())
int  wxPanelCount = 0;
// Chrono NHL live (format 128px du favori en direct) : ou il est dessine,
// rempli par renderUnified(), lu par tickNhlClock().
int  nhlClockPx  = -1;            // px du panneau NHL 128px en direct (-1 = aucun)
int  nhlClockIdx = -1;            // index dans gamesNHL[]
char nhlClockHome[4] = "";        // garde-fou : meme match qu'au dernier render()
unsigned long lastClockTick = 0;
bool dirty = true;         // true => redessiner le panneau
bool wifiResetPending = false;
unsigned long wifiResetAt = 0;
SemaphoreHandle_t dataMutex = NULL;   // protege games[]/wx* entre netTask et loop()

// ====================== MATCHS NHL (globals) ======================
GameNHL gamesNHL[MAX_GAMES_NHL];
int  gameCountNHL = 0;
int  favIdxNHL = -1;             // index du match favori NHL
int  rotListNHL[MAX_GAMES_NHL];  // indices des matchs NHL en rotation
int  rotCountNHL = 0, rotBaseNHL = 0;
unsigned long lastFetchNHL = 0, lastStandNHL = 0;
unsigned long lastStandNFL = 0;

GameNFL gamesNFL[MAX_GAMES_NFL];
int  gameCountNFL = 0;
int  favIdxNFL = -1;
int  rotListNFL[MAX_GAMES_NFL];
int  rotCountNFL = 0;
int  rotBaseNFL = 0;
unsigned long lastFetchNFL = 0;
int  combinedStandRot = 0;       // rotation classement combinee MLB+NHL
int  freeSportRot = 0;           // rotation equitable des panneaux libres entre les 6 sports (0-5)

// ====================== MATCHS PWHL / NBA / NCAA (globals) ======================
GamePWHL gamesPWHL[MAX_GAMES_PWHL];
int  gameCountPWHL = 0;
int  favIdxPWHL = -1;
int  rotListPWHL[MAX_GAMES_PWHL];
int  rotCountPWHL = 0, rotBasePWHL = 0;
unsigned long lastFetchPWHL = 0;

GameNBA gamesNBA[MAX_GAMES_NBA];
int  gameCountNBA = 0;
int  favIdxNBA = -1;
int  rotListNBA[MAX_GAMES_NBA];
int  rotCountNBA = 0, rotBaseNBA = 0;
unsigned long lastFetchNBA = 0, lastStandNBA = 0;

GameNCAA gamesNCAA[MAX_GAMES_NCAA];
int  gameCountNCAA = 0;
int  favListNCAA[6] = {-1, -1, -1, -1, -1, -1};   // index du match de chaque favori (-1 = pas de match)
int  favCountNCAA = 0;
int  rotListNCAA[MAX_GAMES_NCAA];
int  rotCountNCAA = 0, rotBaseNCAA = 0;
RankNCAA rankNCAA[6];
char top25Abbr[25][6];     // Top 25 complet (menu du favori "Top 25" de la page web)
char top25Name[25][20];
int  top25Count = 0;
int  rankCountNCAA = 0;
volatile bool rankForceNCAA = false;   // true = retelecharger le classement tout de suite (toggle AP/CFP)
char top25Type[8] = "";                // sondage reellement charge ("ap" / "cfp"), "" = aucun
bool rankValidNCAA = false;
// Un favori NCAA n'est actif que si le toggle de SA section est a on (conferences 0-4 : toggle de la
// conference ; 5 = Top 25 : toggle "Top 25"). Toggle a off -> ni match, ni "aucun match", ni telechargement.
bool ncaaFavActive(int k) {
  if (k < 0 || k > 5 || cfgFavNCAA[k][0] == 0) return false;
  return (k < 5) ? cfgNcaaConf[k] : cfgShowStandNCAA;
}
bool ncaaFetched = false;   // au moins un cycle de fetch termine (evite un "aucun match" au demarrage)
unsigned long lastFetchNCAA = 0, lastStandNCAA = 0;
// ---- Animation de but/touche + indicateurs AN/PP (LNH) et possession (NFL) ----
// (cfgGoalAnimNHL / cfgGoalAnimNFL sont declares plus haut, avec les autres reglages cfg*, avant loadCfg().)
struct GoalReq { volatile bool pending; uint8_t sport; uint8_t kind; char abbr[5]; };
GoalReq goalReq = {false, 0, 0, ""};   // ecrit par netTask (ou la web UI), consomme par loop() ; sport 0 = LNH, 1 = NFL ; kind NFL : 0 touche, 1 placement
// Situation de jeu LNH (derniere valeur connue par match) : pp 0 = egalite, 1 = visiteur en AN, 2 = local en AN.
// Perime apres SIT_FRESH_MS (rien affiche plutot qu'une info fausse si le fetch du play-by-play a rate).
struct NhlSit { uint32_t id; int8_t pp; unsigned long ms; };
NhlSit nhlSit[MAX_GAMES_NHL];
int    nhlSitCount = 0;
#define SIT_FRESH_MS 60000UL
#define SIT_STEP_MS   8000UL     // un telechargement de play-by-play toutes les 8 s (favori en priorite)
#define FETCH_FAV_MS_NHL 10000UL // favori LNH en direct : score toutes les 10 s (detecte le but rapidement)
#define FETCH_FAV_MS_NFL 15000UL // idem NFL

int  sportWinRot = 0;       // fenetre de sports quand il y en a plus que de panneaux (voir renderUnified)

// ====================== ALLOCATEUR PSRAM POUR ARDUINOJSON ======================
// Les JsonDocument (~53KB pour les matchs) allouaient par defaut dans la RAM
// interne (malloc standard). Repetee toutes les 30s, cette alloc/liberation
// de gros blocs fragmentait la RAM interne au fil du temps -> web UI de plus
// en plus instable apres quelques minutes (erreurs HTTP -11, etc.). Le
// ESP32-S3 a 8MB de PSRAM : on y redirige ces gros documents JSON.
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
  void deallocate(void* pointer) override { heap_caps_free(pointer); }
  void* reallocate(void* ptr, size_t new_size) override { return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM); }
};
SpiRamAllocator psramAllocator;

// ====================== LOGS (consultables via web UI) ======================
// Buffer de logs de taille FIXE (pas de String qui grandit/retrecit sans
// cesse -> evite une source supplementaire de fragmentation RAM interne).
#define LOG_MAX 4000
char logBuf[LOG_MAX + 1] = "";
size_t logLen = 0;
SemaphoreHandle_t logMutex = NULL;   // protege logBuf/logLen : logAdd() est appele par netTask ET par loop()

void logAdd(const String& line) {
  char ts[9] = "--:--:--";
  struct tm t;
  if (getLocalTime(&t, 10)) strftime(ts, sizeof(ts), "%H:%M:%S", &t);
  Serial.printf("[%s] %s\n", ts, line.c_str());

  char entry[220];
  int n = snprintf(entry, sizeof(entry), "[%s] %s\n", ts, line.c_str());
  if (n <= 0) return;
  // snprintf renvoie la longueur VOULUE, pas la longueur ecrite : une ligne trop
  // longue est tronquee dans entry[], il ne faut donc pas copier plus que ca.
  if (n >= (int)sizeof(entry)) { n = sizeof(entry) - 1; entry[n - 1] = '\n'; }

  if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY);
  if (logLen + n > LOG_MAX) {
    // decale le buffer pour faire de la place (coupe les plus vieilles lignes)
    size_t drop = logLen + n - LOG_MAX;
    memmove(logBuf, logBuf + drop, logLen - drop);
    logLen -= drop;
  }
  memcpy(logBuf + logLen, entry, n);
  logLen += n;
  logBuf[logLen] = '\0';
  if (logMutex) xSemaphoreGive(logMutex);
}
long tzOffsetSec = -4 * 3600;

volatile bool otaActive = false;   // vrai pendant une mise a jour OTA (ElegantOTA onStart/onEnd)

// ---- Mise a jour depuis GitHub (t8_mise_a_jour.ino) ----
// Paquet de certificats racine (Mozilla) integre au core ESP32 : sert a verifier
// les serveurs GitHub avant de telecharger un firmware.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");
enum { FW_IDLE, FW_CHECKING, FW_UP_TO_DATE, FW_AVAILABLE, FW_INSTALLING, FW_ERROR };
volatile uint8_t fwStat = FW_IDLE;
volatile bool fwCheckReq = false, fwInstallReq = false;   // demandes de la web UI, executees par netTask
volatile int  fwProgress = 0;          // % pendant l'installation
char     fwLatest[16] = "";           // derniere version publiee ("1.2.0")
char     fwUrl[256]   = "";           // adresse du .bin de cette release
char     fwSha[65]    = "";           // SHA-256 attendu (hex)
uint32_t fwSize       = 0;
uint8_t  fwErrCode    = 0;            // derniere erreur, code traduit par la web UI (FW_E_*), 0 = aucune
char     fwNotes[1500] = "";          // nouveautes de la derniere version (texte de la release)
// Codes d'erreur de mise a jour. La web UI a le texte FR/EN de chaque code (TRX.fwErrs) :
// garder le meme ordre des deux cotes.
enum { FW_E_NONE, FW_E_CONFIG, FW_E_UNREACHABLE, FW_E_BAD_REPLY, FW_E_NO_RELEASE, FW_E_NO_FILE,
       FW_E_NO_CHECKSUM, FW_E_REFUSED, FW_E_SIZE, FW_E_NO_ROOM, FW_E_WRITE, FW_E_INTERRUPTED,
       FW_E_CHECKSUM, FW_E_INVALID, FW_E_BAD_URL };
unsigned long lastFetch = 0, lastRotate = 0;
const unsigned long FETCH_MS = 30000;      // intervalle si au moins un match est en direct
const unsigned long FETCH_MS_SLOW = 900000; // intervalle sinon (15 min) - reduit la frequence de connexions reseau quand rien ne se passe (etait 5 min)

// ====================== METEO (globals) ======================
unsigned long lastWx = 0, lastAnim = 0;
const unsigned long WX_MS = 900000;   // rafraichit la meteo toutes les 15 min (si succes)
const unsigned long WX_RETRY_MS = 120000; // reessai rapide (2 min) apres un echec
// Meteo : Open-Meteo (sans cle API). Position : voir "Position de la meteo" plus haut.
float wxTemp  = NAN;
int   wxCode  = -1;
bool  wxIsDay = true;
bool  wxValid = false;
uint16_t animFrame = 0;               // compteur d'animation meteo

// ====================== CLASSEMENTS (globals) ======================
struct StandTeam { char abbr[4]; int id; int w; int l; char gb[6]; };
struct StandDiv  { const char* title; const char* titleEn; int id; StandTeam teams[5]; int n; };
StandDiv standDiv[6] = {
  { "AL EST",    "AL EAST",    201, {}, 0 },
  { "AL CENTRE", "AL CENTRAL", 202, {}, 0 },
  { "AL OUEST",  "AL WEST",    200, {}, 0 },
  { "NL EST",    "NL EAST",    204, {}, 0 },
  { "NL CENTRE", "NL CENTRAL", 205, {}, 0 },
  { "NL OUEST",  "NL WEST",    203, {}, 0 },
};
bool standValid = false;
unsigned long lastStand = 0;
const unsigned long STAND_MS = 1800000;   // rafraichit les classements toutes les 30 min

// ====================== CLASSEMENTS NHL (globals) ======================
// apiName doit correspondre exactement a "divisionName" renvoye par
// /v1/standings/ ("Atlantic","Metropolitan","Central","Pacific").
struct StandTeamNHL { char abbr[4]; int w; int l; int otl; int pts; };
struct StandDivNHL  { const char* title; const char* titleEn; const char* apiName; StandTeamNHL teams[8]; int n; };
StandDivNHL standDivNHL[4] = {
  { "ATL.",   "ATL.",   "Atlantic",     {}, 0 },
  { "METRO.", "METRO.", "Metropolitan", {}, 0 },
  { "CENT.",  "CENT.",  "Central",      {}, 0 },
  { "PAC.",   "PAC.",   "Pacific",      {}, 0 },
};
bool standValidNHL = false;

// ====================== CLASSEMENTS NFL (globals) ======================
// L'API ESPN ne descend pas au niveau division : les equipes sont rangees dans
// leur division par une table statique (nflDivisionForTeam), voir fetchStandingsNFL().
struct StandTeamNFL { char abbr[4]; int w; int l; int t; int pct1000; };
struct StandDivNFL  { const char* title; const char* titleEn; StandTeamNFL teams[5]; int n; };
StandDivNFL standDivNFL[8] = {
  { "AFC EST",   "AFC EAST",   {}, 0 },
  { "AFC NORD",  "AFC NORTH",  {}, 0 },
  { "AFC SUD",   "AFC SOUTH",  {}, 0 },
  { "AFC OUEST", "AFC WEST",   {}, 0 },
  { "NFC EST",   "NFC EAST",   {}, 0 },
  { "NFC NORD",  "NFC NORTH",  {}, 0 },
  { "NFC SUD",   "NFC SOUTH",  {}, 0 },
  { "NFC OUEST", "NFC WEST",   {}, 0 },
};
bool standValidNFL = false;

// ====================== CLASSEMENTS NBA (globals) ======================
// Meme forme que la NFL : l'API ESPN (standings) renvoie 2 CONFERENCES avec une
// liste plate de 15 equipes, sans niveau division -> regroupement par une table
// statique equipe -> division (nbaDivisionForTeam). 6 divisions de 5 equipes.
struct StandTeamNBA { char abbr[5]; int w; int l; int pct1000; };
struct StandDivNBA  { const char* title; const char* titleEn; StandTeamNBA teams[5]; int n; };
StandDivNBA standDivNBA[6] = {
  { "ATLANTIQUE", "ATLANTIC",  {}, 0 },
  { "CENTRALE",   "CENTRAL",   {}, 0 },
  { "SUD-EST",    "SOUTHEAST", {}, 0 },
  { "NORD-OUEST", "NORTHWEST", {}, 0 },
  { "PACIFIQUE",  "PACIFIC",   {}, 0 },
  { "SUD-OUEST",  "SOUTHWEST", {}, 0 },
};
bool standValidNBA = false;
