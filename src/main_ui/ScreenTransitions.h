// Transizioni tra schermate/stati del motore: caricamento gioco, avvio
// script del place, ingresso/uscita da Studio, avvio/stop del Test in
// gioco, Avatar Publisher, ritorno al menu, avvio di Game Engine/Server
// dedicato come processi separati. Non e' un header riusabile: usa
// direttamente le globali di main.cpp (g_screen, g_world, g_menu, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

static void startLoadingGame(int idx) {
    if (idx < 0 || idx >= (int)g_games.size()) return;
    g_activeGame = g_games[idx];
    g_loadTimer = 0.f;
    g_screen = AppScreen::Loading;
    // La schermata di caricamento (dentro il canvas) mostra gia' il nome
    // del gioco — questo aggiorna anche la barra del titolo della finestra
    // di Windows, che prima restava ferma sul nome precedente per tutta la
    // durata del caricamento e cambiava solo a fine giro in finishLoading().
    glfwSetWindowTitle(glfwGetCurrentContext(), ("Blocky — " + g_activeGame.title + " (caricamento...)").c_str());
}

static void bootPlaceScript(const std::string& scriptRel) {
    if (!g_lua.ok) {
        if (!g_lua.init(&g_world)) {
            std::fprintf(stderr, "Lua init fallita: %s\n", g_lua.lastError.c_str());
            return;
        }
        bindLuaServices();
    }
    if (scriptRel.empty()) return;
    if (scriptRel.find('\n') != std::string::npos ||
        (scriptRel.size() > 8 && scriptRel.find(".lua") == std::string::npos
         && scriptRel.find(".bbc") == std::string::npos
         && scriptRel.find('\\') == std::string::npos && scriptRel.find('/') == std::string::npos)) {
        std::fprintf(stdout, "Eseguo script inline (%d char)\n", (int)scriptRel.size());
        if (!g_lua.runString(scriptRel))
            g_lua.runString("print('Avviso: script non caricato')");
        return;
    }
    std::string scriptPath = fileExistsPath(scriptRel) ? scriptRel : resolveScriptPath(scriptRel);
    std::fprintf(stdout, "Carico script (SafeBytecode preferito): %s\n", scriptPath.c_str());
    if (!g_lua.runScriptPathSmart(scriptPath, true)) {
        g_lua.runString("print('Avviso: script non caricato')");
    }
}

static void finishLoading() {
    applyGamePlace(g_activeGame, g_world, &g_lighting);
    syncFriendsFromWorld();

    const char* uname = g_menu.username[0] ? g_menu.username : "Player";
    int gi = 0;
    for (int i = 0; i < (int)g_games.size(); ++i)
        if (g_games[i].id == g_activeGame.id) { gi = i; break; }
    int maxP = g_activeGame.maxPlayers > 0 ? g_activeGame.maxPlayers : 8;
    maxP = std::clamp(maxP, 1, 32);
    g_gatePassed = false;
    g_gateDeadline = glfwGetTime() + 8.0;
    g_gateTitle[0] = '\0';
    g_gateMsg[0] = '\0';

    if (g_joinByIp) {
        g_joinByIp = false;
        std::snprintf(g_net.localName, sizeof(g_net.localName), "%.23s", uname);
        if (!g_net.startClient(g_menu.joinIp, g_menu.joinPort, gi, uname)) {
            std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus),
                          "Impossibile contattare %s:%d", g_menu.joinIp, g_menu.joinPort);
        }
    } else if (g_joinByCode) {
        g_joinByCode = false;
        std::snprintf(g_net.localName, sizeof(g_net.localName), "%.23s", uname);
        std::string code = g_menu.joinCode;
        if (code.compare(0, 6, "blk://") == 0) code = code.substr(6);
        if (!g_net.startClientViaPool(code.c_str(), blk::PoolConfig::kPoolHost, blk::PoolConfig::kPoolPort, gi, uname)) {
            std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus),
                          "Codice \"%s\" non valido o pool irraggiungibile", code.c_str());
        }
    } else {
        g_net.startAutoMatch(27015, gi, uname, maxP);
        if (g_net.role == NetRole::Host)
            openPortAsync(27015);
    }
    if (g_net.role == NetRole::Client)
        g_playerPos.z = 4.f;
    else
        g_playerPos.z = 0.f;
    std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "%s", g_net.status);

    g_lua.requireSealedScripts = true;
    if (scriptShouldRunHere(g_activeGame.scriptType))
        bootPlaceScript(g_activeGame.script);
    runAllPartScripts(false);
    g_lua.requireSealedScripts = false;
    acBoot();
    respawnPlayerAtSpawn();
    g_playerYaw = 180.f;
    g_orbitYaw = 0.f;
    g_orbitPitch = -22.f;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_selectedPart = -1;
    g_mode = CamMode::Play;
    g_screen = AppScreen::Playing;
    g_playMenuOpen = false;
    g_showChat = false;
    g_shiftLock = false;
    g_firstPerson = false;
    g_moveVel = {0, 0, 0};
    g_menu.wantPlay = false;
    g_menu.launchGame = -1;
    g_useGlbAvatar = false;
    g_remoteBodies.clear();

    g_world.equippedBackpack = -1;
    for (bool& b : g_bpKeyWas) b = false;
    for (bool& b : g_bpKpWas) b = false;

    glfwSetWindowTitle(glfwGetCurrentContext(), ("Blocky — " + g_activeGame.title).c_str());
    setPlayCursor(glfwGetCurrentContext(), true);
}

