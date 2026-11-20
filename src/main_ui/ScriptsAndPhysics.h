// Generazione script per le part (stamina, salto, esplosione, neon),
// attach/detach script sulla selezione, esecuzione script all'avvio del
// test, e fisica/collisioni del player (assi, respawn, esplosioni,
// touch-detection). Non e' un header riusabile: usa direttamente le
// globali di main.cpp (g_world, g_player, g_playerPos, g_lua, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

#include "engine/PartGroups.h"

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
    g_showPartScriptWin = true;   // come Roblox: lo script nuovo si apre nell editor
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
    if (g_lua.proximity) {
        g_lua.proximity->clickHint().clear();
        if (inGame && !io.WantCaptureMouse) {
            int idx = pickPartAtMouse((float)mx, (float)my);
            for (int guard = 0; idx >= 0 && idx < (int)g_world.parts.size() && guard < 32; ++guard) {
                if (g_lua.partHasClick(idx)) {
                    const WorkspacePart& p = g_world.parts[(size_t)idx];
                    float reach = 32.f;
                    auto it = scriptControls().clickReach.find(idx);
                    if (it != scriptControls().clickReach.end()) reach = it->second;
                    if (glm::length(p.position - g_playerPos) <= reach + 0.5f * glm::length(p.size))
                        g_lua.proximity->clickHint() = "Clic";
                    break;
                }
                idx = g_world.parts[(size_t)idx].parentIndex;
            }
        }
    }
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
                    float reach = 32.f;
                    auto it = scriptControls().clickReach.find(idx);
                    if (it != scriptControls().clickReach.end()) reach = it->second;
                    reach += 0.5f * glm::length(p.size);
                    if (glm::length(p.position - g_playerPos) <= reach) g_lua.firePartClicked(idx);
                    break;
                }
                idx = g_world.parts[(size_t)idx].parentIndex;
            }
        }
        if (b == 0) {
            static int dragPart = -1;
            static int dragGroup = 0;
            static glm::vec3 dragGrab{0.f};
            static glm::vec3 dragN{0.f, 1.f, 0.f};
            if (down && !mouseWas[b] && !io.WantCaptureMouse) {
                const int hit = pickPartAtMouse((float)mx, (float)my);
                dragPart = -1;
                dragGroup = 0;
                if (hit >= 0) dragGroup = partgroups::dragGroupOf(g_world, hit);
                if (dragGroup <= 0) {
                    for (int idx = hit, guard = 0; idx >= 0 && idx < (int)g_world.parts.size() && guard < 32; ++guard) {
                        if (g_world.parts[(size_t)idx].dragDetector) { dragPart = idx; break; }
                        idx = g_world.parts[(size_t)idx].parentIndex;
                    }
                }
                const int ref = dragGroup > 0 ? hit : dragPart;
                if ((dragPart >= 0 || dragGroup > 0) && ref >= 0 && ref < (int)g_world.parts.size()) {
                    dragPart = ref;
                    glm::vec3 o, d;
                    screenToRay((float)mx, (float)my, g_lastView, g_lastProj, o, d);
                    dragN = glm::length(g_cam.front()) > 0.1f ? glm::normalize(g_cam.front()) : glm::vec3(0.f, 0.f, -1.f);
                    float t = 0.f;
                    const glm::vec3 origin = g_world.parts[(size_t)ref].position;
                    dragGrab = rayPlane(o, d, origin, dragN, t) ? origin - (o + d * t) : glm::vec3(0.f);
                }
            }
            if (down && dragPart >= 0 && dragPart < (int)g_world.parts.size()) {
                glm::vec3 o, d;
                screenToRay((float)mx, (float)my, g_lastView, g_lastProj, o, d);
                float t = 0.f;
                auto& pref = g_world.parts[(size_t)dragPart];
                if (rayPlane(o, d, pref.position, dragN, t)) {
                    const glm::vec3 target = o + d * t + dragGrab;
                    if (dragGroup > 0) {
                        const glm::vec3 delta = target - pref.position;
                        for (int i : partgroups::memberParts(g_world, dragGroup)) {
                            if (i < 0 || i >= (int)g_world.parts.size()) continue;
                            auto& mp = g_world.parts[(size_t)i];
                            if (mp.toolSlot >= 0) continue;
                            mp.position += delta;
                        }
                    } else {
                        pref.position = target;
                    }
                }
            }
            if (!down) { dragPart = -1; dragGroup = 0; }
        }
        mouseWas[b] = down;
    }
}

