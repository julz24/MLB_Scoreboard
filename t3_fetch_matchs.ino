// ============================================================
//  t3_fetch_matchs.ino - Reseau : telechargement HTTPS commun, matchs MLB / NFL (ESPN) / NHL.
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

// Le watchdog n'est surveille que sur netTask : pendant un long telechargement
// on le "nourrit" regulierement (le 2 panneaux avait un temoin RTC detaille ;
// ici un simple reset suffit - voir netTask).
// Seulement si la tache courante est inscrite au watchdog (netTask) : les fetchs faits pendant
// setup() tournent dans la tache loop, non inscrite ("task_wdt: task not found" sinon).
static inline void netStep(const char*) { if (esp_task_wdt_status(NULL) == ESP_OK) esp_task_wdt_reset(); }

// Le core ESP32 (3.3.x) remet tout le contexte TLS a zero dans stop(), y compris le lien vers le
// paquet de certificats : il faut le rattacher avant CHAQUE connexion verifiee, sinon la
// verification du certificat echoue (HTTP -1 "connection refused").
static void tlsVerified(WiFiClientSecure& c) {
  c.setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
}

// Texte de la derniere erreur TLS du client (pour les logs).
static String tlsError(WiFiClientSecure& c) {
  char b[100] = "";
  int e = c.lastError(b, sizeof(b));
  return e ? String(" [TLS ") + e + " " + b + "]" : String("");
}

// ====================== TELECHARGEMENT HTTPS (commun a tous les fetchs) ======================
// Telecharge une reponse HTTPS dans un tampon PSRAM (a liberer avec free()).
// Poignee de main TLS bornee a 10 s, lecture bornee a 30 s au total, et
// etapes marquees pour le temoin watchdog. nullptr en cas d'echec.
// verifyTls = true : certificat du serveur verifie (paquet de certificats racine) - utilise pour
// les mises a jour du firmware. Sinon setInsecure(), comme pour les pointages et la meteo.
static uint8_t* httpsDownload(const char* tag, const char* host, const String& url, int cap, int& outLen, unsigned long budgetMs = 45000, bool verifyTls = false) {
  outLen = 0;
  netStep(tag);
  WiFiClientSecure client;
  if (verifyTls) tlsVerified(client);
  else           client.setInsecure();
  client.setTimeout(10000);
  client.setHandshakeTimeout(10);
  // Connexion ouverte d'avance (mesure du temps de connexion dans les logs). Pas en TLS verifie :
  // HTTPClient la jugeait fermee et la refaisait sans paquet de certificats ("No CA Chain is
  // set") ; on le laisse alors ouvrir lui-meme la connexion au GET.
  if (!verifyTls) {
    unsigned long tConn = millis();
    bool connOk = client.connect(host, 443, 5000);
    logAdd(String(tag) + ": connexion " + String(connOk ? "ok" : "ECHEC") + " en " + String(millis() - tConn) + "ms, RSSI " + String(WiFi.RSSI()) + "dBm");
    if (!connOk) return nullptr;
    netStep(tag);
  }

  HTTPClient https;
  int code = 0, len = 0;
  // 2 essais : si la connexion ouverte ci-dessus a ete fermee avant la requete (code < 0),
  // HTTPClient rouvre la sienne (paquet de certificats rattache, voir tlsVerified).
  if (!verifyTls && !client.connected()) logAdd(String(tag) + ": connexion deja fermee avant la requete" + tlsError(client));
  for (int essai = 1; essai <= 2; essai++) {
    if (!https.begin(client, url)) { logAdd(String(tag) + ": begin echec"); client.stop(); return nullptr; }
    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    https.setTimeout(15000);
    https.setConnectTimeout(5000);
    https.useHTTP10(true);
    unsigned long tGet = millis();
    code = https.GET();
    len = https.getSize();
    logAdd(String(tag) + ": GET " + String(millis() - tGet) + "ms, HTTP " + String(code) + ", taille " + String(len) +
           (code < 0 ? " (" + HTTPClient::errorToString(code) + ")" + tlsError(client) : String("")));
    if (code >= 0 || essai == 2) break;
    https.end();
    client.stop();
    if (verifyTls) tlsVerified(client);
    netStep(tag);
    logAdd(String(tag) + ": nouvel essai");
  }
  if (code != 200) { https.end(); client.stop(); return nullptr; }

  if (len <= 0 || len > cap) len = cap;
  uint8_t* buf = (uint8_t*) ps_malloc(len + 1);
  if (!buf) { logAdd(String(tag) + ": ps_malloc echec"); https.end(); client.stop(); return nullptr; }
  netStep(tag);
  WiFiClient* stream = https.getStreamPtr();
  int idx = 0;
  unsigned long t0 = millis(), tStart = millis();
  // Fin de reponse detectee par le JSON lui-meme (accolade racine refermee) :
  // avant, la connexion n'etait pas vue comme fermee et on attendait 15 s
  // d'inactivite apres chaque telechargement. Budget total 45 s par defaut.
  int depth = 0; bool inStr = false, esc = false, started = false, complete = false;
  unsigned long tFeed = millis();
  while (idx < len && !complete && (millis() - t0) < 15000 && (millis() - tStart) < budgetMs) {
    if (millis() - tFeed > 10000) { tFeed = millis(); netStep(tag); }   // telechargement lent mais vivant : on nourrit le watchdog
    int avail = stream->available();
    if (avail > 0) {
      int toRead = (avail < (len - idx)) ? avail : (len - idx);
      int r = stream->read(buf + idx, toRead);
      if (r > 0) {
        for (int k = idx; k < idx + r && !complete; k++) {
          char ch = (char)buf[k];
          if (inStr) { if (esc) esc = false; else if (ch == '\\') esc = true; else if (ch == '"') inStr = false; }
          else if (ch == '"') inStr = true;
          else if (ch == '{' || ch == '[') { depth++; started = true; }
          else if (ch == '}' || ch == ']') { depth--; if (started && depth <= 0) { complete = true; idx = k + 1; r = 0; } }
        }
        if (!complete) idx += r;
        t0 = millis();
      }
    } else if (!stream->connected()) break;
    else delay(2);
  }
  buf[idx] = 0;
  logAdd(String(tag) + ": " + String(idx) + " octets en " + String(millis() - tStart) + "ms" + (complete ? "" : " (INCOMPLET)"));
  https.end();
  client.stop();
  outLen = idx;
  return buf;
}

