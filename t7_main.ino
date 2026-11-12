// ============================================================
//  t7_main.ino - Demarrage : panneau, ecran de chargement, setup(), tache reseau (netTask), loop().
//  Onglet du croquis MLB_Scoreboard : l'IDE Arduino colle les onglets .ino a la
//  suite de MLB_Scoreboard.ino, par ordre alphabetique (t1, t2, ... t7).
// ============================================================

// ====================== SETUP ======================
void initPanel() {
  HUB75_I2S_CFG::i2s_pins _pins = {
    R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, _pins);
  mxconfig.clkphase = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;   // 10MHz (etait 8M) : plus de bande passante pour 4 panneaux
  mxconfig.min_refresh_rate = 100;             // force >=100Hz -> supprime le scintillement permanent sur chaine longue
  mxconfig.latch_blanking = 2;                 // marge anti-ghosting a vitesse plus elevee
  mxconfig.double_buff = false;  // 4 panneaux : le double buffer mangeait trop de RAM interne (web UI lente, OTA impossible). Desactive.
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;  // si ecran noir
  dma = new MatrixPanel_I2S_DMA(mxconfig);
  bool panelOk = dma->begin();
  dma->setBrightness8(cfgBright);
  dma->clearScreen();
  Serial.println("\n========================================");
  Serial.printf("  Panneau begin(): %s\n", panelOk ? "OK" : "ECHEC");
  Serial.printf("  RAM interne libre: %d octets\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("  Plus gros bloc interne: %d octets\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("  PSRAM libre: %d octets\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.println("========================================\n");
}


// ====================== ECRAN DE DEMARRAGE ======================
// 1) Logo WiFi : BLANC tant que le tableau n'est pas connecte (les arcs
//    s'allument un a un pendant la recherche), VERT une fois connecte.
// 2) "Chargement" : les logos NHL / MLB / NFL defilent de droite a gauche
//    pendant que les donnees se telechargent (tache separee, voir plus bas).
// === BOOT_ANIM_BEGIN ===
inline void bootPx(int x, int y, uint16_t c) {
  if (x < 0 || x >= W || y < 0 || y >= PANEL_RES_Y) return;
  dma->drawPixel(x, y, c);
}

// Logo WiFi (point + 3 arcs de 90 degres), centre sur les 256 px.
// arcs = nombre d'arcs allumes (0..3) ; les autres restent gris fonce.
void drawWifiIcon(uint16_t on, int arcs) {
  const int cx = W / 2, by = 27;                    // centre du point
  const float rad[3] = {7.0f, 12.0f, 17.0f};
  const uint16_t off = C(30, 30, 30);
  for (int y = 6; y <= 29; y++) {
    for (int x = cx - 20; x <= cx + 20; x++) {
      float dx = (float)(x - cx), dy = (float)(by - y);
      float d = sqrtf(dx * dx + dy * dy);
      uint16_t col = 0;
      if (d <= 1.9f) {
        col = on;                                   // le point
      } else if (dy > 1.5f && fabsf(dx) <= dy + 0.5f) {   // cone vers le haut
        for (int i = 0; i < 3; i++)
          if (fabsf(d - rad[i]) <= 1.0f) { col = (i < arcs) ? on : off; break; }
      }
      bootPx(x, y, col);
    }
  }
}

void bootWifiAnim() {                               // appelee ~ aux 300 ms pendant la connexion
  static uint8_t tick = 0;
  drawWifiIcon(C(255, 255, 255), tick++ % 4);       // 0..3 arcs : recherche en cours
}
void bootWifiScreen(bool connected) {
  drawWifiIcon(connected ? C(0, 220, 70) : C(255, 255, 255), 3);
}

// Une image du defilement. off = decalage en pixels (0..319, boucle sans saut :
// 320 = 5 logos x 64 px). Chaque logo de ligue (28 px de haut, voir
// logos_leagues_boot.h) est centre dans une case de 64 px (un panneau) ; seule
// la trainee de 2 px a droite de chaque logo est effacee, pas d'effacement
// complet -> pas de scintillement.
#define BOOT_PITCH 64
#define BOOT_STEP  2
#define BOOT_LOGOS 5
void bootAnimFrame(int off) {
  for (int i = 0; i < 10; i++) {                    // 10 cases : couvre l'ecran pour tout decalage 0..319
    int x0 = i * BOOT_PITCH - off;
    if (x0 + BOOT_PITCH <= 0 || x0 >= W) continue;
    const uint16_t* lg; int w, h;
    switch (i % BOOT_LOGOS) {
      case 0:  lg = LG_BOOT_NHL;  w = LG_BOOT_NHL_W;  h = LG_BOOT_NHL_H;  break;
      case 1:  lg = LG_BOOT_MLB;  w = LG_BOOT_MLB_W;  h = LG_BOOT_MLB_H;  break;
      case 2:  lg = LG_BOOT_NFL;  w = LG_BOOT_NFL_W;  h = LG_BOOT_NFL_H;  break;
      case 3:  lg = LG_BOOT_NBA;  w = LG_BOOT_NBA_W;  h = LG_BOOT_NBA_H;  break;
      default: lg = LG_BOOT_PWHL; w = LG_BOOT_PWHL_W; h = LG_BOOT_PWHL_H; break;
    }
    int xo = x0 + (BOOT_PITCH - w) / 2, yo = (PANEL_RES_Y - h) / 2;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        bootPx(xo + x, yo + y, lg[y * w + x]);
    for (int y = 0; y < h; y++)                     // efface la trainee
      for (int x = 0; x < BOOT_STEP; x++)
        bootPx(xo + w + x, yo + y, 0);
  }
}
// === BOOT_ANIM_END ===

volatile bool bootAnimRun = false, bootAnimDone = true;
void bootAnimTask(void*) {
  int off = 0;
  while (bootAnimRun) {
    bootAnimFrame(off);
    off = (off + BOOT_STEP) % (BOOT_LOGOS * BOOT_PITCH);
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  bootAnimDone = true;
  vTaskDelete(NULL);
}
// Le panneau n'est dessine QUE par cette tache pendant le chargement (les
// fetch de setup() ne touchent pas a dma) ; bootAnimStop() attend sa fin
// avant que setup() dessine le premier ecran.
void bootAnimStart() {
  dma->fillScreen(0);
  bootAnimRun = true; bootAnimDone = false;
  xTaskCreatePinnedToCore(bootAnimTask, "bootAnim", 4096, NULL, 1, NULL, 0);
}
void bootAnimStop() {
  bootAnimRun = false;
  unsigned long t0 = millis();
  while (!bootAnimDone && millis() - t0 < 500) delay(5);
  dma->fillScreen(0);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  dataMutex = xSemaphoreCreateMutex();
  logMutex  = xSemaphoreCreateMutex();
  loadCfg();
  initPanel();
  dma->fillScreen(0);
  drawWifiIcon(C(255, 255, 255), 0);   // logo WiFi blanc : pas encore connecte
  unsigned long tBoot = millis();

  // Tente la connexion au reseau enregistre. Si echec -> portail de config.
  if (!connectSTA()) {
    bootWifiScreen(false);   // logo blanc complet : non connecte
    delay(800);
    logAdd("Connexion impossible -> portail de configuration");
    startConfigPortal();
    return;   // on reste en mode AP ; loop() gere DNS + serveur
  }
  bootWifiScreen(true);    // logo WiFi vert : connecte
  delay(700);
  logAdd("[t+" + String(millis() - tBoot) + "ms] WiFi connecte, IP " + WiFi.localIP().toString());

  if (MDNS.begin(HOSTNAME)) logAdd("mDNS: http://" + String(HOSTNAME) + ".local");
  configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");

  server.on("/", []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/state", handleState);
  server.on("/api/fw", handleFw);          // mise a jour depuis GitHub (t8_mise_a_jour.ino)
  server.on("/api/set", handleSet);
  server.on("/api/logs", []() {
    // Copie sous mutex puis envoi hors mutex : netTask peut decaler logBuf a tout moment.
    static char logCopy[LOG_MAX + 1];
    if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY);
    memcpy(logCopy, logBuf, logLen + 1);
    if (logMutex) xSemaphoreGive(logMutex);
    server.send(200, "text/plain", logCopy);
  });
  server.on("/api/reboot", []() { server.send(200, "text/plain", "reboot"); flushCfg(); delay(300); ESP.restart(); });
  server.on("/api/wifi_reset", []() {
    server.send(200, "text/plain", "ok, redemarrage en mode config...");
    logAdd("Reset WiFi demande depuis la web UI (differe)");
    wifiResetPending = true;
    wifiResetAt = millis() + 1000;   // laisse la reponse HTTP se terminer
  });
  ElegantOTA.begin(&server);
  // Pendant une mise a jour OTA : on suspend les fetchs (netTask) et le rendu
  // du panneau. Avec des matchs en direct, MLB + NHL se telechargent toutes
  // les 30 s (TLS, ~45 Ko de RAM interne chacun) pendant que le firmware
  // (~1.3 Mo) arrive - la radio WiFi et la RAM sont partagees, l'envoi
  // ralentit/echoue. Reactive a la fin (succes ou echec).
  ElegantOTA.onStart([]() { otaActive = true; flushCfg(); logAdd("OTA: debut - fetchs et rendu suspendus"); });
  ElegantOTA.onEnd([](bool ok) {
    otaActive = false; dirty = true;
    logAdd(ok ? "OTA: termine avec succes" : "OTA: ECHEC - fetchs et rendu repris");
  });
  server.begin();

  bootAnimStart();          // logos des 5 ligues qui defilent pendant le chargement
  delay(1500);
  if (cfgSportMLB) fetchGames();
  if (cfgSportNHL) fetchGamesNHL();
  if (cfgSportNFL) fetchGamesNFL();
  lastFetch = millis();
  lastFetchNHL = millis() - 5000;   // decale du fetch MLB (evite 2 handshakes TLS au meme instant, a chaque cycle)
  lastFetchNFL = millis() - 10000;  // decale de MLB et NHL, meme raison
  lastRotate = millis();
  // 1ere meteo faite depuis loop(), ~5s apres le debut de la boucle (une
  // fois le serveur web deja actif), pour ne jamais retarder le boot.
  lastWx = millis() - WX_MS + 5000;
  lastStand = millis() - STAND_MS + 10000;   // 1er fetch classements ~10s apres boot (decale de la meteo)
  lastStandNHL = millis() - STAND_MS + 15000; // decale un peu plus (evite de saturer le boot)
  lastStandNFL = millis() - STAND_MS + 20000; // encore decale (3e fetch de classement au boot)
  // Nouveaux sports : 1er fetch par netTask, decale (jamais 2 handshakes TLS en meme temps),
  // sans rallonger le demarrage.
  lastFetchPWHL = millis() - FETCH_MS_SLOW + 25000;    // PWHL ~25 s apres le boot
  lastFetchNBA  = millis() - FETCH_MS_SLOW + 35000;    // NBA  ~35 s
  lastFetchNCAA = millis() - FETCH_MS_SLOW + 55000;    // NCAA ~55 s
  lastStandNBA  = millis() - STAND_MS + 45000;         // classement NBA ~45 s
  lastStandNCAA = millis() - STAND_MS + 65000;         // Top 6 NCAA ~65 s
  bootAnimStop();    // fin du defilement : le panneau est a nouveau libre
  render();          // affiche les matchs tout de suite
  logAdd("[t+" + String(millis() - tBoot) + "ms] boot termine, serveur actif");

  // Tache reseau separee : fait tourner fetchGames()/fetchWeather() sur son
  // propre coeur. En temps normal ca suffit a garder le serveur web reactif -
  // Watchdog SCOPE A netTask UNIQUEMENT (pas loopTask/serveur web/OTA, pour
  // eviter tout risque de redemarrage intempestif pendant une mise a jour
  // OTA qui prendrait du temps). 2 minutes = large marge au-dessus du pire
  // delai observe (35s) sur une connexion degradee, mais suffisamment court
  // pour ne plus jamais avoir a debrancher physiquement l'appareil.
  // IMPORTANT : ceci doit etre configure AVANT de creer netTask (voir plus
  // bas) - sinon netTask peut demarrer et appeler esp_task_wdt_add() avant
  // que le watchdog soit initialise, ce qui causait un boot-loop (bug reel,
  // trouve et corrige suite a un test sur le vrai panneau).
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_deinit();
  esp_task_wdt_config_t twdtConfig = {
    .timeout_ms = 120000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&twdtConfig);
#else
  esp_task_wdt_init(120, true);
#endif

  // MAIS on a observe sur ce projet des connect() qui restent bloques bien
  // au-dela du timeout demande (ex. 35s alors que 5s etait demande, ou pire :
  // un blocage indefini qui exigeait de debrancher/rebrancher). C'est un bug
  // deja documente cote librairie ESP32 (le parametre de timeout n'est pas
  // toujours respecte), pas quelque chose qu'on peut garantir cote appli.
  // Le watchdog ci-dessus est le vrai filet de securite : si netTask reste
  // bloque plus de 2 minutes sans revenir (vraiment coince, pas juste lent),
  // l'appareil redemarre tout seul au lieu d'exiger une intervention manuelle.
  xTaskCreatePinnedToCore(netTask, "netTask", 12288, NULL, 1, NULL, 0);
}

// Vrai si au moins un match est actuellement en direct. Lu par netTask,
// qui est aussi l'unique ecrivain de games[]/gameCount -> pas de mutex
// necessaire ici (meme tache, acces sequentiel).
bool anyLiveGame()     { return anyLive(games,     gameCount);     }
bool anyLiveGameNFL()  { return anyLive(gamesNFL,  gameCountNFL);  }
bool anyLiveGameNHL()  { return anyLive(gamesNHL,  gameCountNHL);  }
bool anyLiveGameNBA()  { return anyLive(gamesNBA,  gameCountNBA);  }
bool anyLiveGameNCAA() { return anyLive(gamesNCAA, gameCountNCAA); }
bool anyLiveGamePWHL() { return anyLive(gamesPWHL, gameCountPWHL); }

// ====================== TACHE RESEAU (coeur separe) ======================
void netTask(void* pv) {
  esp_task_wdt_add(NULL);   // enregistre CETTE tache (netTask) aupres du watchdog configure dans setup()
  unsigned long lastHeapLog = 0;
  static bool wxLastOk = true;   // resultat de la DERNIERE tentative (pas wxValid, qui garde la derniere bonne donnee)
  for (;;) {
    unsigned long now = millis();
    unsigned long fetchInterval = anyLiveGame() ? FETCH_MS : FETCH_MS_SLOW;

    // Filet de securite en mode normal : si le WiFi est perdu plus de 60 s
    // (routeur redemarre alors que le tableau reste allume), on force une
    // reconnexion au reseau connu toutes les 60 s, en plus de l'auto-reconnect.
    static unsigned long wifiLostSince = 0, lastReconnect = 0;
    if (WiFi.status() == WL_CONNECTED) {
      if (wifiLostSince) logAdd("WiFi revenu apres " + String((now - wifiLostSince) / 1000) + " s");
      wifiLostSince = 0;
    } else {
      if (!wifiLostSince) { wifiLostSince = now; logAdd("WiFi perdu"); }
      if (now - wifiLostSince >= 60000 && now - lastReconnect >= 60000) {
        lastReconnect = now;
        logAdd("WiFi toujours perdu -> reconnexion forcee");
        WiFi.reconnect();
      }
    }

    if (WiFi.status() == WL_CONNECTED && !otaActive) {   // aucun fetch pendant une OTA
      ensurePublicDns();   // le DHCP remet son DNS a chaque bail : on reimpose 1.1.1.1 avant les fetchs (sans effet si deja en place)
      if (cfgSportMLB && now - lastFetch >= fetchInterval) { lastFetch = now; fetchGames(); }

      unsigned long fetchIntervalNhl = nhlFavLive() ? FETCH_FAV_MS_NHL : (anyLiveGameNHL() ? FETCH_MS : FETCH_MS_SLOW);
      if (cfgSportNHL && now - lastFetchNHL >= fetchIntervalNhl) { lastFetchNHL = now; fetchGamesNHL(); }
      if (cfgSportNHL) nhlSituationStep(now);   // avantage numerique (play-by-play), un match a la fois

      bool nflFavLive = cfgFavTeamNFL[0] && favIdxNFL >= 0 && favIdxNFL < gameCountNFL && gamesNFL[favIdxNFL].state == 1;
      unsigned long fetchIntervalNfl = nflFavLive ? FETCH_FAV_MS_NFL : (anyLiveGameNFL() ? FETCH_MS : FETCH_MS_SLOW);
      if (cfgSportNFL && now - lastFetchNFL >= fetchIntervalNfl) { lastFetchNFL = now; fetchGamesNFL(); }

      unsigned long fetchIntervalNba = anyLiveGameNBA() ? FETCH_MS : FETCH_MS_SLOW;
      if (cfgSportNBA && now - lastFetchNBA >= fetchIntervalNba) { lastFetchNBA = now; fetchGamesNBA(); }

      unsigned long fetchIntervalNcaa = anyLiveGameNCAA() ? FETCH_MS : FETCH_MS_SLOW;
      if (cfgSportNCAA && now - lastFetchNCAA >= fetchIntervalNcaa) {
        lastFetchNCAA = now; fetchGamesNCAA();
        if (!ncaaCycleDone) lastFetchNCAA = millis() - fetchIntervalNcaa + 3000;   // conference suivante dans ~3 s (les autres sports passent entre-temps)
      }

      unsigned long fetchIntervalPwhl = anyLiveGamePWHL() ? FETCH_MS : FETCH_MS_SLOW;
      if (cfgSportPWHL && now - lastFetchPWHL >= fetchIntervalPwhl) { lastFetchPWHL = now; fetchGamesPWHL(); }

      // Apres un ECHEC, on reessaie vite (2 min) au lieu d'attendre les 15 min
      // au complet -> evite qu'une icone perimee traine des heures si un
      // fetch rate au mauvais moment (ex: pile a la transition jour/nuit).
      unsigned long wxInterval = wxLastOk ? WX_MS : WX_RETRY_MS;
      if (now - lastWx >= wxInterval) { lastWx = now; wxLastOk = fetchWeather(); }

      // Classements : toutes les 30 min si succes, sinon reessai a 2 min.
      static bool standLastOk = false;
      unsigned long standInterval = standLastOk ? STAND_MS : WX_RETRY_MS;
      if (cfgSportMLB && now - lastStand >= standInterval) { lastStand = now; standLastOk = fetchStandings(); }

      static bool standNhlLastOk = false;
      unsigned long standIntervalNhl = standNhlLastOk ? STAND_MS : WX_RETRY_MS;
      if (cfgSportNHL && now - lastStandNHL >= standIntervalNhl) { lastStandNHL = now; standNhlLastOk = fetchStandingsNHL(); }

      static bool standNflLastOk = false;
      unsigned long standIntervalNfl = standNflLastOk ? STAND_MS : WX_RETRY_MS;
      if (cfgSportNFL && now - lastStandNFL >= standIntervalNfl) { lastStandNFL = now; standNflLastOk = fetchStandingsNFL(); }

      static bool standNbaLastOk = false;
      unsigned long standIntervalNba = standNbaLastOk ? STAND_MS : WX_RETRY_MS;
      if (cfgSportNBA && now - lastStandNBA >= standIntervalNba) { lastStandNBA = now; standNbaLastOk = fetchStandingsNBA(); }

      static bool rankNcaaLastOk = false;
      unsigned long rankIntervalNcaa = rankNcaaLastOk ? STAND_MS : 10UL * 60UL * 1000UL;   // fichier lourd : 30 min si ok, 10 min si echec
      if (cfgSportNCAA && (rankForceNCAA || now - lastStandNCAA >= rankIntervalNcaa)) { rankForceNCAA = false; lastStandNCAA = now; rankNcaaLastOk = fetchRankingsNCAA(); }

      fwStep(now);   // mise a jour du firmware : verification periodique, installation sur demande de la web UI
    }
    // RAM libre + plus gros bloc contigu + RSSI WiFi, toutes les 60s : permet
    // de VOIR objectivement si la RAM se fragmente ou si le signal WiFi
    // decroche, plutot que de le supposer.
    if (now - lastHeapLog >= 60000) {
      lastHeapLog = now;
      logAdd("RAM libre: " + String(ESP.getFreeHeap()) + " octets, plus gros bloc: " +
             String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) + " octets, RSSI: " +
             String(WiFi.RSSI()) + " dBm");
    }
    esp_task_wdt_reset();   // "nourrit" le watchdog - une iteration complete vient de se terminer
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ====================== LOOP ======================
void loop() {
  // ----- Mode portail de configuration -----
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    portalRetrySaved();   // apres une panne : retente le reseau connu, redemarre des qu'il revient
    return;
  }

  // ----- Mode normal -----
  server.handleClient();
  ElegantOTA.loop();
  if (otaActive) return;   // OTA en cours : tout le temps CPU va au serveur web (pas de rendu/animation)

  // Reset WiFi differe (demande depuis la web UI) : execute ici, hors
  // contexte HTTP. On efface nos identifiants NVS et on redemarre ;
  // au reboot, connectSTA() trouve un SSID vide et lance le portail.
  if (wifiResetPending && millis() >= wifiResetAt) {
    wifiResetPending = false;
    logAdd("Effacement WiFi (NVS) et redemarrage en mode config...");
    flushCfg();
    clearWifi();
    delay(300);
    ESP.restart();
  }

  if (wxSavePending) { wxSavePending = false; saveCfg(); }   // ville resolue par netTask -> NVS (un seul ecrivain : loop)
  if (cfgSavePending && millis() - cfgSaveAt >= CFG_SAVE_DELAY_MS) flushCfg();   // reglages de la web UI : 2,5 s apres le dernier changement

  unsigned long now = millis();
  if (animTick(now)) return;   // animation de but/touche en cours : elle a l'ecran pour elle seule (serveur web deja servi plus haut)
  if (now - lastRotate >= (unsigned long)cfgRotationSec * 1000) {
    lastRotate = now;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (rotCount > 0) {
      // Avance de 1 (comme NHL/NFL) : renderUnified() n'affiche qu'UN SEUL
      // panneau MLB principal par tick desormais (le "extra" du panneau libre
      // utilise son propre compteur separe), plus 3 comme avant l'ajout de
      // la NHL/NFL. Avec l'ancien pas de 3, (rotBase+3)%rotCount ne bougeait
      // JAMAIS quand rotCount valait 1, 2, 3 ou 6 - rotation completement
      // figee (bug reel, confirme par calcul et vu sur le vrai panneau avec
      // exactement 3 matchs).
      rotBase = (rotBase + 1) % rotCount;
    }
    if (rotCountNHL > 0) rotBaseNHL = (rotBaseNHL + 1) % rotCountNHL;
    if (rotCountNFL > 0) rotBaseNFL = (rotBaseNFL + 1) % rotCountNFL;
    if (rotCountNBA > 0) rotBaseNBA = (rotBaseNBA + 1) % rotCountNBA;
    if (rotCountNCAA > 0) rotBaseNCAA = (rotBaseNCAA + 1) % rotCountNCAA;
    if (rotCountPWHL > 0) rotBasePWHL = (rotBasePWHL + 1) % rotCountPWHL;
    sportWinRot = (sportWinRot + 1) % 60;   // fenetre de sports (60 = multiple de 1..6 : pas de saut quand le nombre de sports change)
    combinedStandRot++;   // classement combine MLB+NHL (drawStandingsCombined fait le modulo)
    freeSportRot = (freeSportRot + 1) % NSPORT;
    dirty = true;
    xSemaphoreGive(dataMutex);
  }

  // Animation limitee a PLUIE/BRUINE, NEIGE et SOLEIL (mouvement utile :
  // gouttes qui tombent, flocons qui volent, rayons qui pulsent). Lune/nuage/orage restent
  // statiques pour eviter le scintillement (pas de double buffering avec 4
  // panneaux). weatherVisible approxime les cas ou la meteo apparait a l'ecran.
  {
    bool weatherVisible = cfgForceWeather || (rotCount < 3);
    int cat = wxValid ? wxCategory(wxCode, wxIsDay) : WX_CLOUD;
    if (cfgScreenOn && weatherVisible && (cat == WX_RAIN || cat == WX_SNOW || cat == WX_SUN) &&
        now - lastAnim >= 180) {
      lastAnim = now;
      animFrame++;
      renderWeatherOnly();   // redessine SEULEMENT le(s) panneau(x) meteo, pas tout l'ecran
    }
  }

  // Chrono NHL live : verifie 4x/s, ne redessine que quand la seconde change.
  if (now - lastClockTick >= 250) {
    lastClockTick = now;
    tickNhlClock();
  }

  if (dirty) {
    unsigned long tR = millis();
    bool drew = render();
    unsigned long dR = millis() - tR;
    if (drew) dirty = false;   // sinon on reessaie au prochain tour de loop() plutot que de perdre la mise a jour
    static unsigned long lastSlowLog = 0;
    if (dR > 50 && millis() - lastSlowLog > 5000) {   // render lent = etouffe le serveur web
      lastSlowLog = millis();
      logAdd("render() lent: " + String(dR) + "ms");
    }
  }
}
