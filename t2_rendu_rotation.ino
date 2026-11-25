// ============================================================
//  t2_rendu_rotation.ino - Rendu : repartition des sports sur les 4 panneaux, render(), fuseau, rotations.
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

// ====================== ROTATION PAR SPORTS (7 sports, 4 panneaux) ======================
// Ordre fixe des sports : 0 NHL, 1 MLB, 2 NFL, 3 NBA, 4 NCAA, 5 PWHL, 6 CFL.
//  - Chaque sport actif a SON panneau : match du favori epingle, sinon le match
//    courant de sa rotation, sinon son classement, sinon "AUCUN MATCH".
//  - Un sport EPINGLE (page Sports) garde son panneau et sort de la rotation.
//  - Le favori NHL EN DIRECT prend 2 panneaux et reste toujours a l'ecran.
//  - S'il y a plus de sports actifs que de panneaux (4, ou 3 avec la meteo),
//    une fenetre de sports glisse d'un cran a chaque rotation (sportWinRot).
//  - Les panneaux restants montrent des matchs supplementaires (equitablement
//    entre sports, freeSportRot), puis les classements combines.
#define NSPORT 7

static bool sportOn(int k) {
  switch (k) {
    case 0: return cfgSportNHL;
    case 1: return cfgSportMLB;
    case 2: return cfgSportNFL;
    case 3: return cfgSportNBA;
    case 4: return cfgSportNCAA;
    case 5: return cfgSportPWHL;
    case 6: return cfgSportCFL;
  }
  return false;
}
static bool anySportOn() {
  for (int k = 0; k < NSPORT; k++) if (sportOn(k)) return true;
  return false;
}
static bool nhlFavLive() {
  return cfgSportNHL && favIdxNHL >= 0 && favIdxNHL < gameCountNHL && gamesNHL[favIdxNHL].state == 1;
}

// Liste des matchs affichables d'un sport NOUVEAU (3 NBA, 4 NCAA, 5 PWHL, 6 CFL) :
// d'abord les matchs epingles (favoris), puis ceux de la rotation. Renvoie le
// nombre total ; pinned = combien sont epingles (le panneau principal tourne
// parmi eux, sinon parmi tous). out[] contient des index dans gamesXXX[].
static int sportGameList(int k, int* out, int& pinned) {
  int n = 0; pinned = 0;
  if (k == 3) {
    if (favIdxNBA >= 0) { out[n++] = favIdxNBA; pinned = 1; }
    for (int i = 0; i < rotCountNBA && n < 48; i++) out[n++] = rotListNBA[i];
  } else if (k == 5) {
    if (favIdxPWHL >= 0) { out[n++] = favIdxPWHL; pinned = 1; }
    for (int i = 0; i < rotCountPWHL && n < 48; i++) out[n++] = rotListPWHL[i];
  } else if (k == 6) {
    if (favIdxCFL >= 0) { out[n++] = favIdxCFL; pinned = 1; }
    for (int i = 0; i < rotCountCFL && n < 48; i++) out[n++] = rotListCFL[i];
  } else if (k == 4) {
    for (int f = 0; f < 7; f++) {
      if (!ncaaFavActive(f) || favListNCAA[f] < 0) continue;
      bool dup = false;                                       // 2 favoris dans le meme match : une seule fois
      for (int j = 0; j < n; j++) if (out[j] == favListNCAA[f]) dup = true;
      if (!dup && n < 48) { out[n++] = favListNCAA[f]; pinned++; }
    }
    for (int i = 0; i < rotCountNCAA && n < 48; i++) out[n++] = rotListNCAA[i];
  }
  return n;
}
// Vrai si le sport a quelque chose a montrer : un match, ou un classement pret et active.
static bool sportHasContent(int k) {
  int L[48], pinned = 0;
  switch (k) {
    case 0: return favIdxNHL >= 0 || rotCountNHL > 0 || (cfgShowStandNHL && standValidNHL);
    case 1: return favIdx >= 0    || rotCount > 0    || (cfgShowStandMLB && standValid);
    case 2: return favIdxNFL >= 0 || rotCountNFL > 0 || (cfgShowStandNFL && standValidNFL);
    case 3: return sportGameList(3, L, pinned) > 0 || (cfgShowStandNBA && standValidNBA);
    case 4: return sportGameList(4, L, pinned) > 0 || (cfgShowStandNCAA && rankValidNCAA);
    case 5: return sportGameList(5, L, pinned) > 0;
    case 6: return sportGameList(6, L, pinned) > 0 || (cfgShowStandCFL && standValidCFL);
  }
  return false;
}
static int sportRotBase(int k) { return k == 3 ? rotBaseNBA : k == 4 ? rotBaseNCAA : k == 6 ? rotBaseCFL : rotBasePWHL; }

