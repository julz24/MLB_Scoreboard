// ============================================================
//  t6_web_wifi.ino - Web UI (/api/state, /api/set), identifiants WiFi, portail captif.
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

#include "web_ui.h"

void applyLive() { dma->setBrightness8(cfgBright); }

void handleState() {
  String j = "{";
  j += "\"bright\":" + String(cfgBright);
  j += ",\"speed\":" + String(cfgRotationSec);
  j += ",\"fav\":" + String(cfgFavTeam);
  j += ",\"liveMlb\":" + String(cfgLiveOnlyMLB ? "true" : "false");
  j += ",\"liveNhl\":" + String(cfgLiveOnlyNHL ? "true" : "false");
  j += ",\"screen\":" + String(cfgScreenOn ? "true" : "false");
  j += ",\"wxforce\":" + String(cfgForceWeather ? "true" : "false");
  j += ",\"wildcard\":" + String(cfgWildCard ? "true" : "false");
  j += ",\"wcNhl\":" + String(cfgWildCardNHL ? "true" : "false");
  j += ",\"showStMlb\":" + String(cfgShowStandMLB ? "true" : "false");
  j += ",\"showStNhl\":" + String(cfgShowStandNHL ? "true" : "false");
  j += ",\"showStNfl\":" + String(cfgShowStandNFL ? "true" : "false");
  j += ",\"spNfl\":" + String(cfgSportNFL ? "true" : "false");
  j += ",\"liveNfl\":" + String(cfgLiveOnlyNFL ? "true" : "false");
  j += ",\"favNfl\":\"" + String(cfgFavTeamNFL) + "\"";
  j += ",\"gamesNfl\":" + String(gameCountNFL);
  j += ",\"lang\":" + String(cfgLang);
  j += ",\"fw\":\"" FW_VERSION "\"";
  j += ",\"fwLatest\":\"" + String(fwLatest) + "\"";
  j += ",\"fwStat\":" + String(fwStat);
  j += ",\"fwProg\":" + String(fwProgress);
  j += ",\"fwErr\":\"" + wxJsonEsc(fwErr) + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"rssi\":" + String(WiFi.RSSI());
  j += ",\"games\":" + String(gameCount);
  j += ",\"favidx\":" + String(favIdx);
  j += ",\"spMlb\":" + String(cfgSportMLB ? "true" : "false");
  j += ",\"spNhl\":" + String(cfgSportNHL ? "true" : "false");
  j += ",\"favNhl\":\"" + String(cfgFavTeamNHL) + "\"";
  j += ",\"altLogoNhl\":" + String(cfgAltLogoNHL ? "true" : "false");
  j += ",\"gamesNhl\":" + String(gameCountNHL);
  j += ",\"wideNhl\":" + String(cfgWideNHL ? "true" : "false");
  j += ",\"goalAnim\":" + String(cfgGoalAnim ? "true" : "false");
  j += ",\"situation\":" + String(cfgSituation ? "true" : "false");
  j += ",\"rkCfp\":" + String(cfgRankCFP ? "true" : "false");
  j += ",\"top25Poll\":\"" + String(top25Type) + "\"";
  j += ",\"spPwhl\":" + String(cfgSportPWHL ? "true" : "false");
  j += ",\"favPwhl\":\"" + String(cfgFavTeamPWHL) + "\"";
  j += ",\"livePwhl\":" + String(cfgLiveOnlyPWHL ? "true" : "false");
  j += ",\"gamesPwhl\":" + String(gameCountPWHL);
  j += ",\"spNba\":" + String(cfgSportNBA ? "true" : "false");
  j += ",\"favNba\":\"" + String(cfgFavTeamNBA) + "\"";
  j += ",\"liveNba\":" + String(cfgLiveOnlyNBA ? "true" : "false");
  j += ",\"showStNba\":" + String(cfgShowStandNBA ? "true" : "false");
  j += ",\"gamesNba\":" + String(gameCountNBA);
  j += ",\"spNcaa\":" + String(cfgSportNCAA ? "true" : "false");
  for (int k = 0; k < 6; k++) j += ",\"favNc" + String(k) + "\":\"" + String(cfgFavNCAA[k]) + "\"";
  j += ",\"top25\":[";
  bool top25First = true;   // virgule selon "premier element ECRIT", pas selon k : un rang vide saute ne doit pas laisser [,"..."]
  for (int k = 0; k < top25Count; k++) {
    if (top25Abbr[k][0] == 0) continue;
    String nm = String(top25Name[k]); nm.replace("\"", ""); nm.replace("\\", "");
    j += String(top25First ? "" : ",") + "\"" + String(top25Abbr[k]) + "|" + nm + "\"";
    top25First = false;
  }
  j += "]";
  j += ",\"liveNcaa\":" + String(cfgLiveOnlyNCAA ? "true" : "false");
  j += ",\"showStNcaa\":" + String(cfgShowStandNCAA ? "true" : "false");
  j += ",\"cfAcc\":" + String(cfgNcaaConf[0] ? "true" : "false");
  j += ",\"cfB12\":" + String(cfgNcaaConf[1] ? "true" : "false");
  j += ",\"cfB10\":" + String(cfgNcaaConf[2] ? "true" : "false");
  j += ",\"cfSec\":" + String(cfgNcaaConf[3] ? "true" : "false");
  j += ",\"cfP12\":" + String(cfgNcaaConf[4] ? "true" : "false");
  j += ",\"gamesNcaa\":" + String(gameCountNCAA);
  {
    char pl[64] = "", cy[40] = ""; bool au = true; int st = 0;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      strncpy(pl, wxPlace, sizeof(pl) - 1); strncpy(cy, cfgWxCity, sizeof(cy) - 1);
      au = wxLocIsAuto; st = wxStat;
      xSemaphoreGive(dataMutex);
    }
    j += ",\"wxPlace\":\"" + wxJsonEsc(pl) + "\"";
    j += ",\"wxCity\":\"" + wxJsonEsc(cy) + "\"";
    j += ",\"wxAuto\":" + String(au ? "true" : "false");
    j += ",\"wxStat\":" + String(st);
  }
  j += "}";
  server.send(200, "application/json", j);
}

