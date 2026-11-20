#pragma once

/**
 * Blocky Render Pipeline — ordine ufficiale (come Studio: un percorso, chiaro).
 *
 * Frame (mondo Studio/Playing), dopo camera + Lighting.tick:
 *
 *   1. beginScene          bloom FBO (o default FB)
 *   2. clearSky            colore da Lighting (outdoorAmbient / night)
 *   3. drawSunSky          cielo + disco sole/luna
 *   4. passPrepare         cam pos, luci dinamiche, soft occluders
 *   5. passShadows         depth map sole/luna (se quality.shadows + Lighting.globalShadows)
 *   6. passTerrain         voxel opachi → trasparenti
 *   7. passOpaqueParts     Part cull/LOD + draw (stud / material)
 *   8. passStudioGizmos    outline + move/scale/rotate (solo edit)
 *   9. passWorkspaceModels mesh importate
 *  10. passAvatars         player locale, remoti, friends, tool in mano
 *  11. endScene            bloom composite / present + grade LookPack
 *  12. ImGui               UI sopra il frame
 *
 * File:
 *   WorldRenderPipeline.h  — pass 4–10 (incluso da main.cpp)
 *   Bloom.h / ShadowMap.h / SunSky.h / LookPack.h / QualityProfile.h
 *
 * Regola clean: un solo pipeline. Non aggiungere un secondo renderer.
 * quality.shadows e Lighting.globalShadows devono entrambi essere true
 * perche' le ombre sole disegnino (prima shadows del profile era ignorato).
 */

#include "../platform/QualityProfile.h"
#include "LookPack.h"

#include <cstdio>

/** Preset “Studio-like” in un click (oltre Low/Med/High/Ultra tecnici). */
enum class StudioGraphicsPreset : int {
    Performance = 0,  // mobile-safe, no post, no shadows
    StudioDefault = 1, // look pulito tipo Studio di giorno
    Beautiful = 2,     // post + ombre + Clean AAA lighting
    Realistic = 3,     // sole bianco, ombre riempite dal cielo, nebbia lunga
    Count
};

inline const char* studioGraphicsPresetLabel(StudioGraphicsPreset p) {
    switch (p) {
    case StudioGraphicsPreset::Performance:  return "Performance";
    case StudioGraphicsPreset::StudioDefault: return "Studio Default";
    case StudioGraphicsPreset::Beautiful:    return "Beautiful";
    case StudioGraphicsPreset::Realistic:    return "Realistico";
    default: return "?";
    }
}

/** Applica Quality + Look + Lighting in modo coerente (un menu Grafica). */
inline void applyStudioGraphicsPreset(StudioGraphicsPreset p,
                                      QualityProfile& q,
                                      LookPack& look,
                                      LightingService& L) {
    switch (p) {
    case StudioGraphicsPreset::Performance:
        q = applyPreset(q, QualityPreset::Low);
        q.shadows = false;
        q.shadowOccluders = 0;
        q.postFx = false;
        look.resetNeutral();
        L.globalShadows = false;
        L.brightness = 2.f;
        L.ambient = 0.45f;
        L.fogStart = 120.f;
        L.fogEnd = 400.f;
        std::snprintf(look.presetName, sizeof(look.presetName), "Performance");
        break;

    case StudioGraphicsPreset::StudioDefault:
        q = applyPreset(q, QualityPreset::High);
        q.shadows = true;
        q.shadowOccluders = std::max(q.shadowOccluders, 4);
        q.postFx = false;
        look.resetNeutral();
        look.wantShadows = true;
        look.wantPostFx = false;
        L.globalShadows = true;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 14.f;
        L.clockTime = 14.f;
        L.brightness = 2.f;
        L.ambient = 0.42f;
        L.outdoorAmbient = {0.38f, 0.42f, 0.52f};
        L.fogColor = {0.55f, 0.65f, 0.78f};
        L.fogStart = 200.f;
        L.fogEnd = 800.f;
        std::snprintf(look.presetName, sizeof(look.presetName), "Studio Default");
        break;

    case StudioGraphicsPreset::Beautiful:
        q = applyPreset(q, q.mobile ? QualityPreset::High : QualityPreset::Ultra);
        q.shadows = true;
        q.shadowOccluders = std::max(q.shadowOccluders, 8);
        q.postFx = true;
        applyLookPreset(LookPreset::CleanAaa, look, L);
        L.globalShadows = true;
        std::snprintf(look.presetName, sizeof(look.presetName), "Beautiful");
        break;

    case StudioGraphicsPreset::Realistic:
        q = applyPreset(q, q.mobile ? QualityPreset::High : QualityPreset::Ultra);
        q.shadows = true;
        q.shadowOccluders = std::max(q.shadowOccluders, 8);
        q.postFx = true;
        applyLookPreset(LookPreset::Realistic, look, L);
        L.globalShadows = true;
        std::snprintf(look.presetName, sizeof(look.presetName), "Realistico");
        break;

    default:
        break;
    }
    L.clampAll();
}

/** Ombre sole attive solo se profile + Lighting sono d’accordo. */
inline bool pipelineSunShadowsEnabled(const QualityProfile& q, const LightingService& L) {
    return q.shadows && L.globalShadows && q.shadowOccluders > 0;
}