static void drawNewGame(int k, int px, int idx) {
  if (k == 3 && idx >= 0 && idx < gameCountNBA)   drawPanelNBA(px, gamesNBA[idx]);
  if (k == 4 && idx >= 0 && idx < gameCountNCAA)  drawPanelNCAA(px, gamesNCAA[idx]);
  if (k == 5 && idx >= 0 && idx < gameCountPWHL)  drawPanelPWHL(px, gamesPWHL[idx]);
  if (k == 6 && idx >= 0 && idx < gameCountCFL)   drawPanelCFL(px, gamesCFL[idx]);
}

// Position du match montre sur le panneau principal d'un sport nouveau. Avec des favoris
// epingles on tourne parmi eux au rythme de combinedStandRot (avance a CHAQUE tick) et non
// de rotBase : celui-ci reste a 0 quand la rotation est vide, et 2 favoris NCAA sans autre
// match en rotation restaient figes sur le premier.
static int newPrimaryPos(int rb, int n, int pinned) {
  return (pinned > 0) ? (combinedStandRot % pinned) : (rb % n);
}
// Panneau PRINCIPAL d'un sport nouveau : son match (favori epingle ou rotation),
// sinon son classement (si actif), sinon "AUCUN MATCH" (logo du favori / de la ligue).
static void drawNewPrimary(int k, int px) {
  int L[48], pinned = 0;
  int n = sportGameList(k, L, pinned);
  if (n > 0) {
    int rb = sportRotBase(k);
    drawNewGame(k, px, L[newPrimaryPos(rb, n, pinned)]);
    return;
  }
  if (k == 3) {
    if (cfgShowStandNBA && standValidNBA) {                   // marque la division : un panneau libre ne la repetera pas
      int di = combinedStandRot % 6;
      standMarkShown(48 + di);
      drawStandingsNBA(px, di);
      return;
    }
    drawNoGameNBA(px);
  } else if (k == 4) {
    if (cfgShowStandNCAA && rankValidNCAA) { standMarkShown(64); drawRankingNCAA(px); return; }
    int slot = -1;                                            // 1er favori actif : son logo sur "aucun match"
    if (ncaaFetched) for (int f = 0; f < 7; f++) if (ncaaFavActive(f)) { slot = f; break; }
    drawNoGameNCAA(px, slot);
  } else if (k == 6) {
    if (cfgShowStandCFL && standValidCFL) {                   // Est / Ouest a tour de role
      int di = combinedStandRot % 2;
      standMarkShown(80 + di);
      drawStandingsCFL(px, di);
      return;
    }
    drawNoGameCFL(px);
  } else {
    drawNoGamePWHL(px);
  }
}

// Match SUPPLEMENTAIRE e (1, 2...) d'un sport nouveau, pour un panneau libre.
// Les matchs deja montres sur le panneau principal sont exclus ; l'ordre
// avance avec la rotation. Renvoie false s'il n'y en a plus.
static bool drawNewExtra(int k, int px, int e) {
  int L[48], pinned = 0;
  int n = sportGameList(k, L, pinned);
  if (n < 2 || e > n - 1) return false;
  int rb = sportRotBase(k);
  int prim = newPrimaryPos(rb, n, pinned);
  int ext[48], m = 0;
  for (int i = 0; i < n; i++) if (i != prim) ext[m++] = L[i];
  drawNewGame(k, px, ext[((pinned > 0 ? combinedStandRot : rb) + e - 1) % m]);
  return true;
}

