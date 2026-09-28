/**
 * Implementazione di libcontroller_touch. Vedi controller_touch.h per il
 * perche'; qui c'e' solo il come.
 *
 * Volutamente senza dipendenze: <cmath> e basta. E' cio' che la rende
 * riusabile da libblockyvr.so e da un domani su console.
 */

#include "controller_touch.h"

#include <cmath>

namespace {

struct State {
    float x = 0.f, y = 0.f;
    float w = 0.f, h = 0.f;
    bool  prevConfirm = false;
    bool  started = false;
    CtlTouchOutput last{};
};

State& state() { static State s; return s; }

/**
 * Zona morta.
 *
 * Senza, il cursore va alla deriva da solo: nessuno stick torna esattamente a
 * zero, e uno scarto dell'1% diventa un movimento costante che in un minuto
 * porta il cursore in un angolo.
 */
float deadzone(float v, float dz) {
    const float a = std::fabs(v);
    if (a < dz) return 0.f;
    // Riscalato, cosi' subito fuori dalla zona morta il cursore parte piano
    // invece di scattare alla velocita' della soglia.
    const float scaled = (a - dz) / (1.f - dz);
    return (v < 0.f ? -scaled : scaled);
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

extern "C" {

void ctlTouchReset(float screenW, float screenH) {
    State& s = state();
    s.w = screenW;
    s.h = screenH;
    s.x = screenW * 0.5f;
    s.y = screenH * 0.5f;
    s.prevConfirm = false;
    s.started = true;
    s.last = CtlTouchOutput{};
}

void ctlTouchUpdate(const CtlTouchInput* in, CtlTouchOutput* out) {
    State& s = state();
    CtlTouchOutput o{};

    if (!in) { if (out) *out = o; s.last = o; return; }

    if (!s.started || s.w != in->screenW || s.h != in->screenH) {
        if (in->screenW > 0.f && in->screenH > 0.f)
            ctlTouchReset(in->screenW, in->screenH);
    }

    if (!in->padPresent) {
        // Pad staccato: il dito si solleva, se era giu'. Altrimenti un
        // controller scollegato mentre si tiene premuto lascerebbe il gioco
        // convinto che ci sia un dito appoggiato per sempre.
        o.released = s.prevConfirm ? 1 : 0;
        o.cursorX = s.x;
        o.cursorY = s.y;
        s.prevConfirm = false;
        if (out) *out = o;
        s.last = o;
        return;
    }

    // Velocita' proporzionale alla diagonale dello schermo: su un tablet
    // grande il cursore deve percorrere piu' pixel nello stesso tempo, o
    // attraversarlo diventa lentissimo.
    const float diag = std::sqrt(s.w * s.w + s.h * s.h);
    const float speed = diag * 0.75f;   // ~mezzo schermo in poco piu' di mezzo secondo

    const float dx = deadzone(in->stickX, 0.18f);
    const float dy = deadzone(in->stickY, 0.18f);

    // Accelerazione dolce: la velocita' cresce col QUADRATO dell'inclinazione,
    // cosi' si puo' sia puntare con precisione (stick appena inclinato) sia
    // attraversare lo schermo (stick a fondo corsa).
    //
    // Moltiplicare la componente per la lunghezza del vettore da esattamente
    // questo: direzione * lunghezza², e a fondo corsa (lunghezza 1) resta la
    // velocita' piena.
    const float mag = std::sqrt(dx * dx + dy * dy);

    const float dt = (in->dt > 0.f && in->dt < 0.25f) ? in->dt : (1.f / 60.f);
    s.x += dx * mag * speed * dt;
    // Lo stick punta in alto col valore positivo, lo schermo cresce in basso.
    s.y -= dy * mag * speed * dt;

    s.x = clampf(s.x, 0.f, s.w > 0.f ? s.w - 1.f : 0.f);
    s.y = clampf(s.y, 0.f, s.h > 0.f ? s.h - 1.f : 0.f);

    const bool now = in->confirmDown != 0;
    o.cursorX   = s.x;
    o.cursorY   = s.y;
    o.touchDown = now ? 1 : 0;
    o.pressed   = (now && !s.prevConfirm) ? 1 : 0;
    o.released  = (!now && s.prevConfirm) ? 1 : 0;
    o.visible   = 1;
    s.prevConfirm = now;

    if (out) *out = o;
    s.last = o;
}

void ctlTouchLast(CtlTouchOutput* out) {
    if (out) *out = state().last;
}

const char* ctlTouchVersion(void) {
    return "libcontroller_touch 1.0 (pad -> tocco)";
}

} // extern "C"
