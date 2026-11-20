// Pipeline di rendering del mondo 3D (pass 4–10 di render/RenderPipeline.h).
// Non e' un header riusabile: usa le globali di main.cpp. Compila solo se
// incluso da main.cpp nel punto previsto.
//
// Ordine in renderWorld3D:
//   passPrepare → passShadows → passTerrain → passOpaqueParts →
//   passStudioGizmos → passWorkspaceModels → passAvatars → friends

#include "render/RenderCullLod.h"
#include "engine/RuntimeScale.h"
#include "render/PartSpatialGrid.h"
#include "main_ui/PartInstancedDraw.h"

#include <chrono>
#include "render/RenderPipeline.h"

static const glm::vec3 kGizmoRGB[3] = {
    {0.95f, 0.22f, 0.20f},
    {0.22f, 0.88f, 0.32f},
    {0.28f, 0.52f, 1.00f},
};

// ---- Gizmo stile Roblox Studio (disegnati sopra la scena) -----------------------------------
// Sempre della stessa grandezza sullo schermo e sempre visibili sopra le part:
// Sposta = 6 frecce (anche verso il negativo) che escono dalla superficie,
// Ridimensiona = una sfera per faccia, Ruota = 3 anelli attorno alla part.
// Si illumina solo la maniglia sotto il mouse.
static ImU32 gizmoCol(int axis, bool hot) {
    const glm::vec3 c = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f, 0.95f, 0.55f), 0.55f) : kGizmoRGB[axis];
    return IM_COL32((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255), 255);
}

static void drawStudioGizmoOverlay() {
    if (g_screen != AppScreen::Studio || g_studioTesting) return;
    if (g_tool != StudioTool::Move && g_tool != StudioTool::Scale && g_tool != StudioTool::Rotate) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const WorkspacePart& p = *pt;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 kShadow = IM_COL32(15, 18, 24, 170);
    double mxd = 0, myd = 0;
    glfwGetCursorPos(glfwGetCurrentContext(), &mxd, &myd);
    const float mx = (float)mxd, my = (float)myd;
    const bool mouseFree = !ImGui::GetIO().WantCaptureMouse && !g_rmbLook;

    if (g_tool == StudioTool::Move) {
        int hAx = -1;
        g_gizmoHoverSign = 0;
        if (g_moveDragging) hAx = g_moveAxis;
        else if (mouseFree && !pickMoveHandle(mx, my, hAx)) hAx = -1;
        for (int axis = 0; axis < 3; ++axis)
            for (int s = -1; s <= 1; s += 2) {
                glm::vec3 a, b;
                moveHandleSeg(p, axis, s, a, b);
                float ax, ay, bx, by;
                if (!gizmoWorldToScreen(a, ax, ay) || !gizmoWorldToScreen(b, bx, by)) continue;
                const bool hot = hAx == axis && (g_moveDragging || g_gizmoHoverSign == s);
                const ImU32 c = gizmoCol(axis, hot);
                const float dx = bx - ax, dy = by - ay, l = std::max(1.f, std::sqrt(dx * dx + dy * dy));
                const ImVec2 dn{dx / l, dy / l}, nn{-dy / l, dx / l};
                const float cone = std::min(18.f, l * 0.45f), w = hot ? 8.f : 6.5f;
                const ImVec2 base{bx - dn.x * cone, by - dn.y * cone};
                dl->AddLine({ax, ay}, base, kShadow, hot ? 6.f : 5.f);
                dl->AddLine({ax, ay}, base, c, hot ? 4.f : 3.f);
                dl->AddTriangleFilled({bx + dn.x, by + dn.y}, {base.x + nn.x * (w + 1), base.y + nn.y * (w + 1)},
                                      {base.x - nn.x * (w + 1), base.y - nn.y * (w + 1)}, kShadow);
                dl->AddTriangleFilled({bx, by}, {base.x + nn.x * w, base.y + nn.y * w}, {base.x - nn.x * w, base.y - nn.y * w}, c);
            }
    } else if (g_tool == StudioTool::Scale) {
        int hAx = -1, hSg = 0;
        if (g_scaleDragging) { hAx = g_scaleAxis; hSg = g_scaleSign; }
        else if (mouseFree && !pickScaleHandle(mx, my, hAx, hSg)) hAx = -1;
        for (int axis = 0; axis < 3; ++axis)
            for (int s = -1; s <= 1; s += 2) {
                float sx, sy;
                if (!gizmoWorldToScreen(handleWorldPos(p, axis, s), sx, sy)) continue;
                const bool hot = hAx == axis && hSg == s;
                const float r = hot ? 9.f : 7.f;
                dl->AddCircleFilled({sx, sy}, r + 1.5f, kShadow, 20);
                dl->AddCircleFilled({sx, sy}, r, gizmoCol(axis, hot), 20);
                dl->AddCircleFilled({sx - r * 0.3f, sy - r * 0.3f}, r * 0.32f, IM_COL32(255, 255, 255, 110), 10);
            }
    } else {
        int hAx = -1;
        if (g_rotateDragging) hAx = g_rotateAxis;
        else if (mouseFree && !pickRotateHandle(mx, my, hAx)) hAx = -1;
        const float R = gizmoRingRadius(p);
        for (int axis = 0; axis < 3; ++axis) {
            const glm::vec3 axisV = g_rotateDragging && g_rotateAxis == axis ? g_rotateAxisW : gizmoAxisDir(p, axis);
            const glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            const glm::vec3 u = glm::normalize(glm::cross(axisV, ref)), v = glm::normalize(glm::cross(axisV, u));
            ImVec2 pts[65];
            int n = 0;
            for (int i = 0; i <= 64; ++i) {
                const float a = (float)i / 64.f * 6.2831853f;
                float sx, sy;
                if (!gizmoWorldToScreen(p.position + (u * std::cos(a) + v * std::sin(a)) * R, sx, sy)) { n = 0; break; }
                pts[n++] = {sx, sy};
            }
            if (n < 2) continue;
            const bool hot = hAx == axis;
            dl->AddPolyline(pts, n, kShadow, 0, hot ? 6.f : 4.5f);
            dl->AddPolyline(pts, n, gizmoCol(axis, hot), 0, hot ? 4.f : 2.5f);
            // Spicchio del giro fatto mentre trascini l'anello.
            if (g_rotateDragging && g_rotateAxis == axis && std::fabs(g_rotateLiveDelta) > 0.2f) {
                float cx, cy;
                if (gizmoWorldToScreen(p.position, cx, cy)) {
                    const int steps = std::clamp((int)std::fabs(g_rotateLiveDelta) / 4 + 2, 2, 90);
                    const float sgn = g_rotateLiveDelta >= 0.f ? 1.f : -1.f;
                    ImVec2 prev{};
                    for (int i = 0; i <= steps; ++i) {
                        const float a = g_rotateStartAngle + sgn * glm::radians(std::fabs(g_rotateLiveDelta)) * (float)i / (float)steps;
                        float sx, sy;
                        if (!gizmoWorldToScreen(p.position + (u * std::cos(a) + v * std::sin(a)) * R, sx, sy)) continue;
                        if (i > 0) dl->AddTriangleFilled({cx, cy}, prev, {sx, sy}, IM_COL32(255, 220, 90, 70));
                        prev = {sx, sy};
                    }
                }
            }
        }
    }

    // Quanto hai spostato / allargato / girato, vicino al mouse.
    char info[48] = "";
    if (g_moveDragging) std::snprintf(info, sizeof(info), "%+.2f stud", g_moveLiveDelta);
    else if (g_scaleDragging) std::snprintf(info, sizeof(info), "%.2f stud", g_scaleLiveSize);
    else if (g_rotateDragging) std::snprintf(info, sizeof(info), "%+.1f\xC2\xB0", g_rotateLiveDelta);
    if (info[0]) {
        const ImVec2 ts = ImGui::CalcTextSize(info);
        dl->AddRectFilled({mx + 14, my + 10}, {mx + 22 + ts.x, my + 14 + ts.y}, IM_COL32(20, 24, 32, 215), 4.f);
        dl->AddText({mx + 18, my + 12}, IM_COL32(255, 235, 140, 255), info);
    }
}