// Panneau principal d'UN sport (px = position). wide = favori NHL en direct (2 panneaux).
static void drawSportPrimary(int k, int px, bool wide) {
  if (k == 0) {                                               // NHL
    GameNHL* gNhl = nullptr;
    if (favIdxNHL >= 0) gNhl = &gamesNHL[favIdxNHL];
    else if (rotCountNHL > 0) gNhl = &gamesNHL[rotListNHL[rotBaseNHL % rotCountNHL]];
    if (wide && gNhl) {
      drawPanelNHL(px, *gNhl);                                // match en direct = 2 panneaux (logos pleine taille)
      if (nhlClockPx < 0) {                                   // un seul chrono suivi par seconde (tickNhlClock) : le 1er
        nhlClockPx = px; nhlClockIdx = (int)(gNhl - gamesNHL);
        strncpy(nhlClockHome, gNhl->homeAbbr, 3); nhlClockHome[3] = 0;
      }
    } else if (gNhl) {
      drawPanelNHLCompact(px, *gNhl);                         // tout le reste = 1 panneau (logos reduits)
    } else {
      // Aucun match NHL : son classement si "Afficher classement" est actif, sinon "AUCUN MATCH".
      if (!drawStandingsSport(px, 1)) drawNoGameNHL(px);
    }
  } else if (k == 1) {                                        // MLB
    if (favIdx >= 0)          drawPanel(px, games[favIdx]);
    else if (rotCount > 0)    drawPanel(px, games[rotList[rotBase % rotCount]]);
    else                      { if (!drawStandingsSport(px, 0)) drawNoGame(px); }
  } else if (k == 2) {                                        // NFL
    if (favIdxNFL >= 0)       drawPanelNFL(px, gamesNFL[favIdxNFL]);
    else if (rotCountNFL > 0) drawPanelNFL(px, gamesNFL[rotListNFL[rotBaseNFL % rotCountNFL]]);
    else                      { if (!drawStandingsSport(px, 2)) drawNoGameNFL(px); }
  } else {
    drawNewPrimary(k, px);                                    // NBA / NCAA / PWHL / CFL
  }
}

// Match supplementaire d'un sport pour un panneau libre. extra[k] = combien de
// matchs supplementaires de ce sport sont deja a l'ecran. false = rien a montrer.
static bool drawSportExtra(int k, int px, int* extra, int room, int* used) {
  *used = 1;
  // p0 = 1 si le panneau principal du sport montre deja rotList[rotBase] (aucun favori epingle),
  // 0 si c'est le favori qui l'occupe : dans ce cas AUCUN match de la rotation n'est encore a
  // l'ecran et les extras partent de rotBase (avant : favori + 1 seul autre match = jamais montre).
  int p0 = (k == 0) ? (favIdxNHL >= 0 ? 0 : 1) : (k == 1) ? (favIdx >= 0 ? 0 : 1) : (favIdxNFL >= 0 ? 0 : 1);
  if (k == 0 && rotCountNHL > extra[0] + p0) {
    int gi = rotListNHL[(rotBaseNHL + extra[0] + p0) % rotCountNHL];
    extra[0]++;
    if (cfgWideNHL && gamesNHL[gi].state == 1 && room >= 2) {   // "Affichage avec chronometre" : match en direct sur 2 panneaux
      drawPanelNHL(px, gamesNHL[gi]);
      if (nhlClockPx < 0) { nhlClockPx = px; nhlClockIdx = gi; strncpy(nhlClockHome, gamesNHL[gi].homeAbbr, 3); nhlClockHome[3] = 0; }
      *used = 2;
    } else {
      drawPanelNHLCompact(px, gamesNHL[gi]);
    }
    return true;
  }
  if (k == 1 && rotCount > extra[1] + p0) {                   // favori deja sur son panneau : les extras montrent les AUTRES matchs
    drawPanel(px, games[rotList[(rotBase + extra[1] + p0) % rotCount]]);
    extra[1]++;
    return true;
  }
  if (k == 2 && rotCountNFL > extra[2] + p0) {
    drawPanelNFL(px, gamesNFL[rotListNFL[(rotBaseNFL + extra[2] + p0) % rotCountNFL]]);
    extra[2]++;
    return true;
  }
  if (k >= 3 && drawNewExtra(k, px, extra[k] + 1)) { extra[k]++; return true; }
  return false;
}

