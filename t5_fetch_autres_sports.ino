// ============================================================
//  t5_fetch_autres_sports.ino - Reseau : avantage numerique LNH, PWHL, NBA, NCAA (matchs et classements).
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

// ====================== LNH : AVANTAGE NUMERIQUE (play-by-play) ======================
// /v1/score ne donne pas la situation de jeu. Le play-by-play, lui, porte sur chaque jeu un
// "situationCode" a 4 chiffres : [gardien visiteur][patineurs visiteur][patineurs local][gardien local]
// (verifie sur un vrai match : penalite du visiteur -> "1451" = 4 contre 5, local en AN). Le
// dernier jeu de la liste donne l'etat actuel. Un gardien retire (chiffre 0) compte comme un
// patineur de moins : 6 contre 5 filet vide n'est PAS un avantage numerique.
// Reponse ~150 Ko : on ne telecharge que pour UN match a la fois (favori en priorite), voir nhlSituationStep.
static bool fetchNhlSituation(uint32_t id) {
  int len = 0;
  uint8_t* buf = httpsDownload("NHL situation", "api-web.nhle.com",
                              "https://api-web.nhle.com/v1/gamecenter/" + String(id) + "/play-by-play", 400000, len, 20000);
  if (!buf) return false;
  JsonDocument filter;
  filter["plays"][0]["situationCode"] = true;
  filter["plays"][0]["sortOrder"] = true;
  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, len, DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(30));
  free(buf);
  if (err) { logAdd("NHL situation: JSON " + String(err.c_str())); return false; }
  const char* code = nullptr;
  long best = -1;
  for (JsonObject p : doc["plays"].as<JsonArray>()) {
    const char* sc = p["situationCode"];
    long so = p["sortOrder"] | 0;
    if (sc && strlen(sc) == 4 && so >= best) { best = so; code = sc; }
  }
  if (!code) return false;
  int ag = code[0] - '0', as = code[1] - '0', hs = code[2] - '0', hg = code[3] - '0';
  int ae = as - (ag == 0 ? 1 : 0), he = hs - (hg == 0 ? 1 : 0);   // patineurs effectifs (gardien retire = 1 de moins)
  int8_t pp = (ae > he) ? 1 : (he > ae) ? 2 : 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  int slot = -1;
  for (int i = 0; i < nhlSitCount; i++) if (nhlSit[i].id == id) slot = i;
  if (slot < 0 && nhlSitCount < MAX_GAMES_NHL) slot = nhlSitCount++;
  if (slot >= 0) { nhlSit[slot].id = id; nhlSit[slot].pp = pp; nhlSit[slot].ms = millis(); }
  dirty = true;
  xSemaphoreGive(dataMutex);
  logAdd("NHL situation " + String(id) + ": " + String(code) + " -> " + (pp == 0 ? "egalite" : pp == 1 ? "visiteur AN" : "local AN"));
  return true;
}

// Un telechargement toutes les SIT_STEP_MS : le favori en direct d'abord (1 tour sur 2 s'il y a d'autres
// matchs en direct), les autres matchs en direct tour a tour. Un match perime n'affiche rien (nhlPPFor).
static void nhlSituationStep(unsigned long now) {
  static unsigned long last = 0;
  static uint8_t turn = 0, rr = 0;
  if (now - last < SIT_STEP_MS) return;
  uint32_t favId = 0, others[MAX_GAMES_NHL]; int nOth = 0;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  for (int i = 0; i < gameCountNHL; i++) {
    if (gamesNHL[i].state != 1 || gamesNHL[i].intermission || !gamesNHL[i].id) continue;
    if (i == favIdxNHL) favId = gamesNHL[i].id; else others[nOth++] = gamesNHL[i].id;
  }
  xSemaphoreGive(dataMutex);
  uint32_t pick = 0;
  turn++;
  if (favId && (nOth == 0 || (turn & 1))) pick = favId;
  else if (nOth > 0) pick = others[rr++ % nOth];
  if (!pick) return;
  last = now;
  fetchNhlSituation(pick);
}

// Le scorebar renvoie aussi le dernier match joue meme s'il date de plusieurs
// mois (ex. finale de mai) : on ne garde que les matchs dont la date LOCALE est
// aujourd'hui, plus ceux en cours.
const char* PWHL_API_KEY = "446521baf8c38984";