// Contorno azzurro della part sotto il mouse (come il "hover" di Roblox Studio)
// e la griglia sul terreno quando "Mostra griglia" e' acceso.
static bool studioMultiContains(int i);   // StudioFeaturesUi.h

static void drawPartOutline(ImDrawList* dl, const WorkspacePart& p, ImU32 col, float thick) {
    const glm::mat4 m = partModelMatrix(p);
    ImVec2 c[8];
    for (int k = 0; k < 8; ++k) {
        const glm::vec3 v = glm::vec3(m * glm::vec4((k & 1) ? 0.5f : -0.5f, (k & 2) ? 0.5f : -0.5f, (k & 4) ? 0.5f : -0.5f, 1.f));
        float x, y;
        if (!gizmoWorldToScreen(v, x, y)) return;   // un angolo dietro la camera: niente contorno
        c[k] = {x, y};
    }
    static const int e[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& ed : e) dl->AddLine(c[ed[0]], c[ed[1]], col, thick);
}

static void drawStudioHoverAndGrid() {
    if (g_screen != AppScreen::Studio || g_studioTesting) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (g_studioGridVisible) {
        // Griglia sul passo di spostamento, centrata sulla part selezionata.
        float step = g_studioSnapEnabled ? g_studioSnapMove : g_studioGridSize;
        step = std::clamp(step, kMinPartSize, 16.f);
        glm::vec3 c = g_cam.pos + g_cam.front() * std::clamp(studioFocusDistance(), 0.2f, 40.f);
        float planeY = 0.02f;
        if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size()) {
            c = g_world.parts[g_selectedPart].position;
            planeY = c.y;
        }
        c.x = std::round(c.x / step) * step;
        c.z = std::round(c.z / step) * step;
        const int n = 16;
        const float half = step * (float)n;
        for (int axis = 0; axis < 2; ++axis)
            for (int i = -n; i <= n; ++i) {
                const float off = (float)i * step;
                const float along = (axis == 0 ? c.z : c.x) + off;
                const float period = step * 4.f;
                const bool major = std::fmod(std::fabs(along), period) < step * 0.25f;
                for (int sgm = -n; sgm < n; ++sgm) {
                    glm::vec3 a = c, b = c;
                    if (axis == 0) { a += glm::vec3((float)sgm * step, 0.f, off); b += glm::vec3((float)(sgm + 1) * step, 0.f, off); }
                    else { a += glm::vec3(off, 0.f, (float)sgm * step); b += glm::vec3(off, 0.f, (float)(sgm + 1) * step); }
                    a.y = b.y = planeY;
                    float ax, ay, bx, by;
                    if (!gizmoWorldToScreen(a, ax, ay) || !gizmoWorldToScreen(b, bx, by)) continue;
                    const float d = glm::length(glm::vec2(a.x - c.x, a.z - c.z)) / half;   // sfuma verso il bordo
                    const int alpha = (int)((major ? 110.f : 55.f) * std::max(0.f, 1.f - d));
                    if (alpha > 3) dl->AddLine({ax, ay}, {bx, by}, IM_COL32(200, 210, 225, alpha), major ? 1.4f : 1.f);
                }
            }
    }
    const bool dragging = g_moveDragging || g_rotateDragging || g_scaleDragging || g_freeDragging;
    if (dragging || ImGui::GetIO().WantCaptureMouse || g_rmbLook) return;
    double mx = 0, my = 0;
    glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    const int hov = pickPartAtMouse((float)mx, (float)my);
    if (hov < 0 || hov >= (int)g_world.parts.size() || hov == g_selectedPart || studioMultiContains(hov)) return;
    const WorkspacePart& p = g_world.parts[(size_t)hov];
    if (p.locked) return;
    drawPartOutline(dl, p, IM_COL32(120, 190, 255, 200), 1.6f);
}

/** Strumento vincoli: il punto gia' scelto e la linea fino al mouse. Esc lo spegne. */
static void drawConstraintToolOverlay() {
    if (g_constraintToolKind < 0) return;
    if (g_screen != AppScreen::Studio || g_studioTesting || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        g_constraintToolKind = -1;
        g_ctPart0 = -1;
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 m = ImGui::GetIO().MousePos;
    float sx, sy;
    if (g_ctPart0 >= 0 && gizmoWorldToScreen(g_ctPoint0, sx, sy)) {
        dl->AddLine({sx, sy}, m, IM_COL32(110, 230, 120, 200), 2.f);
        dl->AddCircleFilled({sx, sy}, 6.f, IM_COL32(110, 230, 120, 255));
    }
    char txt[96];
    std::snprintf(txt, sizeof(txt), "%s: %s", constraintKindName((ConstraintKind)g_constraintToolKind),
                  g_ctPart0 < 0 ? "clic sulla prima part" : "clic sulla seconda part");
    dl->AddRectFilled({m.x + 14, m.y + 12}, {m.x + 22 + ImGui::CalcTextSize(txt).x, m.y + 32}, IM_COL32(20, 24, 32, 215), 4.f);
    dl->AddText({m.x + 18, m.y + 14}, IM_COL32(170, 240, 170, 255), txt);
}

static void drawScriptAdornmentOutlines() {
    if (!ImGui::GetCurrentContext()) return;
    const bool show = g_screen == AppScreen::Playing || g_screen == AppScreen::Studio;
    if (!show) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (const auto& c : g_world.welds) {
        if (!c.enabled || c.part0 < 0 || c.part1 < 0) continue;
        if (c.part0 >= (int)g_world.parts.size() || c.part1 >= (int)g_world.parts.size()) continue;
        float ax, ay, bx, by;
        if (!gizmoWorldToScreen(g_world.parts[(size_t)c.part0].position, ax, ay)) continue;
        if (!gizmoWorldToScreen(g_world.parts[(size_t)c.part1].position, bx, by)) continue;
        dl->AddLine({ax, ay}, {bx, by}, IM_COL32(120, 220, 140, 180), 1.5f);
    }
    for (const auto& p : g_world.parts) {
        if (p.highlightEnabled) {
            const int r = (int)std::lround(std::clamp(p.highlightR, 0.f, 1.f) * 255.f);
            const int g = (int)std::lround(std::clamp(p.highlightG, 0.f, 1.f) * 255.f);
            const int b = (int)std::lround(std::clamp(p.highlightB, 0.f, 1.f) * 255.f);
            drawPartOutline(dl, p, IM_COL32(r, g, b, 230), 2.4f);
        }
        if (p.selectionBox)
            drawPartOutline(dl, p, IM_COL32(80, 200, 255, 220), 1.6f);
        if (p.selectionSphere) {
            float cx, cy, ex, ey;
            const float rad = 0.5f * std::max(p.size.x, std::max(p.size.y, p.size.z));
            if (gizmoWorldToScreen(p.position, cx, cy)
                && gizmoWorldToScreen(p.position + glm::vec3(rad, 0.f, 0.f), ex, ey)) {
                const float pr = std::max(6.f, std::hypot(ex - cx, ey - cy));
                dl->AddCircle({cx, cy}, pr, IM_COL32(80, 220, 255, 210), 32, 1.8f);
            }
        }
    }
}

static void drawGizmoOverlayHud() {
    drawStudioHoverAndGrid();   // contorno sotto il mouse + griglia
    drawConstraintToolOverlay(); // strumento vincoli (due clic)
    drawStudioGizmoOverlay();   // le maniglie (stile Roblox)
    if (g_studioTesting || g_screen != AppScreen::Studio) return;
    if (!g_moveDragging && !g_rotateDragging && !g_scaleDragging) return;
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 140.f, 64.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##GizmoHud", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    if (g_rotateDragging) {
        const char* ax = (g_rotateAxis == 0) ? "X" : (g_rotateAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({1.f, 0.85f, 0.3f, 1.f}, "Rotate %s  %+0.1f°", ax, g_rotateLiveDelta);
        ImGui::TextDisabled("Scatti dal passo di rotazione · Ctrl=libero");
    } else if (g_moveDragging) {
        const char* ax = (g_moveAxis == 0) ? "X" : (g_moveAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.5f, 0.9f, 1.f, 1.f}, "Move %s  %+0.2f", ax, g_moveLiveDelta);
        ImGui::TextDisabled("Scatti dal passo di spostamento · Ctrl=libero");
    } else if (g_scaleDragging) {
        const char* ax = (g_scaleAxis == 0) ? "X" : (g_scaleAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.6f, 1.f, 0.55f, 1.f}, "Scale %s  %.2f", ax, g_scaleLiveSize);
        ImGui::TextDisabled("Ctrl = tutti e due i lati · Shift = in proporzione");
    }
    ImGui::End();
}