// ---- Match force par la page web (forceOn) ----
// (macro et non template : l'IDE Arduino genere mal les prototypes des templates)
#define FIND_FORCED(G, N) do { for (int i = 0; i < (N); i++) if (strcmp(G[i].awayAbbr, forceA) == 0 && strcmp(G[i].homeAbbr, forceH) == 0) return i; return -1; } while (0)
// Index du match force dans le tableau de son sport, -1 s'il n'y est plus (ou aucun).
static int forcedIdx() {
  if (!forceOn) return -1;
  switch (forceSp) {
    case 0: FIND_FORCED(gamesNHL, gameCountNHL);
    case 1: { int a = atoi(forceA), h = atoi(forceH);
              for (int i = 0; i < gameCount; i++) if (games[i].awayId == a && games[i].homeId == h) return i;
              return -1; }
    case 2: FIND_FORCED(gamesNFL, gameCountNFL);
    case 3: FIND_FORCED(gamesNBA, gameCountNBA);
    case 4: FIND_FORCED(gamesNCAA, gameCountNCAA);
    case 5: FIND_FORCED(gamesPWHL, gameCountPWHL);
    case 6: FIND_FORCED(gamesCFL, gameCountCFL);
  }
  return -1;
}
// Dessine le match force a px ; renvoie le nombre de panneaux pris (0 = rien dessine).
// LNH en direct avec "Affichage avec chronometre" : 2 panneaux, comme dans la rotation.
static int drawForced(int px, int room) {
  int i = forcedIdx();
  if (i < 0) return 0;
  switch (forceSp) {
    case 0:
      if (cfgWideNHL && gamesNHL[i].state == 1 && room >= 2) {
        drawPanelNHL(px, gamesNHL[i]);
        if (nhlClockPx < 0) { nhlClockPx = px; nhlClockIdx = i; strncpy(nhlClockHome, gamesNHL[i].homeAbbr, 3); nhlClockHome[3] = 0; }
        return 2;
      }
      drawPanelNHLCompact(px, gamesNHL[i]); return 1;
    case 1: drawPanel(px, games[i]); return 1;
    case 2: drawPanelNFL(px, gamesNFL[i]); return 1;
    case 3: drawPanelNBA(px, gamesNBA[i]); return 1;
    case 4: drawPanelNCAA(px, gamesNCAA[i]); return 1;
    case 5: drawPanelPWHL(px, gamesPWHL[i]); return 1;
    case 6: drawPanelCFL(px, gamesCFL[i]); return 1;
  }
  return 0;
}

// Nombre de panneaux LIBRES consecutifs a partir de fs[s] (fs = panneaux libres, en ordre) :
// un match NHL sur 2 panneaux ne peut pas enjamber un panneau epingle.
static int slotRoom(const int* fs, int nf, int s) {
  int r = 0;
  while (s + r < nf && fs[s + r] == fs[s] + r) r++;
  return r;
}
// Panneaux (bit i = panneau i, 0 = gauche) ou le sport k est epingle ; 0 s'il est dans la rotation
// (ou si l'epingle ne vaut plus : sport desactive, panneaux inexistants ou reserve a la meteo).
static int pinMask(int k, int limit) {
  return sportOn(k) ? (cfgPin[k] & ((1 << limit) - 1)) : 0;
}

