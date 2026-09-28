#pragma once

/**
 * L'unico posto dell'interfaccia che sa su quale dispositivo gira.
 *
 * ── Perche' esiste, e perche' e' uno solo ─────────────────────────────────
 * Le 6.729 righe di interfaccia di Blocky (263 pulsanti fra Hub, Studio, HUD
 * e Shop) non contengono una sola riga che nomini Android o Windows: ImGui
 * disegna, e disegna uguale ovunque. E' la ragione per cui il porting su
 * Android non ha richiesto di riscrivere nemmeno un pulsante, ed e' una
 * proprieta' da difendere, non da buttare via.
 *
 * Ma un'interfaccia pensata per il mouse su un monitor non va bene su un
 * telefono, e fingere di no e' peggio. Serviva quindi un posto — UNO — che
 * sappia dove gira, e da cui tutto il resto discenda.
 *
 * L'alternativa, cioe' spargere #ifdef __ANDROID__ dentro i pannelli, avrebbe
 * distrutto esattamente la proprieta' che rende il porting economico: con
 * quella strada iPhone significherebbe ripassare 263 pulsanti a mano.
 *
 * ── Su cosa si basa, e perche' ────────────────────────────────────────────
 * La misura che conta su un touch non e' il pixel, e' il POLPASTRELLO. Un
 * bersaglio sotto i ~9 mm non si colpisce in modo affidabile: e' anatomia,
 * non gusto. Tutte le linee guida (Android 48dp, Apple 44pt) dicono la stessa
 * cosa con numeri diversi.
 *
 * ── La proprieta' di sicurezza ────────────────────────────────────────────
 * Su PC la scala e' ESATTAMENTE 1.0 e i valori restano quelli di sempre:
 * qualunque cosa si scriva qui, l'aspetto su PC non cambia di un pixel.
 * Questo rende il file sicuro da modificare senza rimettere in discussione
 * tutto cio' che gia' funziona.
 */

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "platform/TargetPlatform.h"

namespace blk {
namespace ui {

struct Metrics {
    bool  touch = false;      // si tocca con le dita: niente hover, bersagli grandi
    float scale = 1.f;        // moltiplicatore generale di spaziature e testo
    float minTarget = 0.f;    // altezza minima di un controllo toccabile, in pixel
};

inline Metrics& metrics() { static Metrics m; return m; }

/** Comodo da leggere nei pannelli: `if (blk::ui::isTouch())`. */
inline bool  isTouch()   { return metrics().touch; }
inline float uiScale()   { return metrics().scale; }

/**
 * Altezza da dare a un controllo che deve essere toccabile.
 *
 * Su PC restituisce l'altezza chiesta e basta. Su touch la alza al minimo
 * anatomico, cosi' un pulsante disegnato a 28 pixel non diventa impossibile
 * da centrare col pollice.
 */
inline float touchHeight(float desired) {
    const Metrics& m = metrics();
    if (!m.touch) return desired;
    return std::max(desired * m.scale, m.minTarget);
}

/**
 * Ricalcola le misure per questo schermo. Da chiamare quando cambia la
 * dimensione della finestra (e all'avvio).
 *
 * La scala nasce dalla larghezza in pixel rispetto a un riferimento da
 * desktop: uno schermo da telefono e' piccolo in pixel ma vicinissimo agli
 * occhi, quindi ogni elemento deve occuparne una frazione maggiore.
 */
inline void update(int screenW, int screenH) {
    Metrics& m = metrics();

#if defined(__ANDROID__) || defined(__APPLE_IOS__)
    m.touch = true;
#else
    m.touch = false;
#endif

    if (!m.touch) {
        // PC: nessuna modifica, mai. Vedi "proprieta' di sicurezza" sopra.
        m.scale = 1.f;
        m.minTarget = 0.f;
        return;
    }

    // Il lato corto e' quello che conta: in orizzontale su un telefono lo
    // spazio verticale e' pochissimo, ed e' li' che i pannelli non ci stanno.
    const float shortSide = (float)std::min(screenW, screenH);

    // 720 px di lato corto e' un telefono tipico; sotto si stringe ancora.
    m.scale = std::clamp(shortSide / 620.f, 1.15f, 2.0f);

    // ~9 mm tradotti in pixel: su questi schermi sta fra 44 e 64 px.
    m.minTarget = std::max(44.f, shortSide * 0.075f);
}

/**
 * Riversa le misure nello stile di ImGui.
 *
 * E' QUESTA la riga che vale per tutti i 263 pulsanti insieme: FramePadding
 * entra nell'altezza di ogni pulsante, campo e casella disegnati da ImGui,
 * quindi ingrandirlo qui li ingrandisce tutti senza toccarne nessuno.
 *
 * Va chiamata DOPO il tema (applyClassicRobloxTheme / applyStudioTheme), che
 * riscrive lo stile da capo leggendo assets/ui/theme.json: invertendo
 * l'ordine il tema cancellerebbe queste misure e sul telefono non
 * cambierebbe nulla — con il file giusto, la chiamata giusta, e nessun
 * errore da nessuna parte.
 */
inline void applyToStyle(ImGuiStyle& st) {
    const Metrics& m = metrics();
    if (!m.touch) return;

    st.FramePadding  = ImVec2(st.FramePadding.x * m.scale, st.FramePadding.y * m.scale);
    st.ItemSpacing   = ImVec2(st.ItemSpacing.x * m.scale, st.ItemSpacing.y * m.scale);
    st.ItemInnerSpacing = ImVec2(st.ItemInnerSpacing.x * m.scale, st.ItemInnerSpacing.y * m.scale);
    st.WindowPadding = ImVec2(st.WindowPadding.x * m.scale, st.WindowPadding.y * m.scale);

    // Barre di scorrimento e maniglie: si afferrano col dito, non col mouse.
    st.ScrollbarSize = std::max(st.ScrollbarSize * m.scale, m.minTarget * 0.5f);
    st.GrabMinSize   = std::max(st.GrabMinSize * m.scale, m.minTarget * 0.5f);

    // Il dito copre cio' che sta toccando: un po' di raggio in piu' aiuta a
    // capire dove finisce un controllo quando non lo si vede.
    st.FrameRounding  = st.FrameRounding * 1.2f;
    st.GrabRounding   = st.GrabRounding * 1.2f;

    // Nessun hover su un touch: i riquadri di passaggio del mouse non
    // appaiono mai, quindi il bordo attivo deve bastare da solo.
    st.FrameBorderSize = std::max(st.FrameBorderSize, 1.f);
}

} // namespace ui
} // namespace blk
