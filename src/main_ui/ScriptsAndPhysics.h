// Generazione script per le part (stamina, salto, esplosione, neon),
// attach/detach script sulla selezione, esecuzione script all'avvio del
// test, e fisica/collisioni del player (assi, respawn, esplosioni,
// touch-detection). Non e' un header riusabile: usa direttamente le
// globali di main.cpp (g_world, g_player, g_playerPos, g_lua, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

// Part non ancorate: vive solo mentre si gioca (Playing o Test in Studio).
static bphys::World g_bphys;
static partconstraints::AnchoredWeldFollow g_anchoredWelds;

static void bindPhysBridgeToLua() {
    using W = bphys::World;
    g_lua.phys.world = &g_bphys;
    g_lua.phys.applyImpulse = [](void* w, int part, float x, float y, float z) {
        return w && static_cast<W*>(w)->applyImpulse(part, {x, y, z});
    };
    g_lua.phys.setLinVel = [](void* w, int part, float x, float y, float z) {
        return w && static_cast<W*>(w)->setLinearVelocity(part, {x, y, z});
    };
    g_lua.phys.getLinVel = [](void* w, int part, float* x, float* y, float* z) {
        if (!w || !x || !y || !z) return false;
        glm::vec3 v;
        if (!static_cast<W*>(w)->getLinearVelocity(part, v)) return false;
        *x = v.x; *y = v.y; *z = v.z;
        return true;
    };
    g_lua.phys.setAngVel = [](void* w, int part, float x, float y, float z) {
        return w && static_cast<W*>(w)->setAngularVelocity(part, {x, y, z});
    };
    g_lua.phys.getMass = [](void* w, int part) {
        return w ? static_cast<W*>(w)->getMass(part) : 0.f;
    };
    g_lua.phys.setGroupsCollidable = [](void* w, int a, int b, bool on) {
        if (w) static_cast<W*>(w)->setGroupsCollidable(a, b, on);
    };
    g_lua.phys.groupsCollidable = [](void* w, int a, int b) {
        return !w || static_cast<W*>(w)->groupsCollidable(a, b);
    };
    g_lua.phys.radialImpulse = [](void* w, float cx, float cy, float cz, float radius, float force) {
        if (w) static_cast<W*>(w)->applyRadialImpulse({cx, cy, cz}, radius, force);
    };
}

static std::string loadStaminaScriptSource() {
    std::string path = resolveScriptPath("scripts/stamina.lua");
    std::string src = readTextFile(path);
    return src;
}

static bool applyStaminaToSelectedPart() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size())
        return false;
    std::string src = loadStaminaScriptSource();
    if (src.empty()) {
        g_luaRunMsg = "File stamina.lua non trovato in assets/scripts";
        return false;
    }
    auto& p = g_world.parts[g_selectedPart];
    bool wasEmpty = p.script.empty();
    p.script = std::move(src);
    syncPartEditors();
    g_editingPartScript = true;
    g_luaRunMsg = "Stamina su \"" + p.name + "\" — Test (F5), poi CTRL+WASD";
    if (wasEmpty) g_showScriptCreatedGuide = true;
    return true;
}


/**
 * Script "arma da fuoco" per una Part, gia' legato a un oggetto del Backpack.
 *
 * In Blocky gli script stanno sulle Part, non sugli oggetti: l'oggetto e' cosa
 * tieni in mano, mentre a decidere cosa FA quando spari e' uno script del mondo
 * che ascolta l'evento ItemActivated. Non e' ovvio, e finche' non esiste un
 * preset che lo mostra bisogna saperlo gia'.
 *
 * Riusa item_fucile.lua invece di generare un fucile nuovo riga per riga:
 * quello e' gia' scritto contro le API vere del motore (Humanoid.GetLook,
 * workspace:Raycast, Camera.Shake, Fx:Sparks) ed e' coperto da test_lua. Un
 * fucile riscritto qui a mano sarebbe codice non provato che assomiglia a
 * codice provato — il modo piu' rapido per regalare all'utente un'arma muta.
 *
 * L'unica cosa che cambia e' la riga del nome: ed e' anche l'unica che si
 * sbaglia sempre, perche' deve combaciare ESATTAMENTE con l'oggetto nel
 * Backpack.
 */
static std::string makeWeaponScript(const std::string& itemName) {
    std::string src = readTextFile(resolveScriptPath("scripts/item_fucile.lua"));
    if (src.empty()) return "";

    // Il nome finisce dentro una stringa Lua: virgolette e backslash vanno protetti.
    std::string safe;
    for (char c : itemName) {
        if (c == '"' || c == '\\') safe += '\\';
        safe += c;
    }
    if (safe.empty()) safe = "Fucile";

    const std::string oldLine = "local ITEM_NAME = \"Fucile\"";
    const size_t at = src.find(oldLine);
    if (at != std::string::npos)
        src.replace(at, oldLine.size(), "local ITEM_NAME = \"" + safe + "\"");
    return src;
}