static void startTestScripts() {
    CrashHandler::note("startTestScripts: inizio");
    // Il fuoco, gli attachment e i ClickDetector del posto restano. Quello
    // che gli script creano durante il Test si butta allo Stop.
    rememberStudioFx(g_lua.scriptEmitters_);
    // Sessione Luau pulita: gli script vedono il workspace C++ così com'è
    // in Studio (Size/Position aggiornati), senza stato vecchio dal test prima.
    g_lua.shutdown();
    if (!g_lua.init(&g_world)) {
        std::fprintf(stderr, "[Test] Lua init fallita: %s\n", g_lua.lastError.c_str());
        LOG_ERR("test", "Lua non parte: %s", g_lua.lastError.c_str());
        return;
    }
    CrashHandler::note("startTestScripts: g_lua.init ok");
    recallStudioFx(g_lua.scriptEmitters_);
    bindPhysBridgeToLua();
    bindLuaServices();
    g_lua.clearHeartbeats();
    scriptControls().jumpEnabled = true;
    scriptControls().walkEnabled = true;
    scriptControls().scriptMoveAge = 10.f;
    scriptControls().moveToActive = false;
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
    auto& part = g_world.parts[g_selectedPart];
    const glm::vec3 prev = part.size;
    part.size *= factor;
    part.size = glm::max(part.size, glm::vec3(kMinPartSize));
    part.size = glm::min(part.size, glm::vec3(1.0e5f));
    if (studioCollisionBlocked(part)) part.size = prev;
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
        // Come Roblox: il cilindro e' lungo su Size.X. L'asse locale X e' la
        // prima colonna della rotazione; il raggio sta su Y e Z.
        float r = std::min(h.y, h.z);
        glm::mat3 rot(partRotationMatrix(p.rotation));
        glm::vec3 ext;
        for (int i = 0; i < 3; ++i)
            ext[i] = h.x * std::fabs(rot[0][i])
                   + r * std::sqrt(rot[1][i] * rot[1][i] + rot[2][i] * rot[2][i]);
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
        if (now_.meshPath.empty() && now_.tool == BackpackTool::None
            && !g_world.backpackHasParts(cur)) {
            g_exportMsg = "\"" + now_.name + "\" non ha un Handle: metti una Part di nome "
                          "Handle dentro il Tool, oppure scegli un modello.";
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

static bool pointInPart(const WorkspacePart& p, const glm::vec3& world) {
    const glm::vec3 local = glm::vec3(glm::inverse(partRotationMatrix(p.rotation))
                                      * glm::vec4(world - p.position, 1.f));
    const glm::vec3 h = p.size * 0.5f;
    return std::abs(local.x) <= h.x + 0.05f && std::abs(local.y) <= h.y + 0.05f
        && std::abs(local.z) <= h.z + 0.05f;
}

static bool playerInWater() {
    const glm::vec3 chest = g_playerPos + glm::vec3(0.f, 2.2f, 0.f);
    for (const auto& p : g_world.parts) {
        if (p.toolSlot >= 0 || p.material != PartMaterial::Water) continue;
        if (pointInPart(p, chest)) return true;
    }
    return false;
}

static const WorkspacePart* playerOnLadder() {
    const glm::vec3 chest = g_playerPos + glm::vec3(0.f, 2.0f, 0.f);
    const WorkspacePart* best = nullptr;
    float bestD = 1e9f;
    for (const auto& p : g_world.parts) {
        if (p.toolSlot >= 0 || !(p.ladder || p.name == "Ladder")) continue;
        const glm::vec3 grown = p.size * 0.5f + glm::vec3(1.15f, 0.4f, 1.15f);
        const glm::vec3 local = glm::vec3(glm::inverse(partRotationMatrix(p.rotation))
                                          * glm::vec4(chest - p.position, 1.f));
        if (std::abs(local.x) > grown.x || std::abs(local.y) > grown.y || std::abs(local.z) > grown.z)
            continue;
        const float d = glm::length(glm::vec2(chest.x - p.position.x, chest.z - p.position.z));
        if (d < bestD) { bestD = d; best = &p; }
    }
    return best;
}

static void respawnPlayerAtSpawn() {
    struct Spot { glm::vec3 pos; float yaw; std::string team; };
    std::vector<Spot> mine, any;
    const std::string team = g_teams.mine();
    for (const auto& p : g_world.parts) {
        if (p.toolSlot >= 0) continue;
        if (!p.spawnPoint && p.name != "SpawnLocation") continue;
        Spot s;
        s.pos = p.position + glm::vec3(0.f, p.size.y * 0.5f + 0.15f, 0.f);
        s.yaw = p.rotation.y;
        s.team = p.spawnTeam;
        if (!team.empty() && s.team == team) mine.push_back(s);
        if (s.team.empty()) any.push_back(s);
    }
    const std::vector<Spot>& pool = !mine.empty() ? mine : any;
    if (!pool.empty()) {
        static int spin = 0;
        const Spot& s = pool[(size_t)(spin++ % (int)pool.size())];
        g_playerPos = s.pos;
        g_playerYaw = s.yaw;
    } else {
        g_playerPos = g_world.spawn.position;
        if (!std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y) || !std::isfinite(g_playerPos.z)
            || g_playerPos.y < 0.01f) {
            g_playerPos = {0.f, 0.f, 0.f};
        } else {
            g_playerPos.y = std::max(0.f, g_playerPos.y);
        }
        if (g_playerPos.y < 0.05f)
            g_playerPos.y = 0.05f;
    }
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

static void tickSeatAndTrails(float dt);

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
    if (h.scriptPose == 3 || h.ragdoll) {
        if (!g_ragdoll.active)
            g_ragdoll.start(h, g_playerPos, g_playerYaw, g_moveVel, 0.f);
        g_ragdoll.update(dt);
        if (g_ragdoll.torsoIdx >= 0)
            g_playerPos = g_ragdoll.focusPos() - glm::vec3(0.f, 3.f, 0.f);
        g_moveVel = {0, 0, 0};
        h.velocityY = 0.f;
        h.grounded = false;
        return;
    }
    if (g_ragdoll.active)
        g_ragdoll.clear();

    // Alla guida la posizione la decide l'auto (syncVeicoli in main.cpp).
    if (g_veicoli.seated()) {
        h.velocityY = 0.f;
        h.grounded = true;
        h.climbing = false;
        h.swimming = false;
        return;
    }

    static float ladderRelease = 0.f;
    if (ladderRelease > 0.f) ladderRelease = std::max(0.f, ladderRelease - dt);
    if (h.velocityY > 8.f) ladderRelease = 0.35f;
    if (const WorkspacePart* ladder = playerOnLadder(); ladder && ladderRelease <= 0.f && h.velocityY < 8.f) {
        h.climbing = true;
        h.swimming = false;
        h.grounded = true;
        h.velocityY = 0.f;
        const float dx = g_playerPos.x - ladder->position.x;
        const float dz = g_playerPos.z - ladder->position.z;
        if (dx * dx + dz * dz < 8.f) {
            g_playerPos.x = ladder->position.x;
            g_playerPos.z = ladder->position.z;
        }
        const float top = ladder->position.y + ladder->size.y * 0.5f;
        const float bot = ladder->position.y - ladder->size.y * 0.5f;
        g_playerPos.y += std::clamp(g_climbAxis, -1.f, 1.f) * h.walkSpeed * 0.55f * dt;
        if (g_playerPos.y > top - 0.05f && g_climbAxis > 0.2f) {
            g_playerPos.y = top + 0.2f;
            h.climbing = false;
        }
        if (g_playerPos.y < bot) g_playerPos.y = bot;
        g_moveVel = {0, 0, 0};
        tickSeatAndTrails(dt);
        return;
    }
    h.climbing = false;

    const bool flying = h.scriptPose == 1 && h.flyEnabled;
    const bool scriptSwim = h.scriptPose == 2 && h.swimEnabled;
    const bool inWater = playerInWater() && h.swimEnabled && !flying;
    h.flying = flying;
    h.swimming = scriptSwim || inWater;
    const float gravity = bphys::kGravity;
    float prevVy = h.velocityY;
    if (flying) {
        h.velocityY = std::clamp(g_climbAxis, -1.f, 1.f) * h.walkSpeed;
        if (g_bphys.ready()) h.velocityY += gravity * dt;
        h.grounded = false;
    } else if (h.swimming) {
        h.velocityY += (-1.6f - h.velocityY) * std::min(1.f, 3.f * dt);
        h.velocityY += g_swimUp * 22.f * dt;
        h.velocityY += std::clamp(g_climbAxis, -1.f, 1.f) * 8.f * dt;
        h.velocityY = std::clamp(h.velocityY, -5.f, 12.f);
    } else {
        h.velocityY -= gravity * dt;
    }
    if (g_bphys.ready()) {
        const bphys::CharacterStep r = g_bphys.moveCharacter(g_playerPos, h.velocityY, dt);
        g_playerPos = r.feet;
        h.velocityY = r.velY;
        h.grounded = flying ? false : r.grounded;
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

    // Come su Roblox: si muore solo cadendo sotto workspace.FallenPartsDestroyHeight.
    // (Prima c'era anche un limite di 144 stud in X e Z: mappe piu' grandi di
    // 288x288 uccidevano il giocatore ai bordi anche col pavimento sotto i piedi.)
    const float killY = g_lua.ok ? g_lua.readFallenPartsDestroyHeight() : -12.f;
    bool fellOffMap = g_playerPos.y < killY
        || !std::isfinite(g_playerPos.x) || !std::isfinite(g_playerPos.y)
        || !std::isfinite(g_playerPos.z);
    if (fellOffMap)
        h.takeDamage(h.maxHealth + 1.f);
    tickSeatAndTrails(dt);
}

static void tickSeatAndTrails(float dt) {
    auto& h = g_player.humanoid;
    if (!h.climbing) {
    if (h.velocityY > 1.5f) { h.seated = false; h.seatPart = -1; }
    if (!h.seated && h.velocityY <= 0.5f) {
        for (int si = 0; si < (int)g_world.parts.size(); ++si) {
            const auto& p = g_world.parts[(size_t)si];
            if (!p.seat || p.toolSlot >= 0) continue;
            const glm::vec3 d = g_playerPos - p.position;
            if (std::abs(d.x) < p.size.x * 0.5f + 0.8f && std::abs(d.z) < p.size.z * 0.5f + 0.8f
                && d.y > -1.2f && d.y < p.size.y * 0.5f + 1.6f) {
                g_playerPos = p.position + glm::vec3(0.f, p.size.y * 0.5f + 0.05f, 0.f);
                h.velocityY = 0.f;
                h.grounded = true;
                h.seated = true;
                h.seatPart = si;
                g_moveVel = {0, 0, 0};
                break;
            }
        }
    } else if (h.seated) {
        h.grounded = true;
        h.velocityY = 0.f;
        g_moveVel = {0, 0, 0};
        // come su Roblox: chi e' seduto va con il sedile (treni, veicoli, giostre)
        if (h.seatPart >= 0 && h.seatPart < (int)g_world.parts.size() && g_world.parts[(size_t)h.seatPart].seat) {
            const auto& sp = g_world.parts[(size_t)h.seatPart];
            g_playerPos = sp.position + glm::vec3(0.f, sp.size.y * 0.5f + 0.05f, 0.f);
        } else {
            h.seated = false;   // il sedile non c'e' piu'
            h.seatPart = -1;
        }
    }
    }
    auto& trails = scriptControls().trails;
    for (auto& t : trails) {
        if (!t.enabled || t.partIndex < 0 || t.partIndex >= (int)g_world.parts.size()) continue;
        const glm::vec3 pos = g_world.parts[(size_t)t.partIndex].position;
        if (t.pts.empty() || glm::length(pos - t.pts.back()) > 0.15f) {
            t.pts.push_back(pos);
            t.age.push_back(0.f);
        }
        for (float& a : t.age) a += dt;
        while (!t.age.empty() && t.age.front() > t.life) {
            t.age.erase(t.age.begin());
            t.pts.erase(t.pts.begin());
        }
        if (t.pts.size() > 24) {
            t.pts.erase(t.pts.begin());
            t.age.erase(t.age.begin());
        }
    }
}

static void applyWaterBuoyancy(float dt) {
    if (dt <= 0.f || !g_bphys.ready()) return;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.toolSlot >= 0 || p.anchored || p.material == PartMaterial::Water) continue;
        bool wet = false;
        for (const auto& w : g_world.parts) {
            if (w.material != PartMaterial::Water || w.toolSlot >= 0) continue;
            if (pointInPart(w, p.position)) { wet = true; break; }
        }
        if (!wet || p.density >= 1.f) continue;
        const float vol = std::min(64.f, p.size.x * p.size.y * p.size.z);
        const float lift = (1.f - p.density) * vol * 6.f * dt;
        g_bphys.applyImpulse(i, glm::vec3(0.f, lift, 0.f));
    }
}