static void enterStudio() {
    g_world.loadBaseplatePlace();
    g_placeFilePath.clear();
    g_wsMeshes.clear();
    g_sounds.stopAll();
    g_sounds.sounds.clear();
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_animator.stop();
    g_showAnimEditor = false;
    g_showRigPrompt = false;
    g_animPlaying = false;
    g_animEditModel = -1;
    g_showPlayerRigPrompt = false;
    g_showPlayerRigEditor = false;
    clearPlayerSelection();
    acBoot();
    ensureDefaultBeepWav();
    if (g_gameNameBuf[0])
        g_world.placeName = g_gameNameBuf;
    else
        std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    g_publishMsg.clear();
    g_studioTesting = false;
    g_activeGame = {"studio", "Studio", "Build", "scripts/baseplate.lua", 0, {0.2f, 0.62f, 0.35f}};
    if (g_lua.ok) g_lua.clearHeartbeats();
    g_mode = CamMode::Studio;
    g_cam.pos = {0, 12, 28};
    g_screen = AppScreen::Studio;
    g_useGlbAvatar = false;
    selectPart(g_world.parts.empty() ? -1 : 0);
    g_tool = StudioTool::Select;
    applyStudioTheme();
    glfwSetWindowTitle(glfwGetCurrentContext(), kAppBrand);
    setPlayCursor(glfwGetCurrentContext(), false);
    clearPlaceDirty();
}

static void stopStudioTest(GLFWwindow* win);

static void startStudioTest(GLFWwindow* win) {
    if (g_screen != AppScreen::Studio) return;
    CrashHandler::note("Start Studio Test");
    g_worldTestBackup = g_world;
    g_hasTestBackup = true;
    g_friendsTestBackup = g_friends;
    g_playerPosTestBackup = g_playerPos;
    g_playerYawTestBackup = g_playerYaw;
    g_selectedPartTestBackup = g_selectedPart;
    g_selectedModelTestBackup = g_selectedModel;
    g_selectedLabelTestBackup = g_selectedLabel;

    g_studioCamBackup = g_cam.pos;
    g_studioYawBackup = g_cam.yaw;
    g_studioPitchBackup = g_cam.pitch;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_studioTesting = true;
    g_mode = CamMode::Play;
    g_modelAnimClock = 0.f;
    respawnPlayerAtSpawn();
    g_playerYaw = 180.f;
    g_orbitYaw = 0.f;
    g_orbitPitch = -22.f;
    g_orbitDist = 14.f;
    g_moveVel = {0, 0, 0};
    g_shiftLock = false;
    g_firstPerson = false;
    g_playMenuOpen = false;
    g_showChat = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    g_player = BlockPlayer{};
    g_ragdoll.clear();
    g_particles.clear();
    g_touching.clear();
    g_world.equippedBackpack = -1;
    for (bool& b : g_bpKeyWas) b = false;
    for (bool& b : g_bpKpWas) b = false;
    for (auto& p : g_world.parts) {
        if (p.name.size() >= 7 && p.name.compare(0, 7, "Explode") == 0
            && p.script.empty()) {
            p.boomOnTouch = true;
            if (p.boomAutoSecs <= 0.f) p.boomAutoSecs = 3.f;
            p.canCollide = false;
        }
    }
    std::string crashReason;
    bool okBoot = TestSessionGuard::runGuarded([&]() {
        startTestScripts();
    }, &crashReason);
    if (!okBoot) {
        // Un crash nativo durante l'avvio degli script (bug in una delle
        // funzioni C esposte a Lua) — invece di lasciare g_studioTesting=true
        // con uno stato a meta' inizializzato, si torna subito a Studio
        // usando lo stesso ripristino di stopStudioTest().
        CrashHandler::note(("Crash contenuto in startTestScripts: " + crashReason).c_str());
        std::fprintf(stderr, "[TestSessionGuard] crash contenuto in startTestScripts: %s\n",
                     crashReason.c_str());
        stopStudioTest(win);
        g_luaRunMsg = "Test non avviato — crash in uno script: " + crashReason;
        return;
    }
    setPlayCursor(win, true);
    char t[192];
    std::snprintf(t, sizeof(t), "%s — Test: %s", kAppBrand, g_world.placeName.c_str());
    glfwSetWindowTitle(win, t);
}