bool fetchGamesPWHL() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGamesPWHL: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();

  const char* host = "lscluster.hockeytech.com";
  String url = "https://lscluster.hockeytech.com/feed/index.php?feed=modulekit&view=scorebar&key=";
  url += PWHL_API_KEY;
  url += "&fmt=json&client_code=pwhl&lang=en&numberofdaysback=1&numberofdaysahead=1";

  // ~2,5 Ko par match ; 3 jours = rarement plus de 15 matchs.
  int idx = 0;
  uint8_t* buf = httpsDownload("fetchGamesPWHL", host, url, 200000, idx);
  if (!buf) return false;

  JsonDocument filter;
  JsonObject gf = filter["SiteKit"]["Scorebar"][0].to<JsonObject>();
  gf["ID"] = true;
  gf["GameDateISO8601"] = true;
  gf["HomeCode"] = true;  gf["VisitorCode"] = true;
  gf["HomeGoals"] = true; gf["VisitorGoals"] = true;
  gf["Period"] = true;    gf["PeriodNameShort"] = true;
  gf["GameStatus"] = true; gf["GameStatusString"] = true;
  gf["Intermission"] = true;

  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, idx, DeserializationOption::Filter(filter));
  free(buf);
  if (err) { logAdd("fetchGamesPWHL: JSON " + String(err.c_str())); return false; }

  JsonArray arr = doc["SiteKit"]["Scorebar"].as<JsonArray>();
  if (arr.isNull()) { logAdd("fetchGamesPWHL: pas de SiteKit.Scorebar (cle changee?)"); return false; }

  long todayDay = nflDaysFromCivil(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
  int n = 0, total = 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  for (JsonObject ev : arr) {
    total++;
    if (n >= MAX_GAMES_PWHL) break;
    GamePWHL g;
    strncpy(g.awayAbbr, ev["VisitorCode"] | "???", 3); g.awayAbbr[3] = 0;
    strncpy(g.homeAbbr, ev["HomeCode"]    | "???", 3); g.homeAbbr[3] = 0;
    g.awayScore = atoi(ev["VisitorGoals"] | "0");     // tout est en texte dans cette API
    g.homeScore = atoi(ev["HomeGoals"]    | "0");
    g.period    = (uint8_t)atoi(ev["Period"] | "0");
    g.intermission = atoi(ev["Intermission"] | "0") != 0;
    strcpy(g.timeStr, "--:--");

    int gs = atoi(ev["GameStatus"] | "0");
    const char* gss = ev["GameStatusString"] | "";
    const char* pns = ev["PeriodNameShort"]  | "";
    if (gs == 4 || strstr(gss, "Final") != nullptr) g.state = 2;
    else if (gs == 1 || gs == 0)                     g.state = 0;
    else {
      g.state = 1;
      static int lastLoggedGs = -1;   // statut "en direct" encore non confirme : on le note une fois
      if (gs != lastLoggedGs) {
        lastLoggedGs = gs;
        logAdd("PWHL: statut " + String(gs) + " \"" + String(gss) + "\" per=" + String(pns) +
               " (match " + String(ev["ID"] | "?") + ") traite comme EN DIRECT");
      }
    }
    // Prolongation / tirs de barrage : "OT"/"SO" dans le nom court de la periode
    // ou dans le statut ("Final OT", "Final SO").
    bool so = strstr(pns, "SO") || strstr(gss, "SO");
    bool ot = !so && (strstr(pns, "OT") || strstr(gss, "OT") || g.period >= 4);
    if (so) g.period = 5; else if (ot && g.period < 4) g.period = 4;
    g.wasSO = (g.state == 2) && so;
    g.wasOT = (g.state == 2) && ot;

    // "2026-11-22T21:50:00-05:00" : heure locale DE L'ARENA + son decalage.
    // -> epoch UTC, puis notre fuseau (tzOffsetSec) pour le jour et l'heure.
    const char* dt = ev["GameDateISO8601"] | "";
    if (strlen(dt) >= 25) {
      long offS = (atoi(dt + 20) * 3600L + atoi(dt + 23) * 60L) * (dt[19] == '-' ? -1 : 1);
      long utc  = nflDaysFromCivil(atoi(dt), atoi(dt + 5), atoi(dt + 8)) * 86400L
                  + atoi(dt + 11) * 3600L + atoi(dt + 14) * 60L - offS;
      long loc  = utc + tzOffsetSec;
      long day  = (loc >= 0) ? loc / 86400L : -((-loc + 86399L) / 86400L);
      if (g.state != 1 && day != todayDay) continue;   // autre jour (et pas en cours)
      long mins = ((loc % 86400L) + 86400L) % 86400L / 60L;
      snprintf(g.timeStr, sizeof(g.timeStr), "%02ld:%02ld", mins / 60, mins % 60);
    } else if (g.state != 1) {
      continue;                                         // date illisible : seulement si en direct
    }
    gamesPWHL[n++] = g;
  }
  gameCountPWHL = n;
  buildRotationPWHL();
  dirty = true;
  logAdd("PWHL: " + String(gameCountPWHL) + " matchs aujourd'hui sur " + String(total) +
         " recus (favori idx=" + String(favIdxPWHL) + ", rotation=" + String(rotCountPWHL) + ")");
  xSemaphoreGive(dataMutex);
  return true;
}

