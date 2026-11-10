#pragma once

// Testo per il pannello F4. Numeri del piano, non una spiegazione nuova:
// se una riga non corrisponde a un campo vero, non ci deve essere.

#include "AntiLag.h"

#include <cstdio>
#include <string>

namespace antilag {

inline void appendLine(std::string& out, const char* s) {
    out += s;
    out += '\n';
}

inline std::string report(const Engine& eng) {
    const FramePlan& p = eng.plan();
    const FrameClock& c = eng.clock();
    char line[256];
    std::string out;
    out.reserve(1400);

    std::snprintf(line, sizeof(line), "Anti-lag %s  livello %d  %s",
                  modeName(eng.mode()), p.level, reasonName(p.reason));
    appendLine(out, line);
    std::snprintf(line, sizeof(line), "Frame %.1f ms  media %.1f  p95 %.1f  target %.1f",
                  c.latestMs(), p.averageMs, p.p95Ms, p.targetMs);
    appendLine(out, line);
    std::snprintf(line, sizeof(line), "Causa %s  (%.0f%%)  zona %s",
                  causeName(eng.cause().cause), eng.cause().confidence * 100.f,
                  eng.cause().pinZone ? "ferma" : "libera");
    appendLine(out, line);

    std::snprintf(line, sizeof(line),
                  "Scala %.0f%%  vista %.0f  part %d  px min %.1f",
                  p.renderScale * 100.f, p.maxDrawDistance, p.maxVisibleParts, p.minScreenPx);
    appendLine(out, line);
    std::snprintf(line, sizeof(line),
                  "Fisica %.0f Hz  script %d us  stride %d  particelle %d",
                  p.physicsHz, p.luaBudgetUs, p.heartbeatStride, p.maxParticles);
    appendLine(out, line);
    std::snprintf(line, sizeof(line),
                  "Luci %d  ombre %d  stud %s  post %s  rete %.0f ms",
                  p.maxLights, p.shadowOccluders,
                  p.studTextures ? "si" : "no",
                  p.postFx ? "si" : "no",
                  p.netMinInterval * 1000.f);
    appendLine(out, line);

    std::snprintf(line, sizeof(line), "Scatti in finestra: stalli %d  pesanti %d  lievi %d",
                  c.stalls(), c.heavies(), c.milds());
    appendLine(out, line);
    std::snprintf(line, sizeof(line), "Calore qui %.2f  zone in memoria %d  diario %d",
                  eng.heat().around(eng.camera().eye.x, eng.camera().eye.y, eng.camera().eye.z),
                  eng.zones().zones(), eng.journal().size());
    appendLine(out, line);

    if (eng.journal().size() > 0) {
        HitchEvent e = eng.journal().at(0);
        std::snprintf(line, sizeof(line), "Ultimo scatto %.0f ms  L%d %s  %s  (%.0f %.0f %.0f)",
                      e.ms, e.level, reasonName(e.reason), phaseName(e.phase), e.x, e.y, e.z);
        appendLine(out, line);
    }

    appendLine(out, "Se il livello salisse adesso:");
    for (int level = 0; level <= Governor::kMaxLevel; ++level) {
        FramePlan alt = planFrom(eng.base(), level, StepReason::None, p.targetMs);
        std::snprintf(line, sizeof(line),
                      "  L%d  scala %.0f%%  part %d  fisica %.0f  script %d us",
                      level, alt.renderScale * 100.f, alt.maxVisibleParts,
                      alt.physicsHz, alt.luaBudgetUs);
        appendLine(out, line);
    }
    return out;
}

}
