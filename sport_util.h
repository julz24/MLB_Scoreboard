// ============================================================
//  sport_util.h - fonctions generiques partagees par les sports
//  Ce sont des templates, places dans un .h plutot que dans un .ino :
//  le generateur de prototypes de l'IDE Arduino gere mal les templates.
//  Ils fonctionnent avec toute structure de match qui possede les champs
//  state / awayAbbr / homeAbbr (GameNHL, GamePWHL, GameESPN...).
// ============================================================
#pragma once
#include <Arduino.h>

// Vrai si au moins un match du tableau est en direct (state == 1).
template <typename G>
bool anyLive(const G* g, int n) {
  for (int i = 0; i < n; i++) if (g[i].state == 1) return true;
  return false;
}

// Construit la rotation d'un sport identifie par abreviation :
//   favIdx   = index du 1er match du favori (-1 = aucun ; fav "" = pas de favori)
//   rot[]    = index des autres matchs (seulement ceux en direct si liveOnly)
//   rotBase  = ramene dans les limites du nouveau rotCount
template <typename G>
void buildRotAbbr(const G* g, int n, const char* fav, bool liveOnly,
                  int& favIdx, int* rot, int& rotCount, int& rotBase) {
  favIdx = -1;
  rotCount = 0;
  for (int i = 0; i < n; i++) {
    bool isFav = fav[0] && (strcmp(g[i].awayAbbr, fav) == 0 || strcmp(g[i].homeAbbr, fav) == 0);
    if (isFav && favIdx < 0) { favIdx = i; continue; }
    if (liveOnly && g[i].state != 1) continue;
    rot[rotCount++] = i;
  }
  rotBase = (rotCount > 0) ? rotBase % rotCount : 0;
}
