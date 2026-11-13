// ============================================================
//  t1_dessin.ino - Dessin : couleurs d'equipes, logos, panneaux de match, classements, meteo, animation de but.
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

// ====================== COULEUR ======================
uint16_t C(uint8_t r, uint8_t g, uint8_t b) { return dma->color565(r, g, b); }

// Abreviation d'une equipe par son id (reutilise la table de logos_mlb.h)
const char* mlbAbbrById(int id) {
  for (int i = 0; i < MLB_LOGO_COUNT; i++)
    if (MLB_LOGOS[i].id == id) return MLB_LOGOS[i].abbr;
  return "???";
}

// Couleur officielle (primaire) d'une equipe par son id, pour le classement.
// Valeurs RGB24 ; converties en 565 a l'affichage via C().
uint32_t mlbTeamRGB(int id) {
  switch (id) {
    case 108: return 0xBA0021;  // LAA  rouge
    case 109: return 0xA71930;  // ARI  rouge sedona
    case 110: return 0xDF4601;  // BAL  orange
    case 111: return 0xBD3039;  // BOS  rouge
    case 112: return 0x0E3386;  // CHC  bleu
    case 113: return 0xC6011F;  // CIN  rouge
    case 114: return 0x00385D;  // CLE  bleu marine
    case 115: return 0x333366;  // COL  violet
    case 116: return 0x0C2340;  // DET  marine
    case 117: return 0xEB6E1F;  // HOU  orange
    case 118: return 0x004687;  // KC   bleu
    case 119: return 0x005A9C;  // LAD  bleu
    case 120: return 0xAB0003;  // WSH  rouge
    case 121: return 0xFF5910;  // NYM  orange
    case 133: return 0x23FFDB;  // ATH  vert (officiel, eclairci pour visibilite sur fond noir)
    case 134: return 0xFDB827;  // PIT  jaune
    case 135: return 0xFFC425;  // SD   or
    case 136: return 0x005C5C;  // SEA  Northwest Green
    case 137: return 0xFD5A1E;  // SF   orange
    case 138: return 0xC41E3A;  // STL  rouge
    case 139: return 0x8FBCE6;  // TB   columbia blue
    case 140: return 0x003278;  // TEX  bleu
    case 141: return 0x134A8E;  // TOR  bleu
    case 142: return 0x002B5C;  // MIN  marine
    case 143: return 0xE81828;  // PHI  rouge
    case 144: return 0xCE1141;  // ATL  rouge
    case 145: return 0xC4CED4;  // CWS  argent
    case 146: return 0x00A3E0;  // MIA  bleu clair
    case 147: return 0xC4CED3;  // NYY  grey
    case 158: return 0xFFC52F;  // MIL  jaune
    default:  return 0xEBEBEB;  // inconnu : blanc casse
  }
}

// Couleur d'accent (visible sur fond noir) par equipe NHL, pour le classement.
// APPROXIMATIF : couleurs choisies pour lisibilite sur LED (evite les tons
// trop pres du noir comme certains "primary" officiels) - a ajuster une
// fois vues sur le vrai panneau, comme les logos.
uint32_t nhlTeamRGB(const char* ab) {
  struct Entry { const char* a; uint32_t rgb; };
  static const Entry kColors[] = {
    {"ANA",0xF47A38},{"BOS",0xFFB81C},{"BUF",0x003087},{"CGY",0xD2001C},
    {"CAR",0xCE1126},{"CHI",0xCF0A2C},{"COL",0x6F263D},{"CBJ",0x002654},
    {"DAL",0x006847},{"DET",0xCE1126},{"EDM",0xFF4C00},{"FLA",0xC8102E},
    {"LAK",0xA2AAAD},{"MIN",0x154734},{"MTL",0xAF1E2D},{"NSH",0xFFB81C},
    {"NJD",0xCE1126},{"NYI",0xF47D30},{"NYR",0x0038A8},{"OTT",0xDA1A32},
    {"PHI",0xF74902},{"PIT",0xFCB514},{"SJS",0x006D75},{"STL",0x002F87},
    {"TBL",0x002868},{"TOR",0x00205B},{"UTA",0x6CACE4},{"VGK",0xB4975A},
    {"WSH",0xC8102E},{"WPG",0x004C97},{"SEA",0x639FB6},{"VAN",0x00205B},
  };
  for (auto &e : kColors) if (strcmp(e.a, ab)==0) return e.rgb;
  return 0xEBEBEB;
}

// Couleur d'accent NFL par equipe, meme principe que mlbTeamRGB/nhlTeamRGB.
uint32_t nflTeamRGB(const char* ab) {
  struct Entry { const char* a; uint32_t rgb; };
  static const Entry kColors[] = {
    {"ARI",0x97233F},{"ATL",0xA71930},{"BAL",0x241773},{"BUF",0x00338D},
    {"CAR",0x0085CA},{"CHI",0xC83803},{"CIN",0xFB4F14},{"CLE",0xFF3C00},
    {"DAL",0x869397},{"DEN",0xFB4F14},{"DET",0x0076B6},{"GB", 0x203731},
    {"HOU",0x03202F},{"IND",0x002C5F},{"JAX",0xD7A22A},{"KC", 0xE31837},
    {"LV", 0xA5ACAF},{"LAC",0x0080C6},{"LAR",0x003594},{"MIA",0x008E97},
    {"MIN",0x4F2683},{"NE", 0xC60C30},{"NO", 0xD3BC8D},{"NYG",0x0B2265},
    {"NYJ",0x125740},{"PHI",0x004C54},{"PIT",0xFFB612},{"SF", 0xAA0000},
    {"SEA",0x69BE28},{"TB", 0xD50A0A},{"TEN",0x4B92DB},{"WSH",0x5A1414},
  };
  for (auto &e : kColors) if (strcmp(e.a, ab)==0) return e.rgb;
  return 0xEBEBEB;
}

// Couleur d'accent par equipe NBA pour le classement (eclaircie quand la
// couleur officielle est trop sombre sur LED) - a ajuster apres un test visuel.
uint32_t nbaTeamRGB(const char* ab) {
  struct Entry { const char* a; uint32_t rgb; };
  static const Entry kColors[] = {
    {"ATL",0xE03A3E},{"BOS",0x2FB36A},{"BKN",0xCCCCCC},{"CHA",0x00A3AD},
    {"CHI",0xCE1141},{"CLE",0xC8305A},{"DAL",0x00A0E0},{"DEN",0xFEC524},
    {"DET",0xC8102E},{"GS", 0xFFC72C},{"HOU",0xCE1141},{"IND",0xFDBB30},
    {"LAC",0xC8102E},{"LAL",0xFDB927},{"MEM",0x7B9AD6},{"MIA",0xF9423A},
    {"MIL",0x3DAA5C},{"MIN",0x78BE20},{"NO", 0xC8102E},{"NY", 0xF58426},
    {"OKC",0x007AC1},{"ORL",0x0077C0},{"PHI",0xED174C},{"PHX",0xE56020},
    {"POR",0xE03A3E},{"SAC",0x9A6BD0},{"SA", 0xC4CED4},{"TOR",0xCE1141},
    {"UTAH",0xF9A01B},{"WSH",0xE31837},
  };
  for (auto &e : kColors) if (strcmp(e.a, ab)==0) return e.rgb;
  return 0xEBEBEB;
}

// ====================== BLIT LOGO (16x16) ======================
void drawLogo(int xo, int yo, const uint16_t* logo) {
  if (!logo) { dma->drawRect(xo, yo, MLB_LOGO_W, MLB_LOGO_H, C(40, 40, 40)); return; }
  for (int y = 0; y < MLB_LOGO_H; y++)
    for (int x = 0; x < MLB_LOGO_W; x++)
      dma->drawPixel(xo + x, yo + y, logo[y * MLB_LOGO_W + x]);
}

// ====================== SCORE SOUS LE LOGO ======================
void drawScore(int colLeft, int score) {
  if (score < 0) return;
  dma->setTextColor(C(255, 255, 255));
  dma->setTextSize(1);
  // Centre sous le logo 16px
  int x = (score < 10) ? colLeft + 5   // 1 chiffre (6px) : centre
                        : colLeft + 2;  // 2 chiffres (12px) : centre
  dma->setCursor(x, 22);
  dma->print(score);
}

// ====================== LOSANGE (BUT) ======================
void drawBase(int cx, int cy, bool occupied) {
  uint16_t fill = C(255, 255, 255);
  uint16_t empt = C(90, 90, 90);
  int r = 3;
  if (occupied) {
    dma->fillTriangle(cx, cy - r, cx - r, cy, cx + r, cy, fill);
    dma->fillTriangle(cx, cy + r, cx - r, cy, cx + r, cy, fill);
  } else {
    dma->drawLine(cx, cy - r, cx - r, cy, empt);
    dma->drawLine(cx - r, cy, cx, cy + r, empt);
    dma->drawLine(cx, cy + r, cx + r, cy, empt);
    dma->drawLine(cx + r, cy, cx, cy - r, empt);
  }
}

// ====================== CENTRE : LIVE (colonne centrale du panneau) ======================
void drawCenterLive(int px, const Game& g) {
  uint16_t amber = C(239, 159, 39);
  // Manche : fleche + numero
  if (g.topInning) dma->fillTriangle(px + 22, 6, px + 26, 6, px + 24, 2, amber);
  else             dma->fillTriangle(px + 22, 2, px + 26, 2, px + 24, 6, amber);
  dma->setTextSize(1);
  dma->setTextColor(amber);
  dma->setCursor(px + 29, 0);
  dma->print(g.inning);

  // Buts (losange), centre x = px+32
  drawBase(px + 32, 12, g.b2);   // 2e (haut)
  drawBase(px + 38, 18, g.b1);   // 1er (droite)
  drawBase(px + 26, 18, g.b3);   // 3e (gauche)

  // Retraits : 3 carres en bas
  uint16_t empt = C(90, 90, 90);
  for (int i = 0; i < 3; i++) {
    int x = px + 25 + i * 6;
    if (i < g.outs) dma->fillRect(x, 25, 4, 4, amber);
    else            dma->drawRect(x, 25, 4, 4, empt);
  }
}