void handleSet() {
  // Tout le bloc sous dataMutex : les favoris/toggles et les buildRotation*() touchent
  // les memes donnees (games*[], rotList*[]) que netTask reecrit pendant un fetch.
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (server.hasArg("bright")) { cfgBright = constrain(server.arg("bright").toInt(), 5, 255); applyLive(); }
  if (server.hasArg("speed"))  { cfgRotationSec = constrain(server.arg("speed").toInt(), 3, 30); }
  if (server.hasArg("fav"))    { cfgFavTeam = server.arg("fav").toInt(); buildRotation(); }
  if (server.hasArg("liveMlb")) { cfgLiveOnlyMLB = server.arg("liveMlb").toInt() == 1; buildRotation(); }
  if (server.hasArg("liveNhl")) { cfgLiveOnlyNHL = server.arg("liveNhl").toInt() == 1; buildRotationNHL(); }
  if (server.hasArg("screen")) { cfgScreenOn = server.arg("screen").toInt() == 1; }
  if (server.hasArg("wxforce")) { cfgForceWeather = server.arg("wxforce").toInt() == 1; dirty = true; }
  if (server.hasArg("wildcard")) { cfgWildCard = server.arg("wildcard").toInt() == 1; dirty = true; }
  if (server.hasArg("wcNhl")) { cfgWildCardNHL = server.arg("wcNhl").toInt() == 1; dirty = true; }
  if (server.hasArg("showStMlb")) { cfgShowStandMLB = server.arg("showStMlb").toInt() == 1; dirty = true; }
  if (server.hasArg("showStNhl")) { cfgShowStandNHL = server.arg("showStNhl").toInt() == 1; dirty = true; }
  if (server.hasArg("showStNfl")) { cfgShowStandNFL = server.arg("showStNfl").toInt() == 1; dirty = true; }
  if (server.hasArg("spNfl")) { cfgSportNFL = server.arg("spNfl").toInt() == 1; dirty = true; if (cfgSportNFL) lastFetchNFL = 0; }
  if (server.hasArg("liveNfl")) { cfgLiveOnlyNFL = server.arg("liveNfl").toInt() == 1; buildRotationNFL(); dirty = true; }
  if (server.hasArg("favNfl")) {
    String fk = server.arg("favNfl");
    fk.toUpperCase();
    fk.toCharArray(cfgFavTeamNFL, sizeof(cfgFavTeamNFL));
    buildRotationNFL();
  }
  if (server.hasArg("lang")) { cfgLang = server.arg("lang").toInt() == 1 ? 1 : 0; dirty = true; }
  if (server.hasArg("spMlb")) { cfgSportMLB = server.arg("spMlb").toInt() == 1; dirty = true; }
  if (server.hasArg("spNhl")) { cfgSportNHL = server.arg("spNhl").toInt() == 1; dirty = true; if (cfgSportNHL) { lastFetchNHL = 0; lastStandNHL = 0; } }
  if (server.hasArg("altLogoNhl")) { cfgAltLogoNHL = server.arg("altLogoNhl").toInt() == 1; dirty = true; }
  if (server.hasArg("favNhl")) {
    String fn = server.arg("favNhl");
    fn.toUpperCase();
    fn.toCharArray(cfgFavTeamNHL, sizeof(cfgFavTeamNHL));
    buildRotationNHL();
  }
  if (server.hasArg("wideNhl")) { cfgWideNHL = server.arg("wideNhl").toInt() == 1; dirty = true; }
  if (server.hasArg("goalAnim"))  { cfgGoalAnim  = server.arg("goalAnim").toInt() == 1; }
  if (server.hasArg("situation")) { cfgSituation = server.arg("situation").toInt() == 1; dirty = true; }
  if (server.hasArg("testGoal")) {   // bouton "Tester" de la web UI : 0 = LNH, 1 = NFL touche, 2 = NFL placement (equipe favorite, sinon une equipe par defaut)
    int t = server.arg("testGoal").toInt();
    const char* fav = (t == 0) ? cfgFavTeamNHL : cfgFavTeamNFL;
    strncpy(goalReq.abbr, fav[0] ? fav : (t == 0 ? "MTL" : "KC"), 4); goalReq.abbr[4] = 0;
    goalReq.sport = (t == 0) ? 0 : 1; goalReq.kind = (t == 2) ? 1 : 0;
    goalReq.pending = true;
  }
  if (server.hasArg("rkCfp")) { cfgRankCFP = server.arg("rkCfp").toInt() == 1; logAdd(String("ClassementNCAA: toggle -> ") + (cfgRankCFP ? "cfp" : "ap")); if (cfgSportNCAA) rankForceNCAA = true; }   // retelecharge le classement
  if (server.hasArg("spPwhl")) { cfgSportPWHL = server.arg("spPwhl").toInt() == 1; dirty = true; if (cfgSportPWHL) lastFetchPWHL = millis() - FETCH_MS_SLOW - 1; }
  if (server.hasArg("livePwhl")) { cfgLiveOnlyPWHL = server.arg("livePwhl").toInt() == 1; buildRotationPWHL(); }
  if (server.hasArg("favPwhl")) {
    String fw = server.arg("favPwhl");
    fw.toUpperCase();
    fw.toCharArray(cfgFavTeamPWHL, sizeof(cfgFavTeamPWHL));
    buildRotationPWHL();
  }
  if (server.hasArg("spNba")) { cfgSportNBA = server.arg("spNba").toInt() == 1; dirty = true; if (cfgSportNBA) { lastFetchNBA = millis() - FETCH_MS_SLOW - 1; lastStandNBA = millis() - STAND_MS - 1; } }
  if (server.hasArg("liveNba")) { cfgLiveOnlyNBA = server.arg("liveNba").toInt() == 1; buildRotationNBA(); }
  if (server.hasArg("showStNba")) { cfgShowStandNBA = server.arg("showStNba").toInt() == 1; dirty = true; }
  if (server.hasArg("favNba")) {
    String fb = server.arg("favNba");
    fb.toUpperCase();
    fb.toCharArray(cfgFavTeamNBA, sizeof(cfgFavTeamNBA));
    buildRotationNBA();
  }
  if (server.hasArg("spNcaa")) { cfgSportNCAA = server.arg("spNcaa").toInt() == 1; dirty = true; if (cfgSportNCAA) { lastFetchNCAA = millis() - FETCH_MS_SLOW - 1; lastStandNCAA = millis() - STAND_MS - 1; } }
  if (server.hasArg("liveNcaa")) { cfgLiveOnlyNCAA = server.arg("liveNcaa").toInt() == 1; buildRotationNCAA(); }
  if (server.hasArg("showStNcaa")) { cfgShowStandNCAA = server.arg("showStNcaa").toInt() == 1; buildRotationNCAA(); if (cfgSportNCAA) lastFetchNCAA = millis() - FETCH_MS_SLOW - 1; }
  {
    static const char* const cfKeys[5] = {"cfAcc", "cfB12", "cfB10", "cfSec", "cfP12"};
    bool confChanged = false;
    for (int i = 0; i < 5; i++)
      if (server.hasArg(cfKeys[i])) { cfgNcaaConf[i] = server.arg(cfKeys[i]).toInt() == 1; confChanged = true; }
    if (confChanged) { buildRotationNCAA(); dirty = true; if (cfgSportNCAA) lastFetchNCAA = millis() - FETCH_MS_SLOW - 1; }   // refetch pour appliquer le filtre
  }
  for (int k = 0; k < 6; k++) {
    char key[8]; snprintf(key, sizeof(key), "favNc%d", k);
    if (!server.hasArg(key)) continue;
    String fn = server.arg(key);
    fn.toUpperCase();
    fn.toCharArray(cfgFavNCAA[k], 6);
    buildRotationNCAA();
    if (cfgSportNCAA) lastFetchNCAA = millis() - FETCH_MS_SLOW - 1;   // sa conference n'est peut-etre pas encore telechargee
  }
  if (server.hasArg("wxCity")) {   // ville de la meteo ("" = automatique par IP)
    String c = server.arg("wxCity");
    c.trim();
    c.toCharArray(cfgWxCity, sizeof(cfgWxCity));   // tronque a 39 caracteres (dataMutex deja pris en haut de handleSet)
    wxCityDirty = true;
    wxStat = 1;                                    // "recherche..."
    lastWx = millis() - WX_MS - 1;                 // netTask re-resout a son prochain passage (~1 s)
  }
  dirty = true;
  xSemaphoreGive(dataMutex);
  // Sauvegarde NVS differee (voir flushCfg). Le bouton "Tester" seul ne change aucun reglage.
  if (!(server.args() == 1 && server.hasArg("testGoal"))) { cfgSaveAt = millis(); cfgSavePending = true; }
  server.send(200, "text/plain", "ok");
}

