#pragma once

/**
 * Dove finiscono i millisecondi di ogni frame.
 *
 * ── Perche' esiste ────────────────────────────────────────────────────────
 * Blocky misurava una cosa sola: gli FPS.
 *
 *     [perf] 42.3 fps
 *
 * Quel numero dice CHE e' lento, mai PERCHE'. Se si scende da 60 a 30, non
 * distingue fra troppe draw call, riempimento della GPU, fisica, script Lua
 * o mappa delle ombre. E senza sapere quale, si ottimizza a caso.
 *
 * ── Il vero motivo per cui i motori diventano lenti ───────────────────────
 * Non e' l'eta': e' che ogni funzione nuova aggiunge 0,3 ms e nessuno se ne
 * accorge. Dopo due anni si e' a 30 fps e non si sa quale delle duecento
 * aggiunte sia stata, perche' nessuna singola aggiunta e' mai sembrata
 * responsabile. Con la misura divisa per fase, quel mezzo millisecondo si
 * vede il giorno in cui compare, quando costa ancora poco toglierlo.
 *
 * ── Cosa costa ────────────────────────────────────────────────────────────
 * Due letture di orologio per fase, una dozzina di fasi: qualche decina di
 * nanosecondi su un frame da 16 milioni. E' cosi' poco che resta acceso
 * SEMPRE, anche nelle build pubblicate — un profiler che si accende solo
 * quando lo si cerca non misura mai il caso che interessa, cioe' la macchina
 * dell'utente mentre gioca davvero.
 *
 * ── Cosa NON misura, e va detto ───────────────────────────────────────────
 * Questi sono tempi di CPU. Una chiamata OpenGL ritorna subito: mette un
 * comando in coda e la GPU lo esegue dopo. Quindi "Mondo 3D: 2 ms" vuol dire
 * "la CPU ci ha messo 2 ms a ORDINARE il disegno", non "la GPU ha disegnato
 * in 2 ms".
 *
 * Il sintomo di una GPU sovraccarica e' riconoscibile lo stesso: tutte le
 * fasi risultano rapide, la somma e' molto minore del tempo di frame vero, e
 * il tempo sparito e' nello scambio dei buffer (la CPU aspetta la GPU). Per
 * questo esiste la fase Presentazione: e' li' che si vede.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace plat {
namespace prof {

enum class Phase {
    Input,        // eventi, tastiera, mouse, pad
    Network,      // pacchetti in arrivo e in partenza
    Scripts,      // Lua: heartbeat ed eventi
    Physics,      // giocatore, collisioni, ragdoll
    Shadows,      // mappa delle ombre
    World,        // la scena 3D
    PostFx,       // bloom, ricostruzione, nitidezza
    Ui,           // ImGui: costruzione e disegno
    Present,      // scambio buffer: qui la CPU aspetta la GPU
    Count
};

inline const char* phaseName(Phase p) {
    switch (p) {
    case Phase::Input:    return "Input";
    case Phase::Network:  return "Rete";
    case Phase::Scripts:  return "Script";
    case Phase::Physics:  return "Fisica";
    case Phase::Shadows:  return "Ombre";
    case Phase::World:    return "Mondo 3D";
    case Phase::PostFx:   return "Post-effetti";
    case Phase::Ui:       return "Interfaccia";
    case Phase::Present:  return "Presentazione";
    default:              return "?";
    }
}

struct PhaseStat {
    float lastMs = 0.f;
    float avgMs  = 0.f;   // media mobile: quella che si legge senza impazzire
    float peakMs = 0.f;   // il picco recente: e' lui a causare gli scatti
};

struct State {
    PhaseStat phase[(int)Phase::Count];
    float frameMs = 0.f;
    float frameAvgMs = 0.f;
    float peakDecayAcc = 0.f;
    bool  enabled = true;
};

inline State& state() { static State s; return s; }

inline double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

inline void addSample(Phase p, float ms) {
    if (!state().enabled) return;
    PhaseStat& s = state().phase[(int)p];
    s.lastMs = ms;
    // 0.1 = circa dieci frame di memoria: abbastanza stabile da leggersi,
    // abbastanza reattiva da mostrare subito un peggioramento.
    s.avgMs = s.avgMs * 0.9f + ms * 0.1f;
    if (ms > s.peakMs) s.peakMs = ms;
}

/**
 * Misura una fase per la durata del blocco in cui vive.
 *
 * Si usa cosi':
 *     { PROF_SCOPE(Phase::World); renderWorld3D(...); }
 *
 * Misura anche se dal blocco si esce con un return o un'eccezione, che e' il
 * motivo per cui e' uno scope e non una coppia begin/end: una fase che esce
 * presto non deve sparire dalla misura proprio quando e' interessante.
 */
struct Scope {
    Phase p;
    double t0;
    explicit Scope(Phase phase) : p(phase), t0(nowMs()) {}
    ~Scope() { addSample(p, (float)(nowMs() - t0)); }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

/**
 * Per le regioni che NON stanno dentro un blocco solo.
 *
 * Il disegno del mondo comincia con beginScene e finisce trecento righe
 * dopo, attraversando piu rami: uno Scope non ci sta. Qui si segna linizio
 * e si chiude a mano.
 */
inline double* manualStarts() { static double t[(int)Phase::Count] = {0}; return t; }
inline void begin(Phase p) { manualStarts()[(int)p] = nowMs(); }
inline void end(Phase p) { addSample(p, (float)(nowMs() - manualStarts()[(int)p])); }

/** Chiudi il frame: aggiorna il totale e fa calare i picchi. */
inline void endFrame(float dt) {
    State& s = state();
    if (!s.enabled) return;

    s.frameMs = dt * 1000.f;
    s.frameAvgMs = s.frameAvgMs * 0.9f + s.frameMs * 0.1f;

    // I picchi calano ogni due secondi, invece di restare per sempre: un
    // singolo scatto all'avvio non deve sporcare la lettura per un'ora.
    s.peakDecayAcc += dt;
    if (s.peakDecayAcc >= 2.f) {
        s.peakDecayAcc = 0.f;
        for (auto& ph : s.phase) ph.peakMs *= 0.5f;
    }
}

/**
 * Quanto del frame NON e' spiegato dalle fasi misurate.
 *
 * Se e' grande, il tempo se ne va da qualche parte che non stiamo guardando:
 * o manca una sonda, o la CPU sta aspettando la GPU. In entrambi i casi e'
 * l'informazione piu' utile della tabella, ed e' proprio quella che gli FPS
 * da soli non possono dare.
 */
inline float unaccountedMs() {
    const State& s = state();
    float sum = 0.f;
    for (const auto& ph : s.phase) sum += ph.avgMs;
    return std::max(0.f, s.frameAvgMs - sum);
}

} // namespace prof
} // namespace plat

// Doppia indirezione: senza, ##__LINE__ incolla la parola "__LINE__" invece
// del numero, e due PROF_SCOPE nella stessa funzione non compilano piu'.
#define PROF_CAT_(a, b) a##b
#define PROF_CAT(a, b) PROF_CAT_(a, b)
#define PROF_SCOPE(p) ::plat::prof::Scope PROF_CAT(prof_scope_, __LINE__)(p)