// ====================== FETCH MLB ======================
bool fetchGames() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGames: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tmNow);

  String url = "https://statsapi.mlb.com/api/v1/schedule?sportId=1&date=";
  url += dateStr;
  url += "&hydrate=linescore";

  int idx = 0;
  uint8_t* buf = httpsDownload("fetchGames", "statsapi.mlb.com", url, 400000, idx);
  if (!buf) return false;

  // Filtre
  JsonDocument filter;
  JsonObject gf = filter["dates"][0]["games"][0].to<JsonObject>();
  gf["gameDate"] = true;
  gf["gameType"] = true;
  gf["status"]["abstractGameState"] = true;
  gf["status"]["detailedState"] = true;
  gf["teams"]["away"]["team"]["id"] = true;
  gf["teams"]["away"]["team"]["name"] = true;
  gf["teams"]["away"]["score"] = true;
  gf["teams"]["home"]["team"]["id"] = true;
  gf["teams"]["home"]["team"]["name"] = true;
  gf["teams"]["home"]["score"] = true;
  JsonObject ls = gf["linescore"].to<JsonObject>();
  ls["currentInning"] = true;
  ls["isTopInning"] = true;
  ls["outs"] = true;
  ls["offense"]["first"]["id"] = true;
  ls["offense"]["second"]["id"] = true;
  ls["offense"]["third"]["id"] = true;

  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, idx,
                                             DeserializationOption::Filter(filter));
  free(buf);
  if (err) { logAdd("fetchGames: JSON " + String(err.c_str())); return false; }

  JsonArray arr = doc["dates"][0]["games"].as<JsonArray>();
  int n = 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);   // debut section critique : ecriture games[]
  for (JsonObject gm : arr) {
    if (n >= MAX_GAMES) break;
    Game g;
    g.awayId    = gm["teams"]["away"]["team"]["id"] | 0;
    g.homeId    = gm["teams"]["home"]["team"]["id"] | 0;
    g.awayScore = gm["teams"]["away"]["score"] | -1;
    g.homeScore = gm["teams"]["home"]["score"] | -1;

    // Match des etoiles (gameType "A") : les equipes AL/NL n'ont pas de logo
    // dans mlbLogoById() - on detecte via le nom d'equipe et on marque g.allStar
    // pour afficher les logos Ligue Americaine / Nationale a la place.
    g.allStar = 0;
    const char* gtype = gm["gameType"];
    if (gtype && strcmp(gtype, "A") == 0) {
      const char* awayName = gm["teams"]["away"]["team"]["name"] | "";
      // "American League" contient "American" ; "National League" contient "National"
      if (strstr(awayName, "American")) g.allStar = 1;      // away = AL, home = NL
      else if (strstr(awayName, "National")) g.allStar = 2; // away = NL, home = AL
      else g.allStar = 1;  // repli : suppose l'ordre habituel AL(visiteur)/NL(local)
    }

    // Retard/pluie/suspension/report : detailedState va plus loin que
    // abstractGameState (qui ne dit que avant/en cours/termine).
    g.delayState = 0;
    const char* detState = gm["status"]["detailedState"] | "";
    if (strncmp(detState, "Delayed Start", 13) == 0)      g.delayState = 1;
    else if (strncmp(detState, "Delayed", 7) == 0)        g.delayState = 2;
    else if (strncmp(detState, "Suspended", 9) == 0)      g.delayState = 3;
    else if (strcmp(detState, "Postponed") == 0)          g.delayState = 4;

    const char* gst = gm["status"]["abstractGameState"];
    if (gst && strcmp(gst, "Final") == 0)      g.state = 2;
    else if (gst && strcmp(gst, "Live") == 0)  g.state = 1;
    else                                        g.state = 0;

    g.inning    = gm["linescore"]["currentInning"] | 0;
    g.topInning = gm["linescore"]["isTopInning"] | true;
    g.outs      = gm["linescore"]["outs"] | 0;
    g.b1 = !gm["linescore"]["offense"]["first"].isNull();
    g.b2 = !gm["linescore"]["offense"]["second"].isNull();
    g.b3 = !gm["linescore"]["offense"]["third"].isNull();

    strcpy(g.timeStr, "--:--");
    const char* gd = gm["gameDate"];
    if (g.state == 0 && gd && strlen(gd) >= 16) {
      int uh = atoi(gd + 11), um = atoi(gd + 14);
      long mins = uh * 60 + um + tzOffsetSec / 60;
      mins = ((mins % 1440) + 1440) % 1440;
      snprintf(g.timeStr, sizeof(g.timeStr), "%02ld:%02ld", mins / 60, mins % 60);
    }
    games[n++] = g;
  }
  gameCount = n;
  buildRotation();
  dirty = true;
  logAdd("Matchs: " + String(gameCount) + " (favori idx=" + String(favIdx) + ", rotation=" + String(rotCount) + ")");
  xSemaphoreGive(dataMutex);                  // fin section critique
  return true;
}