// ====================== WIFI : identifiants NVS ======================
void loadWifi() {
  prefs.begin("scoreboard", true);
  staSsid = prefs.getString("wssid", WIFI_SSID);   // defaut = 1er flash
  staPass = prefs.getString("wpass", WIFI_PASS);
  prefs.end();
}
void saveWifi(const String& s, const String& p) {
  prefs.begin("scoreboard", false);
  prefs.putString("wssid", s);
  prefs.putString("wpass", p);
  prefs.end();
}
void clearWifi() {
  // Ecrit des chaines vides (pas remove) pour forcer le portail au reboot.
  prefs.begin("scoreboard", false);
  prefs.putString("wssid", "");
  prefs.putString("wpass", "");
  prefs.end();
}

// ====================== WIFI : connexion STA ======================
void bootWifiAnim();   // ecran de demarrage (defini plus bas)

// ====================== DNS PUBLIC, DHCP CONSERVE ======================
// Impose 1.1.1.1 (principal) et 8.8.8.8 (secours) SANS toucher a l'adresse IP : le
// DHCP reste actif, le bail est renouvele normalement et le tableau suit un changement
// de routeur ou de plage d'adresses. Le DNS fourni par le DHCP (routeur, Pi-hole...)
// est garde en 3e position : sur un reseau qui bloque les DNS publics, la resolution
// de noms continue de fonctionner.
// A chaque bail (connexion, reconnexion, renouvellement) le DHCP reecrit son propre
// DNS, sans prevenir : cette fonction est donc rappelee a chaque tour de netTask et ne
// fait rien si 1.1.1.1 est deja en place. Renvoie true si elle a du le (re)mettre.
bool ensurePublicDns() {
  esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!nif) return false;
  const uint32_t want1 = (uint32_t)IPAddress(1, 1, 1, 1), want2 = (uint32_t)IPAddress(8, 8, 8, 8);
  esp_netif_dns_info_t cur = {};
  if (esp_netif_get_dns_info(nif, ESP_NETIF_DNS_MAIN, &cur) != ESP_OK) return false;
  uint32_t curMain = (cur.ip.type == ESP_IPADDR_TYPE_V4) ? cur.ip.u_addr.ip4.addr : 0;
  if (curMain == want1) return false;                 // deja en place

  esp_netif_dns_info_t d = {};
  d.ip.type = ESP_IPADDR_TYPE_V4;
  d.ip.u_addr.ip4.addr = want1;
  bool ok = esp_netif_set_dns_info(nif, ESP_NETIF_DNS_MAIN, &d) == ESP_OK;
  d.ip.u_addr.ip4.addr = want2;
  ok = (esp_netif_set_dns_info(nif, ESP_NETIF_DNS_BACKUP, &d) == ESP_OK) && ok;
  bool kept = false;
  if (curMain != 0 && curMain != want2) {             // DNS du DHCP : garde en dernier recours
    d.ip.u_addr.ip4.addr = curMain;
    kept = esp_netif_set_dns_info(nif, ESP_NETIF_DNS_FALLBACK, &d) == ESP_OK;
  }
  logAdd("DNS 1.1.1.1 / 8.8.8.8 : " + String(ok ? "ok" : "echec") +
         (kept ? ", DNS du reseau " + IPAddress(curMain).toString() + " garde en secours" : String("")));
  return true;
}
bool connectSTA() {
  loadWifi();
  if (staSsid.length() == 0) return false;       // vide -> portail
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);   // explicite : se reconnecte seul si le routeur redemarre en cours de route
  WiFi.begin(staSsid.c_str(), staPass.c_str());
  Serial.printf("Connexion a \"%s\" ", staSsid.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - t0 < (unsigned long)WIFI_CONNECT_TIMEOUT_S * 1000) {
    delay(300); Serial.print(".");
    bootWifiAnim();   // logo WiFi blanc anime pendant la recherche
  }
  Serial.println();
  bool ok = WiFi.status() == WL_CONNECTED;
  logAdd("WiFi \"" + staSsid + "\": " + (ok ? "connecte" : "echec") +
         " en " + String(millis() - t0) + "ms");

  if (ok) {
    // Force un DNS public (Cloudflare + Google) au lieu du DNS fourni par
    // le DHCP (souvent Pi-hole), qui echouait de facon repetee et bornee
    // a ~120s sur certains hotes API. On garde l'IP, la
    // passerelle et le masque DHCP intacts : seul le DNS change.
    // Le DHCP reste ACTIF (voir ensurePublicDns) : plus de WiFi.config(), qui figeait
    // l'adresse IP en statique (bail jamais renouvele -> conflit d'adresse possible).
    ensurePublicDns();
  }
  return ok;
}

