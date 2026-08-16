#pragma once

/**
 * Tracce di animazione per i modelli del mondo.
 *
 * Il modello non ha "una clip attiva": ha un Animator che carica *tracce*, e
 * ogni traccia si comanda da sola — parte, si ferma, ha la sua velocità e il
 * suo peso. È la forma che serve a un NPC vero, perché camminata e inseguimento
 * vanno tenute entrambe pronte e alternate, non ricaricate ogni volta.
 *
 * Qui c'è solo lo *stato*: nessun riferimento alla mesh o a OpenGL. Il renderer
 * legge quale traccia sta suonando e applica la posa. Così il DataModel resta
 * serializzabile e Lua può comandarlo senza toccare la grafica.
 */

#include <algorithm>
#include <string>
#include <vector>

/** Una traccia caricata da una clip del file. */
struct AnimTrack {
    std::string clipName;      // nome cercato nel file (sottostringa, no maiuscole)
    bool playing = false;
    bool looped = true;
    float speed = 1.f;
    float weight = 1.f;        // 0..1, per fondere due tracce
    int priority = 0;          // a parità di peso vince la priorità più alta
    float time = 0.f;          // secondi trascorsi, per riprendere dove si era

    bool valid() const { return !clipName.empty(); }
};

/**
 * L'Animator del modello: tiene le tracce e dice quale comanda la posa.
 * Le tracce non vengono mai rimosse, solo fermate: uno script che tiene un
 * riferimento non deve trovarselo invalido.
 */
struct ModelAnimator {
    std::vector<AnimTrack> tracks;

    /** Carica una traccia, o restituisce quella già caricata per la stessa clip. */
    int load(const std::string& clipName) {
        for (size_t i = 0; i < tracks.size(); ++i)
            if (tracks[i].clipName == clipName) return (int)i;

        AnimTrack t;
        t.clipName = clipName;
        tracks.push_back(std::move(t));
        return (int)tracks.size() - 1;
    }

    AnimTrack* get(int id) {
        if (id < 0 || id >= (int)tracks.size()) return nullptr;
        return &tracks[(size_t)id];
    }

    const AnimTrack* get(int id) const {
        if (id < 0 || id >= (int)tracks.size()) return nullptr;
        return &tracks[(size_t)id];
    }

    void play(int id) {
        if (AnimTrack* t = get(id)) t->playing = true;
    }

    void stop(int id) {
        if (AnimTrack* t = get(id)) { t->playing = false; t->time = 0.f; }
    }

    void stopAll() {
        for (AnimTrack& t : tracks) { t.playing = false; t.time = 0.f; }
    }

    /** Traccia che comanda la posa: peso più alto, poi priorità. -1 se ferme tutte. */
    int dominant() const {
        int best = -1;
        float bestWeight = 0.f;
        int bestPriority = -1;

        for (size_t i = 0; i < tracks.size(); ++i) {
            const AnimTrack& t = tracks[i];
            if (!t.playing || !t.valid() || t.weight <= 0.f) continue;
            if (t.weight > bestWeight ||
                (t.weight == bestWeight && t.priority > bestPriority)) {
                bestWeight = t.weight;
                bestPriority = t.priority;
                best = (int)i;
            }
        }
        return best;
    }

    /** Avanza il tempo delle tracce in riproduzione. */
    void advance(float dt) {
        for (AnimTrack& t : tracks)
            if (t.playing) t.time += dt * std::max(0.f, t.speed);
    }

    bool anyPlaying() const {
        for (const AnimTrack& t : tracks)
            if (t.playing) return true;
        return false;
    }
};