/**
 * Script vuoto, con dentro solo l'indispensabile per non partire dal nulla.
 *
 * Serviva un modo di aggiungere uno script QUALSIASI: finora il pannello
 * offriva solo i preset (salto, esplosione, neon, stamina), quindi chi voleva
 * scrivere il proprio codice semplicemente non aveva un bottone — e senza
 * script sulla Part l'Explorer non mostrava nemmeno il nodo da cliccare.
 */
static std::string makeEmptyScript(const WorkspacePart& p) {
    return
        "-- Script su \"" + p.name + "\"\n"
        "--\n"
        "-- script.Parent e' questa Part. Premi Test (F5) per eseguirlo.\n"
        "\n"
        "local parte = script.Parent\n"
        "\n"
        "print(\"Ciao da \" .. parte.Name)\n";
}

static std::string makeJumpBlockScript(const WorkspacePart& p) {
    (void)p;
    return
        "-- Blocco che salta (solo questa Part) · Lua 5.4\n"
        "local part = script.Parent\n"
        "local base = part.Position\n"
        "local altezza = 5\n"
        "local velocita = 2.5\n"
        "local t = 0\n"
        "\n"
        "print(\"Salto: \" .. part.Name)\n"
        "\n"
        "game.RunService.Heartbeat:Connect(function(dt)\n"
        "    t = t + dt\n"
        "    local y = base.y + math.abs(math.sin(t * velocita)) * altezza\n"
        "    part.Position = Vector3.new(base.x, y, base.z)\n"
        "end)\n";
}

static std::string makeExplodeBlockScript(const WorkspacePart& p) {
    (void)p;
    return
        "-- Esplosione leggera (anti-freeze)\n"
        "local part = script.Parent\n"
        "part.CanDestroy = true\n"
        "part.CanCollide = false\n"
        "local esploso = false\n"
        "part.Touched:Connect(function()\n"
        "    if esploso then return end\n"
        "    esploso = true\n"
        "    print(\"BOOM!\")\n"
        "    part:Explode(16)\n"
        "end)\n";
}

static std::string makeNeonBlockScript(const WorkspacePart& p) {
    (void)p;
    return
        "-- Parte Neon (Material + luce dinamica)\n"
        "local part = script.Parent\n"
        "\n"
        "part.Material = \"Neon\"           -- oppure Enum.Material.Neon\n"
        "part.Emissive = 1\n"
        "part.PointLight = true\n"
        "part.LightRange = 22\n"
        "part.LightBrightness = 1.4\n"
        "\n"
        "print(\"Neon attivo:\", part.Name, part.Material)\n"
        "\n"
        "-- Pulse leggero (opzionale)\n"
        "local t = 0\n"
        "game.RunService.Heartbeat:Connect(function(dt)\n"
        "    t = t + dt\n"
        "    part.Emissive = 0.85 + math.sin(t * 3) * 0.35\n"
        "    part.LightBrightness = 1.1 + math.sin(t * 3) * 0.4\n"
        "end)\n"
        "\n"
        "-- Tocca → flash piu' forte\n"
        "part.Touched:Connect(function()\n"
        "    part.Emissive = 2\n"
        "    part.LightBrightness = 2.5\n"
        "    print(\"Neon flash!\")\n"
        "end)\n";
}

/**
 * "Aggiungi Script" dava in realta' lo script del BLOCCO CHE SALTA.
 *
 * Il bottone prometteva uno script e ne consegnava un altro: chi voleva
 * scrivere il proprio codice — per esempio far sparare un'arma — si trovava
 * in mano un blocco che rimbalza e nessun modo di partire da zero. Ora fa
 * quello che dice: crea uno script vuoto. Il salto resta, ma nel suo bottone.
 */
static void addScriptToSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& p = g_world.parts[g_selectedPart];
    bool wasEmpty = p.script.empty();
    if (p.script.empty())
        p.script = makeEmptyScript(p);
    syncPartEditors();
    g_editingPartScript = true;
    g_showProps = true;
    g_luaRunMsg = "Script vuoto su " + p.name + " — scrivilo, poi Esegui / Test";
    if (wasEmpty) g_showScriptCreatedGuide = true;
}

/**
 * A quale oggetto del Backpack si riferisce lo script "arma"?
 *
 * Quello equipaggiato se c'e' (e' quello che hai in mano mentre provi),
 * altrimenti quello selezionato, altrimenti l'ultimo aggiunto: chi ha creato
 * un'arma un minuto fa quasi sempre intende quella.
 */
static std::string weaponScriptTargetName() {
    const auto& bp = g_world.backpack;
    auto at = [&](int i) { return i >= 0 && i < (int)bp.size(); };
    if (at(g_world.equippedBackpack)) return bp[g_world.equippedBackpack].name;
    if (at(g_selectedBackpack))       return bp[g_selectedBackpack].name;
    if (!bp.empty())                  return bp.back().name;
    return "Fucile";
}