static void drawScaleHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                             const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Scale || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    if (!g_scaleDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1, sg = 0;
        g_gizmoHoverAxis = pickScaleHandle((float)mx, (float)my, ax, sg) ? ax : -1;
    }
    float baseHs = scaleHandleDrawSize(p);
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            glm::vec3 hp = handleWorldPos(p, axis, s);
            bool hot = (g_scaleDragging && g_scaleAxis == axis && g_scaleSign == s)
                    || (g_gizmoHoverAxis == axis && !g_scaleDragging);
            float hs = hot ? baseHs * 1.35f : baseHs;
            glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.35f) : kGizmoRGB[axis];
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
            glm::mat4 m2 = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs * 1.18f));
            drawCube(vao, partP, m2, view, proj, col * 0.45f, sun, false, 0);
        }
    }
}

static void drawMoveHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                            const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Move || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float len = gizmoArrowLen(p);
    if (!g_moveDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickMoveHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
        else
            g_gizmoHoverAxis = ax;
    }
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.35f));
        drawCube(vao, partP, m, view, proj, glm::vec3(0.95f, 0.95f, 0.98f), sun, false, 0);
    }
    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_moveDragging && g_moveAxis == axis) || (g_gizmoHoverAxis == axis && !g_moveDragging);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.4f) : kGizmoRGB[axis];
        glm::vec3 dir = gizmoAxisDir(p, axis);
        float stemLen = len * 0.78f;
        glm::vec3 stemMid = p.position + dir * (stemLen * 0.5f);
        float thick = hot ? 0.16f : 0.12f;
        glm::mat4 mStem = orientAlongZ(stemMid, dir, {thick, thick, stemLen});
        drawCube(vao, partP, mStem, view, proj, col * 0.92f, sun, false, 0);
        glm::vec3 tip = p.position + dir * len;
        float tipS = hot ? 0.55f : 0.42f;
        glm::mat4 mTip = orientAlongZ(tip - dir * (tipS * 0.35f), dir, {tipS, tipS, tipS * 1.35f});
        drawCube(vao, partP, mTip, view, proj, col, sun, false, 0);
    }
}

static void drawRotateHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Rotate || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float R = gizmoRingRadius(p);
    const int segs = 64;

    if (!g_rotateDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickRotateHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
    }

    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_rotateDragging && g_rotateAxis == axis)
                || (g_gizmoHoverAxis == axis);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.45f) : kGizmoRGB[axis];
        float thick = hot ? 0.22f : 0.14f;
        glm::vec3 axisV = gizmoAxisDir(p, axis);
        glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
        glm::vec3 v = glm::normalize(glm::cross(axisV, u));

        for (int i = 0; i < segs; ++i) {
            float a = (float)i / (float)segs * 6.2831853f;
            glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * R;
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(thick));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
        }

        for (int tick = 0; tick < 24; ++tick) {
            float a = (float)tick / 24.f * 6.2831853f;
            glm::vec3 radial = u * std::cos(a) + v * std::sin(a);
            glm::vec3 pos = p.position + radial * (R + 0.28f);
            float ts = (tick % 6 == 0) ? 0.20f : 0.11f;
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(ts));
            drawCube(vao, partP, m, view, proj, col * 0.75f, sun, false, 0);
        }

        if (g_rotateDragging && g_rotateAxis == axis && std::fabs(g_rotateLiveDelta) > 0.5f) {
            int nFill = std::clamp((int)std::fabs(g_rotateLiveDelta) / 3, 2, 40);
            float sign = (g_rotateLiveDelta >= 0.f) ? 1.f : -1.f;
            float startA = g_rotateStartAngle;
            for (int i = 0; i <= nFill; ++i) {
                float a = startA + sign * glm::radians(std::fabs(g_rotateLiveDelta)) * ((float)i / nFill);
                glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * (R * 0.92f);
                glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(0.18f));
                drawCube(vao, partP, m, view, proj, glm::vec3(1.f, 0.92f, 0.35f), sun, false, 0);
            }
        }
    }
    glm::mat4 core = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.28f));
    drawCube(vao, partP, core, view, proj, glm::vec3(0.9f, 0.9f, 0.95f), sun, false, 0);
}