void renderUnified() {
  int PX[PANELS_MAX];
  for (int i = 0; i < cfgPanels; i++) PX[i] = i * PANEL_RES_X;
  standShownN = 0;                // aucune division de classement affichee pour l'instant
  int limit = cfgForceWeather ? cfgPanels - 1 : cfgPanels;   // la meteo reserve le dernier panneau

  // Match choisi dans la page web : il remplace le panneau de son sport pour une etape.
  int fSp = (forceOn && forcedIdx() >= 0) ? forceSp : -1;

  // --- 0) Sports epingles : sur LEURS panneaux, hors de la rotation ---
  // 1er panneau coche = panneau principal du sport (favori, sinon rotation, sinon classement) ;
  // les suivants montrent ses autres matchs. Plus de match a montrer : le panneau retourne dans la rotation.
  bool busy[PANELS_MAX] = {false};
  int pinAt[NSPORT];              // 1er panneau epingle du sport, -1 = dans la rotation
  int pinM[NSPORT];
  int taken = 0;
  for (int k = 0; k < NSPORT; k++) {
    int m = pinMask(k, limit) & ~taken;                       // panneau coche pour 2 sports (ne devrait pas arriver) : le 1er gagne
    pinM[k] = m; taken |= m; pinAt[k] = -1;
    for (int i = 0; i < limit; i++) if (m & (1 << i)) { if (pinAt[k] < 0) pinAt[k] = i; busy[i] = true; }
  }
  // NHL epingle en direct : 2 panneaux si celui de droite est libre ou aussi epingle a la LNH (sinon format compact).
  bool nhlWide = false;
  if (pinAt[0] >= 0) {
    int p = pinAt[0];
    int gi = (fSp == 0) ? forcedIdx() : (favIdxNHL >= 0 ? favIdxNHL : (rotCountNHL > 0 ? rotListNHL[rotBaseNHL % rotCountNHL] : -1));
    bool live = gi >= 0 && gi < gameCountNHL && gamesNHL[gi].state == 1;
    bool want = live && (cfgWideNHL || (fSp != 0 && gi == favIdxNHL));
    if (want && p + 1 < limit && (!busy[p + 1] || (pinM[0] & (1 << (p + 1))))) { busy[p + 1] = true; nhlWide = true; }
  }
  int extra[NSPORT] = {0};        // matchs supplementaires de chaque sport deja a l'ecran
  for (int k = 0; k < NSPORT; k++) {
    if (pinAt[k] < 0) continue;
    int w = (k == 0 && nhlWide) ? 2 : 1;
    if (k == fSp) drawForced(PX[pinAt[k]], w);              // le match choisi s'affiche sur le 1er panneau epingle de son sport
    else          drawSportPrimary(k, PX[pinAt[k]], w == 2);
    for (int i = pinAt[k] + w; i < limit; i++) {              // autres panneaux coches : les autres matchs du sport
      if (!(pinM[k] & (1 << i))) continue;
      int room = 0, used = 1;
      while (i + room < limit && (pinM[k] & (1 << (i + room)))) room++;
      if (drawSportExtra(k, PX[i], extra, room, &used)) i += used - 1;
      else busy[i] = false;                                   // rien de plus a montrer : le panneau reprend la rotation
    }
  }

  // Panneaux libres pour la rotation, de gauche a droite.
  int fs[PANELS_MAX], nf = 0;
  for (int i = 0; i < limit; i++) if (!busy[i]) fs[nf++] = i;
  int s = 0;                      // prochain panneau libre (index dans fs)

  // --- 1) Match choisi dans la page web (sport non epingle) : 1er panneau libre ---
  //     (sport epingle : deja dessine sur son panneau a l'etape 0)
  if (fSp >= 0 && pinAt[fSp] < 0 && nf > 0) s = drawForced(PX[fs[0]], slotRoom(fs, nf, 0));

  // --- 2) Favori NHL en direct (non epingle) : 2 panneaux, toujours a l'ecran ---
  bool pinNhl = pinAt[0] < 0 && nhlFavLive() && s < nf && slotRoom(fs, nf, s) >= 2 && !(fSp == 0 && forcedIdx() == favIdxNHL);
  if (pinNhl) { drawSportPrimary(0, PX[fs[s]], true); s += 2; }

  // --- 3) Un panneau par sport actif non epingle (fenetre glissante s'il y en a trop) ---
  int rest[NSPORT], nr = 0;
  for (int k = 0; k < NSPORT; k++) if (sportOn(k) && pinAt[k] < 0 && !(k == 0 && pinNhl) && k != fSp) rest[nr++] = k;
  if (nr > nf - s) {
    // Plus de sports que de panneaux : on ne fait pas tourner les sports qui n'ont
    // rien a montrer (ni match, ni classement) - leur "AUCUN MATCH" volerait la
    // place d'un sport qui a du contenu. Si aucun n'en a, on garde tout.
    int keep[NSPORT], nk = 0;
    for (int i = 0; i < nr; i++) if (sportHasContent(rest[i])) keep[nk++] = rest[i];
    if (nk > 0) { for (int i = 0; i < nk; i++) rest[i] = keep[i]; nr = nk; }
  }
  int startK = (nr > nf - s) ? (sportWinRot % nr) : 0;
  for (int i = 0; i < nr && s < nf; i++) {
    int k = rest[(startK + i) % nr];
    bool w = false;                                           // "Affichage avec chronometre" : NHL en direct sur 2 panneaux
    if (k == 0 && cfgWideNHL && slotRoom(fs, nf, s) >= 2) {
      int gi = favIdxNHL >= 0 ? favIdxNHL : (rotCountNHL > 0 ? rotListNHL[rotBaseNHL % rotCountNHL] : -1);
      w = gi >= 0 && gi < gameCountNHL && gamesNHL[gi].state == 1;
    }
    drawSportPrimary(k, PX[fs[s]], w);
    s += w ? 2 : 1;
  }

  // --- 4) Panneaux libres : matchs supplementaires puis classement ---
  // Montre un 2e (3e...) match d'un sport MEME s'il a un favori epingle sur
  // son propre panneau (rotList l'exclut deja, donc pas de doublon possible) -
  // permet de suivre le score du favori ET les autres matchs en rotation.
  // L'ORDRE de priorite entre sports tourne (freeSportRot) plutot que d'etre
  // fixe, pour une rotation equitable quand plusieurs sports ont des matchs
  // supplementaires disponibles en meme temps. Un sport epingle a un panneau
  // n'en fait plus partie : ses matchs restent sur son panneau.
  int standIdx = 0;
  int panelsFilled = 0;
  while (s < nf) {
    bool filled = false;
    for (int attempt = 0; attempt < NSPORT && !filled; attempt++) {
      int k = (freeSportRot + panelsFilled + attempt) % NSPORT;
      int used = 1;
      if (sportOn(k) && pinAt[k] < 0 && drawSportExtra(k, PX[fs[s]], extra, slotRoom(fs, nf, s), &used)) { filled = true; s += used - 1; panelsFilled += used - 1; }
    }
    if (!filled) {
      standIdx += 1 + drawStandingsCombined(PX[fs[s]], standIdx);   // honore les toggles wild card ; saute les divisions deja affichees
    }
    panelsFilled++;
    s++;
  }

  // --- 5) Meteo sur le dernier panneau ---
  if (cfgForceWeather) { drawWeatherClock(W - PANEL_RES_X); wxPanels[wxPanelCount++] = W - PANEL_RES_X; }
}

