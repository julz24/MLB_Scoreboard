// ============================================================
//  t8_mise_a_jour.ino - Mise a jour du firmware depuis les releases GitHub.
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t8).
// ============================================================
// Fonctionnement :
//  - netTask verifie la derniere release de FW_REPO ~2 min apres le demarrage puis
//    toutes les 6 h (et sur le bouton "Verifier" de la web UI). Il ne fait que
//    SIGNALER une nouvelle version : l'installation se fait seulement sur le bouton
//    "Installer" de la web UI.
//  - Toutes les connexions de mise a jour VERIFIENT le certificat TLS (paquet de
//    certificats racine integre au core ESP32) : pas de setInsecure() ici, un faux
//    serveur ne peut pas faire installer un autre firmware.
//  - Le .bin est controle par sa somme SHA-256 (champ "digest" de l'API GitHub)
//    AVANT d'etre marque comme partition de demarrage ; en cas d'ecart, rien n'est
//    installe et l'ancien firmware reste en place.

#define FW_CHECK_EVERY_MS (6UL * 3600UL * 1000UL)
#define FW_FIRST_CHECK_MS (2UL * 60UL * 1000UL)

// "v1.10.2" / "1.10.2" -> 1010002 (comparaison numerique, pas alphabetique). -1 = illisible.
static long fwVersionNum(const char* v) {
  if (!v) return -1;
  if (*v == 'v' || *v == 'V') v++;
  int a = 0, b = 0, c = 0;
  if (sscanf(v, "%d.%d.%d", &a, &b, &c) < 2) return -1;
  return (long)a * 1000000L + (long)b * 1000L + c;
}

static void fwSetError(const char* msg) {
  strncpy(fwErr, msg, sizeof(fwErr) - 1); fwErr[sizeof(fwErr) - 1] = 0;
  fwStat = FW_ERROR;
  logAdd(String("MAJ: ") + msg);
}

// Lit la derniere release publiee. Renvoie true si la reponse a ete comprise.
bool fwCheck() {
  if (!FW_REPO[0]) { fwSetError("depot GitHub non configure (version.h)"); return false; }
  fwStat = FW_CHECKING;
  int len = 0;
  uint8_t* buf = httpsDownload("MAJ", "api.github.com",
                               String("https://api.github.com/repos/") + FW_REPO + "/releases/latest",
                               200000, len, 30000, true);
  if (!buf) { fwSetError("GitHub injoignable"); return false; }

  JsonDocument filter;
  filter["tag_name"] = true;
  JsonObject af = filter["assets"][0].to<JsonObject>();
  af["name"] = true;
  af["size"] = true;
  af["digest"] = true;
  af["browser_download_url"] = true;
  JsonDocument doc(&psramAllocator);
  DeserializationError err = deserializeJson(doc, (char*)buf, len, DeserializationOption::Filter(filter));
  free(buf);
  if (err) { fwSetError("reponse GitHub illisible"); return false; }

  const char* tag = doc["tag_name"] | "";
  if (!tag[0]) { fwSetError("aucune release publiee"); return false; }
  JsonObject asset;
  for (JsonObject a : doc["assets"].as<JsonArray>())
    if (strcmp(a["name"] | "", FW_ASSET) == 0) { asset = a; break; }
  if (asset.isNull()) { fwSetError("release sans " FW_ASSET); return false; }

  strncpy(fwLatest, tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag, sizeof(fwLatest) - 1);
  fwLatest[sizeof(fwLatest) - 1] = 0;
  strncpy(fwUrl, asset["browser_download_url"] | "", sizeof(fwUrl) - 1); fwUrl[sizeof(fwUrl) - 1] = 0;
  fwSize = asset["size"] | 0;
  const char* dg = asset["digest"] | "";          // "sha256:<64 hex>" (absent sur les tres vieilles releases)
  fwSha[0] = 0;
  if (strncmp(dg, "sha256:", 7) == 0 && strlen(dg) == 71) { strncpy(fwSha, dg + 7, 64); fwSha[64] = 0; }

  long have = fwVersionNum(FW_VERSION), latest = fwVersionNum(fwLatest);
  fwErr[0] = 0;
  fwStat = (latest > have) ? FW_AVAILABLE : FW_UP_TO_DATE;
  logAdd("MAJ: version installee " FW_VERSION ", derniere release " + String(fwLatest) +
         (fwStat == FW_AVAILABLE ? " -> mise a jour disponible" : " -> a jour"));
  return true;
}