static void stopStudioTest(GLFWwindow* win) {
    if (!g_studioTesting) return;
    g_studioTesting = false;
    g_mode = CamMode::Studio;

    g_lua.clearHeartbeats();
    scriptControls().jumpEnabled = true;
    scriptControls().walkEnabled = true;
    scriptControls().walkSpeedOverride = -1.f;
    scriptControls().jumpPowerOverride = -1.f;
    g_particles.clear();
    g_touching.clear();
    g_ragdoll.clear();
    g_moveVel = {0, 0, 0};
    g_walking = false;
    g_shiftLock = false;
    g_firstPerson = false;
    g_playMenuOpen = false;
    g_swordSwing = 0.f;
    g_swordCooldown = 0.f;

    if (g_hasTestBackup) {
        g_world = std::move(g_worldTestBackup);
        g_hasTestBackup = false;
        g_friends = std::move(g_friendsTestBackup);
        g_playerPos = g_playerPosTestBackup;
        g_playerYaw = g_playerYawTestBackup;
        g_player = BlockPlayer{};
        g_animator.stop();
        g_selectedPart = -1;
        g_selectedModel = -1;
        g_selectedLabel = -1;
        g_selectedSound = -1;
        g_selectedSpawn = false;
        g_selectedPlayer = false;
        if (g_selectedPartTestBackup >= 0
            && g_selectedPartTestBackup < (int)g_world.parts.size())
            selectPart(g_selectedPartTestBackup);
        else if (g_selectedModelTestBackup >= 0
                 && g_selectedModelTestBackup < (int)g_world.models.size())
            selectModel(g_selectedModelTestBackup);
        else if (g_selectedLabelTestBackup >= 0
                 && g_selectedLabelTestBackup < (int)g_world.labels.size())
            selectLabel(g_selectedLabelTestBackup);
        syncPartEditors();
    }

    g_cam.pos = g_studioCamBackup;
    g_cam.yaw = g_studioYawBackup;
    g_cam.pitch = g_studioPitchBackup;
    g_luaRunMsg = "Test fermato — place e player ripristinati";
    setPlayCursor(win, false);
    applyStudioTheme();
    glfwSetWindowTitle(win, kAppBrand);
}

static void enterAvatarPublisher() {
    g_screen = AppScreen::AvatarPublisher;
    g_catalog.open = true;
    g_catalog.wizardStep = 0;
    g_catalog.pendingSlot = AvatarSlot::Head;
    g_catalog.camYaw = 35.f;
    g_catalog.camPitch = -15.f;
    g_catalog.camDist = 12.f;
    g_catalog.status = "Scegli dove mettere il modello (Head / Torso / Arms / Legs)";
    g_playerPos = {0, 0, 0};
    g_playerYaw = 180.f;
    applyStudioTheme();
    if (kIsGameEngine)
        glfwSetWindowTitle(glfwGetCurrentContext(), "Blocky Game Engine — Avatar Catalog");
    else
        glfwSetWindowTitle(glfwGetCurrentContext(), "Blocky — Avatar Catalog");
    setPlayCursor(glfwGetCurrentContext(), false);
}