// ====================== PORTAIL CAPTIF ======================
void portalShowPanel() {
  dma->fillScreen(0);
  dma->setTextSize(1);
  dma->setTextColor(C(239, 159, 39));
  dma->setCursor(2, 1);  dma->print("Config WiFi");
  dma->setTextColor(C(255, 255, 255));
  dma->setCursor(2, 12); dma->print(AP_NAME);
  dma->setTextColor(C(150, 150, 150));
  dma->setCursor(2, 23); dma->print("192.168.4.1");
}

void handlePortalRoot() { server.send_P(200, "text/html", PORTAL_HTML); }

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    String ss = WiFi.SSID(i);
    ss.replace("\\", "\\\\");
    ss.replace("\"", "\\\"");
    j += "{\"ssid\":\"" + ss + "\",\"rssi\":" + String(WiFi.RSSI(i));
    j += ",\"enc\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1) + "}";
  }
  j += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleSave() {
  String s = server.arg("ssid");
  String p = server.arg("pass");
  if (s.length() == 0) { server.send(400, "text/plain", "SSID requis"); return; }
  saveWifi(s, p);
  logAdd("WiFi enregistre: \"" + s + "\". Redemarrage.");
  server.send(200, "text/plain",
              "Enregistre. Le tableau redemarre et se connecte a " + s + ".");
  delay(800);
  ESP.restart();
}