static bool addWeaponScriptToSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return false;
    auto& p = g_world.parts[g_selectedPart];
    const std::string item = weaponScriptTargetName();
    std::string src = makeWeaponScript(item);
    if (src.empty()) {
        g_luaRunMsg = "File item_fucile.lua non trovato in assets/scripts";
        return false;
    }
    bool wasEmpty = p.script.empty();
    p.script = std::move(src);
    syncPartEditors();
    g_editingPartScript = true;
    g_showProps = true;
    g_luaRunMsg = "Arma collegata a \"" + item + "\" — Test (F5), equipaggia e clicca";
    if (wasEmpty) g_showScriptCreatedGuide = true;
    return true;
}

static void removeScriptFromSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& p = g_world.parts[g_selectedPart];
    p.script.clear();
    g_partScriptBuf[0] = 0;
    g_editingPartScript = false;
    g_luaRunMsg = "Script rimosso da " + p.name;
}

static bool scriptShouldRunHere(ScriptType t) {
    // ModuleScript: mai auto-run — solo require() da altri script.
    if (t == ScriptType::ModuleScript) return false;
    // Script (Server): solo host / non-client.
    if (t == ScriptType::Server) return g_net.role != NetRole::Client;
    // LocalScript + Legacy: ogni peer (PC Test e Android Hub).
    return true;
}

static void runAllPartScripts(bool clearFirst = true) {
    CrashHandler::note("runAllPartScripts: inizio");
    if (!g_lua.ok) g_lua.init(&g_world);
    bindPhysBridgeToLua();
    bindLuaServices();
    if (clearFirst)
        g_lua.clearHeartbeats();
    int n = 0;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        auto& p = g_world.parts[i];
        if (p.script.empty() || !scriptShouldRunHere(p.scriptType)) continue;
        char bc[192];
        std::snprintf(bc, sizeof(bc), "runAllPartScripts: part[%d] '%s'", i, p.name.c_str());
        CrashHandler::note(bc);
        if (g_lua.runPartScript(p.script, i))
            ++n;
        else
            std::fprintf(stderr, "[Lua] Part '%s': %s\n", p.name.c_str(), g_lua.lastError.c_str());
    }
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        auto& m = g_world.models[i];
        if (m.script.empty() || !scriptShouldRunHere(m.scriptType)) continue;
        char bc[192];
        std::snprintf(bc, sizeof(bc), "runAllPartScripts: model[%d] '%s'", i, m.name.c_str());
        CrashHandler::note(bc);
        if (g_lua.runModelScript(m.script, i))
            ++n;
        else
            std::fprintf(stderr, "[Lua] Model '%s': %s\n", m.name.c_str(), g_lua.lastError.c_str());
    }
    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
        auto& l = g_world.labels[i];
        if (l.script.empty() || !scriptShouldRunHere(l.scriptType)) continue;
        char bc[192];
        std::snprintf(bc, sizeof(bc), "runAllPartScripts: label[%d] '%s'", i, l.name.c_str());
        CrashHandler::note(bc);
        if (g_lua.runLabelScript(l.script, i))
            ++n;
        else
            std::fprintf(stderr, "[Lua] Label '%s': %s\n", l.name.c_str(), g_lua.lastError.c_str());
    }
    // ServerScriptService / shared Script + LocalScript (non ModuleScript).
    for (int i = 0; i < (int)g_world.instances.size(); ++i) {
        auto& s = g_world.instances[i];
        if (s.source.empty()) continue;
        if (s.className != "Script" && s.className != "LocalScript") continue;
        if (!scriptShouldRunHere(s.scriptType)) continue;
        char bc[192];
        std::snprintf(bc, sizeof(bc), "runAllPartScripts: shared[%d] '%s'", i, s.name.c_str());
        CrashHandler::note(bc);
        if (g_lua.runSharedScript(s.source, i))
            ++n;
        else
            std::fprintf(stderr, "[Lua] %s '%s': %s\n",
                         s.className.c_str(), s.name.c_str(), g_lua.lastError.c_str());
    }
    // Registra RemoteEvent/Function presenti nel place.
    for (const auto& s : g_world.instances) {
        if (s.className == "RemoteEvent" || s.className == "RemoteFunction") {
            scriptControls().remoteServer[s.name];
            scriptControls().remoteClient[s.name];
        }
    }
    CrashHandler::note("runAllPartScripts: fine");
    if (n > 0)
        g_luaRunMsg = "Test: script avviati (" + std::to_string(n) + ")";
}

/**
 * UserInputService.InputBegan / InputEnded per gli script: ogni frame, in
 * Play e nel Test, i tasti e i pulsanti del mouse che cambiano stato. Prima
 * gli script si collegavano ma l'evento non arrivava mai.
 */
