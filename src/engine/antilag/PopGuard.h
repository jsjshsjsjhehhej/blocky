#pragma once

// Quando si corre, le part davanti entrano in vista di colpo. Allungare
// la distanza di qualche stud nella direzione del moto costa poco e toglie
// il pop. Non si supera la distanza che il governor ha gia' concesso di
// molto: l'allungo non deve annullare il taglio.

#include "Math.h"

namespace antilag {

inline float popExtend(float distance, V3 velocity, V3 forward, float heat) {
    float speed = len(velocity);
    if (speed < 2.f) return distance;
    V3 f = normalize(forward);
    if (lenSq(f) < 1e-6f) return distance;
    float align = dot(normalize(velocity), f);
    if (align <= 0.15f) return distance;
    float extra = speed * 0.12f * align;
    if (extra > 16.f) extra = 16.f;
    if (heat > 0.5f) extra *= (1.f - heat);
    if (extra < 0.f) extra = 0.f;
    return distance + extra;
}

// Il tetto e' la distanza di base del giocatore, non quella gia' tagliata
// piu' un bonus infinito.
inline float capExtend(float extended, float governorDistance, float baseDistance) {
    float cap = governorDistance + 16.f;
    if (cap > baseDistance) cap = baseDistance;
    if (extended > cap) extended = cap;
    if (extended < 24.f) extended = 24.f;
    return extended;
}

}