static void returnToStudioEditor(GLFWwindow* win) {
    stopStudioTest(win);
    g_catalog.open = false;
    g_screen = AppScreen::Studio;
    g_mode = CamMode::Studio;
    g_studioTesting = false;
    applyStudioTheme();
    glfwSetWindowTitle(win, kAppBrand);
    setPlayCursor(win, false);
}

static void goGameEngineHome(GLFWwindow* win) {
    stopStudioTest(win);
    g_catalog.open = false;
    g_studioTesting = false;
    g_screen = AppScreen::MainMenu;
    g_mode = CamMode::Studio;
    applyStudioTheme();
    glfwSetWindowTitle(win, "Blocky Game Engine — Home");
    setPlayCursor(win, false);
}

static void returnToMenu(GLFWwindow* win) {
    if (kIsGameEngine) {
        goGameEngineHome(win);
        return;
    }
    g_screen = AppScreen::MainMenu;
    g_lua.shutdown();
    bool wasIn = g_menu.loggedIn;
    char user[64];
    std::snprintf(user, sizeof(user), "%s", g_menu.username);
    int coins = ecoBalance(user);
    auto chat = g_menu.chatLines;
    auto mod = std::move(g_menu.mod);
    auto shopItems = std::move(g_menu.shopItems);
    bool shopSeeded = g_menu.shopSeeded;
    g_menu = ClassicMenuState{};
    g_menu.loggedIn = wasIn;
    g_menu.showLogin = !wasIn;
    std::snprintf(g_menu.username, sizeof(g_menu.username), "%s", user);
    ecoBoot(g_menu.username, coins);
    ecoSyncMenu(g_menu);
    g_menu.chatLines = std::move(chat);
    g_menu.mod = std::move(mod);
    g_menu.shopItems = std::move(shopItems);
    g_menu.shopSeeded = shopSeeded;
    g_net.shutdown();
    closePortIfOpen();
    g_remoteBodies.clear();
    g_selectedPart = -1;
    g_studioTesting = false;
    saveUserProfile(g_menu);
    refreshGamesList();
    applyClassicRobloxTheme();
    setPlayCursor(win, false);
    glfwSetWindowTitle(win, "Blocky");
}

/** Nome di un eseguibile di Blocky accanto a questo, con l'estensione giusta. */
static std::string siblingExePath(const char* baseName) {
    const std::string dir = plat::selfDir();
    if (dir.empty() || dir == ".") return {};
#ifdef _WIN32
    return dir + "\\" + baseName + ".exe";
#else
    return dir + "/" + baseName;
#endif
}

static bool launchGameEngineApp() {
    const std::string engine = siblingExePath("BlockyGameEngine");
    if (engine.empty() || !plat::fs::exists(engine)) return false;
    return plat::sys::launchProcess(engine, {}, plat::selfDir());
}

static bool launchDedicatedServer(int maxPlayers, const char* title) {
    const std::string server = siblingExePath("BlockyServer");
    if (server.empty() || !plat::fs::exists(server)) {
        g_publishMsg = "BlockyServer non trovato — ricompila il target BlockyServer";
        return false;
    }
    const int nmax = std::clamp(maxPlayers, 1, 32);
    char args[256];
    std::snprintf(args, sizeof(args), "--port 27015 --max %d --name \"%s\"",
                  nmax, title && title[0] ? title : "Blocky");

    if (plat::sys::launchProcess(server, args, plat::selfDir())) {
        g_publishMsg = std::string("Server dedicato avviato (max ")
            + std::to_string(nmax) + ") — Join da Blocky con IP di questo PC";
        return true;
    }
    // launchProcess dice "non fatto" invece di fingere. Su Android non e' un
    // guasto: un'app semplicemente non avvia altri eseguibili, e l'utente
    // deve leggere quello, non un errore generico che lo manda a cercare una
    // causa che non esiste.
    g_publishMsg = plat::has(plat::Service::LaunchProcess)
        ? "Impossibile avviare BlockyServer"
        : "Questa piattaforma non puo' avviare un server: usane uno su un PC.";
    return false;
}