// Matchs NBA du jour (heure locale) + ceux en cours. Meme filtre de jour que la NFL.
bool fetchGamesNBA() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGamesNBA: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();

  // dates=AAAAMMJJ explicite : sans lui, ESPN garde la grille de la veille jusqu'au
  // matin (~10 h) -> le filtre "jour local" jetait tous les matchs ("0 matchs").
  char dStr[9];
  strftime(dStr, sizeof(dStr), "%Y%m%d", &tmNow);
  int blen = 0;
  uint8_t* buf = httpsDownload("fetchGamesNBA", "site.api.espn.com",
                               String("https://site.api.espn.com/apis/site/v2/sports/basketball/nba/scoreboard?dates=") + dStr, 400000, blen);
  if (!buf) return false;

  JsonDocument filter;
  espnScoreboardFilter(filter);
  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(30));
  free(buf);
  if (err) { logAdd("fetchGamesNBA: JSON " + String(err.c_str())); return false; }

  long todayDay = nflDaysFromCivil(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
  int n = 0;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  for (JsonObject ev : doc["events"].as<JsonArray>()) {
    if (n >= MAX_GAMES_NBA) break;
    GameNBA g;
    if (!espnParseEvent(ev, todayDay, g, 4)) continue;
    g.poss = 0;                                        // pas d'indicateur de possession au basketball
    gamesNBA[n++] = g;
  }
  gameCountNBA = n;
  buildRotationNBA();
  dirty = true;
  logAdd("NBA: " + String(gameCountNBA) + " matchs (favori idx=" + String(favIdxNBA) + ", rotation=" + String(rotCountNBA) + ")");
  xSemaphoreGive(dataMutex);
  return true;
}

// Table statique equipe -> division (0-5, meme ordre que standDivNBA[]).
// Abreviations ESPN (NY, GS, SA, NO, UTAH, WSH) + quelques alias courants.
int nbaDivisionForTeam(const char* ab) {
  struct Entry { const char* a; uint8_t div; };
  static const Entry kDiv[] = {
    {"BOS",0},{"BKN",0},{"NY",0},{"NYK",0},{"PHI",0},{"TOR",0},       // Atlantique
    {"CHI",1},{"CLE",1},{"DET",1},{"IND",1},{"MIL",1},                // Centrale
    {"ATL",2},{"CHA",2},{"MIA",2},{"ORL",2},{"WSH",2},{"WAS",2},      // Sud-Est
    {"DEN",3},{"MIN",3},{"OKC",3},{"POR",3},{"UTAH",3},{"UTA",3},     // Nord-Ouest
    {"GS",4},{"GSW",4},{"LAC",4},{"LAL",4},{"PHX",4},{"PHO",4},{"SAC",4},   // Pacifique
    {"DAL",5},{"HOU",5},{"MEM",5},{"NO",5},{"NOP",5},{"SA",5},{"SAS",5},    // Sud-Ouest
  };
  for (auto &e : kDiv) if (strcmp(e.a, ab) == 0) return e.div;
  return -1;
}

