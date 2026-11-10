#pragma once

/**
 * NON e' un header riusabile come quelli in src/ui/ o src/engine/ — e' il
 * pezzo di main.cpp che disegna l'HUD della schermata di Play, spostato qui
 * solo per accorciare main.cpp: hamburger menu, chat (bolla + finestra +
 * bolle di dialogo), barre salute, sistema Player amico (sync/insert/
 * update/draw), inventario, e drawPlayingHud() che compone tutto quanto
 * insieme al menu Esc (Riprendi/Shift Lock/Prima persona/Grafica/Leave).
 * Legge/scrive le static globali di main.cpp e chiama le sue funzioni
 * helper per nome (main.cpp le forward-dichiara tutte, ~righe 302-377).
 * Compila SOLO se incluso da main.cpp, esattamente nel punto in cui questo
 * codice viveva prima — mai da studio_main.cpp, BlockyApi.cpp o altre TU.
 */

static void setPlayMenuOpen(GLFWwindow* win, bool open) {
    g_playMenuOpen = open;
    if (g_screen == AppScreen::Playing || g_studioTesting)
        syncPlayCursor(win);
}

static void drawHamburgerMenuHud() {
    const float r = 22.f;
    const float x = 14.f;
    const float y = 56.f;
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r * 2.f + 8.f, r * 2.f + 8.f}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, r + 4.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.f, 0.f, 0.f, 0.f});
    ImGui::Begin("##HamburgerMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 c{origin.x + r, origin.y + r};
    if (ImGui::InvisibleButton("##hamburgerclick", {r * 2.f, r * 2.f})) {
        if (GLFWwindow* w = glfwGetCurrentContext())
            setPlayMenuOpen(w, !g_playMenuOpen);
    }
    const bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool on = g_playMenuOpen || hover;
    ImU32 fill = on ? IM_COL32(70, 72, 78, 245) : IM_COL32(45, 46, 50, 230);
    dl->AddCircleFilled({c.x + 1.5f, c.y + 2.f}, r, IM_COL32(0, 0, 0, 55), 28);
    dl->AddCircleFilled(c, r, fill, 28);
    const float barW = 22.f, barH = 3.f, gap = 6.5f;
    for (int i = -1; i <= 1; ++i) {
        float by = c.y + (float)i * gap;
        dl->AddRectFilled({c.x - barW * 0.5f, by - barH * 0.5f},
                          {c.x + barW * 0.5f, by + barH * 0.5f},
                          IM_COL32(255, 255, 255, 250), 1.5f);
    }
    if (hover)
        ImGui::SetTooltip("Menu");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

static bool sendPlayChat(const char* msg) {
    if (!msg || !msg[0]) return false;
    const char* uname = g_menu.username[0] ? g_menu.username : "Player";
    if (!Moderation::tryAppendChat(g_menu.chatLines, g_menu.mod, uname, msg))
        return false;
    const bool inWorld = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (inWorld && !g_menu.mod.history.empty()) {
        const auto& e = g_menu.mod.history.back();
        if (!e.blocked && !e.display.empty()) {
            // Fuori dal proprio schermo: prima la chat restava locale e chi
            // scriveva dal telefono non lo vedeva nessuno.
            g_net.sendChat(e.display.c_str());
            g_speechText = e.display;
            if (g_speechText.size() > 90)
                g_speechText = g_speechText.substr(0, 87) + "...";
            g_speechUntil = glfwGetTime() + 5.0;
        }
    }
    return true;
}

static void drawSpeechBubbles() {
    if (g_speechText.empty() || glfwGetTime() > g_speechUntil) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    glm::vec3 anchor = g_playerPos + glm::vec3(0.f, 6.35f, 0.f);
    float sx = 0.f, sy = 0.f;
    if (!worldToScreen(anchor, sx, sy)) return;

    const double left = g_speechUntil - glfwGetTime();
    float a = left < 0.7 ? (float)(left / 0.7) : 1.f;
    a = glm::clamp(a, 0.f, 1.f);
    const int ai = (int)(a * 255.f);

    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize() * 0.95f;
    const float wrapW = 220.f;
    ImVec2 textSize = font->CalcTextSizeA(fs, wrapW, wrapW, g_speechText.c_str());
    const float padX = 12.f, padY = 8.f;
    const float tipH = 10.f;
    ImVec2 boxMin{sx - textSize.x * 0.5f - padX, sy - textSize.y - padY * 2.f - tipH};
    ImVec2 boxMax{sx + textSize.x * 0.5f + padX, sy - tipH};
    if (boxMin.x < 8.f) {
        float d = 8.f - boxMin.x;
        boxMin.x += d; boxMax.x += d;
    }
    if (boxMax.x > (float)g_w - 8.f) {
        float d = boxMax.x - ((float)g_w - 8.f);
        boxMin.x -= d; boxMax.x -= d;
    }
    if (boxMin.y < 8.f) {
        float d = 8.f - boxMin.y;
        boxMin.y += d; boxMax.y += d;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float round = 12.f;
    dl->AddRectFilled({boxMin.x + 2.f, boxMin.y + 3.f},
                      {boxMax.x + 2.f, boxMax.y + 3.f},
                      IM_COL32(0, 0, 0, (int)(a * 45.f)), round);
    dl->AddRectFilled(boxMin, boxMax, IM_COL32(255, 255, 255, ai), round);
    const float midX = (boxMin.x + boxMax.x) * 0.5f;
    dl->AddTriangleFilled(
        {midX - 7.f, boxMax.y - 0.5f},
        {midX + 7.f, boxMax.y - 0.5f},
        {sx + 1.5f, boxMax.y + tipH + 1.5f},
        IM_COL32(0, 0, 0, (int)(a * 40.f)));
    dl->AddTriangleFilled(
        {midX - 8.f, boxMax.y - 1.f},
        {midX + 8.f, boxMax.y - 1.f},
        {sx, boxMax.y + tipH},
        IM_COL32(255, 255, 255, ai));
    dl->AddText(font, fs, {boxMin.x + padX, boxMin.y + padY},
                IM_COL32(25, 25, 25, ai), g_speechText.c_str(), nullptr, wrapW);
}

static void drawChatBubbleHud() {
    const float r = 22.f;
    const float x = 76.f;
    const float y = 56.f;
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r * 2.f + 8.f, r * 2.f + 8.f}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, r + 4.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.f, 0.f, 0.f, 0.f});
    ImGui::Begin("##ChatBubble", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 c{origin.x + r, origin.y + r};
    if (ImGui::InvisibleButton("##chatbubclick", {r * 2.f, r * 2.f})) {
        g_showChat = !g_showChat;
        if (GLFWwindow* w = glfwGetCurrentContext())
            syncPlayCursor(w);
    }
    const bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool on = g_showChat || hover;
    ImU32 fill = on ? IM_COL32(0, 162, 255, 245) : IM_COL32(0, 132, 232, 230);
    dl->AddCircleFilled({c.x + 1.5f, c.y + 2.f}, r, IM_COL32(0, 0, 0, 55), 28);
    dl->AddCircleFilled(c, r, fill, 28);
    dl->AddRectFilled({c.x - 10.f, c.y - 8.f}, {c.x + 10.f, c.y + 4.f},
                      IM_COL32(255, 255, 255, 250), 5.f);
    dl->AddTriangleFilled({c.x - 4.f, c.y + 4.f}, {c.x + 2.f, c.y + 4.f},
                          {c.x - 6.f, c.y + 11.f}, IM_COL32(255, 255, 255, 250));
    dl->AddCircleFilled({c.x - 5.f, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    dl->AddCircleFilled({c.x, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    dl->AddCircleFilled({c.x + 5.f, c.y - 2.f}, 1.5f, IM_COL32(0, 120, 210, 255), 8);
    if (hover)
        ImGui::SetTooltip("Chat");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Nome (sempre) + barra vita (solo se showHealthBar) sopra la testa di un
// player. I remoti non hanno la salute sincronizzata via rete, quindi per
// loro si vede solo il nome (showHealthBar=false).
/**
 * Il distintivo di piattaforma accanto al nome.
 *
 * NON e' il badge "Disponibile su Google Play": quello e' un marchio
 * registrato, usabile solo secondo le linee guida di Google e solo per
 * portare a una scheda Play che esista davvero. Blocky su Play non c'e',
 * quindi mostrarlo direbbe una cosa falsa.
 *
 * Questo e' nostro, e dice una cosa piu' utile: su cosa sta giocando l'altro.
 * Chi gioca col telefono ha comandi a schermo invece di mouse e tastiera, e
 * saperlo cambia come lo si giudica — esattamente il motivo per cui i giochi
 * grossi mostrano l'icona della piattaforma accanto ai giocatori.
 *
 * Restituisce la larghezza disegnata, 0 se non c'e' niente da dire.
 */
static float drawPlatformBadge(ImDrawList* dl, float x, float y, uint8_t platformPlusOne) {
    if (platformPlusOne == 0) return 0.f;   // client che non lo dichiara

    const char* label = nullptr;
    ImU32 bg = IM_COL32(70, 70, 80, 200);
    switch ((TargetPlatform)(platformPlusOne - 1)) {
    case TargetPlatform::PC:            label = "PC";  bg = IM_COL32(45, 95, 170, 210); break;
    case TargetPlatform::MobileAndroid: label = "AND"; bg = IM_COL32(50, 140, 70, 210); break;
    case TargetPlatform::MobileIOS:     label = "iOS"; bg = IM_COL32(95, 95, 105, 210); break;
    case TargetPlatform::PlayStation5:  label = "PS5"; bg = IM_COL32(30, 70, 160, 210); break;
    default: return 0.f;                // sconosciuta: meglio niente che una bugia
    }

    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float padX = 4.f, padY = 1.f;
    const float w = ts.x + padX * 2.f;
    const float h = ts.y + padY * 2.f;

    dl->AddRectFilled({x, y}, {x + w, y + h}, bg, 3.f);
    dl->AddText({x + padX, y + padY}, IM_COL32(255, 255, 255, 235), label);
    return w;
}

static void drawHeadNameTag(const glm::vec3& headWorld, const char* name,
                            float pct, bool dead, bool showHealthBar,
                            uint8_t platformPlusOne = 0) {
    float sx = 0.f, sy = 0.f;
    if (!worldToScreen(headWorld, sx, sy)) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float y = sy - 18.f;

    if (showHealthBar) {
        const float barW = 56.f, barH = 7.f;
        float x = sx - barW * 0.5f;
        dl->AddRectFilled({x - 1.5f, y - 1.5f}, {x + barW + 1.5f, y + barH + 1.5f},
                          IM_COL32(0, 0, 0, 170), 3.f);
        dl->AddRectFilled({x, y}, {x + barW, y + barH}, IM_COL32(30, 30, 35, 220), 2.f);
        float p = pct;
        if (dead) p = 1.f;
        ImU32 fillCol = healthBarColor(pct, dead);
        float fillW = barW * p;
        if (fillW > 0.5f)
            dl->AddRectFilled({x, y}, {x + fillW, y + barH}, fillCol, 2.f);
        y -= 4.f;
    }

    const ImVec2 ts = ImGui::CalcTextSize(name);

    // Nome e distintivo si centrano INSIEME, non uno per volta: centrare solo
    // il nome e poi appendere il distintivo a destra sposterebbe il nome fuori
    // asse rispetto alla testa, e con nomi di lunghezza diversa si vedrebbe.
    const float badgeW = (platformPlusOne != 0)
        ? ImGui::CalcTextSize("AND").x + 8.f + 4.f : 0.f;
    const float totalW = ts.x + badgeW;

    const float tx = sx - totalW * 0.5f;
    const float ty = y - ts.y - 2.f;
    dl->AddText({tx + 1, ty + 1}, IM_COL32(0, 0, 0, 200), name);
    dl->AddText({tx, ty}, IM_COL32(255, 255, 255, 255), name);

    if (platformPlusOne != 0)
        drawPlatformBadge(dl, tx + ts.x + 4.f, ty, platformPlusOne);
}

static void drawPlayerHeadHealthBar() {
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;

    if (!g_firstPerson || g_ragdoll.active) {
        const auto& h = g_player.humanoid;
        glm::vec3 head = g_playerPos + glm::vec3(0.f, 5.6f, 0.f);
        bool showBar = h.dead || h.health < 90.f;
        // Anche il proprio: cosi si vede subito che funziona, senza dover
        // accendere due dispositivi per provarlo.
        drawHeadNameTag(head, g_menu.username[0] ? g_menu.username : "Tu",
                       h.dead ? 0.f : h.healthPct(), h.dead, showBar,
                       (uint8_t)((uint8_t)detectTargetPlatform() + 1));
    }
    for (const auto& r : g_net.remotes) {
        glm::vec3 head = r.displayPos + glm::vec3(0.f, 5.6f, 0.f);
        drawHeadNameTag(head, r.name, 1.f, false, false, r.platform);
    }
}

static void syncFriendsToWorld() {
    g_world.friends.clear();
    g_world.friends.reserve(g_friends.size());
    for (const auto& f : g_friends) {
        WorkspaceFriend w;
        w.name = f.name;
        w.position = f.pos;
        w.yaw = f.yaw;
        g_world.friends.push_back(std::move(w));
    }
}

static void syncFriendsFromWorld() {
    g_friends.clear();
    g_friends.reserve(g_world.friends.size());
    int maxN = 0;
    for (const auto& w : g_world.friends) {
        FriendPlayer f;
        f.name = w.name.empty() ? "Amico" : w.name;
        f.pos = w.position;
        f.pos.y = 0.f;
        f.yaw = w.yaw;
        if (f.name.size() > 5 && f.name.compare(0, 5, "Amico") == 0) {
            int n = std::atoi(f.name.c_str() + 5);
            if (n > maxN) maxN = n;
        }
        g_friends.push_back(std::move(f));
    }
    if (maxN >= g_friendSeq) g_friendSeq = maxN + 1;
}

static void insertFriendPlayer() {
    FriendPlayer f;
    f.name = "Amico" + std::to_string(g_friendSeq++);
    f.pos = insertPositionInFront();
    f.pos.y = 0.f;
    glm::vec3 to = g_playerPos - f.pos;
    to.y = 0.f;
    if (glm::length(to) > 0.1f) {
        to = glm::normalize(to);
        f.yaw = glm::degrees(std::atan2(to.x, to.z));
    }
    g_friends.push_back(std::move(f));
    syncFriendsToWorld();
    markPlaceDirty();
    g_exportMsg = "Player amico aggiunto — in Test ti segue";
}

static void updateFriendPlayers(float dt) {
    if (dt <= 0.f) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    for (auto& f : g_friends) {
        glm::vec3 to = g_playerPos - f.pos;
        to.y = 0.f;
        float dist = glm::length(to);
        bool walking = false;
        if (dist > 3.5f && dist < 80.f) {
            glm::vec3 dir = to / dist;
            float spd = (dist > 12.f) ? 14.f : 9.f;
            f.pos += dir * spd * dt;
            f.yaw = glm::degrees(std::atan2(dir.x, dir.z));
            walking = true;
        }
        f.pos.y = 0.f;
        f.body.update(dt, walking);
    }
}

static void drawFriendPlayers(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (g_friends.empty()) return;
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
    for (auto& f : g_friends) {
        f.body.draw(bctx, drawCubePlayer, f.pos, f.yaw, nullptr);
        glm::vec3 namePos = f.pos + glm::vec3(0.f, 5.4f, 0.f);
        drawBillboardText(namePos, f.name.c_str(), {0.45f, 1.f, 0.55f}, 0.85f, true, false);
    }
}

static void drawHealthBarHud() {
    const auto& h = g_player.humanoid;
    const float barW = 220.f, barH = 18.f;
    const float x = (float)g_w - barW - 16.f;
    const float y = 56.f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    dl->AddRectFilled({x - 3, y - 3}, {x + barW + 3, y + barH + 3},
                      IM_COL32(0, 0, 0, 160), 4.f);
    dl->AddRectFilled({x, y}, {x + barW, y + barH}, IM_COL32(35, 35, 40, 220), 3.f);

    float pct = h.dead ? 0.f : h.healthPct();
    ImU32 fillCol = healthBarColor(pct, h.dead || h.health <= 0.f);
    float fillW = std::max(0.f, barW * pct);
    if (h.dead)
        fillW = barW;
    if (fillW > 0.5f)
        dl->AddRectFilled({x, y}, {x + fillW, y + barH}, fillCol, 3.f);

    char lab[64];
    if (h.dead)
        std::snprintf(lab, sizeof(lab), "%s", L("hud.dead"));
    else
        std::snprintf(lab, sizeof(lab), "%s  %.0f / %.0f", L("hud.health"), h.health, h.maxHealth);
    ImVec2 ts = ImGui::CalcTextSize(lab);
    dl->AddText({x + (barW - ts.x) * 0.5f, y + (barH - ts.y) * 0.5f},
                IM_COL32(255, 255, 255, 240), lab);
}

// Slot mostrato "in mano" mentre si regola la presa in Studio.
//
// Serve perche' la mesh in mano viene disegnata solo per l'oggetto
// EQUIPAGGIATO, mentre in Studio l'oggetto si SELEZIONA per modificarlo: si
// cambiavano rotazione e scala e non si vedeva cambiare niente, perche' il
// personaggio non aveva nulla in mano.
//
// Non si tocca g_world.equippedBackpack perche' quello viene salvato nel
// place: un'anteprima di lavoro non deve finire dentro il gioco pubblicato.

/**
 * L'oggetto da disegnare in mano.
 *
 * In Studio, mentre si regola la presa, vince l'anteprima; in gioco e durante
 * il Test vince sempre quello davvero equipaggiato.
 */
static int effectiveHeldSlot() {
    if (g_screen == AppScreen::Studio && !g_studioTesting && g_gripPreviewSlot >= 0)
        return g_gripPreviewSlot;
    return g_world.equippedBackpack;
}

static const BackpackItem* equippedBackpackItem() {
    int i = effectiveHeldSlot();
    if (i < 0 || i >= (int)g_world.backpack.size()) return nullptr;
    return &g_world.backpack[i];
}

static bool equippedSword() {
    const BackpackItem* it = equippedBackpackItem();
    return it && it->tool == BackpackTool::Sword;
}

static bool equippedTorch() {
    const BackpackItem* it = equippedBackpackItem();
    return it && it->tool == BackpackTool::Torch;
}

static void tryUseEquippedItem() {
    const BackpackItem* it = equippedBackpackItem();
    if (!it || it->tool == BackpackTool::Sword) return;
    if (!g_lua.ok) return;
    g_lua.fireItemActivated(it->id, it->name);
}

static void drawEquippedSword(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (!equippedSword() || g_player.humanoid.dead) return;

    const HumanoidPartDef* arm = nullptr;
    for (const auto& p : g_player.humanoid.parts) {
        if (p.name == "Right Arm") { arm = &p; break; }
    }
    if (!arm) return;

    float walkArm = humanoidArmSwing(g_player.humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(g_player.humanoid, jL, jR, jl, jr);
    float bob = humanoidBob(g_player.humanoid);
    float rx = arm->rotX;
    float rz = arm->rotZ;
    bool animDriven = (std::fabs(arm->rotX) + std::fabs(arm->rotZ)) > 0.01f;
    // Stessa regola del braccio in BlockPlayer::draw: se c e un oggetto
    // equipaggiato la posa di presa vince su tutto, altrimenti il braccio si
    // alzerebbe e la mesh resterebbe indietro a meta strada.
    if (effectiveHeldSlot() >= 0) {
        rx = kEquippedArmHoldDeg;
    } else if (!animDriven)
        rx = -walkArm * (1.f - g_player.humanoid.jumpArmBlend) + jR;

    float swingDeg = g_swordSwing * 70.f;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                 glm::radians(g_playerYaw), glm::vec3(0, 1, 0));
    glm::vec3 c = arm->offset;
    glm::mat4 armM = glm::translate(root, c + arm->pivot);
    if (rx != 0) armM = glm::rotate(armM, glm::radians(rx), glm::vec3(1, 0, 0));
    if (rz != 0) armM = glm::rotate(armM, glm::radians(rz), glm::vec3(0, 0, 1));
    armM = glm::translate(armM, -arm->pivot);
    armM = glm::translate(armM, {0.15f, -0.85f, 0.35f});
    armM = glm::rotate(armM, glm::radians(-90.f + swingDeg), glm::vec3(1, 0, 0));
    armM = glm::rotate(armM, glm::radians(8.f), glm::vec3(0, 0, 1));

    const glm::vec3 steel{0.75f, 0.78f, 0.85f};
    const glm::vec3 hilt{0.35f, 0.22f, 0.12f};
    const glm::vec3 guard{0.55f, 0.55f, 0.60f};

    auto drawBit = [&](glm::vec3 local, glm::vec3 size, glm::vec3 col) {
        glm::mat4 m = glm::translate(armM, local) * glm::scale(glm::mat4(1.f), size);
        drawCube(vao, partP, m, view, proj, col, sun, false, 0);
    };
    drawBit({0, 0.15f, 0}, {0.18f, 0.45f, 0.18f}, hilt);
    drawBit({0, 0.42f, 0}, {0.55f, 0.12f, 0.18f}, guard);
    drawBit({0, 1.35f, 0}, {0.14f, 1.6f, 0.06f}, steel);
    (void)bob;
}

// Torcia in mano fatta solo di cubi (manico + fiamma emissiva), stesso
// schema di drawEquippedSword — cosi' un item puo' avere un aspetto in mano
// senza bisogno di importare nessuna mesh esterna, esattamente come un
// Handle costruito con Part in Roblox Studio.
static void drawEquippedTorch(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (!equippedTorch() || g_player.humanoid.dead) return;

    const HumanoidPartDef* arm = nullptr;
    for (const auto& p : g_player.humanoid.parts) {
        if (p.name == "Right Arm") { arm = &p; break; }
    }
    if (!arm) return;

    float walkArm = humanoidArmSwing(g_player.humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(g_player.humanoid, jL, jR, jl, jr);
    float rx = arm->rotX;
    float rz = arm->rotZ;
    bool animDriven = (std::fabs(arm->rotX) + std::fabs(arm->rotZ)) > 0.01f;
    // Stessa regola del braccio in BlockPlayer::draw: se c e un oggetto
    // equipaggiato la posa di presa vince su tutto, altrimenti il braccio si
    // alzerebbe e la mesh resterebbe indietro a meta strada.
    if (effectiveHeldSlot() >= 0) {
        rx = kEquippedArmHoldDeg;
    } else if (!animDriven)
        rx = -walkArm * (1.f - g_player.humanoid.jumpArmBlend) + jR;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                 glm::radians(g_playerYaw), glm::vec3(0, 1, 0));
    glm::vec3 c = arm->offset;
    glm::mat4 armM = glm::translate(root, c + arm->pivot);
    if (rx != 0) armM = glm::rotate(armM, glm::radians(rx), glm::vec3(1, 0, 0));
    if (rz != 0) armM = glm::rotate(armM, glm::radians(rz), glm::vec3(0, 0, 1));
    armM = glm::translate(armM, -arm->pivot);
    armM = glm::translate(armM, {0.15f, -0.85f, 0.35f});
    armM = glm::rotate(armM, glm::radians(-70.f), glm::vec3(1, 0, 0));

    const glm::vec3 wood{0.36f, 0.24f, 0.14f};
    // Fiamma pulsante: stessa idea del flicker dei neon nel mondo, cosi' non
    // sembra una luce statica finta.
    float t = (float)glfwGetTime();
    float flicker = 1.f + 0.12f * std::sin(t * 11.f) + 0.06f * std::sin(t * 27.f + 1.7f);
    const glm::vec3 flame = glm::vec3(1.f, 0.62f, 0.18f) * flicker;

    auto drawBit = [&](glm::vec3 local, glm::vec3 size, glm::vec3 col) {
        glm::mat4 m = glm::translate(armM, local) * glm::scale(glm::mat4(1.f), size);
        drawCube(vao, partP, m, view, proj, col, sun, false, 0, 36, 0.f, PartMaterial::Wood);
    };
    drawBit({0, 0.f, 0}, {0.12f, 1.1f, 0.12f}, wood);
    // Emissive alto per far reagire davvero la fiamma alla luce/bloom, non
    // solo colore acceso: stesso linguaggio del materiale Neon delle part.
    glm::mat4 flameM = glm::translate(armM, {0, 0.68f, 0}) * glm::scale(glm::mat4(1.f), glm::vec3(0.30f, 0.34f, 0.30f));
    drawCube(vao, partP, flameM, view, proj, flame, sun, false, 0, 36, 1.4f * flicker, PartMaterial::Neon);
}

static std::map<std::string, StaticMesh> g_heldItemMeshCache;

static StaticMesh* getHeldItemMesh(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = g_heldItemMeshCache.find(path);
    if (it != g_heldItemMeshCache.end())
        return it->second.loaded ? &it->second : nullptr;

    StaticMesh mesh;
    mesh.load(path);
    auto [ins, ok] = g_heldItemMeshCache.emplace(path, std::move(mesh));
    (void)ok;
    return ins->second.loaded ? &ins->second : nullptr;
}

static void drawEquippedItemMesh(GLuint, GLuint partP, GLuint meshP,
                                 const glm::mat4& view, const glm::mat4& proj,
                                 const glm::vec3& sun) {
    (void)partP;
    const BackpackItem* it = equippedBackpackItem();
    if (!it || it->tool == BackpackTool::Sword || it->meshPath.empty()) return;
    if (g_player.humanoid.dead) return;

    StaticMesh* mesh = getHeldItemMesh(it->meshPath);
    if (!mesh) return;

    const HumanoidPartDef* arm = nullptr;
    for (const auto& p : g_player.humanoid.parts) {
        if (p.name == "Right Arm") { arm = &p; break; }
    }
    if (!arm) return;

    float walkArm = humanoidArmSwing(g_player.humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(g_player.humanoid, jL, jR, jl, jr);
    float rx = arm->rotX;
    float rz = arm->rotZ;
    bool animDriven = (std::fabs(arm->rotX) + std::fabs(arm->rotZ)) > 0.01f;
    // Stessa regola del braccio in BlockPlayer::draw: se c e un oggetto
    // equipaggiato la posa di presa vince su tutto, altrimenti il braccio si
    // alzerebbe e la mesh resterebbe indietro a meta strada.
    if (effectiveHeldSlot() >= 0) {
        rx = kEquippedArmHoldDeg;
    } else if (!animDriven)
        rx = -walkArm * (1.f - g_player.humanoid.jumpArmBlend) + jR;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                 glm::radians(g_playerYaw), glm::vec3(0, 1, 0));
    glm::mat4 armM = glm::translate(root, arm->offset + arm->pivot);
    if (rx != 0) armM = glm::rotate(armM, glm::radians(rx), glm::vec3(1, 0, 0));
    if (rz != 0) armM = glm::rotate(armM, glm::radians(rz), glm::vec3(0, 0, 1));
    armM = glm::translate(armM, -arm->pivot);
    // Punto di presa: in fondo al braccio, leggermente in avanti — grosso modo
    // dove sta la mano.
    armM = glm::translate(armM, {0.15f, -0.85f, 0.35f});

    // Scala: se l'item non ne impone una, si adatta la mesh perche' il suo lato
    // piu' lungo misuri kGripTargetLen stud.
    //
    // Serve perche' un modello importato arriva con la dimensione con cui e'
    // stato esportato, che non ha nessun rapporto con il nostro personaggio: lo
    // stesso fucile puo' risultare un grissino o un tronco. Prima si disegnava
    // cosi' com'era, ed e' il motivo per cui la pistola sembrava minuscola.
    float scale = 1.f;
    {
        const glm::vec3 size = mesh->boundsMax - mesh->boundsMin;
        const float longest = std::max(size.x, std::max(size.y, size.z));
        const float autoFit = (longest > 0.001f) ? (kGripTargetLen / longest) : 1.f;
        // L adattamento automatico porta qualunque modello a una dimensione
        // sensata; il numero dell item lo moltiplica da li in su o in giu.
        scale = autoFit * (it->gripScale > 0.001f ? it->gripScale : 1.f);
    }

    // Orientamento: la mesh viene coricata lungo il braccio. Un'arma esportata
    // sta quasi sempre in piedi lungo il proprio asse piu' lungo, mentre in mano
    // deve puntare in avanti — da qui la rotazione di base, che gripRotation
    // corregge quando un modello e' orientato diversamente.
    armM = glm::translate(armM, it->gripOffset);
    if (it->gripRotation != glm::vec3(0.f))
        armM = armM * partRotationMatrix(it->gripRotation);
    armM = glm::scale(armM, glm::vec3(scale));

    glUseProgram(meshP);
    glUniformMatrix4fv(glGetUniformLocation(meshP, "uModel"), 1, GL_FALSE, glm::value_ptr(armM));
    glUniformMatrix4fv(glGetUniformLocation(meshP, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(meshP, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(meshP, "uSun"), 1, glm::value_ptr(sun));
    mesh->drawTextured(meshP, {1.f, 1.f, 1.f}, 1.f);
}

static void trySwordAttack() {
    if (!equippedSword() || g_player.humanoid.dead) return;
    if (g_swordCooldown > 0.f) return;
    g_swordCooldown = 0.45f;
    g_swordSwing = 1.f;

    glm::vec3 fwd = camFlatForward();
    glm::vec3 origin = g_playerPos + glm::vec3(0, 2.5f, 0) + fwd * 1.2f;
    const float reach = 4.5f;
    bool hit = false;
    for (auto& p : g_world.parts) {
        if (!p.canDestroy) continue;
        glm::vec3 to = p.position - origin;
        float dist = glm::length(to);
        if (dist > reach || dist < 0.01f) continue;
        float align = glm::dot(glm::normalize(to), fwd);
        if (align < 0.25f) continue;
        p.health -= 40.f;
        p.position += fwd * 0.8f;
        p.position.y += 0.25f;
        g_particles.emitPreset(ParticlePreset::Sparks, p.position, 22, {-1,-1,-1}, fwd);
        if (p.health <= 0.f) {
            g_particles.spawnExplosion(p.position, 8.f, 16);
            g_lua.applyExplosionPlayerDamage(p.position, 8.f, 25.f);
            for (int i = 0; i < (int)g_world.parts.size(); ++i) {
                if (&g_world.parts[i] == &p) {
                    g_world.parts.erase(g_world.parts.begin() + i);
                    if (g_selectedPart == i) selectPart(-1);
                    else if (g_selectedPart > i) --g_selectedPart;
                    break;
                }
            }
        }
        hit = true;
        break;
    }
    if (!hit)
        g_particles.emitPreset(ParticlePreset::Sparks, origin + fwd * 2.2f, 10, {-1,-1,-1}, fwd);
}

static ImU32 backpackHueColor(int hue) {
    int i = hue % 6;
    if (i < 0) i += 6;
    const float* c = uiExtraColors().backpackHue[i];
    return IM_COL32((int)std::lround(c[0] * 255), (int)std::lround(c[1] * 255),
                    (int)std::lround(c[2] * 255), (int)std::lround(c[3] * 255));
}

static void pollBackpackSwitch(GLFWwindow* win) {
    if (!win) return;

    static const int kRow[] = {
        GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
        GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9
    };
    static const int kPad[] = {
        GLFW_KEY_KP_1, GLFW_KEY_KP_2, GLFW_KEY_KP_3, GLFW_KEY_KP_4, GLFW_KEY_KP_5,
        GLFW_KEY_KP_6, GLFW_KEY_KP_7, GLFW_KEY_KP_8, GLFW_KEY_KP_9
    };

    for (int i = 0; i < DataModel::kHotbarSlots; ++i) {
        bool rd = glfwGetKey(win, kRow[i]) == GLFW_PRESS;
        bool pd = glfwGetKey(win, kPad[i]) == GLFW_PRESS;
        bool pressed = (rd && !g_bpKeyWas[i]) || (pd && !g_bpKpWas[i]);
        g_bpKeyWas[i] = rd;
        g_bpKpWas[i] = pd;
        if (pressed)
            g_world.equipBackpackSlot(i, true);
    }

    static bool leftWas = false, rightWas = false;
    bool left = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool right = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (right && !rightWas) g_world.cycleBackpack(+1);
    if (left && !leftWas) g_world.cycleBackpack(-1);
    leftWas = left; rightWas = right;

    const auto& pad = gamepad();
    if (pad.connected) {
        if (pad.rbPressed) g_world.cycleBackpack(+1);
        if (pad.rtPressed || pad.lbPressed) g_world.cycleBackpack(-1);
    }
}

static void drawInventoryHud() {
    auto& bp = g_world.backpack;
    const int nItems = (int)bp.size();
    const float slotR = 22.f;
    const float gap = 6.f;
    const int showN = DataModel::kHotbarSlots;
    const float totalW = showN * (slotR * 2.f) + (showN - 1) * gap;
    const float x0 = ((float)g_w - totalW) * 0.5f + slotR;
    const float y0 = (float)g_h - slotR - 14.f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGui::SetNextWindowPos({x0 - slotR - 12, y0 - slotR - 28}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({totalW + 40, slotR * 2.f + 48}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##InvCircle", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs);

    dl->AddText({x0 - slotR, y0 - slotR - 22}, IM_COL32(255, 255, 255, 220), L("hud.inventory"));
    if (nItems == 0)
        dl->AddText({x0 - slotR + 100, y0 - slotR - 22}, IM_COL32(170, 170, 180, 170),
                    "empty  ·  1-9");
    else
        dl->AddText({x0 - slotR + 100, y0 - slotR - 22}, IM_COL32(200, 205, 220, 180),
                    "1-9  (di nuovo = togli)");

    for (int i = 0; i < showN; ++i) {
        float cx = x0 + i * (slotR * 2.f + gap);
        float cy = y0;
        bool has = i < nItems;
        bool eq = (g_world.equippedBackpack == i);

        if (has)
            dl->AddCircleFilled({cx, cy}, slotR, backpackHueColor(bp[i].iconHue), 48);
        else
            dl->AddCircleFilled({cx, cy}, slotR, IM_COL32(32, 34, 40, 220), 48);

        ImU32 ring = eq ? IM_COL32(230, 40, 40, 255) : IM_COL32(255, 255, 255, 255);
        dl->AddCircle({cx, cy}, slotR, ring, 48, eq ? 5.0f : 2.8f);
        if (eq)
            dl->AddCircle({cx, cy}, slotR + 6.f, IM_COL32(255, 50, 50, 160), 48, 2.2f);

        char num[4];
        std::snprintf(num, sizeof(num), "%d", i + 1);
        ImVec2 ns = ImGui::CalcTextSize(num);
        ImU32 numCol = eq ? IM_COL32(255, 45, 45, 255) : IM_COL32(255, 255, 255, 230);
        dl->AddText({cx - ns.x * 0.5f, cy - slotR + 4}, numCol, num);

        if (has) {
            char shortN[16];
            std::snprintf(shortN, sizeof(shortN), "%.6s", bp[i].name.c_str());
            ImVec2 ts = ImGui::CalcTextSize(shortN);
            dl->AddText({cx - ts.x * 0.5f, cy - ts.y * 0.5f + 4},
                        IM_COL32(255, 255, 255, 250), shortN);
        } else {
            ImVec2 es = ImGui::CalcTextSize("—");
            dl->AddText({cx - es.x * 0.5f, cy - es.y * 0.5f + 4},
                        IM_COL32(120, 120, 130, 180), "—");
        }
    }
    ImGui::End();
}

static void drawReportPopup() {
    bool wantOpen = (g_menu.reportTargetPlayer >= 0) || g_menu.reportGamePending;
    if (!wantOpen) return;
    ImGui::OpenPopup("Segnala##report");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Segnala##report", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string target;
        if (g_menu.reportGamePending)
            target = g_activeGame.title;
        else if (g_menu.reportTargetPlayer >= 0 && g_menu.reportTargetPlayer < (int)g_net.remotes.size())
            target = g_net.remotes[g_menu.reportTargetPlayer].name;
        ImGui::TextColored({1.f, 0.55f, 0.25f, 1.f}, "Segnala %s: %s",
            g_menu.reportGamePending ? "gioco" : "giocatore", target.c_str());
        ImGui::InputTextMultiline("##reportreason", g_menu.reportReasonBuf,
            sizeof(g_menu.reportReasonBuf), {320, 70});
        ImGui::TextDisabled("Finisce in un log locale — Blocky non ha un server centrale di moderazione.");
        if (ImGui::Button("Invia segnalazione", {160, 32})) {
            std::string reason = g_menu.reportReasonBuf;
            if (g_menu.reportGamePending)
                Moderation::submitGenericReport(g_menu.mod, Moderation::ReportKind::Game,
                    g_menu.username, target, reason);
            else
                Moderation::submitGenericReport(g_menu.mod, Moderation::ReportKind::Player,
                    g_menu.username, target, reason);
            g_menu.chatLines.push_back("[Report] Inviata segnalazione su " + target);
            g_menu.reportTargetPlayer = -1;
            g_menu.reportGamePending = false;
            g_menu.reportReasonBuf[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120, 32})) {
            g_menu.reportTargetPlayer = -1;
            g_menu.reportGamePending = false;
            g_menu.reportReasonBuf[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void drawPlayersPanel() {
    if (!g_menu.showPlayersPanel) return;
    // Sotto la health bar in alto a destra (non in mezzo allo schermo, dove
    // coprirebbe il personaggio).
    ImGui::SetNextWindowPos({(float)g_w - 340 - 16, 82.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({340, 320}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Giocatori", &g_menu.showPlayersPanel);
    ImGui::TextColored({0.6f, 0.9f, 1.f, 1.f}, "In questa sessione");
    ImGui::Separator();
    ImGui::BulletText("%s (Tu)", g_menu.username[0] ? g_menu.username : "Player");
    ImGui::Separator();
    if (g_net.remotes.empty())
        ImGui::TextDisabled("Nessun altro giocatore connesso.");
    for (int i = 0; i < (int)g_net.remotes.size(); ++i) {
        const auto& r = g_net.remotes[i];
        ImGui::PushID(i);
        ImGui::Text("%s", r.name);
        ImGui::SameLine();
        bool friendNow = isFriend(g_menu, r.name);
        if (friendNow) {
            if (ImGui::SmallButton("Rimuovi amico")) removeFriend(g_menu, r.name);
        } else {
            if (ImGui::SmallButton("Aggiungi amico")) addFriend(g_menu, r.name);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.2f, 0.15f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.3f, 0.2f, 1.f});
        if (ImGui::SmallButton("Segnala")) g_menu.reportTargetPlayer = i;
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextColored({0.6f, 0.9f, 1.f, 1.f}, "Amici salvati");
    if (g_menu.userFriends.empty())
        ImGui::TextDisabled("Nessun amico salvato.");
    for (size_t i = 0; i < g_menu.userFriends.size(); ++i) {
        ImGui::PushID(1000 + (int)i);
        ImGui::BulletText("%s", g_menu.userFriends[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Rimuovi")) removeFriend(g_menu, g_menu.userFriends[i]);
        ImGui::PopID();
    }
    ImGui::End();
}

static void drawPlayingHud(GLFWwindow* win) {
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)g_w, 48}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.07f, 0.28f, 0.58f, 0.92f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##PlayTopBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored({1.f, 1.f, 1.f, 1.f}, "BLOCKY");
    ImGui::SameLine();
    ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "  %s", g_activeGame.title.c_str());
    if (g_shiftLock) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 0.88f, 0.30f, 1.f}, "  [SHIFT LOCK]");
    }
    ImGui::SameLine((float)g_w - 290);
    ImGui::TextColored({1.f, 0.90f, 0.40f, 1.f}, "%s  ·  %d B$",
                       g_menu.username[0] ? g_menu.username : "Player", g_menu.shopCoins);
    ImGui::SameLine((float)g_w - 96);
    ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 0.20f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.f, 1.f, 1.f, 0.35f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
    if (ImGui::Button("Menu", {78, 30}))
        setPlayMenuOpen(win, !g_playMenuOpen);
    ImGui::PopStyleColor(3);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    drawHealthBarHud();
    drawPlayerHeadHealthBar();
    drawHamburgerMenuHud();
    drawChatBubbleHud();
    drawSpeechBubbles();
    drawFpCrosshair();
    drawAnimDebugPanel();
    drawWorldLabels();
    if (!g_playMenuOpen)
        drawInventoryHud();

    if (!g_playMenuOpen) {
        ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.06f, 0.20f, 0.42f, 0.75f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
        ImGui::Begin("##PlayHints", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
        {
            const auto& padHint = gamepad();
            if (padHint.connected)
                ImGui::TextColored({0.80f, 0.90f, 1.f, 1.f},
                    "Pad: %s · Start menu · nel menu: LS punta · A/X/L clicca", padHint.name);
            else
                ImGui::TextColored({0.80f, 0.90f, 1.f, 1.f},
                    "WASD · Space · zoom/V = 1ª persona · RMB guarda in 3ª");
            if (g_net.role != NetRole::Offline)
                ImGui::TextColored({0.35f, 0.95f, 0.55f, 1.f}, "%s", g_net.status);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    if (g_playMenuOpen) {
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 180, (float)g_h * 0.5f - 265}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({360, 530}, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.97f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        ImGui::Begin("Menu di gioco", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::TextColored({1.f, 1.f, 1.f, 1.f}, "BLOCKY");
        ImGui::SameLine();
        ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "— %s", g_activeGame.title.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.10f, 0.72f, 0.40f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.16f, 0.85f, 0.50f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Riprendi", {-1, 42}))
            setPlayMenuOpen(win, false);
        ImGui::PopStyleColor(3);

        if (g_net.role == NetRole::Host) {
            ImGui::Spacing();
            if (!g_net.relayActive) {
                bool canHost = blk::PoolConfig::isConfigured();
                if (!canHost) ImGui::BeginDisabled();
                if (ImGui::Button("Ospita Online (blk://)", {-1, 34})) {
                    g_net.registerWithPool(blk::PoolConfig::kPoolHost, blk::PoolConfig::kPoolPort,
                                            g_activeGame.title.c_str());
                }
                if (!canHost) ImGui::EndDisabled();
                if (!canHost)
                    ImGui::TextDisabled("Pool non configurato — vedi PoolConfig.h");
            } else {
                ImGui::TextColored({0.35f, 0.95f, 0.55f, 1.f}, "Codice: blk://%s", g_net.relayCode);
                ImGui::SameLine();
                if (ImGui::SmallButton("Copia")) {
                    std::string full = std::string("blk://") + g_net.relayCode;
                    ImGui::SetClipboardText(full.c_str());
                }
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.42f, 0.82f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.24f, 0.55f, 0.95f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_shiftLock ? "Shift Lock: ON" : "Shift Lock: OFF", {-1, 36})) {
            g_shiftLock = !g_shiftLock;
            if (g_shiftLock) g_firstPerson = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.55f, 0.78f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.65f, 0.92f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_firstPerson ? "Prima persona: ON" : "Prima persona: OFF", {-1, 36})) {
            g_firstPerson = !g_firstPerson;
            if (g_firstPerson) {
                g_shiftLock = false;
                g_playerYaw = yawFromDir(camFlatForward());
                g_orbitDist = kFpEnterDist;
            } else {
                g_orbitDist = std::max(g_orbitDist, kFpExitDist);
            }
            syncPlayCursor(win);
        }
        ImGui::TextDisabled("Oppure zoom (rotella) fino in fondo");
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::TextColored({0.75f, 0.88f, 1.f, 1.f}, "Grafica / Prestazioni");
        {
            const QualityPreset presets[3] = {
                QualityPreset::Low, QualityPreset::Medium, QualityPreset::High };
            const char* labels[3] = { "Low", "Medium", "High" };
            float bw = (ImGui::GetContentRegionAvail().x
                        - ImGui::GetStyle().ItemSpacing.x * 2.f) / 3.f;
            for (int i = 0; i < 3; ++i) {
                bool on = (g_engine.preset == presets[i]);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    on ? ImVec4{0.10f, 0.72f, 0.40f, 1.f} : ImVec4{0.16f, 0.36f, 0.66f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    on ? ImVec4{0.16f, 0.85f, 0.50f, 1.f} : ImVec4{0.24f, 0.50f, 0.85f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
                if (ImGui::Button(labels[i], {bw, 34}))
                    applyGfxPreset(presets[i]);
                ImGui::PopStyleColor(3);
                if (i < 2) ImGui::SameLine();
            }
        }
        ImGui::TextDisabled("%s · vista %.0f · parti %d%s",
                            g_engine.quality.name,
                            g_engine.quality.maxDrawDistance,
                            g_engine.quality.maxVisibleParts,
                            g_engine.quality.studTextures ? "" : " · stud off");
        ImGui::TextDisabled("Ombre luci Neon: %s · MSAA al riavvio",
                            g_engine.quality.shadowOccluders > 0 ? "sì" : "no");
        ImGui::TextDisabled("%s", antilag::engine().summary().c_str());

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.52f, 0.95f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f, 0.62f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button(g_showChat ? LB("hub.hide_chat", "hchat") : LB("hub.show_chat", "schat"), {-1, 40})) {
            g_showChat = !g_showChat;
            if (g_showChat)
                setPlayMenuOpen(win, false);
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.30f, 0.30f, 0.34f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.40f, 0.40f, 0.46f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Giocatori", {-1, 36}))
            g_menu.showPlayersPanel = !g_menu.showPlayersPanel;
        ImGui::PopStyleColor(3);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.55f, 0.30f, 0.10f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.70f, 0.40f, 0.15f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Segnala gioco", {-1, 28}))
            g_menu.reportGamePending = true;
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.82f, 0.22f, 0.22f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.32f, 0.28f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        if (ImGui::Button("Leave", {-1, 44})) {
            g_playMenuOpen = false;
            g_shiftLock = false;
            g_firstPerson = false;
            g_moveVel = {0, 0, 0};
            returnToMenu(win);
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::TextColored({0.70f, 0.82f, 0.98f, 1.f}, "Pad: LS punta · A / X / L clicca · Start chiude");
        ImGui::TextColored({0.70f, 0.82f, 0.98f, 1.f}, "3ª = mouse libero (RMB guarda) · 1ª = mirino");
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    drawPlayersPanel();
    drawReportPopup();

    if (g_showChat) {
        ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({360, 220}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.95f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
        ImGui::Begin("Chat", &g_showChat);
        ImGui::BeginChild("##plog", {0, -36}, false);
        for (const auto& line : g_menu.chatLines)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();
        bool send = ImGui::InputText("##pmsg", g_studioChatBuf, sizeof(g_studioChatBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
            sendPlayChat(g_studioChatBuf);
            g_studioChatBuf[0] = 0;
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}