// ====================== FETCH NFL (API publique ESPN) ======================
// La NFL n'a pas d'API ouverte comme le MLB/NHL : on passe par l'endpoint
// public "scoreboard" d'ESPN. La reponse est nettement plus grosse (beaucoup
// de metadonnees diffusion/cotes) - le filtre ArduinoJson ne garde que le
// strict necessaire.
// Numero de jour (depuis 1970) d'une date civile - sert a comparer la date
// locale d'un match NFL avec celle d'aujourd'hui.
static long nflDaysFromCivil(int y, int m, int d) {
  y -= (m <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + doe - 719468L;
}

// ---- Code commun aux trois scoreboards ESPN (NFL / NBA / NCAA) ----
// Filtre ArduinoJson : ne garde que le strict necessaire de chaque evenement.
static void espnScoreboardFilter(JsonDocument& filter) {
  JsonObject ef = filter["events"][0].to<JsonObject>();
  ef["date"] = true;
  JsonObject cf = ef["competitions"][0].to<JsonObject>();
  JsonObject cpf = cf["competitors"][0].to<JsonObject>();
  cpf["homeAway"] = true;
  cpf["score"] = true;
  cpf["team"]["abbreviation"] = true;
  cpf["team"]["id"] = true;
  cpf["id"] = true;
  cf["situation"]["possession"] = true;     // id de l'equipe qui a le ballon (present seulement en direct)
  JsonObject sf = cf["status"].to<JsonObject>();
  sf["displayClock"] = true;
  sf["period"] = true;
  sf["type"]["state"] = true;
  sf["type"]["name"] = true;
}

// Convertit un evenement ESPN en GameESPN. Renvoie false si l'evenement est a ignorer :
// pas de competition, ou match d'un autre jour que todayDay qui n'est pas en cours (ESPN
// renvoie toute la semaine NFL ; un match qui passe minuit reste affiche jusqu'a la fin).
// abbrMax = longueur maximale de l'abreviation gardee (3 NFL, 4 NBA, 5 NCAA).
static bool espnParseEvent(JsonObject ev, long todayDay, GameESPN& g, int abbrMax) {
  JsonObject comp = ev["competitions"][0];
  if (comp.isNull()) return false;
  const char* st = comp["status"]["type"]["state"] | "pre";
  const char* dt = ev["date"];   // "2026-09-07T17:00Z" (UTC)
  bool hasDate = dt && strlen(dt) >= 16;
  if (hasDate && strcmp(st, "in") != 0) {
    long secs = nflDaysFromCivil(atoi(dt), atoi(dt + 5), atoi(dt + 8)) * 86400L
                + atoi(dt + 11) * 3600L + atoi(dt + 14) * 60L + tzOffsetSec;
    long gameDay = (secs >= 0) ? secs / 86400L : -((-secs + 86399L) / 86400L);
    if (gameDay != todayDay) return false;
  }

  strcpy(g.awayAbbr, "???");
  strcpy(g.homeAbbr, "???");
  g.awayScore = -1; g.homeScore = -1;
  g.poss = 0;
  strcpy(g.timeStr, "--:--");
  // Possession : situation.possession = id d'equipe ESPN (texte ou nombre selon l'endpoint) ; on compare a l'id de chaque participant.
  String possId = "";
  { JsonVariant pv = comp["situation"]["possession"]; if (!pv.isNull()) possId = pv.as<String>(); }

  // ESPN ne distingue pas visiteur/local par l'ordre du tableau : c'est le
  // champ homeAway de chaque participant qui fait foi.
  for (JsonObject c : comp["competitors"].as<JsonArray>()) {
    const char* ab = c["team"]["abbreviation"] | "???";
    int sc = atoi(c["score"] | "0");                  // ESPN renvoie le pointage en texte
    bool home = strcmp(c["homeAway"] | "home", "home") == 0;
    char* dst = home ? g.homeAbbr : g.awayAbbr;
    strncpy(dst, ab, abbrMax); dst[abbrMax] = 0;
    if (home) g.homeScore = sc; else g.awayScore = sc;
    if (possId.length()) {
      JsonVariant tv = c["team"]["id"]; if (tv.isNull()) tv = c["id"];
      String cid = tv.isNull() ? String("") : tv.as<String>();
      if (cid == possId) g.poss = home ? 2 : 1;
    }
  }

  g.state = (strcmp(st, "post") == 0) ? 2 : (strcmp(st, "in") == 0) ? 1 : 0;
  g.period = (uint8_t)(comp["status"]["period"] | 0);
  const char* clk = comp["status"]["displayClock"] | "--:--";
  strncpy(g.clockStr, clk, sizeof(g.clockStr) - 1); g.clockStr[sizeof(g.clockStr) - 1] = 0;
  const char* nm = comp["status"]["type"]["name"] | "";
  g.halftime = (strstr(nm, "HALFTIME") != nullptr);
  g.wasOT = (g.state == 2 && g.period > 4);
  if (g.state != 1 || g.halftime) g.poss = 0;

  if (g.state == 0 && hasDate) {                      // heure locale du debut
    long mins = atoi(dt + 11) * 60 + atoi(dt + 14) + tzOffsetSec / 60;
    mins = ((mins % 1440) + 1440) % 1440;
    snprintf(g.timeStr, sizeof(g.timeStr), "%02ld:%02ld", mins / 60, mins % 60);
  }
  return true;
}

bool fetchGamesNFL() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGamesNFL: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();

  int blen = 0;
  uint8_t* buf = httpsDownload("fetchGamesNFL", "site.api.espn.com",
                               "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard", 600000, blen);
  if (!buf) return false;

  JsonDocument filter;
  espnScoreboardFilter(filter);
  JsonDocument doc(&psramAllocator);
  // ESPN imbrique son JSON beaucoup plus profondement que MLB/NHL - la limite par
  // defaut d'ArduinoJson (10 niveaux) s'applique meme aux branches filtrees pendant
  // qu'il les saute (constate sur le vrai panneau : "JSON TooDeep"). 30 = bonne marge.
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(30));
  free(buf);
  if (err) { logAdd("fetchGamesNFL: JSON " + String(err.c_str())); return false; }

  long todayDay = nflDaysFromCivil(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
  int n = 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  // Pointages du fetch precedent (cle = visiteur+local) pour detecter un touche / placement du favori.
  struct PrevScoreNFL { char a[6], h[6]; int as, hs; uint8_t st; } prevNfl[MAX_GAMES_NFL];
  int prevNflN = 0;
  for (int i = 0; i < gameCountNFL; i++) {
    strcpy(prevNfl[prevNflN].a, gamesNFL[i].awayAbbr); strcpy(prevNfl[prevNflN].h, gamesNFL[i].homeAbbr);
    prevNfl[prevNflN].as = gamesNFL[i].awayScore; prevNfl[prevNflN].hs = gamesNFL[i].homeScore; prevNfl[prevNflN].st = gamesNFL[i].state;
    prevNflN++;
  }
  for (JsonObject ev : doc["events"].as<JsonArray>()) {
    if (n >= MAX_GAMES_NFL) break;
    GameNFL g;
    if (!espnParseEvent(ev, todayDay, g, 3)) continue;

    // Touche / placement du favori : +3 pts ou plus depuis le fetch precedent (le point supplementaire seul n'anime pas).
    if (cfgFavTeamNFL[0] && g.state == 1) {
      bool favHome = strcmp(g.homeAbbr, cfgFavTeamNFL) == 0, favAway = strcmp(g.awayAbbr, cfgFavTeamNFL) == 0;
      for (int i = 0; i < prevNflN && (favHome || favAway); i++) {
        if (strcmp(prevNfl[i].a, g.awayAbbr) != 0 || strcmp(prevNfl[i].h, g.homeAbbr) != 0) continue;
        if (prevNfl[i].st == 1) {
          int dl = favHome ? g.homeScore - prevNfl[i].hs : g.awayScore - prevNfl[i].as;
          if (dl >= 3 && (favHome ? prevNfl[i].hs : prevNfl[i].as) >= 0) {
            strncpy(goalReq.abbr, cfgFavTeamNFL, 4); goalReq.abbr[4] = 0;
            goalReq.sport = 1; goalReq.kind = (dl >= 6) ? 0 : 1; goalReq.pending = true;
            logAdd("NFL: " + String(dl >= 6 ? "TOUCHE " : "PLACEMENT ") + String(cfgFavTeamNFL));
          }
        }
        break;
      }
    }
    gamesNFL[n++] = g;
  }
  gameCountNFL = n;
  buildRotationNFL();
  dirty = true;
  logAdd("NFL: " + String(gameCountNFL) + " matchs (favori idx=" + String(favIdxNFL) + ", rotation=" + String(rotCountNFL) + ")");
  xSemaphoreGive(dataMutex);
  return true;
}