bool fetchStandingsNBA() {
  int blen = 0;
  uint8_t* buf = httpsDownload("ClassementNBA", "site.api.espn.com",
                              "https://site.api.espn.com/apis/v2/sports/basketball/nba/standings", 500000, blen);
  if (!buf) return false;

  JsonDocument filter;
  JsonObject cf = filter["children"][0].to<JsonObject>();
  JsonObject ef = cf["standings"]["entries"][0].to<JsonObject>();
  ef["team"]["abbreviation"] = true;
  JsonObject sf = ef["stats"][0].to<JsonObject>();
  sf["name"] = true;
  sf["value"] = true;
  sf["displayValue"] = true;

  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(20));
  free(buf);
  if (err) { logAdd("ClassementNBA: JSON " + String(err.c_str())); return false; }

  JsonArray confs = doc["children"].as<JsonArray>();
  if (confs.isNull()) { logAdd("ClassementNBA: pas de children"); return false; }

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  for (int d = 0; d < 6; d++) standDivNBA[d].n = 0;
  int matched = 0;
  for (JsonObject conf : confs) {
    JsonArray entries = conf["standings"]["entries"].as<JsonArray>();
    if (entries.isNull()) continue;
    for (JsonObject en : entries) {
      const char* ab = en["team"]["abbreviation"] | "???";
      int slot = nbaDivisionForTeam(ab);
      if (slot < 0 || standDivNBA[slot].n >= 5) continue;
      StandTeamNBA& t = standDivNBA[slot].teams[standDivNBA[slot].n];
      strncpy(t.abbr, ab, 4); t.abbr[4] = 0;
      t.w = t.l = 0;
      int pct1000 = 0;
      for (JsonObject st : en["stats"].as<JsonArray>()) {
        const char* nm = st["name"] | "";
        if      (strcmp(nm, "wins")       == 0) t.w = (int)espnStatNum(st["value"], st["displayValue"]);
        else if (strcmp(nm, "losses")     == 0) t.l = (int)espnStatNum(st["value"], st["displayValue"]);
        else if (strcmp(nm, "winPercent") == 0) pct1000 = (int)(espnStatNum(st["value"], st["displayValue"]) * 1000 + 0.5f);
      }
      t.pct1000 = pct1000;
      standDivNBA[slot].n++;
      matched++;
    }
  }
  // Tri de chaque division par % de victoires decroissant.
  for (int d = 0; d < 6; d++) {
    StandDivNBA& div = standDivNBA[d];
    for (int i = 1; i < div.n; i++) {
      StandTeamNBA key = div.teams[i];
      int j = i - 1;
      while (j >= 0 && div.teams[j].pct1000 < key.pct1000) { div.teams[j + 1] = div.teams[j]; j--; }
      div.teams[j + 1] = key;
    }
  }
  standValidNBA = (matched > 0);
  xSemaphoreGive(dataMutex);
  logAdd("ClassementNBA: " + String(matched) + " equipes matchees" + (standValidNBA ? "" : " (verifier abreviations/structure)"));
  return standValidNBA;
}

// ====================== NCAA FOOTBALL (API publique ESPN) ======================
// conferenceId ESPN -> index cfgNcaaConf[] (ACC=1, Big 12=4, Big Ten=5, SEC=8, Pac-12=9)
static int ncaaConfIndex(int cid) {
  switch (cid) { case 1: return 0; case 4: return 1; case 5: return 2; case 8: return 3; case 9: return 4; }
  return -1;
}

// Logos manquants : une seule ligne de log par abreviation (jusqu'a 16).
static void ncaaNoteMissingLogo(const char* ab) {
  static char seen[16][6];
  static int nSeen = 0;
  if (ncaaLogoByAbbrev(ab)) return;
  for (int i = 0; i < nSeen; i++) if (strcmp(seen[i], ab) == 0) return;
  if (nSeen < 16) { strncpy(seen[nSeen], ab, 5); seen[nSeen][5] = 0; nSeen++; }
  logAdd(String("NCAA: logo manquant: ") + ab + " (logo NCAA affiche)");
}

