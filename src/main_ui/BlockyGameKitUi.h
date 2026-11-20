#pragma once

/**
 * Blocky Game Kit Studio UI — pannelli creatore per pad, porte, wave, match,
 * preset Obby/Arena/Tycoon, log e validazione. Incluso da main.cpp / StudioUiPanel.
 * Dipende da: g_gameKit, g_playerPos, g_world, ImGui, studio:: helpers.
 */

#include "systems/GameKit.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Visibility flags
// ---------------------------------------------------------------------------

static bool g_showGameKitPanel = false;
static bool g_showGameKitPads = false;
static bool g_showGameKitDoors = false;
static bool g_showGameKitWaves = false;
static bool g_showGameKitMatch = false;
static bool g_showGameKitPresets = false;
static bool g_showGameKitLog = false;
static bool g_showGameKitValidator = false;
static bool g_showGameKitQuickPlace = false;
static bool g_showGameKitStats = false;
static bool g_gameKitHudEnabled = true;
static bool g_gameKitDrawPads = true;
static bool g_gameKitAutoTick = true;

// Place form state
static char g_gkPadName[64] = "Pad1";
static int g_gkPadKind = 0;
static float g_gkPadSize[3] = {2.f, 1.f, 2.f};
static float g_gkPadStrength = 1.f;
static int g_gkPadOrder = 0;
static float g_gkTpDest[3] = {0, 2, 0};
static char g_gkDoorName[64] = "Door1";
static bool g_gkDoorKey = false;
static char g_gkDoorKeyId[32] = "key";
static char g_gkWaveName[64] = "Wave1";
static int g_gkWaveCount = 5;
static float g_gkWaveInterval = 2.f;
static float g_gkWaveHp = 50.f;
static float g_gkMatchDur = 120.f;
static float g_gkMatchCd = 3.f;
static char g_gkAnnounceBuf[128] = "Messaggio";
static int g_gkSelectedPad = -1;
static int g_gkSelectedDoor = -1;

static const char* gkPadKindLabels[] = {
    "Checkpoint", "Kill", "Damage", "Speed", "Jump", "Coin", "Teleport", "Heal", "Win"
};

static gamekit::PadKind gkKindFromUi(int i) {
    if (i < 0 || i > 8) return gamekit::PadKind::Checkpoint;
    return (gamekit::PadKind)i;
}

static void gkPlacePadAtPlayer() {
    float x = g_playerPos.x, y = g_playerPos.y, z = g_playerPos.z;
    std::string name = g_gkPadName;
    if (name.empty()) name = "Pad";
    // unique
    std::string base = name;
    int n = 1;
    while (g_gameKit.findPad(name)) {
        name = base + std::to_string(n++);
    }
    g_gameKit.addPad(name, gkKindFromUi(g_gkPadKind), x, y, z,
                     g_gkPadSize[0], g_gkPadSize[1], g_gkPadSize[2],
                     g_gkPadStrength, g_gkPadOrder);
    if (g_gkPadKind == (int)gamekit::PadKind::Teleport)
        g_gameKit.setTeleportDest(name, g_gkTpDest[0], g_gkTpDest[1], g_gkTpDest[2]);
    std::snprintf(g_gkPadName, sizeof(g_gkPadName), "Pad%d", g_gameKit.padCount() + 1);
}

static void gkPlaceDoorAtPlayer() {
    std::string name = g_gkDoorName;
    if (name.empty()) name = "Door";
    std::string base = name;
    int n = 1;
    while (g_gameKit.findDoor(name))
        name = base + std::to_string(n++);
    g_gameKit.addDoor(name, g_playerPos.x, g_playerPos.y + 1.f, g_playerPos.z,
                      g_gkDoorKey, g_gkDoorKeyId);
    std::snprintf(g_gkDoorName, sizeof(g_gkDoorName), "Door%d", g_gameKit.doorCount() + 1);
}

static void gkSpawnVisualPartForPad(const gamekit::Pad& p) {
    WorkspacePart part;
    part.name = std::string("GK_") + p.name;
    part.position = {p.x, p.y, p.z};
    part.size = {p.hx * 2.f, p.hy * 2.f, p.hz * 2.f};
    part.canCollide = false;
    part.anchored = true;
    switch (p.kind) {
    case gamekit::PadKind::Checkpoint: part.color = {0.2f, 0.85f, 0.35f}; break;
    case gamekit::PadKind::Kill: part.color = {0.9f, 0.15f, 0.1f}; break;
    case gamekit::PadKind::Damage: part.color = {0.95f, 0.45f, 0.1f}; break;
    case gamekit::PadKind::Speed: part.color = {0.2f, 0.6f, 1.f}; break;
    case gamekit::PadKind::Jump: part.color = {0.7f, 0.4f, 1.f}; break;
    case gamekit::PadKind::Coin: part.color = {1.f, 0.85f, 0.2f}; break;
    case gamekit::PadKind::Teleport: part.color = {0.3f, 0.9f, 0.9f}; break;
    case gamekit::PadKind::Heal: part.color = {0.4f, 1.f, 0.7f}; break;
    case gamekit::PadKind::Win: part.color = {1.f, 0.95f, 0.4f}; break;
    }
    g_world.parts.push_back(part);
}

static void gkSpawnAllPadVisuals() {
    for (const auto& p : g_gameKit.pads)
        gkSpawnVisualPartForPad(p);
}

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------