// ====================== FETCH NHL ======================
// api-web.nhle.com : API "moderne" de la NHL (statsapi.web.nhl.com est
// depreciee). Meme pattern de connexion/lecture PSRAM que fetchGames().
bool fetchGamesNHL() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGamesNHL: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tmNow);

  String url = "https://api-web.nhle.com/v1/score/";
  url += dateStr;

  int idx = 0;
  uint8_t* buf = httpsDownload("fetchGamesNHL", "api-web.nhle.com", url, 300000, idx);
  if (!buf) return false;

  // Filtre : garde seulement les champs utiles (voir schema confirme
  // sur /v1/score/YYYY-MM-DD : gameState, period, clock.*, gameOutcome.*,
  // awayTeam/homeTeam.abbrev+score).
  JsonDocument filter;
  JsonObject gf = filter["games"][0].to<JsonObject>();
  gf["id"] = true;
  gf["gameState"] = true;
  gf["startTimeUTC"] = true;
  gf["period"] = true;
  gf["clock"]["timeRemaining"] = true;
  gf["clock"]["running"] = true;
  gf["clock"]["inIntermission"] = true;
  gf["gameOutcome"]["lastPeriodType"] = true;
  gf["awayTeam"]["abbrev"] = true;
  gf["awayTeam"]["score"] = true;
  gf["homeTeam"]["abbrev"] = true;
  gf["homeTeam"]["score"] = true;

  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, idx,
                                             DeserializationOption::Filter(filter));
  free(buf);
  if (err) { logAdd("fetchGamesNHL: JSON " + String(err.c_str())); return false; }

  JsonArray arr = doc["games"].as<JsonArray>();
  int n = 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  // Pointages du fetch precedent (par id de match) pour detecter un but du favori.
  struct PrevScoreNHL { uint32_t id; int a, h; uint8_t st; } prevNhl[MAX_GAMES_NHL];
  int prevNhlN = 0;
  for (int i = 0; i < gameCountNHL; i++) prevNhl[prevNhlN++] = { gamesNHL[i].id, gamesNHL[i].awayScore, gamesNHL[i].homeScore, gamesNHL[i].state };
  for (JsonObject gm : arr) {
    if (n >= MAX_GAMES_NHL) break;
    GameNHL g;
    g.id = gm["id"] | 0;
    strncpy(g.awayAbbr, gm["awayTeam"]["abbrev"] | "???", 3); g.awayAbbr[3] = 0;
    strncpy(g.homeAbbr, gm["homeTeam"]["abbrev"] | "???", 3); g.homeAbbr[3] = 0;
    g.awayScore = gm["awayTeam"]["score"] | -1;
    g.homeScore = gm["homeTeam"]["score"] | -1;

    const char* gst = gm["gameState"] | "FUT";
    if (strcmp(gst, "OFF") == 0 || strcmp(gst, "FINAL") == 0)      g.state = 2;
    else if (strcmp(gst, "LIVE") == 0 || strcmp(gst, "CRIT") == 0) g.state = 1;
    else                                                            g.state = 0;

    g.period = (uint8_t)(gm["period"] | 0);
    const char* clk = gm["clock"]["timeRemaining"] | "--:--";
    strncpy(g.clockStr, clk, 5); g.clockStr[5] = 0;
    // inIntermission est un BOOLEEN JSON (true/false, verifie sur l'API en
    // direct pendant un entracte : {"timeRemaining":"09:09","running":false,
    // "inIntermission":true}, period reste a 2). Avec "| 0" ArduinoJson
    // demande un int : un booleen n'est pas un int -> retournait toujours le
    // defaut 0 -> jamais d'entracte, et le decompte de l'entracte s'affichait
    // comme temps restant a la periode. "| false" lit bien le booleen.
    g.intermission = gm["clock"]["inIntermission"] | false;
    // Chrono local entre deux fetchs : temps restant "MM:SS" -> secondes.
    g.clockRunning = gm["clock"]["running"] | false;
    g.clockSecs = -1;
    if (strlen(g.clockStr) == 5 && g.clockStr[2] == ':')
      g.clockSecs = atoi(g.clockStr) * 60 + atoi(g.clockStr + 3);
    g.clockSyncMs = millis();
    // DIAGNOSTIC chrono : compare le chrono LOCAL (avant ce fetch) a la valeur de
    // l'API. dt = secondes reelles ecoulees depuis le fetch precedent ; api =
    // de combien le chrono de l'API a baisse dans ce meme temps. Si api < dt, le
    // jeu a ete arrete (sifflet) pendant que notre chrono local continuait ;
    // ecart > 0 = notre affichage etait en avance sur l'API.
    if (n < gameCountNHL && gamesNHL[n].state == 1 && g.state == 1 && !g.intermission &&
        !gamesNHL[n].intermission && gamesNHL[n].period == g.period &&
        strcmp(gamesNHL[n].homeAbbr, g.homeAbbr) == 0 &&
        gamesNHL[n].clockSecs >= 0 && g.clockSecs >= 0) {
      long dtS  = (long)((g.clockSyncMs - gamesNHL[n].clockSyncMs) / 1000UL);
      long pred = gamesNHL[n].clockSecs - (gamesNHL[n].clockRunning ? dtS : 0);
      if (pred < 0) pred = 0;
      logAdd("NHL chrono " + String(g.homeAbbr) + ": local=" + String(pred) + "s API=" + String(g.clockSecs) +
             "s ecart=" + String((long)g.clockSecs - pred) + "s | dt=" + String(dtS) + "s api=-" +
             String(gamesNHL[n].clockSecs - g.clockSecs) + "s run=" + String(g.clockRunning ? 1 : 0));
    }

    // But du favori : son pointage a augmente depuis le fetch precedent, match en direct des deux cotes
    // (le 1er fetch apres demarrage n'a pas de "precedent" -> pas d'animation fantome).
    if (cfgFavTeamNHL[0] && g.state == 1 && g.id) {
      bool favHome = strcmp(g.homeAbbr, cfgFavTeamNHL) == 0, favAway = strcmp(g.awayAbbr, cfgFavTeamNHL) == 0;
      for (int i = 0; i < prevNhlN && (favHome || favAway); i++) {
        if (prevNhl[i].id != g.id) continue;
        if (prevNhl[i].st == 1 && ((favHome && prevNhl[i].h >= 0 && g.homeScore > prevNhl[i].h) ||
                                   (favAway && prevNhl[i].a >= 0 && g.awayScore > prevNhl[i].a))) {
          strncpy(goalReq.abbr, cfgFavTeamNHL, 4); goalReq.abbr[4] = 0;
          goalReq.sport = 0; goalReq.kind = 0; goalReq.pending = true;
          logAdd("NHL: BUT " + String(cfgFavTeamNHL));
        }
        break;
      }
    }

    const char* lastType = gm["gameOutcome"]["lastPeriodType"] | "REG";
    g.wasOT = (strcmp(lastType, "OT") == 0);
    g.wasSO = (strcmp(lastType, "SO") == 0);

    strcpy(g.timeStr, "--:--");
    const char* st = gm["startTimeUTC"];
    if (g.state == 0 && st && strlen(st) >= 16) {
      int uh = atoi(st + 11), um = atoi(st + 14);
      long mins = uh * 60 + um + tzOffsetSec / 60;
      mins = ((mins % 1440) + 1440) % 1440;
      snprintf(g.timeStr, sizeof(g.timeStr), "%02ld:%02ld", mins / 60, mins % 60);
    }
    gamesNHL[n++] = g;
  }
  gameCountNHL = n;
  buildRotationNHL();
  dirty = true;
  logAdd("NHL: " + String(gameCountNHL) + " matchs (favori idx=" + String(favIdxNHL) + ", rotation=" + String(rotCountNHL) + ")");
  xSemaphoreGive(dataMutex);
  return true;
}