// Un telechargement PAR conference cochee (groups=N) : le scoreboard FBS complet
// (groups=80) fait 1 a 3 Mo, trop pour ce WiFi. UNE conference par appel : le
// netTask peut ainsi servir les autres sports entre deux conferences (5
// telechargements d'affilee bloquaient la NHL pendant ~2 min). Les matchs sont
// accumules puis fusionnes/dedoubles ; la liste affichee n'est remplacee qu'a
// la fin du cycle. ncaaCycleDone = false -> le netTask rappelle bientot.
// Un favori (n'importe laquelle des 6 listes) ?
static bool ncaaIsFav(const char* ab) {
  for (int k = 0; k < 6; k++) if (ncaaFavActive(k) && strcmp(cfgFavNCAA[k], ab) == 0) return true;
  return false;
}
static bool ncaaHasUnknownFav() {   // favori dont la conference est inconnue (equipe du Top 25 hors Power 4)
  for (int k = 0; k < 6; k++) if (ncaaFavActive(k) && ncaaTeamConf(cfgFavNCAA[k]) < 0) return true;
  return false;
}
static unsigned long ncaaExtraAt = 0;   // prochain telechargement "toute la division FBS" autorise
// Conferences a telecharger : celles cochees + celle de chaque equipe favorite (meme decochee ;
// seuls les matchs des favoris y sont alors gardes) + independants (Notre Dame) + en dernier
// recours (slot 6) toute la division FBS pour un favori du Top 25 de conference inconnue.
static bool ncaaConfWanted(int ci) {
  if (ci < 5 && cfgNcaaConf[ci]) return true;
  if (ci == 6) return ncaaHasUnknownFav() && (long)(millis() - ncaaExtraAt) >= 0;
  for (int k = 0; k < 6; k++) if (ncaaFavActive(k) && ncaaTeamConf(cfgFavNCAA[k]) == ci) return true;
  return false;
}
static GameNCAA ncaaTmp[MAX_GAMES_NCAA];
static int  ncaaTmpN = 0, ncaaNextConf = 0, ncaaOkCount = 0, ncaaWanted = 0;
bool ncaaCycleDone = true;

bool fetchGamesNCAA() {
  struct tm tmNow;
  if (!getSportDay(tmNow)) { logAdd("fetchGamesNCAA: NTP pas pret"); return false; }
  tzOffsetSec = computeTzOffset();
  long todayDay = nflDaysFromCivil(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

  static const int confGroup[7] = {1, 4, 5, 8, 9, 18, 80};   // ACC, Big 12, Big Ten, SEC, Pac-12, independants, FBS complet
  char dStrN[9];                                     // dates=AAAAMMJJ : evite la grille de la veille (voir NBA)
  strftime(dStrN, sizeof(dStrN), "%Y%m%d", &tmNow);
  if (ncaaNextConf == 0) { ncaaTmpN = 0; ncaaOkCount = 0; ncaaWanted = 0; for (int k = 0; k < 7; k++) if (ncaaConfWanted(k)) ncaaWanted++; }
  int ci = ncaaNextConf;
  while (ci < 7 && !ncaaConfWanted(ci)) ci++;

  if (ci < 7) {
    ncaaNextConf = ci + 1;
    int blen = 0;
    bool skipDl = false;
    if (ci == 6) {   // FBS complet : seulement si un favori de conference inconnue n'a pas ete trouve ailleurs
      skipDl = true;
      for (int k = 0; k < 6 && skipDl; k++) {
        if (!ncaaFavActive(k) || ncaaTeamConf(cfgFavNCAA[k]) >= 0) continue;
        bool found = false;
        for (int g = 0; g < ncaaTmpN; g++) if (strcmp(ncaaTmp[g].awayAbbr, cfgFavNCAA[k]) == 0 || strcmp(ncaaTmp[g].homeAbbr, cfgFavNCAA[k]) == 0) found = true;
        if (!found) skipDl = false;
      }
      if (skipDl) ncaaExtraAt = millis() + 300000UL;   // tout est deja trouve : on ne retente pas avant 5 min
    }
    uint8_t* buf = skipDl ? nullptr : httpsDownload("fetchGamesNCAA", "site.api.espn.com",
                                "https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard?groups=" + String(confGroup[ci]) + "&limit=" + String(ci == 6 ? 400 : 100) + "&dates=" + dStrN,
                                ci == 6 ? 4000000 : 1200000, blen, ci == 6 ? 70000UL : 45000UL);
    if (buf) {
      JsonDocument filter;
      espnScoreboardFilter(filter);
      JsonDocument doc(&psramAllocator);
      DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                                 DeserializationOption::Filter(filter),
                                                 DeserializationOption::NestingLimit(30));
      free(buf);
      if (err) {
        logAdd("fetchGamesNCAA: JSON " + String(err.c_str()));
      } else {
        ncaaOkCount++;
        bool favOnly = (ci >= 5 || !cfgNcaaConf[ci]);   // conference telechargee pour les favoris seulement
        for (JsonObject ev : doc["events"].as<JsonArray>()) {
          if (ncaaTmpN >= MAX_GAMES_NCAA) break;
          GameNCAA g;
          if (!espnParseEvent(ev, todayDay, g, 5)) continue;
          g.poss = 0;                                   // indicateur de possession : NFL seulement
          if (favOnly && !ncaaIsFav(g.awayAbbr) && !ncaaIsFav(g.homeAbbr)) continue;
          bool dup = false;                             // meme match deja recu par une autre conference
          for (int k = 0; k < ncaaTmpN; k++)
            if (strcmp(ncaaTmp[k].awayAbbr, g.awayAbbr) == 0 && strcmp(ncaaTmp[k].homeAbbr, g.homeAbbr) == 0) { dup = true; break; }
          if (dup) continue;
          ncaaNoteMissingLogo(g.awayAbbr);
          ncaaNoteMissingLogo(g.homeAbbr);
          ncaaTmp[ncaaTmpN++] = g;
        }
      }
    }
    if (ci == 6 && !skipDl) {   // FBS complet : lourd -> 15 min, ou 60 s si un favori joue en direct
      bool liveFav = false;
      for (int g = 0; g < ncaaTmpN; g++) if (ncaaTmp[g].state == 1 && (ncaaIsFav(ncaaTmp[g].awayAbbr) || ncaaIsFav(ncaaTmp[g].homeAbbr))) liveFav = true;
      ncaaExtraAt = millis() + (liveFav ? 60000UL : 900000UL);
    }
    // encore une conference a faire dans ce cycle ?
    int nx = ncaaNextConf;
    while (nx < 7 && !ncaaConfWanted(nx)) nx++;
    if (nx < 7) { ncaaCycleDone = false; return true; }
  }

  // fin de cycle
  ncaaNextConf = 0;
  ncaaCycleDone = true;
  if (ncaaOkCount > 0) ncaaFetched = true;
  if (ncaaWanted > 0 && ncaaOkCount == 0) return false;   // tout a echoue : on garde les anciennes donnees
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  int m = 0;
  for (int pass = 0; pass < 2; pass++)           // en direct d'abord
    for (int k = 0; k < ncaaTmpN; k++)
      if ((ncaaTmp[k].state == 1) == (pass == 0)) gamesNCAA[m++] = ncaaTmp[k];
  gameCountNCAA = m;
  buildRotationNCAA();
  dirty = true;
  logAdd("NCAA: " + String(gameCountNCAA) + " matchs (" + String(ncaaOkCount) + "/" + String(ncaaWanted) + " conferences ok, favoris=" + String(favCountNCAA) + ", rotation=" + String(rotCountNCAA) + ")");
  xSemaphoreGive(dataMutex);
  return true;
}