static void applyConstraintMovers(float dt) {
    if (dt <= 0.f) return;
    for (const auto& c : g_world.welds) {
        if (!c.enabled || c.part0 < 0 || c.part1 < 0) continue;
        if (c.part0 >= (int)g_world.parts.size() || c.part1 >= (int)g_world.parts.size()) continue;
        const WorkspacePart& a = g_world.parts[(size_t)c.part0];
        WorkspacePart& b = g_world.parts[(size_t)c.part1];
        switch (c.kind) {
        case ConstraintKind::AlignPosition: {
            const glm::vec3 goal = a.position + c.relPos;
            glm::vec3 d = goal - b.position;
            const float step = std::max(1.f, c.stiffness) * dt * 6.f;
            if (glm::length(d) > step && glm::length(d) > 1e-4f) d = glm::normalize(d) * step;
            b.position += d;
            break;
        }
        case ConstraintKind::AlignOrientation:
            b.rotation = a.rotation + c.relRot;
            break;
        case ConstraintKind::LinearVelocity:
            g_bphys.setLinearVelocity(c.part1, c.axis * std::max(0.f, c.length));
            break;
        case ConstraintKind::AngularVelocity:
            g_bphys.setAngularVelocity(c.part1, c.axis);
            break;
        case ConstraintKind::VectorForce:
            g_bphys.applyImpulse(c.part1, c.axis * c.stiffness * dt);
            break;
        case ConstraintKind::Torque:
            g_bphys.setAngularVelocity(c.part1, c.axis * std::max(0.1f, c.stiffness) * 0.15f);
            break;
        default:
            break;
        }
    }
}