static void drawAvatarAccessories(GLuint meshP, const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& sun, glm::vec3 worldPos, float yawDeg,
                                  const Humanoid& hum, GLuint markerVao = 0, GLuint partP = 0,
                                  bool editorGhostMode = false) {
    if (g_catalog.items.empty()) return;
    float bob = humanoidBob(hum);
    float legSwing = humanoidLegSwing(hum);
    float armBase = humanoidArmSwing(hum);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(hum, jL, jR, jl, jr);
    const float jb = hum.jumpArmBlend;
    float armL = armBase * (1.f - jb) + jL;
    float armR = -armBase * (1.f - jb) + jR;
    float legL = legSwing * (1.f - jb) + jl;
    float legR = -legSwing * (1.f - jb) + jr;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), worldPos),
                                 glm::radians(yawDeg), glm::vec3(0, 1, 0));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    auto drawOne = [&](int i, float alpha, bool depthWrite) {
        const auto& a = g_catalog.items[i];
        if (!a.mesh.loaded || a.mesh.vertexCount <= 0) return;
        glm::mat4 local = accessoryWorldMatrix(a, hum, bob, armL, armR, legL, legR);
        glm::mat4 model = root * local;

        glm::vec3 tint = a.tint;
        if (editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.88f, 0.35f), 0.22f);
        else if (!editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.85f, 0.2f), 0.25f);

        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
        if (alpha < 0.99f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        glUseProgram(meshP);
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(meshP,"uSun"),1,glm::value_ptr(sun));
        glUniform3fv(glGetUniformLocation(meshP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
        uploadDynamicLights(meshP);
        a.mesh.drawTextured(meshP, tint, alpha);
        (void)markerVao;
        (void)partP;
    };

    if (editorGhostMode) {
        for (int i = 0; i < (int)g_catalog.items.size(); ++i) {
            if (i == g_catalog.selected) continue;
            drawOne(i, 0.38f, false);
        }
        if (g_catalog.selected >= 0 && g_catalog.selected < (int)g_catalog.items.size())
            drawOne(g_catalog.selected, 1.f, true);
        else {
            for (int i = 0; i < (int)g_catalog.items.size(); ++i)
                drawOne(i, 0.72f, true);
        }
    } else {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        for (int i = 0; i < (int)g_catalog.items.size(); ++i)
            drawOne(i, 1.f, true);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

static void drawCubeShadow(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4&,
                           const glm::mat4&, const glm::vec3&, const glm::vec3&) {
    (void)prog;
    int vc = (g_bevelCube.vao && vao == g_bevelCube.vao) ? g_bevelCube.vertexCount : g_sharpCubeVerts;
    g_shadowMap.setModel(m);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vc);
}

static glm::mat4 cachedPartModel(int index, const WorkspacePart& p) {
    struct Key { glm::vec3 pos{0}, rot{0}, size{0}; int shape{-1}; glm::mat4 m{0}; };
    static std::vector<Key> cache;
    if (index < 0) return partModelMatrix(p);
    if ((int)cache.size() <= index) cache.resize((size_t)index + 1);
    Key& k = cache[(size_t)index];
    if (k.m[3][3] == 1.f && k.pos == p.position && k.rot == p.rotation && k.size == p.size
        && k.shape == (int)p.shape)
        return k.m;
    k.pos = p.position;
    k.rot = p.rotation;
    k.size = p.size;
    k.shape = (int)p.shape;
    k.m = partModelMatrix(p);
    return k.m;
}

static void renderShadowDepth(GLuint vao, GLuint meshP) {
    g_sunShadowOn = false;
    bumpSceneUniforms();   // uSunShadow / uLightVP cambiano qui
    // quality.shadows era impostato dai preset ma mai letto — ora conta.
    if (!pipelineSunShadowsEnabled(g_engine.quality, g_lighting)) return;
    if (!g_shadowMap.ready) return;

    glm::vec3 focus = g_renderCamPos;
    if (!g_shadowMap.begin(g_lighting.toSkyLight(), focus)) return;
    g_sunShadowOn = true;

    const float reach = g_shadowMap.extent + 24.f;
    const float reach2 = reach * reach;
    const float shapeLow2Shadow =
        lodDistancesFor(g_engine.quality).shapeLow * lodDistancesFor(g_engine.quality).shapeLow;

    struct ShadowItem { GLuint vao; int verts; glm::mat4 model; };
    static std::vector<ShadowItem> shadowItems;
    shadowItems.clear();
    auto pushShadowPart = [&](int i) {
        if (blk::scale::stream().hides(i)) return;
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (g_world.partHeldInHand(p)) return;
        if (partFileMeshReady(p)) return;
        if (!p.castShadow || p.isTransparent() || p.transparency >= 0.92f) return;
        glm::vec3 d = p.position - focus;
        float d2 = glm::dot(d, d);
        if (d2 > reach2) return;
        GLuint dv = vao;
        int vc = 36;
        PartMeshLod shadowLod = (d2 > shapeLow2Shadow) ? PartMeshLod::Low : PartMeshLod::High;
        if (ShapeMesh* sm = g_partMeshes.meshForLod(p.shape, shadowLod)) {
            dv = sm->vao;
            vc = sm->vertexCount;
        } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
            dv = g_partMeshes.wedge.vao;
            vc = g_partMeshes.wedge.vertexCount;
        }
        if (!dv || vc <= 0) return;
        shadowItems.push_back({dv, vc, cachedPartModel(i, p)});
    };
    for (int i = 0; i < (int)g_world.parts.size(); ++i) pushShadowPart(i);

    if (g_shadowMap.depthInstProg && !shadowItems.empty()) {
        std::sort(shadowItems.begin(), shadowItems.end(), [](const ShadowItem& a, const ShadowItem& b) {
            if (a.vao != b.vao) return a.vao < b.vao;
            return a.verts < b.verts;
        });
        static std::vector<glm::mat4> shadowMats;
        size_t at = 0;
        while (at < shadowItems.size()) {
            size_t end = at + 1;
            while (end < shadowItems.size()
                   && shadowItems[end].vao == shadowItems[at].vao
                   && shadowItems[end].verts == shadowItems[at].verts)
                ++end;
            shadowMats.resize(end - at);
            for (size_t k = at; k < end; ++k)
                shadowMats[k - at] = shadowItems[k].model;
            g_shadowMap.drawInstanceGroup(shadowItems[at].vao, shadowItems[at].verts,
                                          shadowMats.data(), (int)shadowMats.size());
            at = end;
        }
        glUseProgram(g_shadowMap.depthProg);
    } else {
        for (const ShadowItem& it : shadowItems) {
            g_shadowMap.setModel(it.model);
            glBindVertexArray(it.vao);
            glDrawArrays(GL_TRIANGLES, 0, it.verts);
        }
    }
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (i >= (int)g_wsMeshes.size() || !g_wsMeshes[i].loaded) continue;
        const auto& m = g_world.models[i];
        glm::vec3 d = m.position - focus;
        if (glm::dot(d, d) > reach2) continue;
        g_shadowMap.setModel(glm::translate(glm::mat4(1.f), m.position)
                             * partRotationMatrix(m.rotation)
                             * glm::scale(glm::mat4(1.f), m.scale));
        g_wsMeshes[i].drawTextured(g_shadowMap.depthProg, {1.f, 1.f, 1.f}, 1.f);
    }
    drawPartFileMeshShadows(g_shadowMap.depthProg, focus, reach2);

    const glm::mat4 idm(1.f);
    const glm::vec3 zero(0.f);
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext sctx{ playerVao, g_shadowMap.depthProg, idm, idm, zero };
    const bool inWorld = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio);
    if (inWorld && !g_ragdoll.active) {
        bool hideBody[(int)AvatarSlot::Count] = {};
        fillBodyHideMask(g_catalog, hideBody);
        g_player.draw(sctx, drawCubeShadow, g_playerPos, g_playerYaw, hideBody);
        drawAvatarAccessories(g_shadowMap.depthProg, idm, idm, zero,
                              g_playerPos, g_playerYaw, g_player.humanoid);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUseProgram(g_shadowMap.depthProg);
    }
    for (size_t i = 0; i < g_net.remotes.size() && i < g_remoteBodies.size(); ++i)
        g_remoteBodies[i].draw(sctx, drawCubeShadow,
                               g_net.remotes[i].displayPos, g_net.remotes[i].displayYaw);

    // Il viewport va riportato alla risoluzione della SCENA, non a quella
    // della finestra: la scena puo essere piu piccola (Render scale).
    g_shadowMap.end(g_renderW, g_renderH, g_bloom.ready ? g_bloom.sceneFbo : 0);
}

// ---- Terrain: una VBO per chunk, una draw call per (chunk, materiale) ----
// Vale per qualunque place che usa workspace.Terrain: le facce esposte
// arrivano gia' pronte da Terrain::ChunkMesh (greedy 2D), qui si caricano in
// GPU solo quando il chunk cambia (version) e si scartano i chunk fuori
// frustum / oltre la distanza di disegno. Prima era un drawCube per box.
struct TerrainGpuChunk {
    GLuint vao = 0, vbo = 0;
    uint32_t version = 0;
    int vertCount = 0;
    std::vector<Terrain::MeshRange> ranges;
    glm::vec3 bmin{0}, bmax{0};
    bool seen = false;
};
static std::unordered_map<Terrain::Coord, TerrainGpuChunk, Terrain::CoordHash> g_terrainGpu;
static int g_terrainDrawCalls = 0;
static int g_terrainChunksDrawn = 0;

static void terrainGpuFree(TerrainGpuChunk& g) {
    if (g.vbo) glDeleteBuffers(1, &g.vbo);
    if (g.vao) glDeleteVertexArrays(1, &g.vao);
    g.vbo = g.vao = 0;
    g.vertCount = 0;
}

static void terrainGpuUpload(TerrainGpuChunk& g, const Terrain::ChunkMesh& m) {
    g.version = m.version;
    g.ranges = m.ranges;
    g.bmin = m.bmin;
    g.bmax = m.bmax;
    g.vertCount = (int)(m.verts.size() / 6);
    if (g.vertCount == 0) {
        terrainGpuFree(g);
        g.version = m.version;
        return;
    }
    if (!g.vao) {
        glGenVertexArrays(1, &g.vao);
        glGenBuffers(1, &g.vbo);
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    } else {
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    }
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m.verts.size() * sizeof(float)),
                 m.verts.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
}