static void pumpScriptInput(GLFWwindow* win) {
    struct Key { int code; const char* name; };
    // Codici GLFW standard (le lettere sono il loro ASCII): numeri, perche' il GLFW
    // di Android (glfw_shim) non definisce tutte le costanti GLFW_KEY_*.
    static const Key kKeys[] = {
        {65, "A"}, {66, "B"}, {67, "C"}, {68, "D"}, {69, "E"}, {70, "F"},
        {71, "G"}, {72, "H"}, {73, "I"}, {74, "J"}, {75, "K"}, {76, "L"},
        {77, "M"}, {78, "N"}, {79, "O"}, {80, "P"}, {81, "Q"}, {82, "R"},
        {83, "S"}, {84, "T"}, {85, "U"}, {86, "V"}, {87, "W"}, {88, "X"},
        {89, "Y"}, {90, "Z"}, {48, "Zero"}, {49, "One"}, {50, "Two"}, {51, "Three"},
        {52, "Four"}, {53, "Five"}, {54, "Six"}, {55, "Seven"}, {56, "Eight"}, {57, "Nine"},
        {32, "Space"}, {257, "Return"}, {258, "Tab"}, {259, "Backspace"}, {340, "LeftShift"}, {344, "RightShift"},
        {341, "LeftControl"}, {345, "RightControl"}, {342, "LeftAlt"}, {265, "Up"}, {264, "Down"}, {263, "Left"},
        {262, "Right"},
    };
    static bool keyWas[sizeof(kKeys) / sizeof(kKeys[0])] = {};
    static bool mouseWas[3] = {};
    if (!g_lua.ok || !win) return;
    const bool inGame = g_screen == AppScreen::Playing || g_studioTesting;
    double mx = 0, my = 0;
    glfwGetCursorPos(win, &mx, &my);
    const ImGuiIO& io = ImGui::GetIO();
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
        const bool down = inGame && glfwGetKey(win, kKeys[i].code) == GLFW_PRESS;
        if (down != keyWas[i])
            g_lua.fireInput(down, kKeys[i].name, "Keyboard", (float)mx, (float)my, io.WantCaptureKeyboard);
        keyWas[i] = down;
    }
    static const char* kMouse[3] = {"MouseButton1", "MouseButton2", "MouseButton3"};
    for (int b = 0; b < 3; ++b) {
        const bool down = inGame && glfwGetMouseButton(win, b) == GLFW_PRESS;
        if (down != mouseWas[b])
            g_lua.fireInput(down, "Unknown", kMouse[b], (float)mx, (float)my, io.WantCaptureMouse);
        // ClickDetector: clic sinistro su una part (o su una sua figlia),
        // entro 32 stud dal giocatore come MaxActivationDistance di Roblox.
        if (b == 0 && down && !mouseWas[b] && !io.WantCaptureMouse) {
            int idx = pickPartAtMouse((float)mx, (float)my);
            for (int guard = 0; idx >= 0 && idx < (int)g_world.parts.size() && guard < 32; ++guard) {
                if (g_lua.partHasClick(idx)) {
                    const WorkspacePart& p = g_world.parts[(size_t)idx];
                    const float reach = 32.f + 0.5f * glm::length(p.size);
                    if (glm::length(p.position - g_playerPos) <= reach) g_lua.firePartClicked(idx);
                    break;
                }
                idx = g_world.parts[(size_t)idx].parentIndex;
            }
        }
        mouseWas[b] = down;
    }
}

static void startTestScripts() {
    CrashHandler::note("startTestScripts: inizio");
    // Sessione Luau pulita: gli script vedono il workspace C++ così com'è
    // in Studio (Size/Position aggiornati), senza stato vecchio dal test prima.
    g_lua.shutdown();
    if (!g_lua.init(&g_world)) {
        std::fprintf(stderr, "[Test] Lua init fallita: %s\n", g_lua.lastError.c_str());
        LOG_ERR("test", "Lua non parte: %s", g_lua.lastError.c_str());
        return;
    }
    CrashHandler::note("startTestScripts: g_lua.init ok");
    bindPhysBridgeToLua();
    bindLuaServices();
    g_lua.clearHeartbeats();
    scriptControls().jumpEnabled = true;
    scriptControls().walkEnabled = true;
    scriptControls().walkSpeedOverride = -1.f;
    scriptControls().jumpPowerOverride = -1.f;
    if (g_activeGame.script[0]) {
        CrashHandler::note("startTestScripts: bootPlaceScript(g_activeGame.script)");
        bootPlaceScript(g_activeGame.script);
    }
    if (g_loadApiCompleta) {
        CrashHandler::note("startTestScripts: bootPlaceScript(lua_api_completo)");
        bootPlaceScript("scripts/lua_api_completo.lua");
        g_luaRunMsg = "API completa caricata — tocca i pad a z=12";
    }
    if (g_loadStamina) {
        CrashHandler::note("startTestScripts: bootPlaceScript(stamina)");
        bootPlaceScript("scripts/stamina.lua");
        g_luaRunMsg = "Stamina attiva — CTRL+WASD per correre";
    }
    if (g_loadCartLontano) {
        CrashHandler::note("startTestScripts: bootPlaceScript(cart_lontano)");
        bootPlaceScript("scripts/cart_lontano.lua");
        g_luaRunMsg = "Carrello VIA! — Test, poi clicca il bottone";
    }
    if (g_loadFucile) {
        CrashHandler::note("startTestScripts: bootPlaceScript(logica_fucile)");
        bootPlaceScript("scripts/logica_fucile.lua");
        g_luaRunMsg = "Fucile pronto — tasto 1, poi spara sulle Part";
    }
    runAllPartScripts(false);
}

