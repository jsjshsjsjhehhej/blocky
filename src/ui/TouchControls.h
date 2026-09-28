#pragma once

/**
 * Joystick e pulsante di salto per gli schermi a tocco.
 *
 * Non introduce un secondo sistema di movimento: scrive dentro lo stato del
 * GAMEPAD, che il motore sa gia' leggere (pad.lx / pad.ly / pad.a in main.cpp).
 * Cosi' il movimento e' analogico davvero — piu' sposti il pollice, piu'
 * corri — e nessuna riga di fisica, animazione o rete cambia.
 *
 * L'input NON passa da ImGui. Qui si dichiarano solo le zone che i comandi
 * occupano sullo schermo, e lo strato Android assegna ogni dito a un compito
 * solo: joystick, salto o telecamera. Passare da ImGui vorrebbe dire un mouse
 * solo per tutti e tre, che e' esattamente il difetto da cui si e' partiti —
 * il dito sul joystick faceva girare anche la visuale, e il dito del salto
 * faceva dimenticare quello che stava camminando.
 */

#include <imgui.h>

#include <cmath>

#include "platform/GamepadInput.h"

#ifdef __ANDROID__
#include <GLFW/glfw3.h>   // lo shim: per dichiarargli le zone dei comandi
#endif

namespace blk {
namespace touch {

/** Su PC restano spenti; si possono accendere per provarli col mouse. */
inline bool& forceEnabled() { static bool b = false; return b; }

inline bool active() {
#ifdef __ANDROID__
    return true;
#else
    return forceEnabled();
#endif
}

struct State {
    float x = 0.f, y = 0.f;      // -1..1, come una levetta analogica
    bool jumpDown = false;
    bool jumpPressed = false;    // solo il frame in cui viene toccato
};

inline State& state() { static State s; return s; }

// ---- Aspetto (stile Blocky: forme piene, bordi netti, niente sfumature) ----
inline constexpr float kStickRadius = 96.f;
inline constexpr float kKnobRadius  = 40.f;
inline constexpr float kJumpRadius  = 64.f;
inline constexpr float kMargin      = 46.f;
// A destra serve piu' spazio: li' Android ci mette la sua barra di navigazione
// (indietro, home, app recenti), che coprirebbe il pulsante di salto.
inline constexpr float kMarginRight  = 150.f;

/**
 * Disegna i comandi. Da chiamare dentro il frame ImGui, solo mentre si gioca.
 *
 * NON usa pulsanti ImGui, e la scelta e' voluta. Un pulsante ImGui cattura il
 * mouse, e su Android il mouse e' uno solo: il dito sul joystick diventava
 * anche il dito della telecamera, e appoggiando il secondo dito per saltare il
 * primo veniva dimenticato, interrompendo la camminata.
 *
 * Ora i comandi sono soltanto disegno: le zone che occupano vengono dichiarate
 * allo strato Android, che assegna ogni dito a un compito solo e manda i valori
 * su canali dedicati (blkShimStick* / blkShimJump*). Il risultato e' che si
 * puo' camminare, guardarsi intorno e saltare contemporaneamente.
 */
inline void draw() {
    State& st = state();
    st.jumpPressed = false;
    if (!active()) { st.x = st.y = 0.f; st.jumpDown = false; return; }

    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 screen = io.DisplaySize;

    const ImVec2 stickSize(kStickRadius * 2.f, kStickRadius * 2.f);
    const ImVec2 stickPos(kMargin, screen.y - stickSize.y - kMargin);
    const ImVec2 jumpSize(kJumpRadius * 2.f, kJumpRadius * 2.f);
    const ImVec2 jumpPos(screen.x - jumpSize.x - kMarginRight,
                         screen.y - jumpSize.y - kMargin);

#ifdef __ANDROID__
    // L'ordine conta: la zona 0 e' il joystick, la 1 e' il salto. Lo strato
    // Android le identifica per indice, non per posizione sullo schermo.
    blkShimAddUiRect(stickPos.x, stickPos.y, stickPos.x + stickSize.x, stickPos.y + stickSize.y);
    blkShimAddUiRect(jumpPos.x, jumpPos.y, jumpPos.x + jumpSize.x, jumpPos.y + jumpSize.y);

    st.x = blkShimStickX();
    st.y = blkShimStickY();
    st.jumpDown = blkShimJumpDown();
    st.jumpPressed = blkShimTakeJumpPressed();
#else
    // Su PC i comandi restano spenti; se li si accende a mano per provarli,
    // li pilota il mouse con lo stesso stato del tocco.
    if (io.MouseDown[0]) {
        const ImVec2 c(stickPos.x + kStickRadius, stickPos.y + kStickRadius);
        const float dx = (io.MousePos.x - c.x) / kStickRadius;
        const float dy = (io.MousePos.y - c.y) / kStickRadius;
        if (dx * dx + dy * dy <= 1.f) { st.x = dx; st.y = -dy; }
    } else {
        st.x = st.y = 0.f;
    }
#endif

    // Il disegno va sul livello di primo piano: non appartiene a nessuna
    // finestra, quindi non puo' rubare il tocco a nessuno.
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const bool moving = (std::fabs(st.x) > 0.02f || std::fabs(st.y) > 0.02f);

    // ---- Joystick, in basso a sinistra ----
    {
        const ImVec2 center(stickPos.x + kStickRadius, stickPos.y + kStickRadius);
        const float limit = kStickRadius - kKnobRadius * 0.4f;
        const ImVec2 knob(center.x + st.x * limit, center.y - st.y * limit);

        dl->AddCircleFilled(center, kStickRadius, IM_COL32(15, 25, 40, moving ? 130 : 95), 48);
        dl->AddCircle(center, kStickRadius, IM_COL32(255, 255, 255, moving ? 190 : 120), 48, 4.f);
        dl->AddCircleFilled(knob, kKnobRadius, IM_COL32(245, 248, 252, moving ? 250 : 205), 36);
        dl->AddCircle(knob, kKnobRadius, IM_COL32(40, 90, 170, 220), 36, 3.f);
    }

    // ---- Salto, in basso a destra ----
    {
        const ImVec2 center(jumpPos.x + kJumpRadius, jumpPos.y + kJumpRadius);
        const ImU32 fill = st.jumpDown ? IM_COL32(22, 190, 110, 245)
                                       : IM_COL32(16, 165, 92, 205);
        dl->AddCircleFilled(center, kJumpRadius, fill, 40);
        dl->AddCircle(center, kJumpRadius, IM_COL32(255, 255, 255, 210), 40, 4.f);

        // Freccia in su disegnata a mano: nessun file da caricare, quindi
        // niente da sbagliare fra una piattaforma e l'altra.
        const float a = kJumpRadius * 0.46f;
        const ImU32 ink = IM_COL32(255, 255, 255, 240);
        dl->AddTriangleFilled(ImVec2(center.x, center.y - a),
                              ImVec2(center.x - a * 0.85f, center.y + a * 0.12f),
                              ImVec2(center.x + a * 0.85f, center.y + a * 0.12f), ink);
        dl->AddRectFilled(ImVec2(center.x - a * 0.30f, center.y + a * 0.05f),
                          ImVec2(center.x + a * 0.30f, center.y + a * 0.75f), ink, 3.f);
    }
}

/**
 * Riversa lo stato dei comandi dentro il gamepad, subito dopo pollGamepad().
 *
 * Se un pad vero e' collegato non si tocca niente: quello ha la precedenza.
 */
inline void apply(GamepadState& pad) {
    if (!active()) return;
    const State& st = state();
    if (pad.connected) return;

    const bool moving = (std::fabs(st.x) > 0.02f || std::fabs(st.y) > 0.02f);
    if (!moving && !st.jumpDown && !st.jumpPressed) return;

    pad.connected = true;
    pad.lx = st.x;
    pad.ly = st.y;
    pad.a = st.jumpDown;
    pad.aPressed = st.jumpPressed;
}

} // namespace touch
} // namespace blk