// Materiale Part usato per ombreggiare/texturare un voxel: cosi' il terreno
// riusa lo stesso shader delle Part (ombre, foschia, riflessi, texture).
static PartMaterial terrainPartMaterial(uint8_t id) {
    switch (id) {
    case Terrain::Grass: case Terrain::Leaves: case Terrain::Moss:
    case Terrain::Cactus: case Terrain::Sapling: case Terrain::Reed:
    case Terrain::Wheat: case Terrain::Flower: case Terrain::Mushroom:
        return PartMaterial::Grass;
    case Terrain::Wood: case Terrain::Planks: case Terrain::Door:
    case Terrain::DoorOpen: case Terrain::Ladder: case Terrain::ChestBlk:
    case Terrain::Bed:
        return PartMaterial::Wood;
    case Terrain::Brick:
        return PartMaterial::Brick;
    case Terrain::Glass:
        return PartMaterial::Glass;
    case Terrain::Water:
        return PartMaterial::Ice;        // alpha 0.55: si vede il fondale
    case Terrain::IronBlock: case Terrain::GoldBlock: case Terrain::DiamondBlock:
        return PartMaterial::Metal;
    case Terrain::Torch: case Terrain::Lava:
        return PartMaterial::Neon;
    case Terrain::Snow: case Terrain::Wool: case Terrain::Sponge:
        return PartMaterial::SmoothPlastic;
    case Terrain::Stone: case Terrain::Cobble: case Terrain::Bedrock:
    case Terrain::Coal: case Terrain::Iron: case Terrain::Gold:
    case Terrain::Diamond: case Terrain::Gravel: case Terrain::Obsidian:
    case Terrain::Dirt: case Terrain::Sand: case Terrain::Clay:
    case Terrain::Farmland:
        return PartMaterial::Concrete;
    default:
        return PartMaterial::Plastic;
    }
}

static bool terrainMaterialTransparent(uint8_t id) {
    return id == Terrain::Water || id == Terrain::Glass;
}

static void drawTerrainVoxels(GLuint /*vao*/, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    g_terrainDrawCalls = 0;
    g_terrainChunksDrawn = 0;
    if (g_terrain.empty()) {
        if (!g_terrainGpu.empty()) {
            for (auto& kv : g_terrainGpu) terrainGpuFree(kv.second);
            g_terrainGpu.clear();
        }
        return;
    }

    // 1) sincronizza GPU <-> chunk (upload solo se la version e' cambiata)
    for (auto& kv : g_terrainGpu) kv.second.seen = false;
    g_terrain.forEachChunkMesh([&](const Terrain::Coord& c, const Terrain::ChunkMesh& m) {
        TerrainGpuChunk& g = g_terrainGpu[c];
        g.seen = true;
        if (g.version != m.version) terrainGpuUpload(g, m);
    });
    for (auto it = g_terrainGpu.begin(); it != g_terrainGpu.end();) {
        if (!it->second.seen) {
            terrainGpuFree(it->second);
            it = g_terrainGpu.erase(it);
        } else {
            ++it;
        }
    }

    // 2) culling per chunk — distanza terrain dedicata (su mobile il
    // maxDrawDistance da Part e' troppo alto: un mondo 50x50 resta tutto
    // in vista e il telefono resta a ~30 fps).
    const FrustumPlanes frustum = extractFrustum(proj * view);
    const glm::vec3 origin = g_renderCamPos;
    float maxD = g_engine.quality.maxDrawDistance;
    if (g_engine.quality.mobile)
        maxD = std::min(maxD, 56.f);
    if (!g_engine.quality.studTextures || g_engine.quality.studLowDetail)   // qualita' Low
        maxD = std::min(maxD, 52.f);
    maxD = std::max(40.f, maxD);
    const float waterLim = g_engine.quality.mobile ? 42.f : 120.f;
    const float waterMax2 = waterLim * waterLim;
    struct Vis { TerrainGpuChunk* g; float d2; };
    static std::vector<Vis> vis;
    vis.clear();
    for (auto& kv : g_terrainGpu) {
        TerrainGpuChunk& g = kv.second;
        if (!g.vao || g.vertCount == 0) continue;
        const glm::vec3 center = (g.bmin + g.bmax) * 0.5f;
        const float radius = glm::length(g.bmax - g.bmin) * 0.5f;
        const glm::vec3 d = center - origin;
        const float d2 = glm::dot(d, d);
        const float lim = maxD + radius;
        if (d2 > lim * lim) continue;
        if (sphereOutsideFrustum(frustum, center, radius)) continue;
        vis.push_back({&g, d2});
    }
    std::sort(vis.begin(), vis.end(), [](const Vis& a, const Vis& b) { return a.d2 < b.d2; });

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Lite solo con preset Low (stud off): colori flat. Su mobile Medium+ usa
    // le texture materiale sui voxel come su PC.
    const bool lite = !g_engine.quality.studTextures || g_engine.quality.studLowDetail;   // qualita' Low
    TerrainDrawPass tp = beginTerrainPass(partP, view, proj, sun, lite);

    auto emitOf = [](uint8_t id) -> float {
        if (id == Terrain::Torch || id == Terrain::Lava) return 1.f;
        return 0.f;
    };

    // 3) opachi
    for (const Vis& v : vis) {
        bool any = false;
        for (const Terrain::MeshRange& r : v.g->ranges) {
            if (terrainMaterialTransparent(r.id)) continue;
            PartMaterial mat = lite ? PartMaterial::Plastic : terrainPartMaterial(r.id);
            drawTerrainRange(tp, v.g->vao, Terrain::colorOf(r.id), mat,
                             emitOf(r.id), r.first, r.count);
            ++g_terrainDrawCalls;
            any = true;
        }
        if (any) ++g_terrainChunksDrawn;
    }
    // 4) trasparenti (acqua, vetro), dal piu' lontano — acqua lontana saltata su mobile
    for (auto it = vis.rbegin(); it != vis.rend(); ++it) {
        if (g_engine.quality.mobile && it->d2 > waterMax2) continue;
        for (const Terrain::MeshRange& r : it->g->ranges) {
            if (!terrainMaterialTransparent(r.id)) continue;
            PartMaterial mat = (r.id == Terrain::Water) ? PartMaterial::Ice
                               : (lite ? PartMaterial::Plastic : PartMaterial::Glass);
            if (lite && r.id == Terrain::Water) mat = PartMaterial::Ice; // serve l'alpha
            drawTerrainRange(tp, it->g->vao, Terrain::colorOf(r.id), mat,
                             0.f, r.first, r.count);
            ++g_terrainDrawCalls;
        }
    }
}

// ---- Pass nominati (stesso look di prima, ordine leggibile) ----------------

// Mappe open world: oltre questa quantita' di part, in gioco, il disegno guarda
// solo le celle della griglia attorno alla camera invece di tutte le part.
static constexpr int kSpatialGridMinParts = 700;
static blk::PartSpatialGrid g_partGrid;
static std::chrono::steady_clock::time_point g_partGridBuiltAt;

/** True se la griglia e' pronta. Si ricostruisce se cambia il numero di part, altrimenti ogni 1.2 s. */
static bool partGridReadyForFrame() {
    const bool editing = g_screen == AppScreen::Studio && !g_studioTesting;   // nello Studio le part si spostano di continuo
    if (editing || (int)g_world.parts.size() < kSpatialGridMinParts) return false;
    const auto now = std::chrono::steady_clock::now();
    if (g_partGrid.builtCount() != g_world.parts.size() || now - g_partGridBuiltAt > std::chrono::milliseconds(1200)) {
        g_partGrid.build(g_world.parts);
        g_partGridBuiltAt = now;
    }
    return true;
}


static void passPrepare(const glm::mat4& view, const glm::mat4& proj) {
    g_lastView = view;
    g_lastProj = proj;
    g_renderCamPos = (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting)
        ? g_cam.pos : g_playerPos + glm::vec3(0, 5, 0);
    // Mappe grandi: luci e ombre finte guardano solo le part vicine alla camera.
    static std::vector<int> nearLights, nearOcc;
    if (partGridReadyForFrame()) {
        nearLights.clear();
        nearOcc.clear();
        const float lightR = g_engine.quality.maxDrawDistance + 60.f;   // portata delle luci oltre il disegno
        g_partGrid.query(g_renderCamPos, lightR, [&](int i) { nearLights.push_back(i); });
        g_partGrid.query(g_renderCamPos, 96.f, [&](int i) { nearOcc.push_back(i); });
        collectDynamicLights(&nearLights);
        // Poche part vicine (pianura): meglio la ricerca completa.
        collectShadowOccluders(nearOcc.size() >= (size_t)kMaxShadowBoxes ? &nearOcc : nullptr);
    } else {
        collectDynamicLights();
        collectShadowOccluders();
    }
}

static void passShadows(GLuint vao, GLuint meshP) {
    renderShadowDepth(vao, meshP);
}