// ====================== UN MATCH SUR UN PANNEAU (64x32) ======================
void drawPanel(int px, const Game& g) {
  // Mise en page "a venir" (heure en bas) UNIQUEMENT pour un match pas encore
  // commence et sans retard/report. En direct, termine, ou
  // retard/pluie/suspendu/reporte : mise en page ORIGINALE inchangee (logos
  // 16x16 a px+2/px+46) - demande explicitement de ne pas toucher a ces cas-la.
  //
  // Les logos sont maintenant dessines nativement en 16x16 (drawLogo), aux
  // memes coordonnees px+2/px+46 que le direct/termine, plutot qu'agrandis
  // en 20x20 via drawLogoNHLScaled : ce agrandissement nearest-neighbor
  // (ratio non entier 20/16) dupliquait une colonne/rangee source sur quatre
  // de facon irreguliere et deformait legerement le logo. Natif partout =
  // toujours le meme rendu net, quel que soit l'etat du match.
  bool scheduled = (g.state == 0 && g.delayState == 0);

  if (scheduled) {
    const uint16_t* awayLogo;
    const uint16_t* homeLogo;
    if (g.allStar == 1)      { awayLogo = LOGO_AL; homeLogo = LOGO_NL; }
    else if (g.allStar == 2) { awayLogo = LOGO_NL; homeLogo = LOGO_AL; }
    else                     { awayLogo = mlbLogoById(g.awayId); homeLogo = mlbLogoById(g.homeId); }
    drawLogo(px + 2,  0, awayLogo);
    drawLogo(px + 46, 0, homeLogo);
    drawClockNFL(px, g.timeStr, C(120, 120, 120));  // (l'API renvoie parfois 0-0 avant le debut, trompeur)
    return;
  }

  if (g.allStar == 1) {          // away = Ligue Americaine, home = Ligue Nationale
    drawLogo(px + 2,  0, LOGO_AL);
    drawLogo(px + 46, 0, LOGO_NL);
  } else if (g.allStar == 2) {   // away = Ligue Nationale, home = Ligue Americaine
    drawLogo(px + 2,  0, LOGO_NL);
    drawLogo(px + 46, 0, LOGO_AL);
  } else {
    drawLogo(px + 2,  0, mlbLogoById(g.awayId));   // visiteur : 2px du bord gauche
    drawLogo(px + 46, 0, mlbLogoById(g.homeId));   // local    : 2px du bord droit
  }

  if (g.delayState != 0) {
    // Retard/pluie/suspension/report : prend le pas sur l'affichage normal.
    // Pas d'accent (E au lieu de E-accent) - la police embarquee ne gere pas l'UTF-8.
    const char* txt;
    switch (g.delayState) {
      case 1:  txt = T("RETARD", "LATE");           break;
      case 2:  txt = T("PLUIE", "RAIN");             break;
      case 3:  txt = T("SUSPENDU", "SUSPENDED");     break;
      case 4:  txt = T("REPORTE", "POSTPONED");      break;
      default: txt = "";                             break;
    }
    uint16_t col = C(255, 200, 60);
    // Le statut va EN BAS du panneau (comme "Fin"/l'heure), sous les logos :
    // centre a y=12 il chevauchait les logos 16x16 (RETARD, SUSPENDU...).
    bool hasScores = (g.state == 1);   // match deja commence, en pause -> le score reste utile
    if (hasScores) {
      drawScore(px + 2,  g.awayScore);
      drawScore(px + 46, g.homeScore);
    }
    if (hasScores && (int)strlen(txt) * 6 > 30) {
      // Texte long + pointages affiches a la meme hauteur : police 3x5 TomThumb
      // (4px/caractere) pour ne pas toucher les pointages (ex. SUSPENDU = 32px).
      dma->setFont(&TomThumb);
      dma->setTextColor(col);
      int16_t bx, by; uint16_t bw, bh;
      dma->getTextBounds(txt, 0, 29, &bx, &by, &bw, &bh);
      dma->setCursor(px + 32 - (int)bw / 2, 29);   // police GFX : y = ligne de base
      dma->print(txt);
      dma->setFont();                               // retour a la police normale (obligatoire)
    } else {
      drawClockNFL(px, txt, col);                   // en bas, textSize 1
    }
  } else if (g.state == 1) {            // EN COURS : score + situation (manche, buts, retraits)
    drawScore(px + 2,  g.awayScore);
    drawScore(px + 46, g.homeScore);
    drawCenterLive(px, g);
  } else if (g.state == 2) {     // TERMINE : score + "Fin" (en bas, comme NFL/NHL)
    drawScore(px + 2,  g.awayScore);
    drawScore(px + 46, g.homeScore);
    drawClockNFL(px, T("Fin", "Final"), C(120, 120, 120));
  }
}

// Texte "AUCUN / MATCH" commun aux 6 sports : 2 lignes a px+31 ("MATCH" finit a
// px+60), a droite de la zone logo commune 28x28 (px+1..px+28, rangees 2-29).
static void drawNoGameText(int px) {
  dma->setTextSize(1);
  dma->setTextColor(C(120, 120, 120));
  dma->setCursor(px + 31, 8);  dma->print(T("AUCUN", "NO"));
  dma->setCursor(px + 31, 16); dma->print(T("MATCH", "GAME"));
}

// Blit generique d'une image RGB565 de w x h pixels.
static void blit565(int xo, int yo, const uint16_t* img, int w, int h) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      dma->drawPixel(xo + x, yo + y, img[y * w + x]);
}

// MLB "aucun match" : sans favori, logo MLB generique 26x14 (rangees 9-22) ; sinon le
// logo 16x16 natif du favori, centre dans la zone 28x28.
void drawNoGame(int px) {
  if (cfgFavTeam == 0) blit565(px + 2, 9, LOGO_MLB, LOGO_MLB_W, LOGO_MLB_H);
  else                 drawLogo(px + 7, 8, mlbLogoById(cfgFavTeam));
  drawNoGameText(px);
}

// ====================== METEO : pictogrammes animes ======================
#define WX_SUN 0
#define WX_MOON 1
#define WX_PARTLY 2
#define WX_CLOUD 3
#define WX_RAIN 4
#define WX_SNOW 5
#define WX_STORM 6

// Categories selon les codes WMO d'Open-Meteo (current.weather_code) :
// 0 degage, 1 surtout degage, 2 partiellement nuageux, 3 couvert, 45/48 brouillard,
// 51-57 bruine, 61-67 pluie, 71-77 neige, 80-82 averses, 85-86 averses de neige,
// 95-99 orage.
int wxCategory(int code, bool day) {
  if (code == 0 || code == 1) return day ? WX_SUN : WX_MOON;
  if (code == 2) return WX_PARTLY;
  if (code == 3) return WX_CLOUD;
  if (code == 45 || code == 48) return WX_CLOUD;     // brouillard
  if (code >= 51 && code <= 67) return WX_RAIN;      // bruine + pluie (+ verglacante)
  if (code >= 71 && code <= 77) return WX_SNOW;      // neige, grains de neige
  if (code >= 80 && code <= 82) return WX_RAIN;      // averses
  if (code == 85 || code == 86) return WX_SNOW;      // averses de neige
  if (code >= 95 && code <= 99) return WX_STORM;     // orage
  return WX_CLOUD;
}

void drawSun(int cx, int cy, uint16_t f) {
  uint16_t y = C(255, 210, 0);
  int L = 5, e = 2 + (f % 2);                 // rayons qui pulsent
  dma->drawLine(cx, cy - L, cx, cy - L - e, y);
  dma->drawLine(cx, cy + L, cx, cy + L + e, y);
  dma->drawLine(cx - L, cy, cx - L - e, cy, y);
  dma->drawLine(cx + L, cy, cx + L + e, cy, y);
  dma->drawLine(cx - 4, cy - 4, cx - 4 - e, cy - 4 - e, y);
  dma->drawLine(cx + 4, cy - 4, cx + 4 + e, cy - 4 - e, y);
  dma->drawLine(cx - 4, cy + 4, cx - 4 - e, cy + 4 + e, y);
  dma->drawLine(cx + 4, cy + 4, cx + 4 + e, cy + 4 + e, y);
  dma->fillCircle(cx, cy, 3, y);
}

void drawMoon(int cx, int cy, int r = 5) {
  uint16_t m = C(235, 238, 250);   // blanc/argente
  dma->fillCircle(cx, cy, r, m);
  dma->fillCircle(cx + (r * 3) / 5, cy - (r * 2) / 5, r, C(0, 0, 0));
}

void drawCloud(int cx, int cy, int scalePct = 100, bool withBlueLine = true) {
  uint16_t w = C(215, 230, 245);
  uint16_t b = C(120, 160, 220);
  auto sc = [scalePct](int v) { int r = (v * scalePct) / 100; return (v != 0 && r == 0) ? (v > 0 ? 1 : -1) : r; };
  dma->fillCircle(cx + sc(-4), cy + sc(1), sc(3), w);
  dma->fillCircle(cx + sc(1),  cy + sc(-2), sc(4), w);
  dma->fillCircle(cx + sc(5),  cy + sc(1), sc(3), w);
  dma->fillRect(cx + sc(-6), cy + sc(1), sc(13), sc(3), w);
  if (withBlueLine) dma->drawLine(cx + sc(-5), cy + sc(4), cx + sc(6), cy + sc(4), b);  // dessous bleute (omis pour les mini-nuages)
}