// ====================== RENDU GLOBAL (dispatch multi-sport) ======================
bool render() {
  if (!screenOn()) { dma->fillScreen(0); return true; }   // "eteint" applique correctement, pas un echec

  // Attente BORNEE du mutex (100ms max). Si les donnees sont momentanement
  // verrouillees par netTask, on saute ce cycle de rendu (l'ecran garde son
  // image) plutot que de bloquer loop() -> le serveur web reste reactif.
  // IMPORTANT : l'appelant (loop()) ne doit PAS effacer "dirty" quand on
  // saute ainsi, sinon la mise a jour en attente (nouveau favori, nouveau
  // match en rotation, etc.) est perdue silencieusement si ca retombe pile
  // pendant qu'un fetch tient le mutex - bug reel, vu sur le vrai panneau
  // (rotation/changement de favori sans effet visible, de facon persistante
  // si le fetch en cours est assez long/frequent).
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;

  dma->fillScreen(0);
  wxPanelCount = 0;   // reconstruit a chaque render() : quels panneaux montrent la meteo
  nhlClockPx = -1; nhlClockIdx = -1;   // reconstruit aussi : ou est le chrono NHL live

  if (anySportOn()) {
    renderUnified();
  } else {
    // Aucun sport actif : ecran vide sauf la meteo (evite un panneau mort).
    drawWeatherClock(W - PANEL_RES_X);
    wxPanels[wxPanelCount++] = W - PANEL_RES_X;
  }

  xSemaphoreGive(dataMutex);
  return true;
}

// ====================== FUSEAU ======================
long computeTzOffset() {
  time_t t = time(nullptr);
  struct tm lt, gt;
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
  gt.tm_isdst = lt.tm_isdst;
  return (long)(mktime(&lt) - mktime(&gt));
}