static void passTerrain(GLuint vao, GLuint partP, const glm::mat4& view,
                        const glm::mat4& proj, const glm::vec3& sun) {
    drawTerrainVoxels(vao, partP, view, proj, sun);
}

static void passOpaqueParts(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& sun) {
    g_renderCullStats.reset();
    g_renderCullStats.partsTotal = (int)g_world.parts.size();

    glm::vec3 origin = g_renderCamPos;
    const float maxD = g_engine.quality.maxDrawDistance;
    const int maxN = g_engine.quality.maxVisibleParts;
    const RenderLodDistances lodD = lodDistancesFor(g_engine.quality);
    const float studOff2 = lodD.studOff * lodD.studOff;
    const float shapeLow2 = lodD.shapeLow * lodD.shapeLow;
    const float smallCull2 = lodD.smallCull * lodD.smallCull;

    const bool useFrustum = g_engine.quality.autoLod || g_engine.quality.frustumCull;
    // Sotto le 8000 parti il soft-Z costa piu' del disegno e, con le scatole
    // senza rotazione, fa sparire pezzi ancora in vista quando si gira la
    // camera. Oltre, torna: li' il fill rate pesa di piu'.
    const bool useOcclusion = (g_world.parts.size() >= 8000)
        && (g_engine.quality.autoLod || g_engine.quality.occlusionCull);
    const FrustumPlanes frustum = extractFrustum(proj * view);
    const glm::mat4 viewProj = proj * view;
    struct Cand { int i; float d2; bool transparent; };
    static std::vector<Cand> cands;
    cands.clear();
    cands.reserve(g_world.parts.size());
    auto consider = [&](int i) {
        if (i < 0 || i >= (int)g_world.parts.size()) return;   // griglia vecchia di qualche frame
        const auto& part = g_world.parts[i];
        if (g_world.partHeldInHand(part)) return;   // Handle in mano: si disegna sul braccio
        if (part.transparency >= 0.99f) return;
        if (blk::scale::stream().hides(i)) {
            ++g_renderCullStats.partsDistCulled;
            return;
        }
        if (g_viewIsolate && g_isolatePart >= 0
            && i != g_isolatePart && i != g_selectedPart)
            return;
        glm::vec3 d = part.position - origin;
        float d2 = glm::dot(d, d);
        if (d2 > maxD * maxD) {
            ++g_renderCullStats.partsDistCulled;
            return;
        }
        if (lodD.smallMaxSize > 0.f) {
            float maxS = std::max(part.size.x, std::max(part.size.y, part.size.z));
            if (maxS <= lodD.smallMaxSize && d2 > smallCull2) {
                ++g_renderCullStats.partsSmallCulled;
                return;
            }
        }
        float boundRadius = partBoundRadius(part.size);
        if (useFrustum && sphereOutsideFrustum(frustum, part.position, boundRadius)) {
            ++g_renderCullStats.partsFrustumCulled;
            return;
        }
        cands.push_back({i, d2, part.isTransparent() || part.transparency > 0.05f || part.negative});
    };
    if (partGridReadyForFrame()) {
        // Solo le celle entro la distanza di disegno (le altre sono "lontane" per forza).
        int visited = 0;
        g_partGrid.query(origin, maxD, [&](int i) { ++visited; consider(i); });
        g_renderCullStats.partsDistCulled += (int)g_world.parts.size() - visited;
    } else {
        for (int i = 0; i < (int)g_world.parts.size(); ++i) consider(i);
    }
    if (useOcclusion && !cands.empty()) {
        static std::vector<uint8_t> occMask;
        const float minOcc = g_engine.quality.mobile ? 2.5f : 2.f;
        const int maxOcc = g_engine.quality.mobile ? 48 : 96;
        buildAndTestOcclusion(g_world.parts, cands, viewProj, minOcc, maxOcc, occMask);
        int w = 0;
        for (int ci = 0; ci < (int)cands.size(); ++ci) {
            const bool keepSelected = (g_screen == AppScreen::Studio && !g_studioTesting
                && cands[ci].i == g_selectedPart);
            if (occMask[ci] && !keepSelected) {
                ++g_renderCullStats.partsOcclusionCulled;
                continue;
            }
            cands[w++] = cands[ci];
        }
        cands.resize(w);
    }
    const int afterCull = (int)cands.size();
    if (afterCull > maxN) {
        if (g_engine.quality.priorityPartCull) {
            struct Scored { int ci; float score; };
            static std::vector<Scored> scored;
            scored.clear();
            scored.reserve(cands.size());
            for (int ci = 0; ci < (int)cands.size(); ++ci) {
                const auto& c = cands[ci];
                float score = partKeepScore(c.d2, g_world.parts[c.i].size);
                if (g_screen == AppScreen::Studio && !g_studioTesting
                    && c.i == g_selectedPart)
                    score *= 1.0e6f;
                scored.push_back({ci, score});
            }
            std::nth_element(scored.begin(), scored.begin() + maxN, scored.end(),
                [](const Scored& a, const Scored& b) { return a.score > b.score; });
            static std::vector<Cand> kept;
            kept.clear();
            kept.reserve(maxN);
            for (int k = 0; k < maxN; ++k)
                kept.push_back(cands[scored[k].ci]);
            cands.swap(kept);
        } else {
            std::nth_element(cands.begin(), cands.begin() + maxN, cands.end(),
                [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            cands.resize(maxN);
        }
        g_renderCullStats.partsBudgetCulled = afterCull - maxN;
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        if (a.transparent != b.transparent) return !a.transparent && b.transparent;
        if (a.transparent) return a.d2 > b.d2;   // vetro: dal piu' lontano
        // Opache dalla piu' vicina: la GPU scarta subito i pixel coperti.
        return a.d2 < b.d2;
    });
    // Due giri: prima le opache "a gruppi" (instancing), poi le altre una alla volta
    // (trasparenti per ultime, nell'ordine dietro -> davanti di prima).
    partInstBegin();
    static std::vector<int> single;
    single.clear();
    for (int pass = 0; pass < 2; ++pass)
    for (int ci = 0; ci < (pass == 0 ? (int)cands.size() : (int)single.size()); ++ci) {
        const auto& c = pass == 0 ? cands[ci] : cands[single[ci]];
        auto& p = g_world.parts[c.i];
        if (partFileMeshReady(p)) {
            if (pass == 0 && ci + 1 == (int)cands.size()) partInstFlush(view, proj, sun, studId);
            continue;
        }
        glm::mat4 m = cachedPartModel(c.i, p);
        const float dist2 = c.d2;
        bool useStud = p.useStud && g_engine.quality.studTextures
            && !p.isNeon() && p.material == PartMaterial::Plastic
            && p.shape == PartShape::Block;
        if (useStud && dist2 > studOff2) {
            useStud = false;
            ++g_renderCullStats.partsStudLod;
        }
        PartMeshLod meshLod = (dist2 > shapeLow2) ? PartMeshLod::Low : PartMeshLod::High;
        GLuint drawVao = vao;
        int vertCount = 36;
        if (ShapeMesh* sm = g_partMeshes.meshForLod(p.shape, meshLod)) {
            if (meshLod == PartMeshLod::Low)
                ++g_renderCullStats.partsShapeLod;
            drawVao = sm->vao;
            vertCount = sm->vertexCount;
        } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
            drawVao = g_partMeshes.wedge.vao;
            vertCount = g_partMeshes.wedge.vertexCount;
        }
        PartMaterial drawMat = p.material;
        if (p.transparency > 0.05f && drawMat != PartMaterial::ForceField
            && drawMat != PartMaterial::Neon)
            drawMat = PartMaterial::Glass;
        glm::vec3 drawCol = p.color;
        if (p.transparency > 0.05f)
            drawCol = glm::mix(drawCol, glm::vec3(0.85f, 0.9f, 1.f), p.transparency * 0.35f);
        if (p.negative) {   // NegativePart di Roblox: rosa trasparente
            drawCol = {1.f, 0.45f, 0.62f};
            drawMat = PartMaterial::Glass;
        }
        if (g_viewCollision && !p.canCollide)
            drawCol = glm::mix(drawCol, glm::vec3(1.f, 0.2f, 0.2f), 0.55f);
        if (g_viewWireframe)
            drawCol *= 0.55f;
        if (pass == 0) {
            if (partInstCanBatch(drawMat)) {
                partInstAdd(useStud, drawVao, vertCount, drawMat, m, drawCol, p.effectiveEmissive());
                ++g_renderCullStats.partsDrawn;
            } else {
                single.push_back(ci);
            }
            if (ci + 1 == (int)cands.size()) partInstFlush(view, proj, sun, studId);
            continue;
        }
        drawCube(drawVao, useStud ? studP : partP, m, view, proj, drawCol, sun, useStud, studId,
                 vertCount, p.effectiveEmissive(), drawMat);
        ++g_renderCullStats.partsDrawn;
    }
}