static void scaleSelected(float factor) {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    auto& s = g_world.parts[g_selectedPart].size;
    s *= factor;
    s = glm::max(s, glm::vec3(0.05f));
    s = glm::min(s, glm::vec3(1.0e5f));
}

static void partWorldAabb(const WorkspacePart& p, glm::vec3& outMin, glm::vec3& outMax) {
    glm::vec3 h = partHitboxSize(p) * 0.5f;

    // Una palla e un cilindro NON diventano piu' grandi ruotando: gli otto
    // angoli del cubo che li contiene invece si', e di parecchio quando la
    // fisica li fa rotolare o cadere di taglio — e' esattamente li' che il
    // giocatore sbatte contro un muro che non vede, perche' il cubo sporge
    // ben oltre la forma tonda disegnata. Qui l'ingombro e' quello vero della
    // forma, non del cubo che la contiene.
    if (p.shape == PartShape::Ball) {
        // Stesso raggio della fisica (BlockyPhysics.h usa il semiasse piu'
        // corto): una sfera vera ha lo stesso ingombro a ogni rotazione.
        float r = std::min(h.x, std::min(h.y, h.z));
        outMin = p.position - glm::vec3(r);
        outMax = p.position + glm::vec3(r);
        return;
    }
    if (p.shape == PartShape::Cylinder) {
        // Ingombro esatto = componente lungo l'asse + raggio proiettato sul
        // piano perpendicolare, per ognuno dei tre assi del mondo — non gli
        // angoli di un cubo che lo racchiude.
        float r = std::min(h.x, h.z);
        glm::mat3 rot(partRotationMatrix(p.rotation));
        glm::vec3 ext;
        for (int i = 0; i < 3; ++i)
            ext[i] = h.y * std::fabs(rot[1][i])
                   + r * std::sqrt(rot[0][i] * rot[0][i] + rot[2][i] * rot[2][i]);
        outMin = p.position - ext;
        outMax = p.position + ext;
        return;
    }

    glm::mat4 m = partHitboxMatrix(p);
    outMin = glm::vec3(1e9f);
    outMax = glm::vec3(-1e9f);
    for (int ix = 0; ix < 2; ++ix)
        for (int iy = 0; iy < 2; ++iy)
            for (int iz = 0; iz < 2; ++iz) {
                glm::vec4 c = m * glm::vec4(ix ? 0.5f : -0.5f, iy ? 0.5f : -0.5f, iz ? 0.5f : -0.5f, 1.f);
                outMin = glm::min(outMin, glm::vec3(c));
                outMax = glm::max(outMax, glm::vec3(c));
            }
}

static bool aabbOverlap(const glm::vec3& amin, const glm::vec3& amax,
                        const glm::vec3& bmin, const glm::vec3& bmax) {
    return amin.x < bmax.x && amax.x > bmin.x
        && amin.y < bmax.y && amax.y > bmin.y
        && amin.z < bmax.z && amax.z > bmin.z;
}

static void playerAabb(glm::vec3& outMin, glm::vec3& outMax) {
    const float hx = 1.05f, hy = 2.55f, hz = 1.05f;
    glm::vec3 c = g_playerPos + glm::vec3(0.f, hy, 0.f);
    outMin = c - glm::vec3(hx, hy, hz);
    outMax = c + glm::vec3(hx, hy, hz);
}

static void doBoomAt(glm::vec3 boomPos, float radius = 14.f, float maxDmg = 55.f, int fx = 24) {
    g_particles.spawnExplosion(boomPos, radius, fx);
    // Casse / debris volano via (stile Explosion Roblox).
    if (g_bphys.ready())
        g_bphys.applyRadialImpulse(boomPos, radius, maxDmg * 18.f);
    if (g_lua.playerHumanoid && g_lua.playerPos)
        g_lua.applyExplosionPlayerDamage(boomPos, radius, maxDmg);
    else if (!g_player.humanoid.dead) {
        glm::vec3 center = g_playerPos + glm::vec3(0.f, 2.5f, 0.f);
        float d = glm::length(center - boomPos);
        if (d < radius) {
            float t = 1.f - d / radius;
            g_player.humanoid.takeDamage(std::max(1.f, maxDmg * t * t));
            glm::vec3 push = center - boomPos;
            if (glm::length(push) > 1e-3f) {
                g_playerPos += glm::normalize(push) * (2.5f * t);
                g_player.humanoid.velocityY += 6.f * t;
                g_player.humanoid.grounded = false;
            }
        }
    }
}