void drawPartly(int cx, int cy, uint16_t f, bool day) {
  if (day) drawSun(cx + 3, cy - 3, f);
  else     drawMoon(cx + 3, cy - 8, 3);
  drawCloud(cx - 1, cy + 2);
}

void drawRain(int cx, int cy, uint16_t f) {
  drawCloud(cx, cy - 3);
  uint16_t d = C(60, 140, 240);
  for (int i = 0; i < 3; i++) {
    int x = cx - 4 + i * 4;
    int y = cy + 3 + ((f + i * 2) % 4);
    dma->drawLine(x, y, x, y + 2, d);
  }
}

void drawSnow(int cx, int cy, uint16_t f) {
  drawCloud(cx, cy - 3);
  uint16_t s = C(190, 225, 255);              // bleu poudre
  for (int i = 0; i < 3; i++) {
    int x = cx - 4 + i * 4;
    int y = cy + 4 + ((f + i * 2) % 5);
    dma->drawPixel(x, y, s);
    dma->drawPixel(x - 1, y, s); dma->drawPixel(x + 1, y, s);
    dma->drawPixel(x, y - 1, s); dma->drawPixel(x, y + 1, s);
  }
}

void drawStorm(int cx, int cy, uint16_t f) {
  drawCloud(cx, cy - 3);
  if (f % 2) {                                // eclair qui clignote
    uint16_t y = C(255, 220, 0);
    dma->drawLine(cx,     cy + 3, cx - 2, cy + 6, y);
    dma->drawLine(cx - 2, cy + 6, cx + 1, cy + 6, y);
    dma->drawLine(cx + 1, cy + 6, cx - 1, cy + 9, y);
  }
}

void drawWxIcon(int cx, int cy, int cat) {
  switch (cat) {
    case WX_SUN:    drawSun(cx, cy, animFrame);    break;
    case WX_MOON:   drawMoon(cx, cy);              break;
    case WX_PARTLY: drawPartly(cx, cy, animFrame, wxIsDay); break;
    case WX_RAIN:   drawRain(cx, cy, animFrame);   break;
    case WX_SNOW:   drawSnow(cx, cy, animFrame);   break;
    case WX_STORM:  drawStorm(cx, cy, animFrame);  break;
    default:        drawCloud(cx, cy);             break;
  }
}

// ====================== CLASSEMENT (un panneau 64x32) ======================
// Titre (blanc) + les 3 PREMIERS de la division : abreviation dans la couleur
// officielle de l'equipe (a gauche), difference (games behind) en blanc (droite).
void drawStandingsMLB(int px, int di) {
  StandDiv& d = standDiv[di];
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);

  // Titre de division (blanc), rangee du haut
  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(cfgLang == 0 ? d.title : d.titleEn);

  // 3 premieres equipes (rangs 1-3), format "ABR ... DIFF"
  int shown = (d.n < 3) ? d.n : 3;
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;   // rangees 8, 16, 24

    // Abreviation dans la couleur officielle de l'equipe
    uint32_t rgb = mlbTeamRGB(d.teams[k].id);
    dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    dma->setCursor(px + 2, y);
    dma->print(d.teams[k].abbr);

    // Diff (games behind) en BLANC, alignee a droite du panneau (64px)
    const char* diff = (d.teams[k].gb[0] == '-' && d.teams[k].gb[1] == 0) ? "--" : d.teams[k].gb;
    int wpx = strlen(diff) * 6;                 // largeur du texte (6px/caractere)
    dma->setTextColor(white);
    dma->setCursor(px + 62 - wpx, y);           // colle a droite (marge 2px)
    dma->print(diff);
  }
}

// ====================== CLASSEMENT NHL (un panneau 64x32) ======================
void drawStandingsNHL(int px, int di) {
  StandDivNHL& d = standDivNHL[di];
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);
  uint16_t grey  = C(140, 140, 140);

  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(cfgLang == 0 ? d.title : d.titleEn);

  const char* hdr = "PTS";
  int hw = strlen(hdr) * 6;
  dma->setTextColor(grey);
  dma->setCursor(px + 62 - hw, 0);
  dma->print(hdr);

  int shown = (d.n < 3) ? d.n : 3;
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;
    uint32_t rgb = nhlTeamRGB(d.teams[k].abbr);
    dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    dma->setCursor(px + 2, y);
    dma->print(d.teams[k].abbr);

    char pts[4];
    snprintf(pts, sizeof(pts), "%d", d.teams[k].pts);
    int wpx = strlen(pts) * 6;
    dma->setTextColor(white);
    dma->setCursor(px + 62 - wpx, y);
    dma->print(pts);
  }
}

// ====================== CLASSEMENT NFL (un panneau 64x32) ======================
void drawStandingsNFL(int px, int di) {
  StandDivNFL& d = standDivNFL[di];
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);

  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(cfgLang == 0 ? d.title : d.titleEn);

  // 3 rangees max (comme MLB/NHL) : une 4e a y=32 sortirait du panneau
  // (0-31 valide) - la division NFL complete a 4 equipes, la derniere
  // n'est donc jamais montree ici, meme compromis que les autres sports.
  int shown = (d.n < 3) ? d.n : 3;
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;
    uint32_t rgb = nflTeamRGB(d.teams[k].abbr);
    dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    dma->setCursor(px + 2, y);
    dma->print(d.teams[k].abbr);

    char pct[6];
    snprintf(pct, sizeof(pct), ".%03d", d.teams[k].pct1000);
    int wpx2 = strlen(pct) * 6;
    dma->setTextColor(white);
    dma->setCursor(px + 62 - wpx2, y);
    dma->print(pct);
  }
}

// Classement NBA : une division par diapositive (3 premieres equipes, comme
// les autres sports : 4 rangees de 8 px = titre + 3 equipes). Colonne de droite :
// bilan victoires-defaites ("12-5").
void drawStandingsNBA(int px, int di) {
  if (di < 0 || di > 5) return;
  StandDivNBA& d = standDivNBA[di];
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);
  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(cfgLang == 0 ? d.title : d.titleEn);
  int shown = (d.n < 3) ? d.n : 3;
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;
    uint32_t rgb = nbaTeamRGB(d.teams[k].abbr);
    dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    dma->setCursor(px + 2, y);
    dma->print(d.teams[k].abbr);
    char rec[8];
    snprintf(rec, sizeof(rec), "%d-%d", d.teams[k].w, d.teams[k].l);
    dma->setTextColor(white);
    dma->setCursor(px + 62 - (int)strlen(rec) * 6, y);
    dma->print(rec);
  }
}

// ====================== CLASSEMENT MENEURS / MEILLEURS 2e (WILD CARD) ======================
// Entierement DERIVE des donnees deja recuperees par fetchStandings() :
// aucun appel reseau supplementaire (le reseau est le point faible de ce
// projet, on evite d'en rajouter).
//   lg   : 0 = Americaine (standDiv 0-2), 1 = Nationale (standDiv 3-5)
//   mode : 0 = meneurs de division (rang 1 de chaque division)
//          1 = meilleurs 2e / wild card (tous les autres, tries par %)
// NOTE : tout le calcul est INTERNE a drawStandingsWC() et sa signature
// n'utilise que des int. L'IDE Arduino genere automatiquement les prototypes
// de fonctions EN HAUT du fichier, avant les declarations de struct : une
// fonction dont la signature contient StandTeam/WcRow provoquait une erreur
// de compilation ("does not name a type").
void drawStandingsWC(int px, int lg, int mode) {
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);

  const char* title;
  if (mode == 0) title = (lg == 0) ? T("AL MENEURS", "AL LEADERS")
                                   : T("NL MENEURS", "NL LEADERS");
  else           title = (lg == 0) ? T("AL 2e", "AL WILD")
                                   : T("NL 2e", "NL WILD");
  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(title);

  // --- Calcul interne (voir NOTE ci-dessus sur les prototypes Arduino) ---
  struct WcRow { const StandTeam* t; int pct; };
  WcRow all[15];
  int n = 0;
  int base = lg * 3;
  for (int d = 0; d < 3; d++) {
    StandDiv& div = standDiv[base + d];
    int start = (mode == 0) ? 0 : 1;              // meneurs = rang 1 ; wild card = rangs 2+
    int end   = (mode == 0) ? 1 : div.n;
    for (int k2 = start; k2 < end && k2 < 5; k2++) {
      if (k2 >= div.n || n >= 15) break;
      StandTeam& st = div.teams[k2];
      int g = st.w + st.l;
      all[n].t = &st;
      all[n].pct = (g > 0) ? (st.w * 1000) / g : 0;   // ex: .596 -> 596
      n++;
    }
  }
  // Tri decroissant par pourcentage de victoires (insertion : n <= 15, trivial)
  for (int i = 1; i < n; i++) {
    WcRow key = all[i];
    int j = i - 1;
    while (j >= 0 && all[j].pct < key.pct) { all[j + 1] = all[j]; j--; }
    all[j + 1] = key;
  }
  WcRow* rows = all;
  int shown = (n < 3) ? n : 3;
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;

    // Abreviation dans la couleur de l'equipe
    const char* label = rows[k].t->abbr;
    uint32_t rgb = mlbTeamRGB(rows[k].t->id);
    dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    dma->setCursor(px + 2, y);
    dma->print(label);

    // Pourcentage de victoires format ".596", aligne a droite
    char pct[6];
    snprintf(pct, sizeof(pct), ".%03d", rows[k].pct);
    int wpx = strlen(pct) * 6;
    dma->setTextColor(white);
    dma->setCursor(px + 62 - wpx, y);
    dma->print(pct);
  }
}

