#pragma once

/**
 * Studio 2020 parity pack — Terrain Editor, Collision Groups, Find/Replace,
 * Welds, Path debug, Atmosphere UI, Frame/ImageLabel, PrimaryPart.
 * Incluso solo dal path main.cpp (come StudioFeaturesUi.h).
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Stato
// ---------------------------------------------------------------------------

static bool g_showCollisionGroups = false;
static bool g_showTerrainEditor = false;
static bool g_showFindReplace = false;
static bool g_showWeldsWindow = false;
static bool g_showPathDebug = false;
static bool g_showAtmospherePanel = false;
// g_terrainToolActive / brush mat/radius/erase: SelectionAndInput.h (early).
static int  g_terrainGenSeed = 42;
static int  g_terrainGenRadius = 32;
static int  g_terrainGenMaxH = 28;

static char g_findReplaceQuery[128] = {};
static char g_findReplaceWith[128] = {};
static int  g_findReplaceScope = 0; // 0 names · 1 scripts · 2 both
static std::vector<std::string> g_findReplaceHits;

static std::vector<glm::vec3> g_pathDebugPts;
static char g_collisionGroupNames[bphys::kCollisionGroups][24] = {};

static void studio2020InitOnce() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int i = 0; i < bphys::kCollisionGroups; ++i)
        std::snprintf(g_collisionGroupNames[i], sizeof(g_collisionGroupNames[i]),
                      "Group%d", i);
    std::snprintf(g_collisionGroupNames[0], sizeof(g_collisionGroupNames[0]), "Default");
}

// ---------------------------------------------------------------------------
// Collision Group Editor
// ---------------------------------------------------------------------------

static void drawCollisionGroupsWindow() {
    if (!g_showCollisionGroups) return;
    studio2020InitOnce();
    ImGui::SetNextWindowSize({520.f, 420.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Collision Groups (Studio)", &g_showCollisionGroups)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped(
        "Come PhysicsService di Roblox Studio 2020: gruppi 0–31. "
        "Parti con CollisionGroup diversi non collidono se la cella e' OFF.");
    ImGui::Separator();

    if (partIndexValid(g_selectedPart)) {
        auto& p = g_world.parts[g_selectedPart];
        int g = std::clamp((int)std::lround(p.collisionGroup), 0, bphys::kCollisionGroups - 1);
        ImGui::Text("Selezione: %s →", p.name.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##selgrp", g_collisionGroupNames[g])) {
            for (int i = 0; i < bphys::kCollisionGroups; ++i) {
                if (ImGui::Selectable(g_collisionGroupNames[i], i == g)) {
                    p.collisionGroup = (float)i;
                    markPlaceDirty();
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("Seleziona una Part per assegnare il gruppo");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Matrice collisione (clic = toggle). Diagonale = stesso gruppo.");
    const float cell = 18.f;
    const int show = 12; // UI: primi 12 gruppi (resto via Lua)
    ImGui::BeginChild("##cgmatrix", {0, 280}, true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Dummy({cell * 1.5f, 0});
    for (int j = 0; j < show; ++j) {
        ImGui::SameLine(0, 2);
        ImGui::TextDisabled("%d", j);
    }
    for (int i = 0; i < show; ++i) {
        ImGui::Text("%2d", i);
        for (int j = 0; j < show; ++j) {
            ImGui::SameLine(0, 2);
            ImGui::PushID(i * 100 + j);
            bool on = g_bphys.groupsCollidable(i, j);
            ImVec4 col = on ? ImVec4(0.25f, 0.75f, 0.35f, 1.f)
                            : ImVec4(0.55f, 0.2f, 0.2f, 1.f);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            if (ImGui::Button("##c", {cell, cell})) {
                g_bphys.setGroupsCollidable(i, j, !on);
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s × %s = %s",
                    g_collisionGroupNames[i], g_collisionGroupNames[j],
                    on ? "collide" : "no collide");
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (ImGui::Button("Reset: tutto collides")) {
        for (int i = 0; i < bphys::kCollisionGroups; ++i)
            for (int j = 0; j < bphys::kCollisionGroups; ++j)
                g_bphys.setGroupsCollidable(i, j, true);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Terrain Editor (brush + generate)
// ---------------------------------------------------------------------------

static void studioTerrainPaintAtMouse(float mx, float my, bool erase) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    Terrain::Hit h = g_terrain.raycast(o, d, 400.f);
    int cx, cy, cz;
    if (h.ok) {
        cx = h.x; cy = h.y; cz = h.z;
        if (!erase) {
            // Place on face normal (adjacent cell)
            cx += h.nx; cy += h.ny; cz += h.nz;
        }
    } else {
        // Fall onto y=0 plane
        if (std::fabs(d.y) < 1e-4f) return;
        float t = -o.y / d.y;
        if (t < 0.f) return;
        glm::vec3 p = o + d * t;
        auto cell = g_terrain.worldToCell(p);
        cx = cell.x; cy = std::max(0, cell.y); cz = cell.z;
    }
    const int r = std::max(0, g_terrainBrushRadius);
    uint8_t id = erase ? (uint8_t)Terrain::Air
                       : (uint8_t)std::clamp(g_terrainBrushMat, 1, (int)Terrain::Count - 1);
    int n = 0;
    for (int dy = -r; dy <= r; ++dy)
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy + dz * dz > r * r + r) continue;
                if (g_terrainBrushFlat && dy != 0) continue;
                if (g_terrain.set(cx + dx, cy + dy, cz + dz, id)) ++n;
            }
    if (n > 0) {
        markPlaceDirty();
        g_exportMsg = erase ? ("Terrain erase ×" + std::to_string(n))
                            : ("Terrain paint ×" + std::to_string(n));
    }
}

static void drawTerrainEditorWindow() {
    if (!g_showTerrainEditor) return;
    ImGui::SetNextWindowSize({360.f, 400.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain Editor", &g_showTerrainEditor)) {
        ImGui::End();
        return;
    }
    ImGui::TextColored({0.45f, 0.85f, 0.65f, 1.f}, "Come Terrain Editor di Studio 2020");
    ImGui::TextDisabled("Voxel: %d · chunks: %d", g_terrain.blockCount(), g_terrain.chunkCount());
    ImGui::Separator();

    ImGui::Checkbox("Brush attivo (click viewport)", &g_terrainToolActive);
    ImGui::Checkbox("Gomma (erase)", &g_terrainBrushErase);
    ImGui::Checkbox("Solo piano (niente su e giu)", &g_terrainBrushFlat);
    ImGui::SliderInt("Raggio cell", &g_terrainBrushRadius, 0, 5);
    ImGui::Text("Materiale:");
    const char* mats[] = {
        "Air","Grass","Ground","Rock","Sand","Wood","LeafyGrass","Water",
        "Snow","Cobblestone","Bedrock","Coal","Iron","WoodPlanks","Glass","Brick","CrackedLava"
    };
    int matCount = (int)(sizeof(mats) / sizeof(mats[0]));
    if (g_terrainBrushMat < 1) g_terrainBrushMat = 1;
    if (g_terrainBrushMat >= matCount) g_terrainBrushMat = 1;
    ImGui::Combo("##tmat", &g_terrainBrushMat, mats, matCount);

    ImGui::Separator();
    ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Generate");
    ImGui::InputInt("Seed", &g_terrainGenSeed);
    ImGui::SliderInt("Radius", &g_terrainGenRadius, 8, 64);
    ImGui::SliderInt("Max height", &g_terrainGenMaxH, 8, 64);
    if (ImGui::Button("Generate world", {-1, 28})) {
        float spawnY = g_terrain.generate(g_terrainGenSeed, g_terrainGenRadius, g_terrainGenMaxH);
        g_world.spawn.position = {0.f, spawnY + 2.f, 0.f};
        markPlaceDirty();
        g_exportMsg = "Terrain generate seed=" + std::to_string(g_terrainGenSeed);
    }
    if (ImGui::Button("Clear Terrain", {-1, 0})) {
        g_terrain.reset();
        markPlaceDirty();
    }
    ImGui::Separator();
    ImGui::TextWrapped(
        "Gli stessi voxel di Terrain:FillBlock e FillBall. "
        "Si salvano col posto e al Play restano. Stop annulla solo quello che ha fatto lo script.");
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Find / Replace in Place
// ---------------------------------------------------------------------------

static void studioFindInPlace() {
    g_findReplaceHits.clear();
    if (!g_findReplaceQuery[0]) return;
    std::string q = g_findReplaceQuery;
    for (char& c : q) c = (char)std::tolower((unsigned char)c);
    auto hit = [&](const std::string& where, const std::string& hay) {
        std::string h = hay;
        for (char& c : h) c = (char)std::tolower((unsigned char)c);
        if (h.find(q) == std::string::npos) return;
        g_findReplaceHits.push_back(where + ": " + hay.substr(0, 80));
    };
    const bool names = (g_findReplaceScope == 0 || g_findReplaceScope == 2);
    const bool scripts = (g_findReplaceScope == 1 || g_findReplaceScope == 2);
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        if (names) hit("Part[" + std::to_string(i) + "].Name", g_world.parts[i].name);
        if (scripts && !g_world.parts[i].script.empty())
            hit("Part[" + std::to_string(i) + "].Script", g_world.parts[i].script);
    }
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (names) hit("Model[" + std::to_string(i) + "].Name", g_world.models[i].name);
        if (scripts && !g_world.models[i].script.empty())
            hit("Model[" + std::to_string(i) + "].Script", g_world.models[i].script);
    }
    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
        if (names) hit("Label[" + std::to_string(i) + "].Name", g_world.labels[i].name);
        if (scripts && !g_world.labels[i].script.empty())
            hit("Label[" + std::to_string(i) + "].Script", g_world.labels[i].script);
    }
}

static void studioReplaceInPlace() {
    if (!g_findReplaceQuery[0]) return;
    const std::string q = g_findReplaceQuery;
    const std::string with = g_findReplaceWith;
    int n = 0;
    auto repl = [&](std::string& s) {
        size_t pos = 0;
        while ((pos = s.find(q, pos)) != std::string::npos) {
            s.replace(pos, q.size(), with);
            pos += with.size();
            ++n;
        }
    };
    const bool names = (g_findReplaceScope == 0 || g_findReplaceScope == 2);
    const bool scripts = (g_findReplaceScope == 1 || g_findReplaceScope == 2);
    for (auto& p : g_world.parts) {
        if (names) repl(p.name);
        if (scripts) repl(p.script);
    }
    for (auto& m : g_world.models) {
        if (names) repl(m.name);
        if (scripts) repl(m.script);
    }
    for (auto& l : g_world.labels) {
        if (names) repl(l.name);
        if (scripts) repl(l.script);
    }
    if (n > 0) markPlaceDirty();
    g_exportMsg = "Replace ×" + std::to_string(n);
    studioFindInPlace();
}

static void drawFindReplaceWindow() {
    if (!g_showFindReplace) return;
    ImGui::SetNextWindowSize({420.f, 360.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Find / Replace in Place", &g_showFindReplace)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Find", g_findReplaceQuery, sizeof(g_findReplaceQuery));
    ImGui::InputText("Replace", g_findReplaceWith, sizeof(g_findReplaceWith));
    ImGui::RadioButton("Names", &g_findReplaceScope, 0); ImGui::SameLine();
    ImGui::RadioButton("Scripts", &g_findReplaceScope, 1); ImGui::SameLine();
    ImGui::RadioButton("Both", &g_findReplaceScope, 2);
    if (ImGui::Button("Find")) studioFindInPlace();
    ImGui::SameLine();
    if (ImGui::Button("Replace all")) studioReplaceInPlace();
    ImGui::Separator();
    ImGui::TextDisabled("%d risultati", (int)g_findReplaceHits.size());
    ImGui::BeginChild("##frhits", {0, -1}, true);
    for (const auto& line : g_findReplaceHits)
        ImGui::TextUnformatted(line.c_str());
    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Welds
// ---------------------------------------------------------------------------

static void studioCreateWeldFromSelection() {
    auto idxs = studioActivePartIndices();
    if (idxs.size() < 2) {
        g_exportMsg = "Weld: seleziona 2+ parti (Ctrl+click)";
        return;
    }
    int a = idxs[0];
    for (size_t i = 1; i < idxs.size(); ++i) {
        g_world.addWeld(a, idxs[i]);
    }
    markPlaceDirty();
    g_exportMsg = "WeldConstraint ×" + std::to_string(idxs.size() - 1);
    g_showWeldsWindow = true;
}

// Hinge/Rope/Spring/Rod fra le 2 part selezionate (la prima e' Part0).
static void studioCreateConstraintFromSelection(ConstraintKind kind) {
    auto idxs = studioActivePartIndices();
    if (idxs.size() != 2) {
        g_exportMsg = std::string(constraintKindName(kind)) + ": seleziona esattamente 2 parti (Ctrl+click)";
        return;
    }
    const int part0 = (idxs[1] == g_selectedPart) ? idxs[0] : idxs[1];   // la principale e' Part1
    if (partconstraints::create(g_world, kind, part0, g_selectedPart) < 0) return;
    markPlaceDirty();
    g_exportMsg = std::string(constraintKindName(kind)) + " creato";
    g_showWeldsWindow = true;
}

static const char* partNameOr(int i) {
    return (i >= 0 && i < (int)g_world.parts.size()) ? g_world.parts[(size_t)i].name.c_str() : "?";
}

// Finestra Constraints: crea, modifica e cancella i vincoli. In Play li
// simula la fisica (BlockyPhysics::syncJoints).
static void drawWeldsWindow() {
    if (!g_showWeldsWindow) return;
    ImGui::SetNextWindowSize({420.f, 420.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Constraints", &g_showWeldsWindow)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Vincoli fra part, simulati in Play. Funzionano su part NON ancorate "
                       "(o una ancorata e una libera: la libera resta appesa).");
    if (ImGui::Button("Weld (2+ part)", {130, 0})) studioCreateWeldFromSelection();
    ImGui::SameLine();
    if (ImGui::Button("Hinge", {80, 0})) studioCreateConstraintFromSelection(ConstraintKind::Hinge);
    ImGui::SameLine();
    if (ImGui::Button("Rope", {80, 0})) studioCreateConstraintFromSelection(ConstraintKind::Rope);
    if (ImGui::Button("Spring", {130, 0})) studioCreateConstraintFromSelection(ConstraintKind::Spring);
    ImGui::SameLine();
    if (ImGui::Button("Rod", {80, 0})) studioCreateConstraintFromSelection(ConstraintKind::Rod);
    ImGui::TextDisabled("Seleziona 2 part con Ctrl+clic: la prima e' Part0.");
    ImGui::Separator();

    static const char* kKinds[] = {"Weld", "Hinge", "Rope", "Spring", "Rod"};
    for (int i = 0; i < (int)g_world.welds.size(); ++i) {
        auto& w = g_world.welds[i];
        ImGui::PushID(i);
        bool changed = ImGui::Checkbox("##en", &w.enabled);
        ImGui::SameLine();
        int kind = (int)w.kind;
        ImGui::SetNextItemWidth(80);
        if (ImGui::Combo("##kind", &kind, kKinds, IM_ARRAYSIZE(kKinds))) {
            w.kind = (ConstraintKind)kind;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string(partNameOr(w.part0)) + "  <->  " + partNameOr(w.part1)).c_str())) {
            studioMultiClear();
            studioMultiAdd(w.part0);
            studioMultiAdd(w.part1);
            selectPart(w.part1);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            g_world.welds.erase(g_world.welds.begin() + i);
            markPlaceDirty();
            ImGui::PopID();
            break;
        }
        ImGui::Indent();
        switch (w.kind) {
        case ConstraintKind::Rope:
        case ConstraintKind::Rod:
            changed |= ImGui::DragFloat("Lunghezza", &w.length, 0.1f, 0.f, 500.f, "%.1f stud");
            break;
        case ConstraintKind::Spring:
            changed |= ImGui::DragFloat("Riposo", &w.length, 0.1f, 0.f, 500.f, "%.1f stud");
            changed |= ImGui::DragFloat("Rigidita'", &w.stiffness, 0.05f, 0.1f, 20.f, "%.2f Hz");
            changed |= ImGui::SliderFloat("Smorzamento", &w.damping, 0.f, 1.f, "%.2f");
            break;
        case ConstraintKind::Hinge:
        case ConstraintKind::Prismatic:
        case ConstraintKind::Cylindrical:
        case ConstraintKind::BallSocket:
            changed |= ImGui::DragFloat3("Perno (locale Part0)", &w.pivot.x, 0.05f);
            changed |= ImGui::DragFloat3("Asse (locale Part0)", &w.axis.x, 0.01f, -1.f, 1.f);
            break;
        case ConstraintKind::AlignPosition:
            changed |= ImGui::DragFloat("Forza", &w.stiffness, 0.1f, 0.1f, 40.f, "%.1f");
            break;
        case ConstraintKind::LinearVelocity:
            changed |= ImGui::DragFloat("Velocita'", &w.length, 0.1f, 0.f, 80.f, "%.1f");
            changed |= ImGui::DragFloat3("Direzione", &w.axis.x, 0.01f, -1.f, 1.f);
            break;
        case ConstraintKind::AngularVelocity:
        case ConstraintKind::VectorForce:
        case ConstraintKind::Torque:
            changed |= ImGui::DragFloat("Forza", &w.stiffness, 0.1f, 0.f, 200.f, "%.1f");
            changed |= ImGui::DragFloat3("Asse", &w.axis.x, 0.01f, -1.f, 1.f);
            break;
        case ConstraintKind::AlignOrientation:
        case ConstraintKind::NoCollision:
        case ConstraintKind::Weld:
        case ConstraintKind::Count:
            break;
        }
        ImGui::Unindent();
        if (changed) markPlaceDirty();
        ImGui::PopID();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Pathfinding debug
// ---------------------------------------------------------------------------

static void studioComputePathDebug() {
    g_pathDebugPts.clear();
    if (!g_nav.ready()) {
        if (g_lua.navRebuild) g_lua.navRebuild();
    }
    glm::vec3 from = g_playerPos;
    glm::vec3 to = g_cam.pos + g_cam.front() * 20.f;
    if (partIndexValid(g_selectedPart))
        to = g_world.parts[g_selectedPart].position;
    g_pathDebugPts = g_nav.findPath(from, to);
    g_showPathDebug = true;
    g_exportMsg = g_pathDebugPts.empty()
        ? "Path: nessuno"
        : ("Path: " + std::to_string(g_pathDebugPts.size()) + " punti");
}

static void drawPathDebugOverlay() {
    if (!g_showPathDebug || g_pathDebugPts.size() < 2) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto project = [&](const glm::vec3& w, ImVec2& out) -> bool {
        glm::vec4 c = g_lastProj * g_lastView * glm::vec4(w, 1.f);
        if (c.w <= 0.05f) return false;
        c /= c.w;
        out.x = (c.x * 0.5f + 0.5f) * (float)g_w;
        out.y = (1.f - (c.y * 0.5f + 0.5f)) * (float)g_h;
        return true;
    };
    ImVec2 prev{};
    bool have = false;
    for (const glm::vec3& p : g_pathDebugPts) {
        ImVec2 s;
        if (!project(p + glm::vec3(0, 0.5f, 0), s)) { have = false; continue; }
        dl->AddCircleFilled(s, 3.5f, IM_COL32(80, 220, 120, 220));
        if (have) dl->AddLine(prev, s, IM_COL32(60, 200, 100, 200), 2.5f);
        prev = s;
        have = true;
    }
}

// ---------------------------------------------------------------------------
// Atmosphere / SunRays panel
// ---------------------------------------------------------------------------

static void drawAtmospherePanel() {
    if (!g_showAtmospherePanel) return;
    ImGui::SetNextWindowSize({340.f, 460.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Atmosphere / Effetti", &g_showAtmospherePanel)) {
        ImGui::End();
        return;
    }
    ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Lighting children (Studio 2020)");
    ImGui::Checkbox("Atmosphere enabled", &g_lighting.atmosphereEnabled);
    ImGui::SliderFloat("Density", &g_lighting.atmosphereDensity, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Haze", &g_lighting.atmosphereHaze, 0.f, 1.f, "%.2f");
    ImGui::Separator();
    ImGui::Checkbox("SunRays enabled", &g_lighting.sunRaysEnabled);
    ImGui::SliderFloat("SunRays Intensity", &g_lighting.sunRaysIntensity, 0.f, 2.f, "%.2f");
    ImGui::SliderFloat("SunRays Spread", &g_lighting.sunRaysSpread, 0.1f, 4.f, "%.2f");
    ImGui::Separator();
    ImGui::Checkbox("ColorCorrection", &g_lighting.ccEnabled);
    if (g_lighting.ccEnabled) {
        ImGui::SliderFloat("Brightness", &g_lighting.ccBrightness, -1.f, 1.f, "%.2f");
        ImGui::SliderFloat("Contrast", &g_lighting.ccContrast, -1.f, 1.f, "%.2f");
        ImGui::SliderFloat("Saturation", &g_lighting.ccSaturation, -1.f, 1.f, "%.2f");
        ImGui::ColorEdit3("Tint", &g_lighting.ccTint.x);
    }
    ImGui::Checkbox("BloomEffect", &g_lighting.bloomFx);
    if (g_lighting.bloomFx) {
        ImGui::SliderFloat("Bloom Intensity", &g_lighting.bloomFxIntensity, 0.f, 4.f, "%.2f");
        ImGui::SliderFloat("Bloom Threshold", &g_lighting.bloomFxThreshold, 0.f, 2.f, "%.2f");
    }
    ImGui::Checkbox("BlurEffect", &g_lighting.blurFx);
    if (g_lighting.blurFx)
        ImGui::SliderFloat("Blur Size", &g_lighting.blurFxSize, 0.f, 56.f, "%.0f");
    ImGui::Checkbox("DepthOfField", &g_lighting.dofFx);
    if (g_lighting.dofFx)
        ImGui::SliderFloat("Far blur", &g_lighting.dofFar, 0.f, 1.f, "%.2f");
    if (ImGui::Button("Spegni effetti")) {
        g_lighting.ccEnabled = g_lighting.bloomFx = g_lighting.blurFx = g_lighting.dofFx = false;
    }
    ImGui::TextDisabled("ColorCorrection, Bloom, Blur e DoF cambiano l'immagine. Il DoF e' una sfocatura morbida, non un obiettivo.");
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Frame / ImageLabel inserts
// ---------------------------------------------------------------------------

static void insertNewFrame() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("Frame" + std::to_string(n), "",
                               0.5f, 0.5f, {1.f, 1.f, 1.f}, "Frame");
    l.bgColor = {0.18f, 0.18f, 0.22f};
    l.bgTransparency = 0.15f;
    l.sizeW = 220.f;
    l.sizeH = 140.f;
    l.fontScale = 0.01f; // quasi senza testo
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "Frame creato";
}

static void insertNewImageLabel() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("ImageLabel" + std::to_string(n), "[Image]",
                               0.5f, 0.35f, {1.f, 1.f, 1.f}, "ImageLabel");
    l.bgColor = {0.25f, 0.45f, 0.75f};
    l.bgTransparency = 0.f;
    l.sizeW = 96.f;
    l.sizeH = 96.f;
    l.fontScale = 0.9f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "ImageLabel creato (placeholder tint)";
}

// GUI come in Roblox Studio: una ScreenGui accesa, i pezzi dentro come figli.
// Position.Scale = anchorX/Y, Position.Offset = offsetX/Y,
// Size.Offset = sizeW/H, Size.Scale = sizeScaleX/Y.

static bool studioGuiHoldsChildren(const WorkspaceLabel& l) {
    return l.className == "ScreenGui" || l.className == "Frame"
        || l.className == "ScrollingFrame" || l.isWorldGui();
}

static int studioEnsureScreenGui() {
    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
        WorkspaceLabel& l = g_world.labels[(size_t)i];
        if (l.className == "ScreenGui" && l.uiLayout) {
            l.uiAttached = true;
            l.visible = true;
            return i;
        }
    }
    const int n = g_world.nextLabelIndex();
    WorkspaceLabel& l = g_world.addLabel("ScreenGui" + std::to_string(n), "",
                                         0.f, 0.f, {1.f, 1.f, 1.f}, "ScreenGui");
    l.uiLayout = true;
    l.uiAttached = true;
    l.visible = true;
    l.parentIndex = -1;
    l.sizeScaleX = 1.f;
    l.sizeScaleY = 1.f;
    l.bgTransparency = 1.f;
    l.fontScale = 1.f;
    markPlaceDirty();
    return (int)g_world.labels.size() - 1;
}

static int studioGuiInsertParent(const char* cls) {
    const bool decor = std::strcmp(cls, "UIListLayout") == 0 || std::strcmp(cls, "UIGridLayout") == 0
                    || std::strcmp(cls, "UIPadding") == 0 || std::strcmp(cls, "UICorner") == 0
                    || std::strcmp(cls, "UIStroke") == 0;
    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
        const WorkspaceLabel& s = g_world.labels[(size_t)g_selectedLabel];
        if (s.uiLayout || s.isWorldGui()) {
            if (!decor || studioGuiHoldsChildren(s) || s.className == "TextLabel"
                || s.className == "TextButton" || s.className == "TextBox"
                || s.className == "ImageLabel" || s.className == "ImageButton")
                return g_selectedLabel;
        }
    }
    return studioEnsureScreenGui();
}

static void studioStyleNewWidget(WorkspaceLabel& l, const char* cls) {
    l.uiLayout = true;
    l.visible = true;
    l.fontScale = 1.f;
    l.anchorPointX = 0.f;
    l.anchorPointY = 0.f;
    l.anchorX = 0.f;
    l.anchorY = 0.f;
    l.bgTransparency = 0.f;
    l.text = cls;
    if (std::strcmp(cls, "ScreenGui") == 0) {
        l.uiAttached = true;
        l.parentIndex = -1;
        l.sizeScaleX = 1.f;
        l.sizeScaleY = 1.f;
        l.bgTransparency = 1.f;
        l.text.clear();
    } else if (std::strcmp(cls, "Frame") == 0) {
        l.sizeW = 220.f; l.sizeH = 140.f;
        l.offsetX = 40.f; l.offsetY = 40.f;
        l.bgColor = {0.16f, 0.17f, 0.2f};
        l.text.clear();
    } else if (std::strcmp(cls, "ScrollingFrame") == 0) {
        l.sizeW = 240.f; l.sizeH = 180.f;
        l.offsetX = 24.f; l.offsetY = 24.f;
        l.bgColor = {0.12f, 0.13f, 0.16f};
        l.text.clear();
    } else if (std::strcmp(cls, "TextLabel") == 0) {
        l.sizeW = 180.f; l.sizeH = 32.f;
        l.offsetX = 12.f; l.offsetY = 12.f;
        l.bgTransparency = 1.f;
        l.color = {1.f, 1.f, 1.f};
        l.text = "TextLabel";
    } else if (std::strcmp(cls, "TextButton") == 0) {
        l.sizeW = 160.f; l.sizeH = 40.f;
        l.offsetX = 12.f; l.offsetY = 52.f;
        l.bgColor = {0.18f, 0.45f, 0.9f};
        l.color = {1.f, 1.f, 1.f};
        l.text = "Button";
    } else if (std::strcmp(cls, "TextBox") == 0) {
        l.sizeW = 180.f; l.sizeH = 32.f;
        l.offsetX = 12.f; l.offsetY = 100.f;
        l.bgColor = {0.95f, 0.95f, 0.97f};
        l.color = {0.1f, 0.1f, 0.12f};
        l.text = "";
    } else if (std::strcmp(cls, "ImageLabel") == 0) {
        l.sizeW = 80.f; l.sizeH = 80.f;
        l.offsetX = 12.f; l.offsetY = 144.f;
        l.bgColor = {0.3f, 0.5f, 0.8f};
        l.text.clear();
    } else if (std::strcmp(cls, "ImageButton") == 0) {
        l.sizeW = 80.f; l.sizeH = 80.f;
        l.offsetX = 104.f; l.offsetY = 144.f;
        l.bgColor = {0.2f, 0.62f, 0.38f};
        l.text.clear();
    } else if (std::strcmp(cls, "UIListLayout") == 0) {
        l.sizeW = 6.f;
        l.text.clear();
    } else if (std::strcmp(cls, "UIGridLayout") == 0) {
        l.sizeW = 8.f; l.sizeH = 8.f;
        l.text.clear();
    } else if (std::strcmp(cls, "UIPadding") == 0) {
        l.sizeW = 8.f;
        l.text.clear();
    } else if (std::strcmp(cls, "UICorner") == 0) {
        l.sizeW = 8.f;
        l.text.clear();
    } else if (std::strcmp(cls, "UIStroke") == 0) {
        l.sizeW = 2.f;
        l.color = {0.f, 0.f, 0.f};
        l.bgTransparency = 0.f;
        l.text.clear();
    } else if (std::strcmp(cls, "BillboardGui") == 0 || std::strcmp(cls, "SurfaceGui") == 0) {
        l.uiLayout = false;
        l.parentIndex = -1;
        l.adorneePart = (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
            ? g_selectedPart : -1;
        l.studsOffsetY = 3.f;
        l.sizeW = 180.f; l.sizeH = 48.f;
        l.fontScale = 1.2f;
        l.bgColor = {0.08f, 0.08f, 0.1f};
        l.bgTransparency = 0.25f;
        l.color = {1.f, 1.f, 1.f};
        l.text = std::strcmp(cls, "BillboardGui") == 0 ? "Billboard" : "Surface";
    }
}

static void insertRobloxWidget(const char* className) {
    if (!className || !className[0]) return;
    if (std::strcmp(className, "ScreenGui") == 0) {
        selectLabel(studioEnsureScreenGui());
        g_exportMsg = "ScreenGui — i prossimi pezzi finiscono qui dentro";
        return;
    }
    const bool world = std::strcmp(className, "BillboardGui") == 0
                    || std::strcmp(className, "SurfaceGui") == 0;
    const int parent = world ? -1 : studioGuiInsertParent(className);
    const int n = g_world.nextLabelIndex();
    WorkspaceLabel& l = g_world.addLabel(std::string(className) + std::to_string(n), className,
                                         0.f, 0.f, {1.f, 1.f, 1.f}, className);
    studioStyleNewWidget(l, className);
    if (!world) l.parentIndex = parent;
    selectLabel((int)g_world.labels.size() - 1);
    markPlaceDirty();
    g_exportMsg = std::string(className) + " inserito";
}

static bool studioSelectedGui(WorkspaceLabel*& out) {
    if (g_selectedLabel < 0 || g_selectedLabel >= (int)g_world.labels.size()) return false;
    out = &g_world.labels[(size_t)g_selectedLabel];
    return out->uiLayout;
}

static void studioAlignGui(int axis, int mode) {
    WorkspaceLabel* l = nullptr;
    if (!studioSelectedGui(l)) { g_exportMsg = "Seleziona un pezzo di GUI"; return; }
    const float t = mode == 0 ? 0.f : mode == 1 ? 0.5f : 1.f;
    if (axis == 0) { l->anchorX = t; l->anchorPointX = t; l->offsetX = 0.f; }
    else           { l->anchorY = t; l->anchorPointY = t; l->offsetY = 0.f; }
    markPlaceDirty();
}

static void studioNudgeGuiZ(int delta) {
    WorkspaceLabel* l = nullptr;
    if (!studioSelectedGui(l)) return;
    l->zIndex = std::max(0, l->zIndex + delta);
    markPlaceDirty();
}

static void studioDuplicateGui() {
    if (g_selectedLabel < 0 || g_selectedLabel >= (int)g_world.labels.size()) return;
    WorkspaceLabel copy = g_world.labels[(size_t)g_selectedLabel];
    copy.name += "Copy";
    copy.offsetX += 16.f;
    copy.offsetY += 16.f;
    g_world.labels.push_back(std::move(copy));
    selectLabel((int)g_world.labels.size() - 1);
    markPlaceDirty();
    g_exportMsg = "GUI duplicata";
}

static void ribbonGuiMenuItems() {
    if (ImGui::MenuItem(L("rb.screengui"))) insertRobloxWidget("ScreenGui");
    if (ImGui::MenuItem(L("rb.frame"))) insertRobloxWidget("Frame");
    if (ImGui::MenuItem(L("rb.scroll"))) insertRobloxWidget("ScrollingFrame");
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.textlabel"))) insertRobloxWidget("TextLabel");
    if (ImGui::MenuItem(L("rb.textbutton"))) insertRobloxWidget("TextButton");
    if (ImGui::MenuItem(L("rb.textbox"))) insertRobloxWidget("TextBox");
    if (ImGui::MenuItem(L("rb.image"))) insertRobloxWidget("ImageLabel");
    if (ImGui::MenuItem(L("rb.imagebtn"))) insertRobloxWidget("ImageButton");
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.uilist"))) insertRobloxWidget("UIListLayout");
    if (ImGui::MenuItem(L("rb.uigrid"))) insertRobloxWidget("UIGridLayout");
    if (ImGui::MenuItem(L("rb.uipad"))) insertRobloxWidget("UIPadding");
    if (ImGui::MenuItem(L("rb.uicorner"))) insertRobloxWidget("UICorner");
    if (ImGui::MenuItem(L("rb.uistroke"))) insertRobloxWidget("UIStroke");
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.billboard"))) insertRobloxWidget("BillboardGui");
    if (ImGui::MenuItem(L("rb.surface"))) insertRobloxWidget("SurfaceGui");
}

// ---------------------------------------------------------------------------
// PrimaryPart / Pivot helpers
// ---------------------------------------------------------------------------

static void studioSetPrimaryPartFromSelection() {
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) {
        g_exportMsg = "Seleziona un Model";
        return;
    }
    if (!partIndexValid(g_selectedPart)) {
        g_exportMsg = "Seleziona anche una Part (PrimaryPart)";
        return;
    }
    auto& m = g_world.models[g_selectedModel];
    m.primaryPart = g_selectedPart;
    m.pivotOffset = g_world.parts[g_selectedPart].position - m.position;
    markPlaceDirty();
    g_exportMsg = "PrimaryPart = " + g_world.parts[g_selectedPart].name;
}

static void studioPivotModelToSelection() {
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) return;
    auto& m = g_world.models[g_selectedModel];
    if (partIndexValid(m.primaryPart))
        m.position = g_world.parts[m.primaryPart].position - m.pivotOffset;
    markPlaceDirty();
}

static void drawModelPrimaryPartProps(WorkspaceModel& m) {
    ImGui::Separator();
    ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Pivot / PrimaryPart");
    if (m.primaryPart >= 0 && m.primaryPart < (int)g_world.parts.size())
        ImGui::Text("PrimaryPart: %s", g_world.parts[m.primaryPart].name.c_str());
    else
        ImGui::TextDisabled("PrimaryPart: (none)");
    ImGui::DragFloat3("PivotOffset", &m.pivotOffset.x, 0.05f);
    if (ImGui::Button("Set PrimaryPart from selection", {-1, 0}))
        studioSetPrimaryPartFromSelection();
    if (ImGui::Button("PivotTo PrimaryPart", {-1, 0}))
        studioPivotModelToSelection();
}

// ---------------------------------------------------------------------------
// Menus / overlays / hotkeys
// ---------------------------------------------------------------------------

static void drawStudio2020ViewMenu() {
    ImGui::Separator();
    ImGui::TextDisabled("Studio 2020");
    ImGui::MenuItem("Terrain Editor…", nullptr, &g_showTerrainEditor);
    ImGui::MenuItem("Collision Groups…", nullptr, &g_showCollisionGroups);
    ImGui::MenuItem("Find / Replace…", "Ctrl+Shift+F", &g_showFindReplace);
    ImGui::MenuItem("Constraints (Weld, Hinge, Rope, Spring, Rod)…", nullptr, &g_showWeldsWindow);
    ImGui::MenuItem("Mostra vincoli nel viewport", nullptr, &g_showConstraintLines);
    ImGui::MenuItem("Atmosphere / SunRays…", nullptr, &g_showAtmospherePanel);
    ImGui::MenuItem("Pathfinding debug", nullptr, &g_showPathDebug);
    if (ImGui::MenuItem("Compute path to selection"))
        studioComputePathDebug();
    if (ImGui::MenuItem("Insert Frame")) insertNewFrame();
    if (ImGui::MenuItem("Insert ImageLabel")) insertNewImageLabel();
    if (ImGui::MenuItem("Weld selection")) studioCreateWeldFromSelection();
}

static void drawStudio2020Overlays() {
    drawStudioSelectionOverlays();
    drawCollisionGroupsWindow();
    drawTerrainEditorWindow();
    drawFindReplaceWindow();
    drawWeldsWindow();
    drawMultiplayerTestWindow();
    drawAtmospherePanel();
    drawPathDebugOverlay();
}

static void studio2020PollHotkeys() {
    if (g_studioTesting) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F))
        g_showFindReplace = true;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_T) && !io.KeyShift)
        g_showTerrainEditor = true;
}