// Ecran pendant l'installation (le rendu normal est suspendu par otaActive).
static void fwPanel(int pct) {
  dma->fillScreen(0);
  dma->setTextSize(1);
  dma->setTextColor(C(239, 159, 39));
  dma->setCursor(2, 1);
  dma->print(T("Mise a jour ", "Updating to "));
  dma->print(fwLatest);
  dma->drawRect(2, 14, W - 4, 8, C(90, 90, 90));
  if (pct > 0) dma->fillRect(4, 16, (W - 8) * pct / 100, 4, C(0, 200, 70));
  dma->setTextColor(C(150, 150, 150));
  dma->setCursor(2, 24);
  dma->print(T("Ne pas debrancher", "Do not unplug"));
}

// Telecharge et installe fwUrl. Ne revient pas en cas de succes (redemarrage).
void fwInstall() {
  if (fwStat != FW_AVAILABLE || !fwUrl[0]) return;
  if (!fwSha[0]) { fwSetError("release sans somme SHA-256"); return; }
  logAdd("MAJ: installation de " + String(fwLatest) + " (" + String(fwSize) + " octets)");
  otaActive = true;                                  // suspend fetchs et rendu (comme ElegantOTA)
  fwStat = FW_INSTALLING; fwProgress = 0;
  flushCfg();                                        // reglages en attente : ecrits avant le redemarrage
  fwPanel(0);

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
  client.setTimeout(20000);
  client.setHandshakeTimeout(15);
  HTTPClient https;
  bool ok = false;
  const char* why = "echec inconnu";
  if (https.begin(client, fwUrl)) {
    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);   // GitHub redirige vers son stockage
    https.setTimeout(20000);
    https.useHTTP10(true);
    netStep("MAJ");
    int code = https.GET();
    int len = https.getSize();
    if (code != 200) why = "telechargement refuse";
    else if (len <= 0 || (fwSize > 0 && (uint32_t)len != fwSize)) why = "taille inattendue";
    else if (!Update.begin(len, U_FLASH)) why = "pas de place pour le firmware";
    else {
      mbedtls_sha256_context sha;
      mbedtls_sha256_init(&sha);
      mbedtls_sha256_starts(&sha, 0);
      static uint8_t chunk[4096];
      WiFiClient* stream = https.getStreamPtr();
      int got = 0, lastPct = -1;
      unsigned long tIdle = millis(), tFeed = millis();
      bool writeErr = false;
      while (got < len && millis() - tIdle < 20000) {
        if (millis() - tFeed > 2000) { tFeed = millis(); netStep("MAJ"); }
        int avail = stream->available();
        if (avail <= 0) { if (!stream->connected()) break; delay(2); continue; }
        int r = stream->read(chunk, min(avail, (int)sizeof(chunk)));
        if (r <= 0) continue;
        if (Update.write(chunk, r) != (size_t)r) { writeErr = true; break; }
        mbedtls_sha256_update(&sha, chunk, r);
        got += r; tIdle = millis();
        int pct = (int)((int64_t)got * 100 / len);
        if (pct / 5 != lastPct / 5) { lastPct = pct; fwProgress = pct; fwPanel(pct); }
      }
      uint8_t dig[32];
      mbedtls_sha256_finish(&sha, dig);
      mbedtls_sha256_free(&sha);
      char hex[65];
      for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", dig[i]);
      if (writeErr) why = "erreur d'ecriture en flash";
      else if (got != len) why = "telechargement interrompu";
      else if (strcasecmp(hex, fwSha) != 0) why = "somme SHA-256 differente";
      else if (!Update.end(false)) why = "firmware refuse par le verificateur";
      else ok = true;
      if (!ok) Update.abort();
    }
    https.end();
  } else {
    why = "adresse de telechargement invalide";
  }
  client.stop();

  if (ok) {
    logAdd("MAJ: " + String(fwLatest) + " installee, redemarrage");
    delay(500);
    ESP.restart();
  }
  fwSetError(why);
  fwStat = FW_AVAILABLE;                             // on peut reessayer depuis la web UI
  otaActive = false;
  dirty = true;                                      // reaffiche les matchs
}

// Appele a chaque tour de netTask (WiFi connecte, pas d'OTA ElegantOTA en cours).
void fwStep(unsigned long now) {
  static unsigned long lastCheck = 0;
  static bool first = true;
  if (fwInstallReq) { fwInstallReq = false; fwInstall(); return; }
  bool due = first ? (now >= FW_FIRST_CHECK_MS) : (now - lastCheck >= FW_CHECK_EVERY_MS);
  if (fwCheckReq || due) {
    fwCheckReq = false; first = false; lastCheck = now;
    fwCheck();
  }
}

// /api/fw?do=check | install : la web UI demande, netTask execute (jamais dans le contexte HTTP).
void handleFw() {
  String d = server.arg("do");
  if (d == "check") { fwCheckReq = true; fwStat = FW_CHECKING; }
  else if (d == "install" && fwStat == FW_AVAILABLE) fwInstallReq = true;
  else { server.send(400, "text/plain", "?"); return; }
  server.send(200, "text/plain", "ok");
}