// Capture toutes les autres requetes -> declenche le popup "se connecter au reseau"
void handleCaptiveRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void startConfigPortal() {
  apMode = true;
  // connectSTA() a active setAutoReconnect(true) puis a echoue : sans ceci le
  // STA continue de chercher le reseau enregistre en arriere-plan (scan de
  // tous les canaux) pendant que le portail tourne. Sur l'ESP32 l'AP suit le
  // canal du STA : chaque scan sort la radio du canal de l'AP et le telephone
  // perd des paquets (portail qui ne charge pas). On coupe donc la recherche
  // ici ; portalRetrySaved() retente le reseau connu seulement quand personne
  // n'est connecte au portail.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP_STA);   // AP_STA (et non AP seul) pour que le scan fonctionne
  bool secured = (strlen(AP_PASS) >= 8);
  WiFi.softAP(AP_NAME, secured ? AP_PASS : nullptr);
  delay(300);
  IPAddress ip = WiFi.softAPIP();        // 192.168.4.1
  dnsServer.start(53, "*", ip);          // tout domaine -> portail

  Serial.println("=== PORTAIL DE CONFIG WIFI ===");
  Serial.printf("Reseau   : %s\n", AP_NAME);
  Serial.printf("Mot passe: %s\n", secured ? AP_PASS : "(ouvert)");
  Serial.print ("Adresse  : http://"); Serial.println(ip);

  portalShowPanel();

  server.on("/", handlePortalRoot);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.onNotFound(handleCaptiveRedirect);
  server.begin();
}