// "Journee sportive" : la date utilisee pour demander les matchs ne bascule qu'a 5 h du
// matin (heure locale), pas a minuit. Sinon un match de l'Ouest encore en direct a 0 h 01
// disparaissait de l'ecran (la requete du "nouveau jour" ne le contient plus). De minuit a
// 5 h on continue donc d'afficher la soiree de la veille (matchs en cours et pointages finals).
#define SPORT_DAY_ROLLOVER_H 5
bool getSportDay(struct tm& t) {
  struct tm nowTm;
  if (!getLocalTime(&nowTm, 5000)) return false;       // NTP pas encore pret
  time_t tt = time(nullptr) - SPORT_DAY_ROLLOVER_H * 3600L;
  localtime_r(&tt, &t);
  return true;
}
// ====================== ROTATION ======================
void buildRotation() {
  favIdx = -1;
  rotCount = 0;
  bool noFav = (cfgFavTeam == 0);   // "Aucun" choisi -> pas de favori fixe
  for (int i = 0; i < gameCount; i++) {
    bool isFav = !noFav && (games[i].awayId == cfgFavTeam || games[i].homeId == cfgFavTeam);
    if (isFav && favIdx < 0) { favIdx = i; continue; }
    if (cfgLiveOnlyMLB && games[i].state != 1) continue;
    rotList[rotCount++] = i;
  }
  // Borne les positions de rotation dans les limites du nouveau rotCount.
  rotBase = (rotCount > 0) ? rotBase % rotCount : 0;
  dirty = true;
}

// ====================== ROTATION NFL / NHL / NBA / PWHL / CFL ======================
// Meme logique pour les 5 sports identifies par abreviation : buildRotAbbr() (sport_util.h).
void buildRotationNFL() {
  buildRotAbbr(gamesNFL, gameCountNFL, cfgFavTeamNFL, cfgLiveOnlyNFL, favIdxNFL, rotListNFL, rotCountNFL, rotBaseNFL);
  dirty = true;
}
void buildRotationNHL() {
  buildRotAbbr(gamesNHL, gameCountNHL, cfgFavTeamNHL, cfgLiveOnlyNHL, favIdxNHL, rotListNHL, rotCountNHL, rotBaseNHL);
  dirty = true;
}
void buildRotationNBA() {
  buildRotAbbr(gamesNBA, gameCountNBA, cfgFavTeamNBA, cfgLiveOnlyNBA, favIdxNBA, rotListNBA, rotCountNBA, rotBaseNBA);
  dirty = true;
}
void buildRotationPWHL() {
  buildRotAbbr(gamesPWHL, gameCountPWHL, cfgFavTeamPWHL, cfgLiveOnlyPWHL, favIdxPWHL, rotListPWHL, rotCountPWHL, rotBasePWHL);
  dirty = true;
}
void buildRotationCFL() {
  buildRotAbbr(gamesCFL, gameCountCFL, cfgFavTeamCFL, cfgLiveOnlyCFL, favIdxCFL, rotListCFL, rotCountCFL, rotBaseCFL);
  dirty = true;
}



// ====================== ROTATION NCAA ======================
void buildRotationNCAA() {
  favCountNCAA = 0;
  rotCountNCAA = 0;
  for (int k = 0; k < 7; k++) favListNCAA[k] = -1;
  bool isFavGame[MAX_GAMES_NCAA];
  for (int i = 0; i < gameCountNCAA; i++) {
    isFavGame[i] = false;
    for (int k = 0; k < 7; k++) {
      if (!ncaaFavActive(k)) continue;
      if (strcmp(gamesNCAA[i].awayAbbr, cfgFavNCAA[k]) == 0 || strcmp(gamesNCAA[i].homeAbbr, cfgFavNCAA[k]) == 0) {
        if (favListNCAA[k] < 0) { favListNCAA[k] = i; favCountNCAA++; }
        isFavGame[i] = true;
      }
    }
  }
  for (int i = 0; i < gameCountNCAA; i++) {
    if (isFavGame[i]) continue;                       // deja montre via le favori
    if (cfgLiveOnlyNCAA && gamesNCAA[i].state != 1) continue;
    rotListNCAA[rotCountNCAA++] = i;
  }
  if (rotCountNCAA > 0) rotBaseNCAA %= rotCountNCAA; else rotBaseNCAA = 0;
  dirty = true;
}
