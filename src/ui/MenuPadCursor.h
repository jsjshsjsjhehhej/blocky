#pragma once

/**
 * Cursore pad nei menu: stick sinistro punta alle voci, A / X / L (LB) clicca.
 */

#include <imgui.h>
#include "platform/GamepadInput.h"

#include <algorithm>
#include <cmath>

struct MenuPadCursor {
    float x = -1.f;
    float y = -1.f;
    bool  active = false;
    bool  confirmHeld = false;
    bool  clickLatched = false;
};

inline MenuPadCursor& menuPadCursor() {
    static MenuPadCursor c;
    return c;
}

/** True se A (Cross), X (Square) o L/LB — conferma click menu. */
inline bool menuPadConfirmHeld(const GamepadState& pad) {
    return pad.a || pad.x || pad.lb;
}

/**
 * Chiama DOPO ImGui_ImplGlfw_NewFrame e PRIMA di ImGui::NewFrame.
 * Inietta posizione mouse + click dal pad.
 */
inline void applyMenuPadCursor(bool enable, float dt, float screenW, float screenH) {
    auto& c = menuPadCursor();
    const auto& pad = gamepad();

    if (!enable || !pad.connected || screenW < 1.f || screenH < 1.f) {
        if (c.clickLatched) {
            ImGui::GetIO().AddMouseButtonEvent(0, false);
            c.clickLatched = false;
        }
        c.active = false;
        c.confirmHeld = false;
        return;
    }

    c.active = true;
    if (c.x < 0.f || c.y < 0.f) {
        c.x = screenW * 0.5f;
        c.y = screenH * 0.45f;
    }

    // Stick: ly positivo = su schermo (Y decresce)
    float spd = 1100.f;
    float mag = std::sqrt(pad.lx * pad.lx + pad.ly * pad.ly);
    if (mag > 0.08f) {
        // velocità cresce con deflessione (più preciso vicino al centro)
        float boost = 0.45f + 0.55f * std::min(1.f, mag);
        c.x += pad.lx * spd * boost * dt;
        c.y -= pad.ly * spd * boost * dt;
    }
    // D-pad: nudge preciso
    float nudge = 420.f * dt;
    if (pad.dpadL) c.x -= nudge;
    if (pad.dpadR) c.x += nudge;
    if (pad.dpadU) c.y -= nudge;
    if (pad.dpadD) c.y += nudge;

    c.x = std::clamp(c.x, 0.f, screenW - 1.f);
    c.y = std::clamp(c.y, 0.f, screenH - 1.f);

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(c.x, c.y);

    bool confirm = menuPadConfirmHeld(pad);
    if (confirm && !c.confirmHeld) {
        io.AddMouseButtonEvent(0, true);
        c.clickLatched = true;
    } else if (!confirm && c.confirmHeld && c.clickLatched) {
        io.AddMouseButtonEvent(0, false);
        c.clickLatched = false;
    }
    c.confirmHeld = confirm;
}

/** Disegna il puntatore pad (dopo i widget menu). */
inline void drawMenuPadCursor() {
    const auto& c = menuPadCursor();
    if (!c.active) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 ring = c.confirmHeld ? IM_COL32(80, 220, 120, 255) : IM_COL32(255, 220, 60, 255);
    ImU32 fill = c.confirmHeld ? IM_COL32(40, 180, 90, 200) : IM_COL32(255, 255, 255, 210);
    dl->AddCircleFilled({c.x, c.y}, 7.f, fill, 16);
    dl->AddCircle({c.x, c.y}, 14.f, ring, 20, 2.5f);
    dl->AddLine({c.x - 18.f, c.y}, {c.x + 18.f, c.y}, ring, 1.5f);
    dl->AddLine({c.x, c.y - 18.f}, {c.x, c.y + 18.f}, ring, 1.5f);
}