static void passStudioGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                             const glm::mat4& proj, const glm::vec3& sun) {
    drawSelectedPartBlueOutline(vao, partP, view, proj);
    // Le maniglie ora le disegna drawStudioGizmoOverlay (2D, stile Roblox).
    (void)&drawScaleHandles; (void)&drawMoveHandles; (void)&drawRotateHandles;
}

static void passWorkspaceModels(GLuint meshP, GLuint skinP, const glm::mat4& view,
                                const glm::mat4& proj, const glm::vec3& sun) {
    drawWorkspaceModels(meshP, skinP, view, proj, sun);
}

static glm::mat4 adornPartMatrix(const WorkspacePart& p, const glm::vec3& localStuds, const glm::vec3& sizeStuds) {
    return glm::translate(glm::mat4(1.f), p.position)
         * partRotationMatrix(p.rotation)
         * glm::translate(glm::mat4(1.f), localStuds)
         * glm::scale(glm::mat4(1.f), sizeStuds);
}

static PartMaterial faceImageMaterial(const std::string& image) {
    if (image.empty()) return PartMaterial::SmoothPlastic;
    const PartMaterial m = partMaterialFromName(image);
    return partMaterialTextureStem(m) ? m : PartMaterial::SmoothPlastic;
}

static void drawFaceSlab(GLuint vao, GLuint partP, const WorkspacePart& p, int face,
                         const glm::vec3& col, float transparency, PartMaterial mat,
                         const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    face = std::clamp(face, 0, 5);
    const glm::vec3 normals[6] = {
        {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f},
        {-1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}
    };
    const glm::vec3 axis = normals[face];
    const glm::vec3 half = p.size * 0.5f;
    const float out = std::max(0.01f, 0.004f * std::max(p.size.x, std::max(p.size.y, p.size.z)));
    const glm::vec3 local = axis * (std::abs(glm::dot(axis, half)) + out);
    glm::vec3 slab = glm::max(p.size * 0.92f, glm::vec3(kMinPartSize));
    if (std::abs(axis.x) > 0.5f) slab.x = out * 2.f;
    if (std::abs(axis.y) > 0.5f) slab.y = out * 2.f;
    if (std::abs(axis.z) > 0.5f) slab.z = out * 2.f;
    const PartMaterial drawMat = transparency > 0.05f && mat == PartMaterial::SmoothPlastic
        ? PartMaterial::Glass : mat;
    drawCube(vao, partP, adornPartMatrix(p, local, slab), view, proj, col, sun,
             false, 0, 36, 0.25f, drawMat);
}

static void drawPartDecal(GLuint vao, GLuint partP, const WorkspacePart& p,
                          const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    if (p.decalEnabled && p.decalTransparency < 0.98f) {
        const int face = (int)std::lround(p.decalFace);
        const PartMaterial mat = p.decalImage.empty()
            ? PartMaterial::SmoothPlastic : faceImageMaterial(p.decalImage);
        drawFaceSlab(vao, partP, p, face, glm::vec3(p.decalR, p.decalG, p.decalB),
                     p.decalTransparency, mat, view, proj, sun);
    }
    for (const auto& fd : p.extraDecals) {
        if (fd.transparency >= 0.98f) continue;
        drawFaceSlab(vao, partP, p, fd.face, glm::vec3(fd.r, fd.g, fd.b),
                     fd.transparency, faceImageMaterial(fd.image), view, proj, sun);
    }
}

static void drawPartTexture(GLuint vao, GLuint partP, const WorkspacePart& p,
                            const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    if (!p.textureEnabled) return;
    const int face = std::clamp((int)std::lround(p.textureFace), 0, 5);
    const glm::vec3 col(p.textureR, p.textureG, p.textureB);
    if (!p.textureImage.empty()) {
        drawFaceSlab(vao, partP, p, face, col, 0.f, faceImageMaterial(p.textureImage), view, proj, sun);
        return;
    }
    const int tiles = std::clamp((int)std::lround(p.textureStuds), 1, 8);
    const glm::vec3 normals[6] = {
        {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f},
        {-1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}
    };
    const glm::vec3 axis = normals[face];
    const glm::vec3 half = p.size * 0.5f;
    const float out = std::max(0.012f, 0.005f * std::max(p.size.x, std::max(p.size.y, p.size.z)));
    glm::vec3 u(0.f), v(0.f);
    if (std::abs(axis.y) > 0.5f) { u = {1, 0, 0}; v = {0, 0, 1}; }
    else if (std::abs(axis.x) > 0.5f) { u = {0, 0, 1}; v = {0, 1, 0}; }
    else { u = {1, 0, 0}; v = {0, 1, 0}; }
    const float su = std::abs(glm::dot(u, p.size));
    const float sv = std::abs(glm::dot(v, p.size));
    for (int i = 0; i < tiles; ++i) {
        if ((i & 1) == 0) continue;
        const float t = ((float)i + 0.5f) / (float)tiles - 0.5f;
        glm::vec3 local = axis * (std::abs(glm::dot(axis, half)) + out) + u * (t * su);
        glm::vec3 slab = glm::max(p.size / glm::vec3((float)tiles), glm::vec3(kMinPartSize));
        slab = u * (su / (float)tiles * 0.82f) + v * (sv * 0.9f) + axis * (out * 2.f);
        drawCube(vao, partP, adornPartMatrix(p, local, glm::max(slab, glm::vec3(kMinPartSize))),
                 view, proj, col, sun, false, 0, 36, 0.15f, PartMaterial::SmoothPlastic);
    }
}

static void drawBarBetween(GLuint vao, GLuint partP, const glm::vec3& a, const glm::vec3& b,
                           float width, const glm::vec3& col,
                           const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    if (len < 0.02f) return;
    const glm::vec3 dir = d / len;
    const glm::vec3 helper = std::abs(dir.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    const glm::vec3 x = glm::normalize(glm::cross(helper, dir));
    const glm::vec3 y = glm::normalize(glm::cross(dir, x));
    const float w = std::max(width, kMinPartSize);
    glm::mat4 m(1.f);
    m[0] = glm::vec4(x * w, 0.f);
    m[1] = glm::vec4(y * w, 0.f);
    m[2] = glm::vec4(dir * len, 0.f);
    m[3] = glm::vec4((a + b) * 0.5f, 1.f);
    drawCube(vao, partP, m, view, proj, col, sun, false, 0, 36, 0.35f, PartMaterial::Neon);
}

static bool attachmentWorldPos(const ScriptControls::ScriptAttachment& a, glm::vec3& out) {
    if (a.partIndex < 0 || a.partIndex >= (int)g_world.parts.size()) return false;
    const WorkspacePart& p = g_world.parts[(size_t)a.partIndex];
    out = p.position + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(a.ox, a.oy, a.oz, 0.f));
    return true;
}

