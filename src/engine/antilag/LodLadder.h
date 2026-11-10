#pragma once

// Banda di dettaglio da quanti pixel copre la part. Full disegna gli stud,
// Flat e' il cubo liscio, Impostor e' "c'e' qualcosa" senza materiale,
// Skip non si manda alla GPU. Una luce o uno script non scendono sotto Flat:
// spegnerli perche' sono piccoli li farebbe comparire a scatti.

#include <cstdint>

namespace antilag {

enum class Lod : uint8_t {
    Full = 0,
    Flat,
    Impostor,
    Skip
};

inline const char* lodName(Lod l) {
    switch (l) {
    case Lod::Full:     return "full";
    case Lod::Flat:     return "flat";
    case Lod::Impostor: return "impostor";
    case Lod::Skip:     return "skip";
    }
    return "?";
}

inline Lod lodFor(float screenPx, bool studsAllowed, bool important) {
    if (screenPx < 0.f) screenPx = 0.f;
    if (screenPx >= 18.f && studsAllowed) return Lod::Full;
    if (screenPx >= 8.f) return studsAllowed ? Lod::Full : Lod::Flat;
    if (screenPx >= 3.f) return Lod::Flat;
    if (screenPx >= 1.2f) return important ? Lod::Flat : Lod::Impostor;
    if (important) return Lod::Flat;
    return Lod::Skip;
}

// Quanti "livelli" di lavoro vale questa banda, per i conti del budget.
inline int lodCost(Lod l) {
    switch (l) {
    case Lod::Full:     return 4;
    case Lod::Flat:     return 2;
    case Lod::Impostor: return 1;
    case Lod::Skip:     return 0;
    }
    return 0;
}

inline bool lodDrawsStuds(Lod l) { return l == Lod::Full; }

}