// ---- Stato del giocatore deciso dal server dedicato -----------------------------------------------
// Il server manda "H=vita;M=max;W=velocita';J=salto;L=Monete:10;T=x,y,z" con
// l'evento riservato "__blkPlayerState" (blocky_servers/ServerWorld.h): qui lo
// si applica al giocatore locale prima che gli eventi arrivino agli script.
static void applyServerPlayerState(const std::string& st) {
    auto& h = g_player.humanoid;
    size_t pos = 0;
    while (pos < st.size()) {
        size_t end = st.find(';', pos);
        if (end == std::string::npos) end = st.size();
        const std::string item = st.substr(pos, end - pos);
        pos = end + 1;
        if (item.size() < 3 || item[1] != '=') continue;
        const std::string v = item.substr(2);
        switch (item[0]) {
        case 'M': h.maxHealth = std::max(1.f, (float)std::atof(v.c_str())); break;
        case 'H': {
            const float target = std::clamp((float)std::atof(v.c_str()), 0.f, std::max(1.f, h.maxHealth));
            // Danni e cure passano dal Humanoid: cosi' scattano lampo rosso, morte e respawn.
            if (target < h.health - 0.01f) h.takeDamage(h.health - target);
            else if (target > h.health + 0.01f) h.heal(target - h.health);
            break;
        }
        case 'W': h.walkSpeed = std::clamp((float)std::atof(v.c_str()), 0.f, 500.f); break;
        case 'J': h.jumpPower = std::clamp((float)std::atof(v.c_str()), 0.f, 1000.f); break;
        case 'L': {
            // "Monete:10,Livello:2" -> leaderstats del giocatore locale (classifica con Tab)
            size_t a = 0;
            while (a < v.size()) {
                size_t b = v.find(',', a);
                if (b == std::string::npos) b = v.size();
                const std::string kv = v.substr(a, b - a);
                const size_t c = kv.find(':');
                if (c != std::string::npos) g_classifica.set(kv.substr(0, c), std::atof(kv.c_str() + c + 1));
                a = b + 1;
            }
            break;
        }
        case 'T': {
            float x = 0, y = 0, z = 0;
            if (std::sscanf(v.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
                g_playerPos = {x, y, z};   // piedi
                g_exportMsg = "Teletrasportato dal server";
            }
            break;
        }
        default: break;
        }
    }
}

/** Toglie dalla coda della rete gli stati del giocatore e li applica. */
static void consumeServerPlayerStates() {
    auto& q = g_net.incomingRemotes;
    // Solo un client accetta lo stato dal server: chi ospita una partita LAN lo scarta,
    // cosi' un client non puo' cambiare vita o posizione all'host.
    const bool fromServer = g_net.role == NetRole::Client;
    for (auto it = q.begin(); it != q.end();) {
        if (std::strcmp(it->name, "__blkPlayerState") == 0) {
            if (fromServer && !it->toServer) applyServerPlayerState(it->payload);
            it = q.erase(it);
        } else {
            ++it;
        }
    }
}