static void drawBeamsTrailsAttachments(GLuint vao, GLuint partP, const glm::mat4& view,
                                       const glm::mat4& proj, const glm::vec3& sun) {
    const auto& sc = scriptControls();
    for (const auto& a : sc.attachments) {
        glm::vec3 w;
        if (!attachmentWorldPos(a, w)) continue;
        glm::mat4 m = glm::translate(glm::mat4(1.f), w) * glm::scale(glm::mat4(1.f), glm::vec3(0.18f));
        drawCube(vao, partP, m, view, proj, {1.f, 0.35f, 0.15f}, sun, false, 0, 36, 0.4f, PartMaterial::Neon);
    }
    for (const auto& b : sc.beams) {
        if (!b.enabled) continue;
        if (b.a0 < 0 || b.a1 < 0 || b.a0 >= (int)sc.attachments.size() || b.a1 >= (int)sc.attachments.size())
            continue;
        glm::vec3 p0, p1;
        if (!attachmentWorldPos(sc.attachments[(size_t)b.a0], p0)) continue;
        if (!attachmentWorldPos(sc.attachments[(size_t)b.a1], p1)) continue;
        drawBarBetween(vao, partP, p0, p1, std::max(b.width0, b.width1), {b.r, b.g, b.b}, view, proj, sun);
    }
    for (const auto& t : sc.trails) {
        if (!t.enabled || t.pts.size() < 2) continue;
        for (size_t i = 1; i < t.pts.size(); ++i) {
            const float fade = 1.f - t.age[i] / std::max(0.05f, t.life);
            drawBarBetween(vao, partP, t.pts[i - 1], t.pts[i], t.width * std::max(0.15f, fade),
                           glm::vec3(t.r, t.g, t.b) * std::max(0.25f, fade), view, proj, sun);
        }
    }
}

static void drawPartHighlightBody(GLuint vao, GLuint partP, const WorkspacePart& p,
                                  const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    if (!p.highlightEnabled) return;
    WorkspacePart grown = p;
    grown.size = glm::max(p.size * 1.045f, p.size + glm::vec3(0.02f));
    drawCube(vao, partP, partModelMatrix(grown), view, proj,
             glm::vec3(p.highlightR, p.highlightG, p.highlightB), sun,
             false, 0, 36, 0.45f, PartMaterial::ForceField);
}

static void drawScriptAdornments3D(GLuint vao, GLuint partP, const glm::mat4& view,
                                   const glm::mat4& proj, const glm::vec3& sun) {
    for (const auto& p : g_world.parts) {
        if (p.transparency >= 0.99f) continue;
        drawPartDecal(vao, partP, p, view, proj, sun);
        drawPartTexture(vao, partP, p, view, proj, sun);
        drawPartHighlightBody(vao, partP, p, view, proj, sun);
    }
    drawBeamsTrailsAttachments(vao, partP, view, proj, sun);
}

static void passAvatars(GLuint vao, GLuint partP, GLuint meshP, GLuint skinP,
                        AnimatedModel* avatarGlb, bool useGlb,
                        const glm::mat4& view, const glm::mat4& proj,
                        const glm::vec3& sun) {
    bool drawGlb = useGlb && avatarGlb && avatarGlb->loaded
                && g_player.humanoid.jumpArmBlend < 0.15f
                && !g_walking
                && !(g_firstPerson && (g_screen == AppScreen::Playing
                    || (g_screen == AppScreen::Studio && g_studioTesting)));
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext bctx{ playerVao, partP, view, proj, sun };

    if (drawGlb) {
        avatarGlb->update(0.f);
        glm::mat4 fm = avatarGlb->placement(5.5f, g_playerPos.y,
            { g_playerPos.x, 0.f, g_playerPos.z });
        fm = glm::translate(glm::mat4(1.f), g_playerPos)
           * glm::rotate(glm::mat4(1.f), glm::radians(g_playerYaw + 90.f), {0,1,0})
           * glm::translate(glm::mat4(1.f), -g_playerPos)
           * fm;
        glUseProgram(skinP);
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(fm));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
        avatarGlb->draw(skinP);
        // Remoti anche con avatar GLB locale (prima sparivano).
        if (g_remoteBodies.size() < g_net.remotes.size())
            g_remoteBodies.resize(g_net.remotes.size());
        for (size_t i = 0; i < g_net.remotes.size(); ++i) {
            const auto& r = g_net.remotes[i];
            if (i < g_remoteBodies.size())
                g_remoteBodies[i].draw(bctx, drawCubePlayer, r.displayPos, r.displayYaw);
        }
    } else {
        const bool hideLocalFp = g_firstPerson
            && (g_screen == AppScreen::Playing
                || (g_screen == AppScreen::Studio && g_studioTesting))
            && !g_ragdoll.active;
        if (g_ragdoll.active) {
            g_ragdoll.draw(drawCubePlayer, playerVao, partP, view, proj, sun);
        } else if (!hideLocalFp) {
            bool hideBody[(int)AvatarSlot::Count] = {};
            fillBodyHideMask(g_catalog, hideBody);
            float armHold = (effectiveHeldSlot() >= 0) ? kEquippedArmHoldDeg : 0.f;
            const BackpackItem* heldIt = nullptr;
            {
                const int hs = effectiveHeldSlot();
                if (hs >= 0 && hs < (int)g_world.backpack.size())
                    heldIt = &g_world.backpack[hs];
            }
            const std::vector<HoldPartPose>* holds =
                (heldIt && !heldIt->holdPose.empty()) ? &heldIt->holdPose : nullptr;
            if (holds && findHoldPose(*holds, "Right Arm"))
                armHold = 0.f;
            g_player.draw(bctx, drawCubePlayer, g_playerPos, g_playerYaw, hideBody, armHold, holds);
            drawAvatarAccessories(meshP, view, proj, sun,
                g_playerPos, g_playerYaw, g_player.humanoid, vao, partP);
            drawEquippedSword(vao, partP, view, proj, sun);
            drawEquippedTorch(vao, partP, view, proj, sun);
            drawEquippedHandle(vao, partP, view, proj, sun);
            drawEquippedItemMesh(vao, partP, meshP, view, proj, sun);
        } else {
            drawEquippedSword(vao, partP, view, proj, sun);
            drawEquippedTorch(vao, partP, view, proj, sun);
            drawEquippedHandle(vao, partP, view, proj, sun);
            drawEquippedItemMesh(vao, partP, meshP, view, proj, sun);
        }
        if (g_remoteBodies.size() < g_net.remotes.size())
            g_remoteBodies.resize(g_net.remotes.size());
        for (size_t i = 0; i < g_net.remotes.size(); ++i) {
            const auto& r = g_net.remotes[i];
            if (i < g_remoteBodies.size())
                g_remoteBodies[i].draw(bctx, drawCubePlayer, r.displayPos, r.displayYaw);
        }
    }
    drawFriendPlayers(vao, partP, view, proj, sun);
    drawNpcActors(vao, partP, view, proj, sun);
    drawScriptAdornments3D(vao, partP, view, proj, sun);
}

// prepara(luci+occluder) / ombre / terreno / part / modelli / avatar
static float g_renderPassMs[6] = {};

static void renderWorld3D(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                          GLuint skinP, GLuint meshP, AnimatedModel* avatarGlb, bool useGlb,
                          const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    // Tempo CPU di ogni pass (media mobile), per capire dove va il "Mondo 3D".
    using clk = std::chrono::steady_clock;
    auto t = clk::now();
    auto lap = [&](int k) {
        const auto n = clk::now();
        const float ms = std::chrono::duration<float, std::milli>(n - t).count();
        g_renderPassMs[k] = g_renderPassMs[k] * 0.95f + ms * 0.05f;
        t = n;
        // BLOCKY_PROFILE: i frame lenti mostrano quale pass si prende il tempo
        static const char* const kPass[6] = {"pass prepara", "pass ombre", "pass terreno",
                                             "pass part", "pass modelli", "pass avatar"};
        plat::prof::mark(kPass[k]);
    };
    plat::prof::mark("inizio mondo 3D");
    passPrepare(view, proj);                                        lap(0);
    passShadows(vao, meshP);                                        lap(1);
    passTerrain(vao, partP, view, proj, sun);                       lap(2);
    passOpaqueParts(vao, studP, partP, studId, view, proj, sun);    lap(3);
    passStudioGizmos(vao, partP, view, proj, sun);
    passWorkspaceModels(meshP, skinP, view, proj, sun);             lap(4);
    passAvatars(vao, partP, meshP, skinP, avatarGlb, useGlb, view, proj, sun); lap(5);
}