// Portail actif MAIS un reseau est deja enregistre : c'est presque toujours
// une panne de courant. L'ESP32 redemarre en quelques secondes, le routeur
// en 1-3 minutes -> les 18 s de connectSTA() expiraient avant que le reseau
// existe, et on restait coince en portail pour toujours (d'ou le
// debrancher/rebrancher). On retente donc le reseau connu toutes les 30 s ;
// des que ca connecte -> redemarrage propre en mode normal (le portail et
// ses scans s'arretent). Pas de tentative pendant qu'un telephone est
// connecte au portail : la recherche du routeur change de canal et
// couperait la page de config. Reseau jamais configure (SSID vide) : portail
// seulement, comme avant.
void portalRetrySaved() {
  static bool trying = false;
  static unsigned long tryStart = 0, lastTry = 0;
  if (staSsid.length() == 0) return;

  if (WiFi.status() == WL_CONNECTED) {
    logAdd("Portail: reseau \"" + staSsid + "\" revenu -> redemarrage en mode normal");
    delay(300);
    ESP.restart();
  }
  if (trying) {
    if (millis() - tryStart >= 15000) {          // echec de cette tentative
      trying = false;
      lastTry = millis();
      WiFi.disconnect(false);                     // arrete la recherche STA, le portail (AP) reste actif
    }
    return;
  }
  if (WiFi.softAPgetStationNum() > 0) return;     // quelqu'un utilise le portail
  if (millis() - lastTry < 30000) return;
  trying = true;
  tryStart = millis();
  WiFi.begin(staSsid.c_str(), staPass.c_str());
}