// Top 6 : prefere le sondage AP ("ap"), sinon le premier sondage de la liste.
// ---- Classement CFP : l'API "site" ne donne que le sondage de la semaine en cours (AP) ; le CFP
// (publie le mardi) vient de l'API "core" ESPN, qui ne donne que des ids d'equipes -> petite table id -> abbr/nom.
struct TeamIdNCAA { uint16_t id; char ab[6]; char nm[20]; };
static TeamIdNCAA teamIdCache[90];
static int teamIdCount = 0;

static int teamIdFind(int id) {
  for (int i = 0; i < teamIdCount; i++) if (teamIdCache[i].id == id) return i;
  return -1;
}
static void teamIdAdd(int id, const char* ab, const char* nm) {
  if (id <= 0 || !ab || !ab[0] || teamIdFind(id) >= 0 || teamIdCount >= 90) return;
  TeamIdNCAA& t = teamIdCache[teamIdCount++];
  t.id = (uint16_t)id;
  strncpy(t.ab, ab, 5); t.ab[5] = 0;
  strncpy(t.nm, (nm && nm[0]) ? nm : ab, 19); t.nm[19] = 0;
}
// Resout un id d'equipe via l'API "site" (petite reponse), puis le garde en cache.
static bool teamIdFetch(int id) {
  char url[130];
  snprintf(url, sizeof(url), "https://site.api.espn.com/apis/site/v2/sports/football/college-football/teams/%d", id);
  int blen = 0;
  uint8_t* buf = httpsDownload("EquipeNCAA", "site.api.espn.com", url, 60000, blen, 15000);
  if (!buf) return false;
  JsonDocument filter;
  filter["team"]["id"] = true;
  filter["team"]["abbreviation"] = true;
  filter["team"]["location"] = true;
  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(20));
  free(buf);
  if (err) return false;
  const char* ab = doc["team"]["abbreviation"] | "";
  const char* nm = doc["team"]["location"] | ab;
  if (!ab[0]) return false;
  teamIdAdd(id, ab, nm);
  return true;
}