static void drawGameKitMainWindow() {
    if (!g_showGameKitPanel) return;
    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit", &g_showGameKitPanel)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Kit creatore: pad gameplay, porte, wave, match timer. "
                       "Funziona in Play / Studio Test.");
    ImGui::Separator();
    ImGui::Checkbox("HUD in partita", &g_gameKitHudEnabled);
    ImGui::Checkbox("Auto tick pads", &g_gameKitAutoTick);
    ImGui::Checkbox("Mostra marker pad (debug)", &g_gameKitDrawPads);
    ImGui::Separator();
    if (ImGui::Button("Pads…", ImVec2(-1, 0))) g_showGameKitPads = true;
    if (ImGui::Button("Porte…", ImVec2(-1, 0))) g_showGameKitDoors = true;
    if (ImGui::Button("Waves…", ImVec2(-1, 0))) g_showGameKitWaves = true;
    if (ImGui::Button("Match…", ImVec2(-1, 0))) g_showGameKitMatch = true;
    if (ImGui::Button("Preset kit…", ImVec2(-1, 0))) g_showGameKitPresets = true;
    if (ImGui::Button("Quick Place…", ImVec2(-1, 0))) g_showGameKitQuickPlace = true;
    if (ImGui::Button("Stats sessione…", ImVec2(-1, 0))) g_showGameKitStats = true;
    if (ImGui::Button("Validator…", ImVec2(-1, 0))) g_showGameKitValidator = true;
    if (ImGui::Button("Log…", ImVec2(-1, 0))) g_showGameKitLog = true;
    ImGui::Separator();
    ImGui::Text("Pads: %d  Porte: %d  Waves: %d",
                g_gameKit.padCount(), g_gameKit.doorCount(), g_gameKit.waveCount());
    ImGui::Text("Fase: %s  Tempo: %.0fs",
                g_gameKit.phase == gamekit::MatchPhase::Playing ? "Playing"
                : g_gameKit.phase == gamekit::MatchPhase::Countdown ? "Countdown"
                : g_gameKit.phase == gamekit::MatchPhase::Ended ? "Ended" : "Idle",
                g_gameKit.matchTimeLeft);
    ImGui::Text("Coins: %d  CP: %d  Deaths: %d",
                g_gameKit.stats.coins, g_gameKit.stats.checkpoints, g_gameKit.stats.deaths);
    ImGui::Separator();
    if (ImGui::Button("Reset GameKit", ImVec2(-1, 0)))
        g_gameKit.reset();
    if (ImGui::Button("Spawn parti visuali pad", ImVec2(-1, 0)))
        gkSpawnAllPadVisuals();
    ImGui::End();
}