// ====================== NHL : COURSE AUX SERIES (WILD CARD) ======================
// Format NHL : 8 qualifies par conference = les 3 premiers de chaque division
// (6 places) + 2 wild cards pris dans le reste de la conference, classes par
// POINTS (pas par pourcentage comme au baseball).
//   conf : 0 = Est (Atlantique + Metropolitaine), 1 = Ouest (Centrale + Pacifique)
// Affiche les 2 detenteurs des places wild card + la 1re equipe EXCLUE
// (en gris) : c'est la ligne de coupure, la vraie info de la course.
// Comme cote MLB, tout est DERIVE de fetchStandingsNHL() - aucun fetch de plus.
// Signature en int seulement (prototypes Arduino generes avant les structs).
void drawWildCardNHL(int px, int conf) {
  dma->setTextSize(1);
  uint16_t white = C(255, 255, 255);
  uint16_t grey  = C(120, 120, 120);

  dma->setTextColor(white);
  dma->setCursor(px + 2, 0);
  dma->print(conf == 0 ? T("EST WC", "EAST WC") : T("OUEST WC", "WEST WC"));

  // --- Calcul interne : pool = equipes classees 4e et + dans les 2 divisions ---
  struct WcNhl { const StandTeamNHL* t; int pts; };
  WcNhl pool[16];
  int n = 0;
  for (int i = 0; i < 2; i++) {
    StandDivNHL& d = standDivNHL[conf * 2 + i];
    for (int k = 3; k < d.n && k < 8; k++) {      // rangs 4+ : les 3 premiers sont deja qualifies
      if (n >= 16) break;
      pool[n].t = &d.teams[k];
      pool[n].pts = d.teams[k].pts;
      n++;
    }
  }
  // Tri decroissant par points (insertion : n <= 16)
  for (int i = 1; i < n; i++) {
    WcNhl key = pool[i];
    int j = i - 1;
    while (j >= 0 && pool[j].pts < key.pts) { pool[j + 1] = pool[j]; j--; }
    pool[j + 1] = key;
  }

  int shown = (n < 3) ? n : 3;                    // 2 places wild card + 1re exclue
  for (int k = 0; k < shown; k++) {
    int y = 8 + k * 8;
    bool isIn = (k < 2);                          // les 2 premiers detiennent les places
    if (isIn) {
      uint32_t rgb = nhlTeamRGB(pool[k].t->abbr);
      dma->setTextColor(C((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
    } else {
      dma->setTextColor(grey);                    // 1re equipe exclue : gris = sous la ligne
    }
    dma->setCursor(px + 2, y);
    dma->print(pool[k].t->abbr);

    char pts[4];
    snprintf(pts, sizeof(pts), "%d", pool[k].pts);
    int wpx = strlen(pts) * 6;
    dma->setTextColor(isIn ? white : grey);
    dma->setCursor(px + 62 - wpx, y);
    dma->print(pts);
  }
}

// Paire de panneaux de la course NHL : slot 0 = qualifies d'office (3 premiers
// d'une division, en rotation sur les 4), slot 1 = course wild card de la
// conference correspondante -> les deux panneaux restent coherents entre eux.
void drawRaceNHL(int px, int div, int slot) {
  if (slot == 0) drawStandingsNHL(px, div);
  else           drawWildCardNHL(px, (div <= 1) ? 0 : 1);
}

// Classement combine : alterne MLB/NHL selon les sports actifs. "offset"
// permet a 2-3 panneaux simultanes de montrer des divisions differentes
// plutot que de repeter la meme (meme principe que le fallback MLB existant).
// Divisions de classement DEJA affichees dans le rendu en cours (cle = sport*16 + slot).
// Bug reel : le panneau NHL (sans match -> son classement, drawStandingsSport) et un
// panneau libre (drawStandingsCombined, offset 0) lisaient le MEME compteur
// combinedStandRot -> quand seul le classement NHL est actif, les deux montraient
// toujours la meme division. renderUnified() remet ce compteur a zero a chaque rendu.
int standShown[10];
int standShownN = 0;
// Cle unique d'une division/diapositive de classement (MLB 0-5, NHL 16+, NFL 32+, NBA 48+, NCAA 64).
static int standKeyOf(int c, int nM, int nH, int nF, int nB) {
  if (c < nM) return c;
  if (c < nM + nH) return 16 + (c - nM);
  if (c < nM + nH + nF) return 32 + (c - nM - nH);
  if (c < nM + nH + nF + nB) return 48 + (c - nM - nH - nF);
  return 64;
}
bool standIsShown(int key) {
  for (int i = 0; i < standShownN; i++) if (standShown[i] == key) return true;
  return false;
}
void standMarkShown(int key) {
  if (standShownN < 10) standShown[standShownN++] = key;
}

// Renvoie le nombre de positions SAUTEES (divisions deja affichees ailleurs) :
// l'appelant avance son offset d'autant pour que le panneau libre suivant ne
// retombe pas sur la meme division.
int drawStandingsCombined(int px, int offset) {
  // Chaque sport contribue un nombre de "slots" a la rotation, TOUS derives
  // de idx seul (aucun compteur separe type standRot/combinedStandRot%4 a
  // l'interieur des sous-fonctions) - sinon deux compteurs qui avancent a
  // des rythmes differents se recoupent sur un sous-ensemble fige au lieu de
  // couvrir tout le cycle (bug reel, vu sur le vrai panneau : "AL meneurs et
  // NL 2e seulement", "CENT. et OUEST WC seulement").
  //   Normal   : MLB = 6 slots (1/division), NHL = 4 slots (1/division),
  //              NFL = 8 slots (1/division, pas de mode wild card pour l'instant)
  //   Wild card: MLB = 4 slots (AL-meneurs, AL-2e, NL-meneurs, NL-2e)
  //              NHL = 8 slots (par division : qualifies, course WC de sa
  //              conference - la course WC repasse donc 2x par conference,
  //              sans consequence, juste une repetition douce)
  int nMlb = (cfgSportMLB && standValid    && cfgShowStandMLB) ? (cfgWildCard    ? 4 : 6) : 0;
  int nNhl = (cfgSportNHL && standValidNHL && cfgShowStandNHL) ? (cfgWildCardNHL ? 8 : 4) : 0;
  int nNfl = (cfgSportNFL && standValidNFL && cfgShowStandNFL) ? 8 : 0;
  int nNba  = (cfgSportNBA  && standValidNBA   && cfgShowStandNBA)  ? 6 : 0;
  int nNcaa = (cfgSportNCAA && rankValidNCAA   && cfgShowStandNCAA) ? 1 : 0;
  int total = nMlb + nNhl + nNfl + nNba + nNcaa;
  if (total == 0) { drawEmptySlot(px, "-"); return 0; }
  int idx = (combinedStandRot + offset) % total;
  int skipped = 0;
  for (int t = 0; t < total; t++) {          // saute ce qui est deja a l'ecran
    int cand = (combinedStandRot + offset + t) % total;
    int key = standKeyOf(cand, nMlb, nNhl, nNfl, nNba);
    if (!standIsShown(key)) { idx = cand; skipped = t; break; }
  }                                           // tout deja affiche (1 seule division) : on garde le 1er
  {
    int key = standKeyOf(idx, nMlb, nNhl, nNfl, nNba);
    standMarkShown(key);
  }
  if (idx < nMlb) {
    if (cfgWildCard) drawStandingsWC(px, idx / 2, idx % 2);   // idx/2=ligue(AL/NL), idx%2=mode(meneurs/2e)
    else             drawStandingsMLB(px, idx);
  } else if (idx < nMlb + nNhl) {
    int k = idx - nMlb;
    if (cfgWildCardNHL) drawRaceNHL(px, k / 2, k % 2);        // k/2=division(0-3), k%2=mode(qualifies/WC)
    else                drawStandingsNHL(px, k);
  } else if (idx < nMlb + nNhl + nNfl) {
    drawStandingsNFL(px, idx - nMlb - nNhl);
  } else if (idx < nMlb + nNhl + nNfl + nNba) {
    drawStandingsNBA(px, idx - nMlb - nNhl - nNfl);
  } else {
    drawRankingNCAA(px);
  }
  return skipped;
}

// Classement d'UN SEUL sport (0=MLB, 1=NHL, 2=NFL), memes slots et memes
// fonctions de dessin que drawStandingsCombined() ci-dessus. Utilise dans
// renderUnified() quand ce sport n'a aucun match a montrer et que son toggle
// "Afficher classement" est actif : son classement remplace "AUCUN MATCH".
// Renvoie false si rien a dessiner (classement pas encore telecharge) ->
// l'appelant affiche alors "AUCUN MATCH" comme avant.
bool drawStandingsSport(int px, int sport) {
  int n = 0;
  if (sport == 0 && cfgShowStandMLB && standValid)    n = cfgWildCard    ? 4 : 6;
  if (sport == 1 && cfgShowStandNHL && standValidNHL) n = cfgWildCardNHL ? 8 : 4;
  if (sport == 2 && cfgShowStandNFL && standValidNFL) n = 8;
  if (n == 0) return false;
  int idx = combinedStandRot % n;   // une division de plus a chaque tick de rotation
  standMarkShown(sport * 16 + idx);  // le panneau libre ne repetera pas cette division
  if (sport == 0) {
    if (cfgWildCard) drawStandingsWC(px, idx / 2, idx % 2);
    else             drawStandingsMLB(px, idx);
  } else if (sport == 1) {
    if (cfgWildCardNHL) drawRaceNHL(px, idx / 2, idx % 2);
    else                drawStandingsNHL(px, idx);
  } else {
    drawStandingsNFL(px, idx);
  }
  return true;
}

// ====================== NHL : BLIT LOGO (28x28) ======================
// Choisit le logo a afficher pour une equipe NHL : l'alternatif si le
// toggle est actif ET que cette equipe en a un, sinon le logo normal
// (toutes les equipes n'ont pas encore de logo alternatif).
const uint16_t* nhlLogoForDisplay(const char* abbr) {
  // Washington : toujours le logo alternatif, peu importe le toggle general.
  if (strcmp(abbr, "WSH") == 0) {
    const uint16_t* alt = nhlAltLogoByAbbrev(abbr);
    if (alt) return alt;
  }
  if (cfgAltLogoNHL) {
    const uint16_t* alt = nhlAltLogoByAbbrev(abbr);
    if (alt) return alt;
  }
  return nhlLogoByAbbrev(abbr);
}

void drawLogoNHL(int xo, int yo, const uint16_t* logo) {
  if (!logo) { dma->drawRect(xo, yo, NHL_LOGO_W, NHL_LOGO_H, C(40, 40, 40)); return; }
  for (int y = 0; y < NHL_LOGO_H; y++)
    for (int x = 0; x < NHL_LOGO_W; x++)
      dma->drawPixel(xo + x, yo + y, logo[y * NHL_LOGO_W + x]);
}

// ====================== NHL : SCORE (gros, a cote du logo) ======================
void drawScoreNHL(int x, int score, bool rightAlign) {
  if (score < 0) return;
  dma->setTextSize(2);
  dma->setTextColor(C(255, 255, 255));
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", score);
  int w = strlen(buf) * 12;             // 12px/caractere a textSize(2)
  int cx = rightAlign ? (x - w) : x;
  dma->setCursor(cx, 8);
  dma->print(buf);
}

const char* nhlPeriodLabel(uint8_t period) {
  switch (period) {
    case 1: return T("1re", "1st");
    case 2: return T("2e", "2nd");
    case 3: return T("3e", "3rd");
    case 4: return T("Pro", "OT");
    case 5: return T("TB", "SO");
    default: return "-";
  }
}

// ====================== NHL : CENTRE (periode + horloge), panneau 128px ======================
void drawCenterLiveNHL(int px, const GameNHL& g) {
  uint16_t amber = C(239, 159, 39);
  dma->setTextSize(1);

  dma->setTextColor(amber);
  const char* per = nhlPeriodLabel(g.period);
  int w1 = strlen(per) * 6;
  dma->setCursor(px + 64 - w1 / 2, 2);
  dma->print(per);

  if (g.intermission) {
    // Entracte : petite police TomThumb (3x5, fournie avec Adafruit GFX), en
    // gris comme "Fin". En police normale, "Entracte" (48px) a y=20
    // chevauchait les pointages (textSize 2, rangees 8-21). Ici : ligne de
    // base y=27 -> rangees 22-26, sous les pointages ; ~32px de large centre
    // (px+48..px+79), entre les deux logos.
    const char* txt = T("Entracte", "Intermission");
    dma->setFont(&TomThumb);
    dma->setTextColor(C(120, 120, 120));
    int16_t bx, by; uint16_t bw, bh;
    dma->getTextBounds(txt, 0, 27, &bx, &by, &bw, &bh);
    dma->setCursor(px + 64 - (int)bw / 2, 27);   // police GFX : y = ligne de base
    dma->print(txt);
    dma->setFont();                               // retour a la police normale (obligatoire)
    return;
  }

  dma->setTextColor(C(255, 255, 255));
  const char* clk = g.clockStr;
  int w2 = strlen(clk) * 6;
  dma->setCursor(px + 64 - w2 / 2, 20);
  dma->print(clk);
}

void drawCenterTextNHL(int px, const char* txt, uint16_t col) {
  int w = strlen(txt) * 6;
  dma->setTextSize(1);
  dma->setTextColor(col);
  dma->setCursor(px + 64 - w / 2, 12);
  dma->print(txt);
}

// ====================== NHL : UN MATCH SUR 64px (programme/termine, pas en direct) =======
// Meme mise en page que le MLB (logos 16x16 aux coins, texte centre) - les
// logos NHL normaux (28x28) sont redimensionnes a la volee via
// drawLogoNHLScaled(), deja utilisee pour le placeholder "aucun match".
// Utilisee uniquement quand le match n'est PAS en direct (voir renderUnified) :
// 1 seul panneau au lieu de 2, le 128px complet restant reserve au direct.
// Logo NHL a 20x20 pour le format compact : natif (net, sans reduction a la
// volee) pour le logo normal ; repli sur l'ancien logo alternatif 28x28
// reduit si le toggle "Logo Alternatif NHL" est actif ou pour Washington
// (l'alternatif n'a pas ete regenere nativement a 20x20, seul le normal l'a
// ete - regenerer les 21 logos alternatifs serait un chantier separe).
// Reduction par MOYENNE DE SURFACE (box filter) d'un logo carre RGB565
// srcSize -> dstSize. Contrairement au plus-proche-voisin de
// drawLogoNHLScaled() (qui saute des pixels : traits fins disparus, bords en
// escalier), chaque pixel de sortie tient compte de TOUS les pixels source
// qu'il recouvre. Le noir (fond) est traite comme "transparent" : on moyenne
// la couleur des pixels allumes seulement, puis on l'attenue selon la
// couverture (avec un gamma 0.55 pour garder les traits fins lumineux, sinon
// ils se diluent dans le noir). Tres leger : calcul entier + 1 powf/pixel.
void drawLogoNHLArea(int xo, int yo, const uint16_t* src, int srcSize, int dstSize) {
  if (!src) { dma->drawRect(xo, yo, dstSize, dstSize, C(40, 40, 40)); return; }
  // Unite : 1 pixel source = dstSize unites ; 1 pixel destination = srcSize unites.
  const int total = srcSize * srcSize;
  for (int y = 0; y < dstSize; y++) {
    int y0 = y * srcSize, y1 = y0 + srcSize;                 // plage verticale (unites)
    int sy0 = y0 / dstSize, sy1 = (y1 - 1) / dstSize;
    for (int x = 0; x < dstSize; x++) {
      int x0 = x * srcSize, x1 = x0 + srcSize;
      int sx0 = x0 / dstSize, sx1 = (x1 - 1) / dstSize;
      long r = 0, g = 0, b = 0, nb = 0;                      // nb = poids des pixels non noirs
      for (int sy = sy0; sy <= sy1; sy++) {
        int oy = min(y1, (sy + 1) * dstSize) - max(y0, sy * dstSize);
        for (int sx = sx0; sx <= sx1; sx++) {
          int ox = min(x1, (sx + 1) * dstSize) - max(x0, sx * dstSize);
          uint16_t p = src[sy * srcSize + sx];
          if (p == 0) continue;
          int w = ox * oy;
          nb += w;
          r += (long)w * ((p >> 11) & 31);
          g += (long)w * ((p >> 5) & 63);
          b += (long)w * (p & 31);
        }
      }
      uint16_t out = 0;
      if (nb > 0) {
        float k = powf((float)nb / (float)total, 0.55f) / (float)nb;   // couleur moyenne x attenuation
        int pr = (int)(r * k + 0.5f), pg = (int)(g * k + 0.5f), pb = (int)(b * k + 0.5f);
        if (pr > 31) pr = 31;
        if (pg > 63) pg = 63;
        if (pb > 31) pb = 31;
        out = (uint16_t)((pr << 11) | (pg << 5) | pb);
      }
      dma->drawPixel(xo + x, yo + y, out);
    }
  }
}

void drawNhlTeam20(int x, const char* abbr) {
  bool useAlt = (strcmp(abbr, "WSH") == 0) || cfgAltLogoNHL;
  if (useAlt) {
    const uint16_t* alt = nhlAltLogoByAbbrev(abbr);
    if (alt) { drawLogoNHLArea(x, NFL_LOGO_Y, alt, NHL_LOGO_W, NFL_LOGO_W); return; }
  }
  const uint16_t* n20 = nhlLogo20ByAbbrev(abbr);
  if (n20) drawLogoNHLScaled(x, NFL_LOGO_Y, n20, NHL_LOGO20_W, NFL_LOGO_W);   // identite (20->20), pas de perte
  else     drawLogoNHLScaled(x, NFL_LOGO_Y, nhlLogoByAbbrev(abbr), NHL_LOGO_W, NFL_LOGO_W);  // repli si jamais absent
}

// Labels de periode COURTS pour le format compact (entre les 2 logos, peu de
// place) - distincts de nhlPeriodLabel() (panneau 128px, plus d'espace,
// "1re"/"1st" etc.) : juste le chiffre, "P"/"OT" pour la prolongation,
// "TB"/"SO" pour les tirs de barrage.
const char* nhlPeriodLabelCompact(uint8_t period) {
  switch (period) {
    case 1: return "1";
    case 2: return "2";
    case 3: return "3";
    case 4: return T("P", "OT");
    case 5: return T("TB", "SO");
    default: return "-";
  }
}

// ====================== INDICATEURS : AVANTAGE NUMERIQUE (LNH) / POSSESSION (NFL) ======================
// Avantage numerique du match g : 0 = aucun/inconnu, 1 = visiteur en AN, 2 = local en AN.
// L'info vient du play-by-play (fetchNhlSituation) ; masquee si perimee, en entracte ou hors direct.
static int nhlPPFor(const GameNHL& g) {
  if (g.state != 1 || g.intermission || g.id == 0) return 0;
  for (int i = 0; i < nhlSitCount; i++)
    if (nhlSit[i].id == g.id) return (millis() - nhlSit[i].ms < SIT_FRESH_MS) ? nhlSit[i].pp : 0;
  return 0;
}

// Fleche pleine (4 px de large, 7 de haut) ; dir > 0 pointe a droite, sinon a gauche.
static void drawBadgeArrow(int x, int y, int dir, uint16_t col) {
  for (int i = 0; i < 4; i++)
    dma->drawFastVLine(dir > 0 ? x + i : x + 3 - i, y + i, 7 - 2 * i, col);
}

// Badge AN/PP du format 64 px : entre les deux logos, sous la periode. La fleche pointe vers l'equipe en avantage.
static void drawPPBadgeCompact(int px, int side) {
  uint16_t col = C(255, 210, 0);
  const char* t = T("AN", "PP");
  dma->setTextSize(1);
  dma->setTextColor(col);
  if (side == 1) { drawBadgeArrow(px + 21, 24, -1, col); dma->setCursor(px + 27, 24); }
  else           { dma->setCursor(px + 25, 24); drawBadgeArrow(px + 40, 24, +1, col); }
  dma->print(t);
}

// Badge AN/PP du format 128 px : petite police 3x5 sous le pointage de l'equipe en avantage
// (le centre est occupe par la periode et le chrono).
static void drawPPBadgeWide(int px, int side) {
  uint16_t col = C(255, 210, 0);
  const char* t = T("AN", "PP");
  dma->setFont(&TomThumb);
  dma->setTextSize(1);
  dma->setTextColor(col);
  if (side == 1) { drawBadgeArrow(px + 33, 24, -1, col); dma->setCursor(px + 39, 29); }
  else           { dma->setCursor(px + 82, 29); drawBadgeArrow(px + 91, 24, +1, col); }
  dma->print(t);
  dma->setFont();   // retour a la police normale (obligatoire)
}

// Ballon de football (8x5) + fleche vers l'equipe qui a la possession, dans la colonne centrale (y 13-17).
static void drawPossessionNFL(int px, int side) {
  uint16_t brown = C(190, 100, 30), lace = C(255, 255, 255), col = C(255, 210, 0);
  int bx = (side == 1) ? px + 26 : px + 30;
  static const uint8_t rowL[5] = {1, 0, 0, 0, 1};     // debut de chaque rangee
  static const uint8_t rowN[5] = {6, 8, 8, 8, 6};     // largeur de chaque rangee
  for (int r = 0; r < 5; r++) dma->drawFastHLine(bx + rowL[r], 13 + r, rowN[r], brown);
  dma->drawFastHLine(bx + 2, 15, 4, lace);            // lacets
  if (side == 1) drawBadgeArrow(px + 21, 12, -1, col);
  else           drawBadgeArrow(px + 40, 12, +1, col);
}

// ====================== ANIMATION PLEIN ECRAN : BUT (LNH) / TOUCHE (NFL) ======================
// Quand le favori marque : ~6,5 s d'animation sur TOUT l'ecran (W px) aux couleurs de l'equipe -
// fond a rayures diagonales qui defilent, flashs au debut, bordure pulsante, gros texte avec
// contour, logo de l'equipe de chaque cote, fondu a la fin. Sans double buffer, chaque pixel est
// calcule puis ecrit UNE seule fois par image (pas de "efface puis redessine") -> aucun
// scintillement. Le texte est rendu dans un masque 1 bit (GFXcanvas1, 1 Ko).
#define ANIM_MS 6500UL
struct AnimState {
  bool active;
  unsigned long t0, lastFrame;
  uint16_t frames;             // images dessinees (diagnostic, voir le log de fin)
  int r, g, b;                 // couleur d'equipe (eclaircie si trop sombre)
  const uint16_t* logo;        // logo RGB565, 0 = transparent
  int logoSize;
  GFXcanvas1* mask;
  int tx0, ty0, tx1, ty1;      // boite du texte (contour compris)
};
AnimState anim = {};

// Eclaircit une couleur trop sombre pour rester visible sur un panneau LED.
static void animBoost(uint32_t rgb, int& r, int& g, int& b) {
  r = (rgb >> 16) & 255; g = (rgb >> 8) & 255; b = rgb & 255;
  int lum = (r * 30 + g * 59 + b * 11) / 100;
  if (lum < 90) {
    int f = (90 - lum) * 256 / (255 - lum);    // part de blanc a melanger (0..256)
    r += (255 - r) * f / 256; g += (255 - g) * f / 256; b += (255 - b) * f / 256;
  }
}

static void animStart() {
  const char* ab = goalReq.abbr;
  uint32_t rgb; const char* txt; const uint16_t* logo; int ls;
  if (goalReq.sport == 0) {
    rgb = nhlTeamRGB(ab);
    txt = T("BUT!", "GOAL!");
    logo = nhlLogoForDisplay(ab); ls = NHL_LOGO_W;
  } else {
    rgb = nflTeamRGB(ab);
    txt = goalReq.kind == 0 ? T("TOUCHE!", "TOUCHDOWN!") : T("PLACEMENT", "FIELD GOAL");
    logo = nflLogoByAbbrev(ab); ls = NFL_LOGO_W;
  }
  animBoost(rgb, anim.r, anim.g, anim.b);
  anim.logo = logo; anim.logoSize = ls;
  anim.mask = new GFXcanvas1(W, H);
  if (!anim.mask) return;                      // plus de RAM : on saute l'animation
  anim.mask->fillScreen(0);
  anim.mask->setTextWrap(false);
  anim.mask->setTextSize(3);                   // 18 x 24 px par caractere
  anim.mask->setTextColor(1);
  int tw = (int)strlen(txt) * 18 - 3;
  int x0 = (W - tw) / 2, y0 = 4;
  anim.mask->setCursor(x0, y0);
  anim.mask->print(txt);
  anim.tx0 = x0 - 2; anim.tx1 = x0 + tw + 1; anim.ty0 = y0 - 2; anim.ty1 = y0 + 24 + 1;
  anim.t0 = millis(); anim.lastFrame = 0; anim.frames = 0; anim.active = true;
  logAdd(String("Animation ") + txt + " " + ab + " : debut");
}

static void animDrawFrame(unsigned long el) {
  // fondu de sortie sur les 700 dernieres ms
  int fade = (el + 700 > ANIM_MS) ? (int)((ANIM_MS - el) * 256 / 700) : 256;
  if (fade < 0) fade = 0;
  bool strobe = (el < 1800) && ((el / 150) % 2 == 0);          // flashs au debut
  int base = strobe ? 100 : (46 + (int)((sinf(el / 260.0f) + 1.0f) * 9.0f));   // % de la couleur d'equipe dans le fond
  int phase = (int)(el / 10);                                   // vitesse de defilement des rayures
  bool pulse = ((el / 200) % 2) == 0;
  int ls = anim.logoSize;
  int lyTop = (H - ls) / 2;
  int lxL = 2, lxR = W - ls - 2;

  // Ordre de balayage des panneaux 1/16 : les lignes y et y+16 s'allument ensemble, on dessine donc
  // chaque paire (y, y+16) a la suite pour que les deux moities de l'ecran changent en meme temps.
  for (int yi = 0; yi < H; yi++) {
    int y = (yi >> 1) + ((yi & 1) ? H / 2 : 0);
    for (int x = 0; x < W; x++) {
      int lvl;
      int d = (x + 2 * y - phase) & 31;
      lvl = base * (d < 16 ? 100 : 55) / 100;
      int pr = anim.r * lvl / 100, pg = anim.g * lvl / 100, pb = anim.b * lvl / 100;

      // bordure pulsante (couleur d'equipe pleine / blanc)
      if (x < 2 || x >= W - 2 || y < 2 || y >= H - 2) {
        if (pulse) { pr = anim.r; pg = anim.g; pb = anim.b; } else { pr = pg = pb = 255; }
      }

      // logos (le pixel noir est transparent)
      if (anim.logo && y >= lyTop && y < lyTop + ls) {
        int lx = (x >= lxL && x < lxL + ls) ? x - lxL : ((x >= lxR && x < lxR + ls) ? x - lxR : -1);
        if (lx >= 0) {
          uint16_t p = anim.logo[(y - lyTop) * ls + lx];
          if (p) { pr = ((p >> 11) & 31) << 3; pg = ((p >> 5) & 63) << 2; pb = (p & 31) << 3; }
        }
      }

      // texte blanc avec contour noir
      if (x >= anim.tx0 && x <= anim.tx1 && y >= anim.ty0 && y <= anim.ty1) {
        if (anim.mask->getPixel(x, y)) { pr = pg = pb = 255; }
        else if (anim.mask->getPixel(x - 1, y) || anim.mask->getPixel(x + 1, y) ||
                 anim.mask->getPixel(x, y - 1) || anim.mask->getPixel(x, y + 1)) { pr = pg = pb = 0; }
      }

      pr = pr * fade >> 8; pg = pg * fade >> 8; pb = pb * fade >> 8;
      dma->drawPixel(x, y, dma->color565(pr, pg, pb));
    }
  }
}

// Appelee a chaque tour de loop(). Renvoie true tant que l'animation occupe l'ecran (loop() ne dessine rien d'autre).
static bool animTick(unsigned long now) {
  if (!anim.active && goalReq.pending) {
    goalReq.pending = false;
    bool on = goalReq.sport == 0 ? cfgGoalAnimNHL : cfgGoalAnimNFL;
    if (on && screenOn()) { animStart(); now = millis(); }   // 'now' date d'avant animStart : sinon now - t0 deborde et l'animation s'arrete aussitot
    else logAdd(String("Animation ignoree : ") + (!on ? "desactivee" : "ecran eteint"));
  }
  if (!anim.active) return false;
  unsigned long el = now - anim.t0;
  if (el >= ANIM_MS) {
    logAdd("Animation : fin (" + String(anim.frames) + " images en " + String(el) + " ms)");
    anim.active = false;
    delete anim.mask; anim.mask = nullptr;
    dirty = true;                               // retour a l'affichage normal (nouveau score compris)
    return false;
  }
  if (now - anim.lastFrame >= 40) { anim.lastFrame = now; animDrawFrame(el); anim.frames++; }
  return true;
}

void drawPanelNHLCompact(int px, const GameNHL& g) {
  // Meme mise en page que le MLB/NFL : logos 20x20 flush aux bords,
  // heure/statut en bas. Voir drawNhlTeam20() pour la source des logos.
  drawNhlTeam20(px,                   g.awayAbbr);
  drawNhlTeam20(px + NFL_COL_RIGHT_X, g.homeAbbr);

  if (g.state == 1) {     // EN DIRECT (format condense : pas le favori, ou aucun favori) : score + periode
    drawScoreNFL(px, g.awayScore);
    drawScoreNFL(px + NFL_COL_RIGHT_X, g.homeScore);
    drawQuarterNFL(px, nhlPeriodLabelCompact(g.period));   // entre les 2 logos, comme le quart NFL - pas au niveau du pointage
    { int pp = nhlPPFor(g); if (pp) drawPPBadgeCompact(px, pp); }   // AN/PP + fleche vers l'equipe en avantage
  } else if (g.state == 2) {     // termine
    drawScoreNFL(px, g.awayScore);
    drawScoreNFL(px + NFL_COL_RIGHT_X, g.homeScore);
    const char* txt = g.wasSO ? T("F/TB", "F/SO")
                     : g.wasOT ? T("F/P", "F/OT")
                     : T("Fin", "Final");
    drawClockNFL(px, txt, C(120, 120, 120));
  } else {                 // programme, pas encore commence
    drawClockNFL(px, g.timeStr, C(120, 120, 120));
  }
}

// ====================== NHL : UN MATCH SUR 128px (2 panneaux fusionnes) ======================
void drawPanelNHL(int px, const GameNHL& g) {
  drawLogoNHL(px + 2, 2, nhlLogoForDisplay(g.awayAbbr));
  drawLogoNHL(px + 128 - NHL_LOGO_W - 2, 2, nhlLogoForDisplay(g.homeAbbr));

  if (g.state == 1) {
    drawScoreNHL(px + 2 + NHL_LOGO_W + 4, g.awayScore, false);
    drawScoreNHL(px + 128 - NHL_LOGO_W - 2 - 4, g.homeScore, true);
    drawCenterLiveNHL(px, g);
    { int pp = nhlPPFor(g); if (pp) drawPPBadgeWide(px, pp); }       // AN/PP + fleche vers l'equipe en avantage
  } else if (g.state == 2) {
    drawScoreNHL(px + 2 + NHL_LOGO_W + 4, g.awayScore, false);
    drawScoreNHL(px + 128 - NHL_LOGO_W - 2 - 4, g.homeScore, true);
    const char* txt = g.wasSO ? T("F/TB", "F/SO")
                     : g.wasOT ? T("F/P", "F/OT")
                     : T("Fin", "Final");
    drawCenterTextNHL(px, txt, C(120, 120, 120));
  } else {
    drawCenterTextNHL(px, g.timeStr, C(120, 120, 120));
  }
}

// Repli generique 64px "rien a montrer ici" - contrairement a drawNoGame()
// (MLB), ne suppose PAS quel sport est actif, donc n'affiche aucun logo
// d'equipe (utilise dans les contextes multi-sport / NHL pour eviter
// d'afficher un logo MLB sur un panneau NHL).
void drawEmptySlot(int px, const char* txt) {
  uint16_t col = C(90, 90, 90);
  dma->setTextSize(1);
  dma->setTextColor(col);
  int w = strlen(txt) * 6;
  dma->setCursor(px + 32 - w / 2, 12);
  dma->print(txt);
}


// Aucun match NHL a montrer sur ce slot 128px.
// Downscale nearest-neighbor generique d'un logo NHL carre (28x28 equipe ou
// 20x20 bouclier) vers une taille plus petite - utilise pour faire tenir le
// placeholder "aucun match" sur UN SEUL panneau 64px, comme le MLB.
void drawLogoNHLScaled(int xo, int yo, const uint16_t* src, int srcSize, int dstSize) {
  if (!src) { dma->drawRect(xo, yo, dstSize, dstSize, C(40, 40, 40)); return; }
  for (int y = 0; y < dstSize; y++) {
    int sy = (y * srcSize) / dstSize;
    for (int x = 0; x < dstSize; x++) {
      int sx = (x * srcSize) / dstSize;
      dma->drawPixel(xo + x, yo + y, src[sy * srcSize + sx]);
    }
  }
}

// NHL "aucun match" - tient maintenant sur UN SEUL panneau 64px (logo a
// gauche 28x28, texte majuscule sur 2 lignes a cote), exactement comme le
// MLB - les panneaux economises servent au classement (voir renderUnified).
// ====================== NFL : RENDU SUR UN PANNEAU 64px ======================
// Disposition : logo 20x20 de chaque cote, quart entre les deux logos,
// chrono entre les deux pointages, un pointage sous chaque logo.
// (NFL_COL_RIGHT_X / NFL_LOGO_Y / NFL_BOT_Y sont definis plus haut, juste
// apres les includes - voir le commentaire la-bas.)

void drawTeamLogoNFL(int xo, int yo, const uint16_t* logo) {
  if (!logo) { dma->drawRect(xo, yo, NFL_LOGO_W, NFL_LOGO_H, C(40, 40, 40)); return; }
  for (int y = 0; y < NFL_LOGO_H; y++)
    for (int x = 0; x < NFL_LOGO_W; x++)
      dma->drawPixel(xo + x, yo + y, logo[y * NFL_LOGO_W + x]);
}

// Pointage centre sous son logo (colonne de 20px). colLeft est ABSOLU.
void drawScoreNFL(int colLeft, int score) {
  if (score < 0) return;
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", score);
  int w = strlen(buf) * 6;
  dma->setTextSize(1);
  dma->setTextColor(C(255, 255, 255));
  dma->setCursor(colLeft + (NFL_LOGO_W - w) / 2, NFL_BOT_Y);
  dma->print(buf);
}

// Quart, entre les deux logos (colonne centrale de 24px). MEME taille que
// le score et la temperature (textSize 1) pour une apparence uniforme sur
// tout le panneau, plutot que le quart en plus gros que le reste.
void drawQuarterNFL(int px, const char* lbl) {
  if (!lbl || !lbl[0]) return;
  int n = strlen(lbl);
  dma->setTextColor(C(239, 159, 39));
  dma->setTextSize(1);
  int w = n * 6;
  dma->setCursor(px + 32 - w / 2, 5); 
  dma->print(lbl);
}

// Chrono / "Fin" / heure de debut, entre les deux pointages.
void drawClockNFL(int px, const char* txt, uint16_t col) {
  if (!txt) return; // Sécurité de chaîne
  int w = strlen(txt) * 6;
  dma->setTextSize(1);
  dma->setTextColor(col);
  dma->setCursor(px + 32 - w / 2, NFL_BOT_Y);
  dma->print(txt);

  }


// NFL / NBA / NCAA : meme structure (GameESPN) et meme mise en page -> un seul dessin.
// la / lh = logos 20x20 du visiteur et du local.
static void drawPanelESPN(int px, const GameESPN& g, const uint16_t* la, const uint16_t* lh) {
  const uint16_t grey = C(120, 120, 120);
  drawTeamLogoNFL(px,                   NFL_LOGO_Y, la);
  drawTeamLogoNFL(px + NFL_COL_RIGHT_X, NFL_LOGO_Y, lh);
  if (g.state == 0) { drawClockNFL(px, g.timeStr, grey); return; }   // pas encore commence : heure locale
  drawScoreNFL(px, g.awayScore);
  drawScoreNFL(px + NFL_COL_RIGHT_X, g.homeScore);
  if (g.state == 2) {
    drawQuarterNFL(px, g.wasOT ? nbaPeriodLabel(g.period) : "");
    drawClockNFL(px, T("Fin", "Final"), grey);
  } else if (g.halftime) {
    drawClockNFL(px, T("Demi", "Half"), grey);
  } else {
    drawQuarterNFL(px, nbaPeriodLabel(g.period));                  // Q1-Q4, OT, 2OT
    if (g.poss) drawPossessionNFL(px, g.poss);     // ballon + fleche (NFL seulement : poss = 0 ailleurs)
    // Entre deux jeux le chrono est fige : c'est normal, l'API renvoie le temps arrete.
    drawClockNFL(px, g.clockStr, grey);
  }
}

void drawPanelNFL(int px, const GameNFL& g) {
  drawPanelESPN(px, g, nflLogoByAbbrev(g.awayAbbr), nflLogoByAbbrev(g.homeAbbr));
}

// Aucun match NFL : logo du favori s'il y en a un, sinon juste le texte.
void drawNoGameNFL(int px) {
  // Logo 20x20 natif du favori, sinon le bouclier NFL : le panneau indique toujours de quel sport il s'agit.
  const uint16_t* lg = cfgFavTeamNFL[0] ? nflLogoByAbbrev(cfgFavTeamNFL) : LOGO_NFL_SHIELD;
  if (lg) drawTeamLogoNFL(px + 5, 6, lg);
  drawNoGameText(px);
}

void drawNoGameNHL(int px) {
  // Sans favori : bouclier NHL 28x28 natif. Sinon SON logo 28x28 (normal ou alternatif selon le toggle).
  if (cfgFavTeamNHL[0] == 0) blit565(px + 1, 2, LOGO_NHL_SHIELD28, NHL_SHIELD28_W, NHL_SHIELD28_H);
  else                       drawLogoNHL(px + 1, 2, nhlLogoForDisplay(cfgFavTeamNHL));
  drawNoGameText(px);
}

// ====================== PWHL / NBA / NCAA : UN MATCH SUR 64x32 ======================
// Meme mise en page que la NFL (logos 20x20 aux deux bouts, periode entre les
// logos, pointage sous chaque logo, chrono / "Fin" / heure au centre) : on
// reutilise les helpers NFL (drawTeamLogoNFL, drawScoreNFL, drawQuarterNFL,
// drawClockNFL) - memes dimensions 20x20.

// PWHL : une equipe sans logo (nouvelle equipe, code inconnu) est affichee par
// son code en texte dans la meme zone 20x20 - jamais un trou noir.
void drawPwhlTeam20(int x, const char* abbr) {
  const uint16_t* lg = pwhlLogoByAbbrev(abbr);
  if (lg) { drawTeamLogoNFL(x, NFL_LOGO_Y, lg); return; }
  dma->drawRect(x, NFL_LOGO_Y, NFL_LOGO_W, NFL_LOGO_H, C(60, 60, 60));
  int n = strlen(abbr);
  dma->setTextSize(1);
  dma->setTextColor(C(200, 200, 200));
  if (n * 6 <= NFL_LOGO_W) {
    dma->setCursor(x + (NFL_LOGO_W - n * 6) / 2 + 1, NFL_LOGO_Y + 6);
    dma->print(abbr);
  } else {   // 4+ caracteres : petite police
    dma->setFont(&TomThumb);
    int16_t bx, by; uint16_t bw, bh;
    dma->getTextBounds(abbr, 0, 13, &bx, &by, &bw, &bh);
    dma->setCursor(x + (NFL_LOGO_W - (int)bw) / 2, NFL_LOGO_Y + 13);
    dma->print(abbr);
    dma->setFont();
  }
}

void drawPanelPWHL(int px, const GamePWHL& g) {
  drawPwhlTeam20(px,                   g.awayAbbr);
  drawPwhlTeam20(px + NFL_COL_RIGHT_X, g.homeAbbr);
  if (g.state == 1) {     // EN DIRECT : pointages + periode (pas de chrono dans cette API)
    drawScoreNFL(px, g.awayScore);
    drawScoreNFL(px + NFL_COL_RIGHT_X, g.homeScore);
    drawQuarterNFL(px, nhlPeriodLabelCompact(g.period));
    if (g.intermission) drawClockNFL(px, T("Ent", "Int"), C(120, 120, 120));
  } else if (g.state == 2) {
    drawScoreNFL(px, g.awayScore);
    drawScoreNFL(px + NFL_COL_RIGHT_X, g.homeScore);
    const char* txt = g.wasSO ? T("F/TB", "F/SO")
                     : g.wasOT ? T("F/P", "F/OT")
                     : T("Fin", "Final");
    drawClockNFL(px, txt, C(120, 120, 120));
  } else {
    drawClockNFL(px, g.timeStr, C(120, 120, 120));
  }
}

// PWHL "aucun match" : logo du favori (20x20) ou, sans favori, l'emblem de la ligue.
void drawNoGamePWHL(int px) {
  const uint16_t* lg = cfgFavTeamPWHL[0] ? pwhlLogoByAbbrev(cfgFavTeamPWHL) : nullptr;
  if (lg) drawTeamLogoNFL(px + 5, 6, lg);
  else    blit565(px + 1, 7, LOGO_PWHL_LEAGUE, PWHL_LEAGUE_W, PWHL_LEAGUE_H);
  drawNoGameText(px);
}

// Periode NBA / NCAA : Q1-Q4, OT, 2OT...
const char* nbaPeriodLabel(uint8_t period) {
  static char buf[8];
  if (period >= 1 && period <= 4) { snprintf(buf, sizeof(buf), "Q%d", period); return buf; }
  if (period == 5) return "OT";
  if (period > 5)  { snprintf(buf, sizeof(buf), "%dOT", period - 4); return buf; }
  return "";
}

void drawPanelNBA(int px, const GameNBA& g) {
  drawPanelESPN(px, g, nbaLogoByAbbrev(g.awayAbbr), nbaLogoByAbbrev(g.homeAbbr));
}

// NBA "aucun match" : logo du favori (20x20) ou, sans favori, la silhouette de la ligue.
void drawNoGameNBA(int px) {
  const uint16_t* lg = cfgFavTeamNBA[0] ? nbaLogoByAbbrev(cfgFavTeamNBA) : nullptr;
  if (lg) drawTeamLogoNFL(px + 5, 6, lg);
  else    blit565(px + 1 + (28 - NBA_LEAGUE_W) / 2, 2, LOGO_NBA_LEAGUE, NBA_LEAGUE_W, NBA_LEAGUE_H);
  drawNoGameText(px);
}

// Logo manquant -> logo NCAA (le classement change, il peut manquer des logos).
static const uint16_t* ncaaLogo(const char* ab) {
  const uint16_t* l = ncaaLogoByAbbrev(ab);
  return l ? l : LOGO_NCAA_NCAA;
}

void drawPanelNCAA(int px, const GameNCAA& g) {
  drawPanelESPN(px, g, ncaaLogo(g.awayAbbr), ncaaLogo(g.homeAbbr));
}

// Top 6 NCAA : 2 colonnes x 3 rangees. Chaque case (32x10) : rang, logo 10x10
// (reduit du 20x20), bilan en police 3x5.
void drawRankingNCAA(int px) {
  for (int i = 0; i < rankCountNCAA && i < 6; i++) {
    int cx = px + (i / 3) * 32;
    int y0 = 1 + (i % 3) * 10;
    char num[3];
    snprintf(num, sizeof(num), "%d", i + 1);
    dma->setFont(&TomThumb);
    dma->setTextSize(1);
    dma->setTextColor(C(255, 255, 255));
    dma->setCursor(cx, y0 + 8);
    dma->print(num);
    dma->setTextColor(C(160, 160, 160));
    dma->setCursor(cx + 16, y0 + 8);
    dma->print(rankNCAA[i].rec);
    dma->setFont();
    drawLogoNHLArea(cx + 5, y0, ncaaLogo(rankNCAA[i].abbr), 20, 10);
  }
}

// NCAA "aucun match" : logo de l'equipe favorite (favSlot 0-5) ou logo NCAA.
void drawNoGameNCAA(int px, int favSlot) {
  const bool hasFav = (favSlot >= 0 && favSlot < 6 && cfgFavNCAA[favSlot][0] != 0);
  drawTeamLogoNFL(px + 5, 6, hasFav ? ncaaLogo(cfgFavNCAA[favSlot]) : LOGO_NCAA_NCAA);
  drawNoGameText(px);
}

// ====================== HORLOGE + METEO (panneau droit) ======================
void drawWeatherClock(int px) {
  // Ligne du haut : heure a GAUCHE, date a DROITE (ex: "16:45    08/07").
  // Les deux font 5 caracteres = 30px chacun ; sur un panneau de 64px il ne
  // reste que ~2px de marge de chaque cote, d'ou le collage aux bords.
  struct tm t;
  char hm[6] = "--:--";
  char dt[6] = "--/--";
  bool haveTime = getLocalTime(&t, 20);
  if (haveTime) {
    strftime(hm, sizeof(hm), "%H:%M", &t);
    strftime(dt, sizeof(dt), "%d/%m", &t);   // jour/mois (convention d'ici)
  }
  dma->setTextSize(1);
  dma->setTextColor(C(255, 255, 255));
  dma->setCursor(px + 1, 1);
  dma->print(hm);
  dma->setTextColor(C(170, 170, 170));       // date un peu plus discrete que l'heure
  dma->setCursor(px + 63 - (strlen(dt) * 6), 1);
  dma->print(dt);

  // Pictogramme (bas-gauche)
  int cat = wxValid ? wxCategory(wxCode, wxIsDay) : WX_CLOUD;
  drawWxIcon(px + 15, 21, cat);

  // Temperature (bas-droite)
  if (wxValid) {
    char tmp[6];
    snprintf(tmp, sizeof(tmp), "%d", (int)lround(wxTemp));
    dma->setTextColor(C(255, 255, 255));
    dma->setCursor(px + 38, 18);
    dma->print(tmp);
    int dx = px + 38 + strlen(tmp) * 6 + 1;
    dma->drawCircle(dx, 19, 1, C(255, 255, 255));   // petit "degre"
  }
}


// Redessine SEULEMENT les panneaux qui montrent la meteo (liste memorisee par
// le dernier render() complet), sans toucher au reste de l'ecran. Utilise pour
// l'animation pluie/neige : evite le scintillement plein-ecran, confine le
// redessin repete a la seule zone qui change reellement.
void renderWeatherOnly() {
  if (!screenOn() || wxPanelCount == 0) return;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
  for (int i = 0; i < wxPanelCount; i++) {
    int px = wxPanels[i];
    dma->fillRect(px, 0, 64, 32, 0);   // efface SEULEMENT ce panneau (pas tout l'ecran)
    drawWeatherClock(px);
  }
  xSemaphoreGive(dataMutex);
}

// Chrono NHL "live" : fait descendre le temps restant seconde par seconde entre
// deux fetchs (30 s), comme sur un vrai tableau. Redessine SEULEMENT la zone du
// chrono (pas de fillScreen -> pas de scintillement). Ne fait rien pendant un
// entracte ou si le chrono est arrete (clock.running=false) ; a chaque fetch
// l'API recale la valeur.
void tickNhlClock() {
  if (!screenOn() || nhlClockPx < 0) return;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  if (nhlClockIdx >= 0 && nhlClockIdx < gameCountNHL) {
    GameNHL& g = gamesNHL[nhlClockIdx];
    if (g.state == 1 && !g.intermission && g.clockRunning && g.clockSecs >= 0 &&
        strcmp(g.homeAbbr, nhlClockHome) == 0) {
      long el  = (long)((millis() - g.clockSyncMs) / 1000UL);
      long rem = g.clockSecs - el;
      if (rem < 0) rem = 0;                       // reste a 00:00 jusqu'au prochain fetch
      char buf[16];
      snprintf(buf, sizeof(buf), "%02ld:%02ld", rem / 60, rem % 60);
      if (strcmp(buf, g.clockStr) != 0) {
        strncpy(g.clockStr, buf, 5); g.clockStr[5] = 0;
        dma->fillRect(nhlClockPx + 48, 20, 32, 8, 0);       // efface seulement le chrono
        dma->setTextSize(1);
        dma->setTextColor(C(255, 255, 255));
        dma->setCursor(nhlClockPx + 64 - (int)(strlen(buf) * 6) / 2, 20);   // meme position que drawCenterLiveNHL()
        dma->print(buf);
      }
    }
  }
  xSemaphoreGive(dataMutex);
}