bool fetchRankingsCFP() {
  time_t tt = time(nullptr);
  struct tm lt; localtime_r(&tt, &lt);
  if (lt.tm_year < 124) { logAdd("ClassementCFP: heure pas prete"); return false; }
  int season = lt.tm_year + 1900;
  if (lt.tm_mon < 6) season--;                 // janvier-juin : encore la saison precedente
  char url[170];
  int blen = 0;

  // 1) la ressource "saison" pointe vers la semaine la plus recente du CFP
  snprintf(url, sizeof(url), "https://sports.core.api.espn.com/v2/sports/football/leagues/college-football/seasons/%d/rankings/21", season);
  uint8_t* b1 = httpsDownload("ClassementCFP", "sports.core.api.espn.com", url, 20000, blen, 15000);
  if (!b1) return false;
  int bestT = 0, bestW = 0;
  {
    const char* p = (const char*)b1;
    for (int i = 0; i + 7 < blen; i++) {
      if (memcmp(p + i, "/types/", 7) != 0) continue;
      int j = i + 7, t = 0;
      while (j < blen && p[j] >= '0' && p[j] <= '9') { t = t * 10 + (p[j] - '0'); j++; }
      if (j + 7 > blen || memcmp(p + j, "/weeks/", 7) != 0) continue;
      j += 7;
      int w = 0;
      while (j < blen && p[j] >= '0' && p[j] <= '9') { w = w * 10 + (p[j] - '0'); j++; }
      if (t * 100 + w > bestT * 100 + bestW) { bestT = t; bestW = w; }
    }
  }
  free(b1);
  if (bestW <= 0) { logAdd("ClassementCFP: semaine introuvable"); return false; }

  // 2) le classement de cette semaine (~20 Ko)
  snprintf(url, sizeof(url), "https://sports.core.api.espn.com/v2/sports/football/leagues/college-football/seasons/%d/types/%d/weeks/%d/rankings/21", season, bestT, bestW);
  uint8_t* buf = httpsDownload("ClassementCFP", "sports.core.api.espn.com", url, 80000, blen, 20000);
  if (!buf) return false;
  JsonDocument filter;
  JsonObject kf = filter["ranks"][0].to<JsonObject>();
  kf["current"] = true;
  kf["recordSummary"] = true;
  kf["record"]["summary"] = true;      // API core : le bilan est dans record.summary
  kf["team"]["$ref"] = true;
  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(20));
  free(buf);
  if (err) { logAdd("ClassementCFP: JSON " + String(err.c_str())); return false; }

  int  ids[25]; char rec[25][8];
  for (int k = 0; k < 25; k++) { ids[k] = 0; rec[k][0] = 0; }
  int got = 0;
  for (JsonObject r : doc["ranks"].as<JsonArray>()) {
    int cur = r["current"] | 0;
    if (cur < 1 || cur > 25) continue;
    const char* ref = r["team"]["$ref"] | "";
    const char* tp = strstr(ref, "/teams/");
    if (!tp) continue;
    ids[cur - 1] = atoi(tp + 7);
    const char* rs = r["record"]["summary"] | (const char*)(r["recordSummary"] | "");
    strncpy(rec[cur - 1], rs, 7); rec[cur - 1][7] = 0;
    got++;
  }
  if (got < 3) { logAdd("ClassementCFP: " + String(got) + " equipes seulement"); return false; }

  // 3) ids -> abbreviation / nom (cache ; les manquants sont demandes un par un)
  int fetched = 0;
  for (int k = 0; k < 25; k++) {
    if (ids[k] <= 0 || teamIdFind(ids[k]) >= 0) continue;
    if (fetched >= 25) break;
    netStep("EquipeNCAA");
    teamIdFetch(ids[k]);
    fetched++;
  }

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  int n = 0;
  for (int k = 0; k < 25; k++) { top25Abbr[k][0] = 0; top25Name[k][0] = 0; }
  top25Count = 0;
  strncpy(top25Type, "cfp", 7); top25Type[7] = 0;
  for (int k = 0; k < 25; k++) {
    int ix = ids[k] > 0 ? teamIdFind(ids[k]) : -1;
    if (ix < 0) continue;
    strncpy(top25Abbr[k], teamIdCache[ix].ab, 5); top25Abbr[k][5] = 0;
    strncpy(top25Name[k], teamIdCache[ix].nm, 19); top25Name[k][19] = 0;
    top25Count = k + 1;
    if (k < 6 && n < 6) {
      strncpy(rankNCAA[k].abbr, teamIdCache[ix].ab, 5); rankNCAA[k].abbr[5] = 0;
      strncpy(rankNCAA[k].rec, rec[k], 7); rankNCAA[k].rec[7] = 0;
      ncaaNoteMissingLogo(rankNCAA[k].abbr);
      n++;
    }
  }
  rankCountNCAA = n;
  rankValidNCAA = (n >= 3);
  dirty = true;
  xSemaphoreGive(dataMutex);
  logAdd("ClassementNCAA: " + String(n) + " equipes (sondage cfp, semaine " + String(bestW) + ", " + String(fetched) + " ids resolus)");
  return rankValidNCAA;
}