static void drawGameKitPadsWindow() {
    if (!g_showGameKitPads) return;
    ImGui::SetNextWindowSize(ImVec2(480, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Pads", &g_showGameKitPads)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Nome", g_gkPadName, sizeof(g_gkPadName));
    ImGui::Combo("Tipo", &g_gkPadKind, gkPadKindLabels, 9);
    ImGui::DragFloat3("Half-size", g_gkPadSize, 0.05f, 0.2f, 40.f);
    ImGui::DragFloat("Strength", &g_gkPadStrength, 0.05f, 0.1f, 100.f);
    ImGui::InputInt("Order (checkpoint)", &g_gkPadOrder);
    if (g_gkPadKind == (int)gamekit::PadKind::Teleport)
        ImGui::DragFloat3("Teleport dest", g_gkTpDest, 0.1f);
    if (ImGui::Button("Piazza al player", ImVec2(-1, 0)))
        gkPlacePadAtPlayer();
    if (ImGui::Button("Piazza + parte visuale", ImVec2(-1, 0))) {
        gkPlacePadAtPlayer();
        if (!g_gameKit.pads.empty())
            gkSpawnVisualPartForPad(g_gameKit.pads.back());
    }
    ImGui::Separator();
    ImGui::BeginChild("padlist", ImVec2(0, -80), true);
    for (int i = 0; i < (int)g_gameKit.pads.size(); ++i) {
        auto& p = g_gameKit.pads[i];
        ImGui::PushID(i);
        bool sel = (g_gkSelectedPad == i);
        if (ImGui::Selectable(
                (p.name + " [" + gamekit::GameKit::padKindName(p.kind) + "]").c_str(),
                sel))
            g_gkSelectedPad = i;
        ImGui::SameLine();
        ImGui::TextDisabled("(%.0f,%.0f,%.0f)", p.x, p.y, p.z);
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (g_gkSelectedPad >= 0 && g_gkSelectedPad < (int)g_gameKit.pads.size()) {
        auto& p = g_gameKit.pads[g_gkSelectedPad];
        ImGui::Checkbox("Enabled", &p.enabled);
        ImGui::DragFloat3("Pos##sel", &p.x, 0.1f);
        ImGui::DragFloat("Strength##sel", &p.strength, 0.05f, 0.1f, 100.f);
        if (p.kind == gamekit::PadKind::Teleport)
            ImGui::DragFloat3("Dest##sel", &p.destX, 0.1f);
        if (ImGui::Button("Vai al pad")) {
            g_playerPos = {p.x, p.y + 2.f, p.z};
        }
        ImGui::SameLine();
        if (ImGui::Button("Elimina pad")) {
            g_gameKit.removePad(p.name);
            g_gkSelectedPad = -1;
        }
    }
    ImGui::End();
}

static void drawGameKitDoorsWindow() {
    if (!g_showGameKitDoors) return;
    ImGui::SetNextWindowSize(ImVec2(400, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Porte", &g_showGameKitDoors)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Nome porta", g_gkDoorName, sizeof(g_gkDoorName));
    ImGui::Checkbox("Richiede chiave", &g_gkDoorKey);
    if (g_gkDoorKey)
        ImGui::InputText("Key id", g_gkDoorKeyId, sizeof(g_gkDoorKeyId));
    if (ImGui::Button("Piazza porta al player", ImVec2(-1, 0)))
        gkPlaceDoorAtPlayer();
    if (ImGui::Button("Dai chiave al player", ImVec2(-1, 0)))
        g_gameKit.giveKey(g_gkDoorKeyId);
    if (ImGui::Button("Togli chiave", ImVec2(-1, 0)))
        g_gameKit.clearKey();
    ImGui::Separator();
    for (int i = 0; i < (int)g_gameKit.doors.size(); ++i) {
        auto& d = g_gameKit.doors[i];
        ImGui::PushID(i);
        ImGui::Text("%s %s anim=%.0f%%", d.name.c_str(),
                    d.open ? "[OPEN]" : "[closed]", d.anim * 100.f);
        if (ImGui::SmallButton("Open")) g_gameKit.openDoor(d.name, true);
        ImGui::SameLine();
        if (ImGui::SmallButton("Close")) g_gameKit.closeDoor(d.name);
        ImGui::SameLine();
        if (ImGui::SmallButton("Del")) {
            g_gameKit.removeDoor(d.name);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::End();
}

static void drawGameKitWavesWindow() {
    if (!g_showGameKitWaves) return;
    ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Waves", &g_showGameKitWaves)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Waves abilitate", &g_gameKit.wavesEnabled);
    ImGui::InputText("Nome", g_gkWaveName, sizeof(g_gkWaveName));
    ImGui::InputInt("Count", &g_gkWaveCount);
    ImGui::DragFloat("Interval", &g_gkWaveInterval, 0.05f, 0.2f, 30.f);
    ImGui::DragFloat("HP mob", &g_gkWaveHp, 1.f, 1.f, 500.f);
    if (ImGui::Button("Aggiungi wave al player", ImVec2(-1, 0))) {
        g_gameKit.addWave(g_gkWaveName, g_gkWaveCount, g_gkWaveInterval,
                          g_playerPos.x, g_playerPos.y + 1.f, g_playerPos.z, g_gkWaveHp);
    }
    ImGui::Text("Wave index: %d / spawned: %d", g_gameKit.waveIndex, g_gameKit.waveSpawned);
    ImGui::Separator();
    for (int i = 0; i < (int)g_gameKit.waves.size(); ++i) {
        const auto& w = g_gameKit.waves[i];
        ImGui::BulletText("%s x%d every %.1fs @ (%.0f,%.0f,%.0f) hp=%.0f",
                          w.name.c_str(), w.count, w.interval,
                          w.spawnX, w.spawnY, w.spawnZ, w.hp);
    }
    if (ImGui::Button("Clear waves")) g_gameKit.waves.clear();
    ImGui::End();
}

static void drawGameKitMatchWindow() {
    if (!g_showGameKitMatch) return;
    ImGui::SetNextWindowSize(ImVec2(380, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Match", &g_showGameKitMatch)) {
        ImGui::End();
        return;
    }
    ImGui::DragFloat("Durata (s)", &g_gkMatchDur, 1.f, 10.f, 600.f);
    ImGui::DragFloat("Countdown", &g_gkMatchCd, 0.1f, 0.f, 10.f);
    ImGui::InputInt("Win coins", &g_gameKit.winCoins);
    ImGui::InputInt("Win checkpoint order (-1=Win pad)", &g_gameKit.winCheckpointOrder);
    ImGui::Checkbox("Respawn auto su kill", &g_gameKit.autoRespawnOnKill);
    ImGui::DragFloat("Kill Y (void)", &g_gameKit.killY, 1.f, -500.f, 0.f);
    if (ImGui::Button("Start Match", ImVec2(-1, 0)))
        g_gameKit.startMatch(g_gkMatchDur, g_gkMatchCd);
    if (ImGui::Button("End Win", ImVec2(-1, 0)))
        g_gameKit.endMatch(true);
    if (ImGui::Button("End Lose", ImVec2(-1, 0)))
        g_gameKit.endMatch(false);
    ImGui::Separator();
    ImGui::InputText("Announce", g_gkAnnounceBuf, sizeof(g_gkAnnounceBuf));
    if (ImGui::Button("Invia announce", ImVec2(-1, 0)))
        g_gameKit.pushAnnounce(g_gkAnnounceBuf, 2.5f, 0);
    ImGui::Text("Phase time left: %.1f", g_gameKit.matchTimeLeft);
    ImGui::Text("Countdown: %.1f", g_gameKit.countdownLeft);
    ImGui::Text("Speed mul: %.2f  Jump mul: %.2f  boostTtl: %.1f",
                g_gameKit.playerSpeedMul, g_gameKit.playerJumpMul, g_gameKit.boostTtl);
    ImGui::End();
}

static void drawGameKitPresetsWindow() {
    if (!g_showGameKitPresets) return;
    ImGui::SetNextWindowSize(ImVec2(400, 320), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Preset", &g_showGameKitPresets)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Genera un kit attorno alla posizione player. "
                       "Poi usa «Spawn parti visuali» dal pannello principale.");
    ImGui::Separator();
    if (ImGui::Button("Obby Kit", ImVec2(-1, 36))) {
        g_gameKit.buildObbyKit(g_playerPos.x, g_playerPos.z);
        gkSpawnAllPadVisuals();
    }
    if (ImGui::Button("Arena Kit + Match", ImVec2(-1, 36))) {
        g_gameKit.buildArenaKit(g_playerPos.x, g_playerPos.z);
        gkSpawnAllPadVisuals();
    }
    if (ImGui::Button("Tycoon Pads", ImVec2(-1, 36))) {
        g_gameKit.buildTycoonPads(g_playerPos.x, g_playerPos.z);
        gkSpawnAllPadVisuals();
    }
    ImGui::Separator();
    ImGui::TextDisabled("Suggerimento script:");
    ImGui::TextWrapped("GameKit:BuildObbyKit(0,0)\n"
                       "GameKit:StartMatch(120, 3)\n"
                       "print(GameKit:GetStats().Coins)");
    ImGui::End();
}

static void drawGameKitQuickPlaceWindow() {
    if (!g_showGameKitQuickPlace) return;
    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Quick Place", &g_showGameKitQuickPlace)) {
        ImGui::End();
        return;
    }
    ImGui::Text("Click = piazza al player");
    for (int i = 0; i < 9; ++i) {
        if (ImGui::Button(gkPadKindLabels[i], ImVec2(-1, 0))) {
            g_gkPadKind = i;
            std::snprintf(g_gkPadName, sizeof(g_gkPadName), "%s%d",
                          gkPadKindLabels[i], g_gameKit.padCount() + 1);
            gkPlacePadAtPlayer();
            if (!g_gameKit.pads.empty())
                gkSpawnVisualPartForPad(g_gameKit.pads.back());
        }
    }
    ImGui::End();
}

static void drawGameKitStatsWindow() {
    if (!g_showGameKitStats) return;
    ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Stats", &g_showGameKitStats)) {
        ImGui::End();
        return;
    }
    auto& s = g_gameKit.stats;
    ImGui::Text("Coins: %d", s.coins);
    ImGui::Text("Kills: %d", s.kills);
    ImGui::Text("Deaths: %d", s.deaths);
    ImGui::Text("Checkpoints: %d", s.checkpoints);
    ImGui::Text("Survive: %.1fs", s.surviveTime);
    ImGui::Text("Best: %.1fs", s.bestTime);
    ImGui::Text("Wins: %d", s.wins);
    ImGui::Text("Has key: %s (%s)", g_gameKit.hasKey ? "yes" : "no",
                g_gameKit.heldKey.c_str());
    ImGui::Text("Checkpoint: %s", g_gameKit.hasCheckpoint ? "yes" : "no");
    if (g_gameKit.hasCheckpoint)
        ImGui::Text("CP pos (%.1f, %.1f, %.1f) order=%d",
                    g_gameKit.cpX, g_gameKit.cpY, g_gameKit.cpZ, g_gameKit.lastCheckpoint);
    if (ImGui::Button("Azzera stats")) g_gameKit.stats = {};
    ImGui::End();
}

static void drawGameKitLogWindow() {
    if (!g_showGameKitLog) return;
    ImGui::SetNextWindowSize(ImVec2(460, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Log", &g_showGameKitLog)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear")) g_gameKit.logLines.clear();
    ImGui::BeginChild("gklog", ImVec2(0, 0), true);
    for (const auto& line : g_gameKit.logLines)
        ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::End();
}

static void drawGameKitValidatorWindow() {
    if (!g_showGameKitValidator) return;
    ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Validator", &g_showGameKitValidator)) {
        ImGui::End();
        return;
    }
    int warns = 0, errs = 0;
    auto issue = [&](bool err, const char* msg) {
        if (err) { ImGui::TextColored(ImVec4(1, 0.35f, 0.3f, 1), "[ERR] %s", msg); errs++; }
        else { ImGui::TextColored(ImVec4(1, 0.85f, 0.3f, 1), "[WARN] %s", msg); warns++; }
    };
    if (g_gameKit.pads.empty())
        issue(false, "Nessun pad — aggiungi checkpoint/win per un obby");
    bool hasWin = false, hasCp = false;
    for (const auto& p : g_gameKit.pads) {
        if (p.kind == gamekit::PadKind::Win) hasWin = true;
        if (p.kind == gamekit::PadKind::Checkpoint) hasCp = true;
        if (p.kind == gamekit::PadKind::Teleport
            && p.destX == 0 && p.destY == 0 && p.destZ == 0
            && p.x != 0)
            issue(false, (std::string("Teleport '") + p.name + "' dest forse non settata").c_str());
        if (p.hx < 0.3f || p.hz < 0.3f)
            issue(false, (std::string("Pad '") + p.name + "' molto piccolo").c_str());
    }
    if (!hasWin && g_gameKit.winCheckpointOrder < 0)
        issue(false, "Nessun Win pad e winCheckpointOrder < 0");
    if (!hasCp)
        issue(false, "Nessun checkpoint — respawn userà spawn world");
    for (const auto& d : g_gameKit.doors) {
        if (d.requiresKey && d.keyId.empty())
            issue(true, (std::string("Porta '") + d.name + "' richiede chiave senza id").c_str());
    }
    if (g_gameKit.wavesEnabled && g_gameKit.waves.empty())
        issue(false, "Waves abilitate ma lista vuota");
    if (g_gameKit.matchDuration < 15.f)
        issue(false, "Match duration molto corta");
    ImGui::Separator();
    if (errs == 0 && warns == 0)
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1), "OK — nessun problema rilevato");
    else
        ImGui::Text("%d errori, %d warning", errs, warns);
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Play HUD
// ---------------------------------------------------------------------------

static void drawGameKitPlayHud() {
    if (!g_gameKitHudEnabled) return;
    const bool playing = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (!playing) return;

    ImGui::SetNextWindowPos(ImVec2(12, 72), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##GameKitHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
    ImGui::Text("Punti %d", g_gameKit.stats.coins);
    if (g_gameKit.phase == gamekit::MatchPhase::Countdown)
        ImGui::Text("Countdown %.0f", std::ceil(g_gameKit.countdownLeft));
    else if (g_gameKit.phase == gamekit::MatchPhase::Playing && g_gameKit.matchDuration > 0.f)
        ImGui::Text("Tempo %.0f", g_gameKit.matchTimeLeft);
    else if (g_gameKit.phase == gamekit::MatchPhase::Ended)
        ImGui::TextUnformatted("Partita finita");
    if (g_gameKit.hasKey)
        ImGui::Text("Key: %s", g_gameKit.heldKey.c_str());
    if (g_gameKit.playerSpeedMul > 1.01f || g_gameKit.playerJumpMul > 1.01f)
        ImGui::Text("Boost x%.1f / jump x%.1f", g_gameKit.playerSpeedMul, g_gameKit.playerJumpMul);
    ImGui::End();

    // Announcements center
    if (!g_gameKit.announce.empty()) {
        const auto& a = g_gameKit.announce.back();
        ImVec2 ts = ImGui::CalcTextSize(a.text.c_str());
        ImGui::SetNextWindowPos(ImVec2(((float)g_w - ts.x) * 0.5f - 16.f, (float)g_h * 0.22f));
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::Begin("##GameKitAnn", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImVec4 col = a.style == 2 ? ImVec4(0.45f, 1.f, 0.55f, 1.f)
                  : a.style == 1 ? ImVec4(1.f, 0.75f, 0.35f, 1.f)
                                 : ImVec4(0.85f, 0.92f, 1.f, 1.f);
        ImGui::TextColored(col, "%s", a.text.c_str());
        ImGui::End();
    }
}

static void drawGameKitPadDebugMarkers() {
    if (!g_gameKitDrawPads) return;
    if (g_screen != AppScreen::Studio && g_screen != AppScreen::Playing) return;
    // Lightweight: list in a corner overlay when in Studio edit
    if (g_screen == AppScreen::Studio && !g_studioTesting && g_showGameKitPanel) {
        ImGui::SetNextWindowPos(ImVec2((float)g_w - 220.f, 100.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.4f);
        if (ImGui::Begin("##gkPadsDbg", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextDisabled("Pads (%d)", g_gameKit.padCount());
            for (const auto& p : g_gameKit.pads) {
                ImGui::BulletText("%s", p.name.c_str());
            }
        }
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// Menus / ribbon / overlays entry
// ---------------------------------------------------------------------------

static void drawGameKitViewMenuExtra(); // defined later with extras

static void drawGameKitViewMenu() {
    ImGui::Separator();
    ImGui::TextDisabled("Game Kit");
    ImGui::MenuItem("Game Kit…", nullptr, &g_showGameKitPanel);
    ImGui::MenuItem("Pads…", nullptr, &g_showGameKitPads);
    ImGui::MenuItem("Porte…", nullptr, &g_showGameKitDoors);
    ImGui::MenuItem("Waves…", nullptr, &g_showGameKitWaves);
    ImGui::MenuItem("Match…", nullptr, &g_showGameKitMatch);
    ImGui::MenuItem("Preset…", nullptr, &g_showGameKitPresets);
    ImGui::MenuItem("Quick Place…", nullptr, &g_showGameKitQuickPlace);
    ImGui::MenuItem("Stats…", nullptr, &g_showGameKitStats);
    ImGui::MenuItem("Validator…", nullptr, &g_showGameKitValidator);
    ImGui::MenuItem("Log…", nullptr, &g_showGameKitLog);
    ImGui::MenuItem("HUD partita", nullptr, &g_gameKitHudEnabled);
    drawGameKitViewMenuExtra();
}

static void drawGameKitRibbonExtras() {
    if (studio::ribbonBigButton("gk", studio::Icon::Play, "Kit", g_showGameKitPanel))
        g_showGameKitPanel = !g_showGameKitPanel;
    if (studio::ribbonBigButton("gkp", studio::Icon::Cube, "Pads", g_showGameKitPads))
        g_showGameKitPads = !g_showGameKitPads;
    if (studio::ribbonBigButton("gkq", studio::Icon::NewFile, "Place", g_showGameKitQuickPlace))
        g_showGameKitQuickPlace = !g_showGameKitQuickPlace;
}

static void drawGameKitOverlaysExtra(); // defined later
static void drawGameKitOverlays() {
    drawGameKitMainWindow();
    drawGameKitPadsWindow();
    drawGameKitDoorsWindow();
    drawGameKitWavesWindow();
    drawGameKitMatchWindow();
    drawGameKitPresetsWindow();
    drawGameKitQuickPlaceWindow();
    drawGameKitStatsWindow();
    drawGameKitLogWindow();
    drawGameKitValidatorWindow();
    drawGameKitPlayHud();
    drawGameKitPadDebugMarkers();
    drawGameKitOverlaysExtra();
}

static void tickGameKitFrame(float dt) {
    if (!g_gameKitAutoTick) return;
    const bool playing = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (!playing) {
        // still decay announces in edit? skip
        return;
    }
    gamekit::GameKit::TickIn in;
    in.px = g_playerPos.x;
    in.py = g_playerPos.y;
    in.pz = g_playerPos.z;
    in.health = &g_player.humanoid.health;
    in.maxHealth = g_player.humanoid.maxHealth;
    in.grounded = g_player.humanoid.grounded;
    gamekit::GameKit::TickOut out;
    g_gameKit.update(dt, in, out);
    g_playerPos.x = in.px;
    g_playerPos.y = in.py;
    g_playerPos.z = in.pz;
    if (out.teleport) {
        g_playerPos = {out.tx, out.ty, out.tz};
        g_moveVel = {0, 0, 0};
    }
    // Apply speed/jump multipliers via scriptControls overrides when boosted
    if (g_gameKit.playerSpeedMul > 1.01f) {
        float base = g_player.humanoid.walkSpeed;
        if (base < 4.f) base = 16.f;
        scriptControls().walkSpeedOverride = base * g_gameKit.playerSpeedMul;
    } else if (scriptControls().walkSpeedOverride > 0.f
               && g_gameKit.boostTtl <= 0.f) {
        // don't clear unrelated overrides aggressively — only if we set boost
    }
    if (g_gameKit.playerJumpMul > 1.01f) {
        float jp = g_player.humanoid.jumpPower;
        if (jp < 1.f) jp = 50.f;
        scriptControls().jumpPowerOverride = jp * g_gameKit.playerJumpMul;
    }
    if (g_gameKit.boostTtl <= 0.f && g_gameKit.playerSpeedMul <= 1.01f) {
        // leave overrides; creator may have set them
    }
    // Spawn mobs from WAVE_SPAWN log lines (consume new ones)
    static size_t gkLogCursor = 0;
    if (gkLogCursor > g_gameKit.logLines.size()) gkLogCursor = 0;
    while (gkLogCursor < g_gameKit.logLines.size()) {
        const std::string& line = g_gameKit.logLines[gkLogCursor++];
        if (line.compare(0, 11, "WAVE_SPAWN|") != 0) continue;
        // WAVE_SPAWN|name|n|x|y|z|hp
        float sx = 0, sy = 2, sz = 0, hp = 50;
        int n = 0;
        char wname[64] = {};
        if (std::sscanf(line.c_str(), "WAVE_SPAWN|%63[^|]|%d|%f|%f|%f|%f",
                        wname, &n, &sx, &sy, &sz, &hp) >= 5) {
            // Mobs API: try spawn if available
            g_mobs.spawn("zombie", sx, sy, sz);
            (void)n;
            (void)wname;
            (void)hp;
        }
    }
}

// ===========================================================================
// Extra creator tools (~700 linee): snippet Lua, libreria pad, batch, export,
// shop soft-currency, hotkeys, round presets, mirror pads, grid stamp
// ===========================================================================

static bool g_showGameKitSnippets = false;
static bool g_showGameKitLibrary = false;
static bool g_showGameKitBatch = false;
static bool g_showGameKitExport = false;
static bool g_showGameKitShop = false;
static bool g_showGameKitHotkeys = false;
static bool g_showGameKitRoundPresets = false;
static bool g_showGameKitMirror = false;
static bool g_showGameKitGridStamp = false;
static bool g_showGameKitColorLegend = false;

static char g_gkExportBuf[4096] = {};
static char g_gkBatchPrefix[32] = "Pad";
static float g_gkMirrorAxis = 0.f; // X mirror plane
static int g_gkGridCount = 5;
static float g_gkGridSpacing = 8.f;
static int g_gkGridKind = 0;
static int g_gkShopCost = 10;
static char g_gkShopItem[64] = "SpeedBoost";

struct GkSnippet {
    const char* title;
    const char* code;
};

static const GkSnippet kGkSnippets[] = {
    {"Obby base",
     "GameKit:Reset()\n"
     "GameKit:BuildObbyKit(0, 0)\n"
     "GameKit:StartMatch(180, 3)\n"
     "print(\"Obby ready\", GameKit:PadCount())\n"},
    {"Listen pads",
     "RunService.Heartbeat:Connect(function()\n"
     "  for _, n in GameKit:GetTriggeredPads() do\n"
     "    print(\"pad\", n)\n"
     "  end\n"
     "end)\n"},
    {"Coins HUD",
     "RunService.Heartbeat:Connect(function()\n"
     "  local s = GameKit:GetStats()\n"
     "  -- usa StarterGui:SetLabelText se hai un label Coins\n"
     "  print(\"coins\", s.Coins)\n"
     "end)\n"},
    {"Arena waves",
     "GameKit:BuildArenaKit(0, 0)\n"
     "GameKit:EnableWaves(true)\n"
     "GameKit:Announce(\"Survive!\")\n"},
    {"Door + key",
     "GameKit:AddDoor(\"Gate\", 10, 2, 0, true, \"gold\")\n"
     "GameKit:GiveKey(\"gold\")\n"
     "GameKit:OpenDoor(\"Gate\")\n"},
    {"Win on CP3",
     "GameKit:SetWinCheckpoint(3)\n"
     "GameKit:AddPad(\"CP3\", \"Checkpoint\", 40, 1, 0, 2, 1, 2, 1, 3)\n"},
    {"Match timer",
     "GameKit:StartMatch(90, 5)\n"
     "RunService.Heartbeat:Connect(function()\n"
     "  if GameKit:GetPhase() == \"Ended\" then\n"
     "    print(\"fine\", GameKit:GetStats().Wins)\n"
     "  end\n"
     "end)\n"},
    {"Teleport pad",
     "GameKit:AddPad(\"TP\", \"Teleport\", 0, 1, 0, 2, 1, 2)\n"
     "GameKit:SetTeleportDest(\"TP\", 50, 2, 0)\n"},
};

static void drawGameKitSnippetsWindow() {
    if (!g_showGameKitSnippets) return;
    ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Snippet Lua", &g_showGameKitSnippets)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Copia negli script ServerScriptService / LocalScript.");
    for (int si = 0; si < (int)(sizeof(kGkSnippets) / sizeof(kGkSnippets[0])); ++si) {
        const auto& sn = kGkSnippets[si];
        ImGui::PushID(si);
        if (ImGui::CollapsingHeader(sn.title)) {
            ImGui::BeginChild("code", ImVec2(-1, 120), true);
            ImGui::TextUnformatted(sn.code);
            ImGui::EndChild();
        }
        ImGui::PopID();
    }
    ImGui::End();
}

static void drawGameKitLibraryWindow() {
    if (!g_showGameKitLibrary) return;
    ImGui::SetNextWindowSize(ImVec2(380, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Libreria pad", &g_showGameKitLibrary)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Template rapidi (piazza al player + parte colorata).");
    struct LibItem { const char* label; int kind; float str; float hx, hy, hz; };
    static const LibItem items[] = {
        {"Checkpoint verde", 0, 1.f, 2, 1, 2},
        {"Lava kill", 1, 1.f, 4, 0.6f, 4},
        {"Spike damage 20", 2, 20.f, 2, 1, 2},
        {"Speed x1.8", 3, 1.8f, 2, 1, 2},
        {"Jump x1.8", 4, 1.8f, 2, 1, 2},
        {"Coin +5", 5, 5.f, 1.5f, 1, 1.5f},
        {"Heal +30", 7, 30.f, 2, 1, 2},
        {"Finish win", 8, 1.f, 3, 1, 3},
    };
    for (const auto& it : items) {
        if (ImGui::Button(it.label, ImVec2(-1, 0))) {
            g_gkPadKind = it.kind;
            g_gkPadStrength = it.str;
            g_gkPadSize[0] = it.hx; g_gkPadSize[1] = it.hy; g_gkPadSize[2] = it.hz;
            std::snprintf(g_gkPadName, sizeof(g_gkPadName), "%s%d",
                          gkPadKindLabels[it.kind], g_gameKit.padCount() + 1);
            gkPlacePadAtPlayer();
            if (!g_gameKit.pads.empty())
                gkSpawnVisualPartForPad(g_gameKit.pads.back());
        }
    }
    ImGui::End();
}

static void drawGameKitBatchWindow() {
    if (!g_showGameKitBatch) return;
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Batch", &g_showGameKitBatch)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Prefisso rename", g_gkBatchPrefix, sizeof(g_gkBatchPrefix));
    if (ImGui::Button("Rinomina tutti i pad col prefisso", ImVec2(-1, 0))) {
        for (int i = 0; i < (int)g_gameKit.pads.size(); ++i) {
            g_gameKit.pads[i].name = std::string(g_gkBatchPrefix) + std::to_string(i + 1);
        }
        g_gameKit.log("Batch rename pads");
    }
    if (ImGui::Button("Disabilita tutti i Kill pad", ImVec2(-1, 0))) {
        for (auto& p : g_gameKit.pads)
            if (p.kind == gamekit::PadKind::Kill) p.enabled = false;
    }
    if (ImGui::Button("Abilita tutti i pad", ImVec2(-1, 0))) {
        for (auto& p : g_gameKit.pads) p.enabled = true;
    }
    if (ImGui::Button("Scala strength x1.5", ImVec2(-1, 0))) {
        for (auto& p : g_gameKit.pads) p.strength *= 1.5f;
    }
    if (ImGui::Button("Offset Y +2 tutti", ImVec2(-1, 0))) {
        for (auto& p : g_gameKit.pads) p.y += 2.f;
    }
    if (ImGui::Button("Elimina pad disabilitati", ImVec2(-1, 0))) {
        auto& v = g_gameKit.pads;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [](const gamekit::Pad& p) { return !p.enabled; }),
                v.end());
    }
    if (ImGui::Button("Duplica pad selezionato +8 X", ImVec2(-1, 0))) {
        if (g_gkSelectedPad >= 0 && g_gkSelectedPad < (int)g_gameKit.pads.size()) {
            gamekit::Pad p = g_gameKit.pads[g_gkSelectedPad];
            p.name += "_copy";
            p.x += 8.f;
            p.inside = false;
            if ((int)g_gameKit.pads.size() < gamekit::GameKit::kMaxPads)
                g_gameKit.pads.push_back(p);
        }
    }
    ImGui::End();
}

static void gkRebuildExportBuffer() {
    std::string o = "{\n  \"pads\": [\n";
    for (size_t i = 0; i < g_gameKit.pads.size(); ++i) {
        const auto& p = g_gameKit.pads[i];
        char line[256];
        std::snprintf(line, sizeof(line),
            "    {\"name\":\"%s\",\"kind\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
            "\"hx\":%.2f,\"hy\":%.2f,\"hz\":%.2f,\"strength\":%.2f,\"order\":%d}%s\n",
            p.name.c_str(), gamekit::GameKit::padKindName(p.kind),
            p.x, p.y, p.z, p.hx, p.hy, p.hz, p.strength, p.order,
            (i + 1 < g_gameKit.pads.size()) ? "," : "");
        o += line;
    }
    o += "  ],\n  \"doors\": [\n";
    for (size_t i = 0; i < g_gameKit.doors.size(); ++i) {
        const auto& d = g_gameKit.doors[i];
        char line[192];
        std::snprintf(line, sizeof(line),
            "    {\"name\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"key\":%s,\"keyId\":\"%s\"}%s\n",
            d.name.c_str(), d.x, d.y, d.z, d.requiresKey ? "true" : "false",
            d.keyId.c_str(), (i + 1 < g_gameKit.doors.size()) ? "," : "");
        o += line;
    }
    o += "  ],\n  \"stats\": {\"coins\":";
    o += std::to_string(g_gameKit.stats.coins);
    o += ",\"wins\":";
    o += std::to_string(g_gameKit.stats.wins);
    o += "}\n}\n";
    if (o.size() >= sizeof(g_gkExportBuf))
        o.resize(sizeof(g_gkExportBuf) - 1);
    std::memcpy(g_gkExportBuf, o.c_str(), o.size() + 1);
}

static void drawGameKitExportWindow() {
    if (!g_showGameKitExport) return;
    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Export JSON", &g_showGameKitExport)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Rigenera", ImVec2(-1, 0)))
        gkRebuildExportBuffer();
    ImGui::InputTextMultiline("##exp", g_gkExportBuf, sizeof(g_gkExportBuf),
                              ImVec2(-1, -30), ImGuiInputTextFlags_ReadOnly);
    ImGui::TextDisabled("Copia manualmente — utile per backup kit / condividere layout");
    ImGui::End();
}

static void drawGameKitShopWindow() {
    if (!g_showGameKitShop) return;
    ImGui::SetNextWindowSize(ImVec2(360, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Shop (soft)", &g_showGameKitShop)) {
        ImGui::End();
        return;
    }
    ImGui::Text("Coins disponibili: %d", g_gameKit.stats.coins);
    ImGui::InputText("Item", g_gkShopItem, sizeof(g_gkShopItem));
    ImGui::InputInt("Costo", &g_gkShopCost);
    if (ImGui::Button("Compra SpeedBoost (costo)", ImVec2(-1, 0))) {
        if (g_gameKit.stats.coins >= g_gkShopCost) {
            g_gameKit.stats.coins -= g_gkShopCost;
            g_gameKit.playerSpeedMul = 1.6f;
            g_gameKit.boostTtl = 8.f;
            g_gameKit.pushAnnounce(std::string("Acquistato ") + g_gkShopItem, 2.f, 2);
        } else {
            g_gameKit.pushAnnounce("Monete insufficienti", 2.f, 1);
        }
    }
    if (ImGui::Button("Compra JumpBoost", ImVec2(-1, 0))) {
        if (g_gameKit.stats.coins >= g_gkShopCost) {
            g_gameKit.stats.coins -= g_gkShopCost;
            g_gameKit.playerJumpMul = 1.6f;
            g_gameKit.boostTtl = 8.f;
            g_gameKit.pushAnnounce("Jump boost acquistato", 2.f, 2);
        } else {
            g_gameKit.pushAnnounce("Monete insufficienti", 2.f, 1);
        }
    }
    if (ImGui::Button("Compra Heal full", ImVec2(-1, 0))) {
        if (g_gameKit.stats.coins >= g_gkShopCost) {
            g_gameKit.stats.coins -= g_gkShopCost;
            g_player.humanoid.health = g_player.humanoid.maxHealth;
            g_gameKit.pushAnnounce("Cura completa", 2.f, 2);
        } else {
            g_gameKit.pushAnnounce("Monete insufficienti", 2.f, 1);
        }
    }
    if (ImGui::Button("+50 coins (debug)", ImVec2(-1, 0)))
        g_gameKit.stats.coins += 50;
    ImGui::End();
}

static void drawGameKitHotkeysWindow() {
    if (!g_showGameKitHotkeys) return;
    ImGui::SetNextWindowSize(ImVec2(380, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Hotkeys", &g_showGameKitHotkeys)) {
        ImGui::End();
        return;
    }
    ImGui::BulletText("View → Game Kit… — pannello principale");
    ImGui::BulletText("Ribbon Kit — apre/chiude pannello");
    ImGui::BulletText("Ribbon Pads — editor pad");
    ImGui::BulletText("Ribbon Place — quick place tipi");
    ImGui::BulletText("In Play: tocca i pad per attivare effetti");
    ImGui::BulletText("Void sotto Kill Y → respawn checkpoint");
    ImGui::BulletText("Luau: game.GameKit o globale GameKit");
    ImGui::Separator();
    ImGui::TextWrapped("Preset Obby/Arena/Tycoon creano pad + parti colorate "
                       "per vedere subito il layout nel viewport.");
    ImGui::End();
}

static void drawGameKitRoundPresetsWindow() {
    if (!g_showGameKitRoundPresets) return;
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Round presets", &g_showGameKitRoundPresets)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Blitz 60s", ImVec2(-1, 0)))
        g_gameKit.startMatch(60.f, 2.f);
    if (ImGui::Button("Standard 3 min", ImVec2(-1, 0)))
        g_gameKit.startMatch(180.f, 3.f);
    if (ImGui::Button("Maratona 10 min", ImVec2(-1, 0)))
        g_gameKit.startMatch(600.f, 5.f);
    if (ImGui::Button("Endless (no timer)", ImVec2(-1, 0))) {
        g_gameKit.startMatch(99999.f, 1.f);
        g_gameKit.matchDuration = 0.f; // update skips timer when 0? check - actually Playing uses matchDuration > 0
        g_gameKit.matchDuration = 0.f;
        g_gameKit.matchTimeLeft = 0.f;
    }
    ImGui::TextDisabled("Endless: duration=0 disattiva timeout in update");
    ImGui::End();
}

static void drawGameKitMirrorWindow() {
    if (!g_showGameKitMirror) return;
    ImGui::SetNextWindowSize(ImVec2(340, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Mirror pads", &g_showGameKitMirror)) {
        ImGui::End();
        return;
    }
    ImGui::DragFloat("Piano X (mirror)", &g_gkMirrorAxis, 0.5f);
    if (ImGui::Button("Mirror tutti i pad su X", ImVec2(-1, 0))) {
        const int n = (int)g_gameKit.pads.size();
        for (int i = 0; i < n && (int)g_gameKit.pads.size() < gamekit::GameKit::kMaxPads; ++i) {
            gamekit::Pad p = g_gameKit.pads[i];
            p.name += "_m";
            p.x = g_gkMirrorAxis * 2.f - p.x;
            p.destX = g_gkMirrorAxis * 2.f - p.destX;
            p.inside = false;
            g_gameKit.pads.push_back(p);
        }
        g_gameKit.log("Mirror pads X");
    }
    if (ImGui::Button("Mirror parti GK_ su X (approx)", ImVec2(-1, 0))) {
        const int n = (int)g_world.parts.size();
        for (int i = 0; i < n; ++i) {
            if (g_world.parts[i].name.compare(0, 3, "GK_") != 0) continue;
            WorkspacePart q = g_world.parts[i];
            q.name += "_m";
            q.position.x = g_gkMirrorAxis * 2.f - q.position.x;
            g_world.parts.push_back(q);
        }
    }
    ImGui::End();
}

static void drawGameKitGridStampWindow() {
    if (!g_showGameKitGridStamp) return;
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Grid stamp", &g_showGameKitGridStamp)) {
        ImGui::End();
        return;
    }
    ImGui::Combo("Tipo", &g_gkGridKind, gkPadKindLabels, 9);
    ImGui::InputInt("Count", &g_gkGridCount);
    ImGui::DragFloat("Spacing", &g_gkGridSpacing, 0.25f, 1.f, 40.f);
    if (ImGui::Button("Stamp linea +X dal player", ImVec2(-1, 0))) {
        int c = std::clamp(g_gkGridCount, 1, 40);
        for (int i = 0; i < c; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "Grid%d_%d", g_gkGridKind, g_gameKit.padCount() + 1);
            g_gameKit.addPad(name, gkKindFromUi(g_gkGridKind),
                             g_playerPos.x + i * g_gkGridSpacing,
                             g_playerPos.y, g_playerPos.z,
                             2.f, 1.f, 2.f, g_gkPadStrength, i);
            if (!g_gameKit.pads.empty())
                gkSpawnVisualPartForPad(g_gameKit.pads.back());
        }
    }
    if (ImGui::Button("Stamp griglia 3x3", ImVec2(-1, 0))) {
        for (int z = -1; z <= 1; ++z)
            for (int x = -1; x <= 1; ++x) {
                char name[64];
                std::snprintf(name, sizeof(name), "G%d_%d", x, z);
                g_gameKit.addPad(name, gkKindFromUi(g_gkGridKind),
                                 g_playerPos.x + x * g_gkGridSpacing,
                                 g_playerPos.y,
                                 g_playerPos.z + z * g_gkGridSpacing,
                                 2.f, 1.f, 2.f, g_gkPadStrength, 0);
                if (!g_gameKit.pads.empty())
                    gkSpawnVisualPartForPad(g_gameKit.pads.back());
            }
    }
    ImGui::End();
}

static void drawGameKitColorLegendWindow() {
    if (!g_showGameKitColorLegend) return;
    ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Kit — Legenda colori", &g_showGameKitColorLegend)) {
        ImGui::End();
        return;
    }
    auto row = [](const char* name, ImVec4 c) {
        ImGui::ColorButton(name, c, ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
        ImGui::SameLine();
        ImGui::TextUnformatted(name);
    };
    row("Checkpoint", ImVec4(0.2f, 0.85f, 0.35f, 1));
    row("Kill", ImVec4(0.9f, 0.15f, 0.1f, 1));
    row("Damage", ImVec4(0.95f, 0.45f, 0.1f, 1));
    row("Speed", ImVec4(0.2f, 0.6f, 1.f, 1));
    row("Jump", ImVec4(0.7f, 0.4f, 1.f, 1));
    row("Coin", ImVec4(1.f, 0.85f, 0.2f, 1));
    row("Teleport", ImVec4(0.3f, 0.9f, 0.9f, 1));
    row("Heal", ImVec4(0.4f, 1.f, 0.7f, 1));
    row("Win", ImVec4(1.f, 0.95f, 0.4f, 1));
    ImGui::End();
}

// Patch menus/overlays to include extras
static void drawGameKitViewMenuExtra() {
    ImGui::MenuItem("Snippet Lua…", nullptr, &g_showGameKitSnippets);
    ImGui::MenuItem("Libreria pad…", nullptr, &g_showGameKitLibrary);
    ImGui::MenuItem("Batch…", nullptr, &g_showGameKitBatch);
    ImGui::MenuItem("Export JSON…", nullptr, &g_showGameKitExport);
    ImGui::MenuItem("Shop soft…", nullptr, &g_showGameKitShop);
    ImGui::MenuItem("Hotkeys…", nullptr, &g_showGameKitHotkeys);
    ImGui::MenuItem("Round presets…", nullptr, &g_showGameKitRoundPresets);
    ImGui::MenuItem("Mirror…", nullptr, &g_showGameKitMirror);
    ImGui::MenuItem("Grid stamp…", nullptr, &g_showGameKitGridStamp);
    ImGui::MenuItem("Legenda colori…", nullptr, &g_showGameKitColorLegend);
}

static void drawGameKitOverlaysExtra() {
    drawGameKitSnippetsWindow();
    drawGameKitLibraryWindow();
    drawGameKitBatchWindow();
    drawGameKitExportWindow();
    drawGameKitShopWindow();
    drawGameKitHotkeysWindow();
    drawGameKitRoundPresetsWindow();
    drawGameKitMirrorWindow();
    drawGameKitGridStampWindow();
    drawGameKitColorLegendWindow();
}
