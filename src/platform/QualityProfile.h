#pragma once

/**
 * Profili di qualità / performance per piattaforma.
 * Unici default ottimizzati; override runtime via EngineConfig.
 */

#include "TargetPlatform.h"

#include <algorithm>
#include <cmath>
#include <string>

struct QualityProfile {
    const char* name = "Default";

    // Rendering
    int   msaaSamples     = 4;
    float renderScale     = 1.f;   // 0.5 .. 1.5 (mobile spesso 0.75–0.9)
    float maxDrawDistance = 400.f;
    int   maxVisibleParts = 4096;
    bool  vsync           = true;
    bool  studTextures    = true;
    bool  shadows         = false; // futuro
    int   shadowMapSize   = 0;
    /** Box occlusori testati in shader per le ombre da luci dinamiche (0 = ombre off). */
    int   shadowOccluders = 8;
    bool  skinningGpu     = true;
    bool  postFx          = false;

    // Sim / gameplay
    float physicsHz       = 60.f;
    int   luaBudgetUs     = 2000;  // microsecondi/frame per Lua
    bool  touchUi         = false;
    float uiScale         = 1.f;

    // Memoria / streaming
    int   textureBudgetMb = 512;
    bool  aggressiveGc    = false;
};

inline QualityProfile profileFor(TargetPlatform p) {
    QualityProfile q;
    switch (p) {
    case TargetPlatform::PC:
        q.name = "PC High";
        q.msaaSamples = 4;
        q.renderScale = 1.f;
        q.maxDrawDistance = 900.f;
        q.maxVisibleParts = 10000;
        q.vsync = true;
        q.studTextures = true;
        q.shadows = false;
        q.shadowMapSize = 2048;
        q.shadowOccluders = 8;
        q.postFx = false;
        q.physicsHz = 72.f;
        q.luaBudgetUs = 5000;
        q.touchUi = false;
        q.uiScale = 1.f;
        q.textureBudgetMb = 1536;
        break;

    case TargetPlatform::MobileAndroid:
        q.name = "Android Balanced";
        q.msaaSamples = 0;          // MSAA costoso su tile GPU
        q.renderScale = 0.85f;
        q.maxDrawDistance = 220.f;
        q.maxVisibleParts = 1500;
        q.vsync = true;
        q.studTextures = true;
        q.shadows = false;
        q.shadowOccluders = 0;      // ray-box per fragment troppo caro su tile GPU
        q.skinningGpu = true;
        q.physicsHz = 30.f;         // half-rate physics su mid-range
        q.luaBudgetUs = 1200;
        q.touchUi = true;
        q.uiScale = 1.25f;
        q.textureBudgetMb = 256;
        q.aggressiveGc = true;
        break;

    case TargetPlatform::MobileIOS:
        q.name = "iOS Balanced";
        q.msaaSamples = 2;          // Metal gestisce bene 2x
        q.renderScale = 0.9f;
        q.maxDrawDistance = 280.f;
        q.maxVisibleParts = 2000;
        q.vsync = true;
        q.studTextures = true;
        q.shadowOccluders = 4;
        q.physicsHz = 60.f;
        q.luaBudgetUs = 1500;
        q.touchUi = true;
        q.uiScale = 1.15f;
        q.textureBudgetMb = 384;
        break;

    case TargetPlatform::PlayStation5:
        q.name = "PS5 Performance";
        q.msaaSamples = 4;
        q.renderScale = 1.f;        // 4K Performance / Quality via override
        q.maxDrawDistance = 1000.f;
        q.maxVisibleParts = 12000;
        q.vsync = true;
        q.studTextures = true;
        q.shadows = true;
        q.shadowMapSize = 4096;
        q.shadowOccluders = 8;
        q.postFx = true;
        q.physicsHz = 60.f;
        q.luaBudgetUs = 3000;
        q.touchUi = false;
        q.uiScale = 1.f;
        q.textureBudgetMb = 2048;
        break;

    default:
        q.name = "Safe Fallback";
        q.msaaSamples = 0;
        q.renderScale = 0.75f;
        q.maxDrawDistance = 150.f;
        q.maxVisibleParts = 800;
        q.shadowOccluders = 0;
        break;
    }
    return q;
}

/** Preset rapidi runtime (menu / Studio). */
enum class QualityPreset : uint8_t { Low, Medium, High, Ultra, PlatformDefault };

inline QualityProfile applyPreset(QualityProfile base, QualityPreset preset) {
    if (preset == QualityPreset::PlatformDefault) return base;
    QualityProfile q = base;
    switch (preset) {
    case QualityPreset::Low:
        q.name = "Low";
        q.msaaSamples = 0;
        q.renderScale = std::min(q.renderScale, 0.7f);
        q.maxDrawDistance *= 0.5f;
        q.maxVisibleParts = std::min(q.maxVisibleParts, 600);
        q.studTextures = false;
        q.shadows = false;
        q.shadowOccluders = 0;      // niente ombre dinamiche
        q.postFx = false;
        q.physicsHz = 30.f;
        break;
    case QualityPreset::Medium:
        q.name = "Medium";
        q.msaaSamples = std::min(q.msaaSamples, 2);
        q.renderScale = std::min(q.renderScale, 0.9f);
        q.maxDrawDistance *= 0.75f;
        q.shadows = false;
        q.shadowOccluders = std::min(q.shadowOccluders, 4);
        break;
    case QualityPreset::High:
        q.name = "High";
        q.msaaSamples = std::max(q.msaaSamples, 4);
        q.renderScale = 1.f;
        q.shadowOccluders = 8;
        break;
    case QualityPreset::Ultra:
        q.name = "Ultra";
        q.msaaSamples = 8;
        q.renderScale = 1.f;
        q.maxDrawDistance = std::max(q.maxDrawDistance, 1000.f);
        q.maxVisibleParts = std::max(q.maxVisibleParts, 10000);
        q.shadows = true;
        q.shadowMapSize = std::max(q.shadowMapSize, 4096);
        q.shadowOccluders = 8;
        q.postFx = true;
        break;
    default: break;
    }
    return q;
}