bool fetchRankingsNCAA() {
  if (cfgRankCFP) {
    if (fetchRankingsCFP()) return true;
    logAdd("ClassementNCAA: CFP indisponible, essai AP");
  }
  int blen = 0;
  uint8_t* buf = httpsDownload("ClassementNCAA", "site.api.espn.com",
                              "https://site.api.espn.com/apis/site/v2/sports/football/college-football/rankings", 900000, blen, 75000);
  if (!buf) return false;

  JsonDocument filter;
  JsonObject rf = filter["rankings"][0].to<JsonObject>();
  rf["type"] = true;
  JsonObject kf = rf["ranks"][0].to<JsonObject>();
  kf["current"] = true;
  kf["recordSummary"] = true;
  kf["team"]["id"] = true;
  kf["team"]["abbreviation"] = true;
  kf["team"]["location"] = true;

  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, blen,
                                             DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(20));
  free(buf);
  if (err) { logAdd("ClassementNCAA: JSON " + String(err.c_str())); return false; }

  JsonArray polls = doc["rankings"].as<JsonArray>();
  if (polls.isNull() || polls.size() == 0) { logAdd("ClassementNCAA: pas de sondage"); return false; }
  // Sondage choisi par le toggle (AP ou CFP) ; s'il est absent (ex. CFP pas encore publie),
  // on prend l'autre, puis le premier de la liste.
  const char* wantTy = cfgRankCFP ? "cfp" : "ap";
  const char* altTy  = cfgRankCFP ? "ap" : "cfp";
  JsonObject pick = polls[0];
  bool gotWanted = false;
  for (JsonObject p : polls) {
    const char* ty = p["type"] | "";
    if (strcmp(ty, wantTy) == 0) { pick = p; gotWanted = true; break; }
  }
  if (!gotWanted) for (JsonObject p : polls) {
    const char* ty = p["type"] | "";
    if (strcmp(ty, altTy) == 0) { pick = p; logAdd(String("ClassementNCAA: sondage ") + wantTy + " absent, utilise " + altTy); break; }
  }

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  int n = 0;
  for (int k = 0; k < 25; k++) { top25Abbr[k][0] = 0; top25Name[k][0] = 0; }
  top25Count = 0;
  strncpy(top25Type, pick["type"] | "", 7); top25Type[7] = 0;
  for (JsonObject r : pick["ranks"].as<JsonArray>()) {
    int cur = r["current"] | 0;
    if (cur >= 1 && cur <= 25) {        // Top 25 complet : menu du favori "Top 25"
      const char* ab25 = r["team"]["abbreviation"] | "";
      const char* nm25 = r["team"]["location"] | ab25;
      teamIdAdd(r["team"]["id"].as<int>(), ab25, nm25);   // alimente la table id -> equipe du CFP
      strncpy(top25Abbr[cur - 1], ab25, 5); top25Abbr[cur - 1][5] = 0;
      strncpy(top25Name[cur - 1], nm25, 19); top25Name[cur - 1][19] = 0;
      if (cur > top25Count) top25Count = cur;
    }
    if (cur < 1 || cur > 6 || n >= 6) continue;
    int slot = cur - 1;                  // place selon le rang officiel
    const char* ab = r["team"]["abbreviation"] | "???";
    const char* rec = r["recordSummary"] | "";
    strncpy(rankNCAA[slot].abbr, ab, 5); rankNCAA[slot].abbr[5] = 0;
    strncpy(rankNCAA[slot].rec, rec, 7); rankNCAA[slot].rec[7] = 0;
    ncaaNoteMissingLogo(rankNCAA[slot].abbr);
    n++;
  }
  rankCountNCAA = n;
  rankValidNCAA = (n >= 3);
  dirty = true;
  xSemaphoreGive(dataMutex);
  logAdd("ClassementNCAA: " + String(n) + " equipes (sondage " + String(pick["type"] | "?") + ")");
  return rankValidNCAA;
}