// Rileva quando cambia l'oggetto equipaggiato nello zaino (tasti 1-9,
// rotellina, o qualunque altro punto del codice che tocchi
// g_world.equippedBackpack) e spara Equipped/Unequipped agli script — stile
// Tool.Equipped/Tool.Unequipped di Roblox. Un solo punto di rilevamento per
// frame invece di dover intercettare ogni singolo posto che cambia l'indice.
static void pollBackpackEquipChanged() {
    static int lastEquipped = -1;
    const int cur = g_world.equippedBackpack;
    if (cur == lastEquipped) return;
    if (lastEquipped >= 0 && lastEquipped < (int)g_world.backpack.size()) {
        const auto& prev = g_world.backpack[lastEquipped];
        g_lua.fireItemUnequipped(prev.id, prev.name);
    }
    if (cur >= 0 && cur < (int)g_world.backpack.size()) {
        const auto& now_ = g_world.backpack[cur];
        // Un item senza mesh e senza tool incorporato non fa comparire NIENTE in
        // mano, e finora non lo diceva nessuno: si equipaggiava e non succedeva
        // nulla, senza sapere se fosse rotto il modello, l equip o l animazione.
        if (now_.meshPath.empty() && now_.tool == BackpackTool::None) {
            g_exportMsg = "\"" + now_.name + "\" non ha un modello: Explorer > Backpack > "
                          + now_.name + " > \"Scegli modello (mesh in mano)...\"";
        } else if (!now_.meshPath.empty()) {
            const std::string resolved =
                resolvePackageAssetPath(g_activeGame.placePath, now_.meshPath);
            if (!fileExistsPath(resolved)) {
                g_exportMsg = "Modello di \"" + now_.name + "\" non trovato: " + now_.meshPath
                    + " (cerca in " + resolved + ")";
            } else if (resolved != now_.meshPath) {
                // Aggiorna subito: drawEquippedItemMesh e Assimp usano il path vero.
                g_world.backpack[cur].meshPath = resolved;
            }
        }
        g_lua.fireItemEquipped(now_.id, now_.name);
    }
    lastEquipped = cur;
}

static void pollPartTouched() {
    if (g_screen != AppScreen::Playing
        && !(g_screen == AppScreen::Studio && g_studioTesting))
        return;
    g_lua.clearPartPartTouchDedup();
    glm::vec3 pmin, pmax;
    playerAabb(pmin, pmax);
    glm::vec3 playerCenter = (pmin + pmax) * 0.5f;
    if ((int)g_touching.size() != (int)g_world.parts.size())
        g_touching.assign(g_world.parts.size(), 0);

    // Tocco con la forma vera della part (Jolt); le part appena create, che
    // la fisica non ha ancora, usano la scatola finche' non entrano.
    static std::vector<uint8_t> exactTouch;
    // Se uno script ha tolto part dopo l'allineamento della fisica, gli indici
    // non corrispondono piu': meglio la scatola per un frame che la part sbagliata.
    const bool exact = g_bphys.touching(g_playerPos, g_world.parts, exactTouch)
                    && exactTouch.size() <= g_world.parts.size();

    std::vector<int> boomIdx;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        auto& part = g_world.parts[i];
        if (!part.canTouch && !part.boomOnTouch) {
            if (i < (int)g_touching.size()) g_touching[i] = 0;
            continue;
        }
        bool aabbHit;
        if (exact && i < (int)exactTouch.size()) {
            aabbHit = exactTouch[i] != 0;
        } else {
            glm::vec3 bmin, bmax;
            partWorldAabb(part, bmin, bmax);
            aabbHit = aabbOverlap(pmin, pmax, bmin, bmax);
        }

        glm::vec3 d = playerCenter - part.position;
        d.y = 0.f;
        float xzDist = glm::length(d);
        float reach = 0.5f * std::max(part.size.x, part.size.z) + 4.5f;
        bool nearHit = xzDist <= reach
            && std::abs(playerCenter.y - part.position.y) < (part.size.y * 0.5f + 4.f);

        if (part.boomOnTouch) {
            if (aabbHit || nearHit)
                boomIdx.push_back(i);
        } else {
            bool now = aabbHit;
            if (now && i < (int)g_touching.size() && !g_touching[i] && g_lua.ok)
                g_lua.firePartTouched(i);
            if (i < (int)g_touching.size())
                g_touching[i] = now ? 1 : 0;
        }
    }

    // Part↔Part Touched (casse che si urtano, pad che toccano altre part).
    if (g_lua.ok && g_bphys.ready()) {
        for (const bphys::TouchPair& tp : g_bphys.lastContactPairs()) {
            if (tp.a < 0 || tp.b < 0) continue;
            if (tp.a >= (int)g_world.parts.size() || tp.b >= (int)g_world.parts.size()) continue;
            if (!g_world.parts[tp.a].canTouch || !g_world.parts[tp.b].canTouch) continue;
            g_lua.firePartTouchedOther(tp.a, tp.b);
            g_lua.firePartTouchedOther(tp.b, tp.a);
        }
    }

    std::sort(boomIdx.begin(), boomIdx.end(), std::greater<int>());
    boomIdx.erase(std::unique(boomIdx.begin(), boomIdx.end()), boomIdx.end());
    for (int idx : boomIdx) {
        if (idx < 0 || idx >= (int)g_world.parts.size()) continue;
        if (!g_world.parts[idx].boomOnTouch) continue;
        glm::vec3 boomPos = g_world.parts[idx].position;
        doBoomAt(boomPos, 14.f, 55.f, 24);
        g_luaRunMsg = "BOOM! " + g_world.parts[idx].name;
        g_exportMsg = g_luaRunMsg;
        std::fprintf(stdout, "[Boom] touch/near '%s'\n", g_world.parts[idx].name.c_str());
        if (g_selectedPart == idx) selectPart(-1);
        else if (g_selectedPart > idx) --g_selectedPart;
        if (g_lua.ok && g_lua.workspace == &g_world) g_lua.destroyPartNow(idx);
        else g_world.parts.erase(g_world.parts.begin() + idx);
        if (idx < (int)g_touching.size())
            g_touching.erase(g_touching.begin() + idx);
    }

    if (g_lua.ok)
        g_lua.flushPendingDestroy();
    if ((int)g_touching.size() != (int)g_world.parts.size())
        g_touching.assign(g_world.parts.size(), 0);
    if (g_selectedPart >= (int)g_world.parts.size())
        selectPart(-1);
}

static void tickBoomAutoFuse(float dt) {
    if (dt <= 0.f) return;
    if (g_screen != AppScreen::Playing
        && !(g_screen == AppScreen::Studio && g_studioTesting))
        return;
    for (int i = (int)g_world.parts.size() - 1; i >= 0; --i) {
        auto& p = g_world.parts[i];
        if (!p.boomOnTouch || p.boomAutoSecs <= 0.f) continue;
        p.boomAutoSecs -= dt;
        if (p.boomAutoSecs > 0.f) continue;
        p.boomAutoSecs = 0.f;
        glm::vec3 boomPos = p.position;
        doBoomAt(boomPos, 14.f, 55.f, 24);
        g_luaRunMsg = "BOOM! (auto) " + p.name;
        g_exportMsg = g_luaRunMsg;
        std::fprintf(stdout, "[Boom] auto fuse '%s'\n", p.name.c_str());
        if (g_selectedPart == i) selectPart(-1);
        else if (g_selectedPart > i) --g_selectedPart;
        if (g_lua.ok && g_lua.workspace == &g_world) g_lua.destroyPartNow(i);
        else g_world.parts.erase(g_world.parts.begin() + i);
        if (i < (int)g_touching.size())
            g_touching.erase(g_touching.begin() + i);
    }
}

static void resolvePlayerAxis(int axis) {
    glm::vec3 pmin, pmax;
    playerAabb(pmin, pmax);

    auto pushOut = [&](const glm::vec3& bmin, const glm::vec3& bmax) {
        if (!aabbOverlap(pmin, pmax, bmin, bmax)) return;

        float penX = std::min(bmax.x - pmin.x, pmax.x - bmin.x);
        float penY = std::min(bmax.y - pmin.y, pmax.y - bmin.y);
        float penZ = std::min(bmax.z - pmin.z, pmax.z - bmin.z);
        if (penX <= 0.f || penY <= 0.f || penZ <= 0.f) return;

        float pushPos = bmax[axis] - pmin[axis];
        float pushNeg = pmax[axis] - bmin[axis];
        if (pushPos <= 0.f && pushNeg <= 0.f) return;

        const float skin = 0.002f;

        if (axis == 1) {
            const float maxStep = 3.2f;
            float up = pushPos;
            float down = pushNeg;
            bool landing = g_player.humanoid.velocityY <= 0.5f && up <= maxStep;
            if (landing || up <= down) {
                if (up > maxStep) return;
                g_playerPos.y += up + skin;
                if (g_player.humanoid.velocityY <= 0.f) {
                    g_player.humanoid.velocityY = 0.f;
                    g_player.humanoid.grounded = true;
                }
            } else {
                g_playerPos.y -= down + skin;
                if (g_player.humanoid.velocityY > 0.f)
                    g_player.humanoid.velocityY = 0.f;
            }
            playerAabb(pmin, pmax);
            return;
        }

        if (penY <= penX + 0.02f && penY <= penZ + 0.02f)
            return;
        if (pmin.y >= bmax.y - 0.65f && g_player.humanoid.velocityY <= 4.f)
            return;

        float amount = (pushPos < pushNeg) ? pushPos : -pushNeg;
        if (std::fabs(amount) > 1.5f) {
            if (penY < 0.9f) return;
            amount = (amount > 0.f) ? 0.75f : -0.75f;
        }

        if (axis == 0) {
            g_playerPos.x += amount + (amount > 0.f ? skin : -skin);
            if (amount > 0.f && g_moveVel.x < 0.f) g_moveVel.x = 0.f;
            if (amount < 0.f && g_moveVel.x > 0.f) g_moveVel.x = 0.f;
        } else {
            g_playerPos.z += amount + (amount > 0.f ? skin : -skin);
            if (amount > 0.f && g_moveVel.z < 0.f) g_moveVel.z = 0.f;
            if (amount < 0.f && g_moveVel.z > 0.f) g_moveVel.z = 0.f;
        }
        playerAabb(pmin, pmax);
    };

    // Con la fisica attiva le part le gestisce il personaggio di Jolt, con la
    // loro forma vera; qui restano solo come ripiego se la fisica non c'e'.
    if (!g_bphys.ready()) {
        for (const WorkspacePart& part : g_world.parts) {
            if (!part.canCollide) continue;
            glm::vec3 bmin, bmax;
            partWorldAabb(part, bmin, bmax);
            pushOut(bmin, bmax);
        }
    }

    if (!g_terrain.empty()) {
        std::vector<glm::vec3> tMins, tMaxs;
        g_terrain.collideAabbs(pmin, pmax, tMins, tMaxs);
        for (size_t ti = 0; ti < tMins.size(); ++ti)
            pushOut(tMins[ti], tMaxs[ti]);
    }

    // I modelli importati non hanno ancora una forma fisica: scatola.
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (!g_world.models[i].canCollide) continue;
        glm::vec3 bmin, bmax;
        if (!modelWorldAabb(i, bmin, bmax)) continue;
        pushOut(bmin, bmax);
    }
}

static void respawnPlayerAtSpawn() {
    g_playerPos = g_world.spawn.position;
    if (!std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y) || !std::isfinite(g_playerPos.z)
        || g_playerPos.y < 0.01f) {
        g_playerPos = {0.f, 0.f, 0.f};
    } else {
        g_playerPos.y = std::max(0.f, g_playerPos.y);
    }
    if (g_playerPos.y < 0.05f)
        g_playerPos.y = 0.05f;
    g_moveVel = {0, 0, 0};
    g_ragdoll.clear();
    g_player.humanoid.respawnFull();
    g_player.humanoid.velocityY = 0.f;
    g_player.humanoid.grounded = true;
    if (g_player.humanoid.walkSpeed < 4.f)
        g_player.humanoid.walkSpeed = 16.f;
    g_shiftLock = false;
    g_playMenuOpen = false;
    acOnLegitimateRespawn();
    g_lua.fireCharacterAdded();
}

static void stepPlayerPhysicsWithCollision(float dt) {
    auto& h = g_player.humanoid;
    if (h.dead) {
        if (!g_ragdoll.active) {
            g_ragdoll.start(h, g_playerPos, g_playerYaw, g_moveVel, 0.f);
        }
        g_ragdoll.update(dt);
        if (g_ragdoll.torsoIdx >= 0)
            g_playerPos = g_ragdoll.focusPos() - glm::vec3(0.f, 3.f, 0.f);
        h.tickDeath(dt);
        g_moveVel = {0, 0, 0};
        h.velocityY = 0.f;
        if (h.shouldRespawn()) {
            g_ragdoll.clear();
            h.respawnFull();
            respawnPlayerAtSpawn();
        }
        return;
    }
    if (g_ragdoll.active)
        g_ragdoll.clear();

    // Alla guida la posizione la decide l'auto (syncVeicoli in main.cpp).
    if (g_veicoli.seated()) {
        h.velocityY = 0.f;
        h.grounded = true;
        return;
    }

    const float gravity = bphys::kGravity;
    float prevVy = h.velocityY;
    h.velocityY -= gravity * dt;
    if (g_bphys.ready()) {
        const bphys::CharacterStep r = g_bphys.moveCharacter(g_playerPos, h.velocityY, dt);
        g_playerPos = r.feet;
        h.velocityY = r.velY;
        h.grounded = r.grounded;
        resolvePlayerAxis(1);
        resolvePlayerAxis(0);
        resolvePlayerAxis(2);
    } else {
        resolvePlayerAxis(0);
        resolvePlayerAxis(2);
        h.grounded = false;
        g_playerPos.y += h.velocityY * dt;
        resolvePlayerAxis(1);
        resolvePlayerAxis(0);
        resolvePlayerAxis(2);
    }

    if (h.grounded && prevVy < -45.f) {
        float dmg = (-prevVy - 45.f) * 1.8f;
        h.takeDamage(dmg);
    }

    const float killY = -12.f;
    const float mapHalfExtent = 144.f;
    bool fellOffMap = g_playerPos.y < killY
        || std::fabs(g_playerPos.x) > mapHalfExtent
        || std::fabs(g_playerPos.z) > mapHalfExtent
        || !std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y)
        || !std::isfinite(g_playerPos.z);
    if (fellOffMap)
        h.takeDamage(h.maxHealth + 1.f);
}

