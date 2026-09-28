#pragma once


#include "TargetPlatform.h"

#include <algorithm>
#include <cmath>
#include <string>

struct QualityProfile {
    const char* name = "Default";
    // Piattaforma a batteria con GPU integrata: i preset "alti" non devono
    // alzare MSAA e risoluzione oltre a quello che regge, altrimenti "High"
    // significa impostazioni da PC su un telefono — cioe' meta' degli fps.
    bool  mobile          = false;

    int   msaaSamples     = 4;
    float renderScale     = 1.f;
    float maxDrawDistance = 400.f;
    int   maxVisibleParts = 4096;
    bool  vsync           = true;
    // 0 = nessun limite (solo vsync). >0 = tetto FPS via frame pacing (sleep
    // di fine frame in main.cpp) — pensato per tenere CPU/GPU sotto carico
    // costante invece che al 100% quando il monitor/vsync lo permetterebbero,
    // che e' la causa piu' diretta di surriscaldamento su mobile.
    int   targetFps       = 0;
    bool  studTextures    = true;
    bool  shadows         = false;
    int   shadowMapSize   = 0;
    int   shadowOccluders = 8;
    bool  skinningGpu     = true;
    bool  postFx          = false;

    float physicsHz       = 60.f;
    int   luaBudgetUs     = 2000;
    bool  touchUi         = false;
    float uiScale         = 1.f;

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
        q.targetFps = 0; // PC: niente tetto, solo vsync — l'utente ha gia' il controllo
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
        q.mobile = true;
        q.msaaSamples = 0;
        q.renderScale = 0.85f;
        q.maxDrawDistance = 220.f;
        q.maxVisibleParts = 1500;
        q.vsync = true;
        q.targetFps = 45; // compromesso caldo/fluido — vedi commento sul campo
        q.studTextures = true;
        q.shadows = false;
        q.shadowOccluders = 0;
        q.skinningGpu = true;
        q.physicsHz = 30.f;
        q.luaBudgetUs = 1200;
        q.touchUi = true;
        q.uiScale = 1.25f;
        q.textureBudgetMb = 256;
        q.aggressiveGc = true;
        break;

    case TargetPlatform::MobileIOS:
        q.name = "iOS Balanced";
        q.msaaSamples = 2;
        q.renderScale = 0.9f;
        q.maxDrawDistance = 280.f;
        q.maxVisibleParts = 2000;
        q.vsync = true;
        q.targetFps = 45;
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
        q.renderScale = 1.f;
        q.maxDrawDistance = 1000.f;
        q.maxVisibleParts = 12000;
        q.vsync = true;
        q.targetFps = 0;
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
        q.targetFps = 30;
        break;
    }
    return q;
}

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
        q.shadowOccluders = 0;
        q.postFx = false;
        q.physicsHz = 30.f;
        q.targetFps = q.targetFps > 0 ? std::min(q.targetFps, 30) : 30;
        break;
    case QualityPreset::Medium:
        q.name = "Medium";
        q.msaaSamples = std::min(q.msaaSamples, 2);
        q.renderScale = std::min(q.renderScale, 0.9f);
        q.maxDrawDistance *= 0.75f;
        q.shadows = false;
        q.shadowOccluders = std::min(q.shadowOccluders, 4);
        q.targetFps = q.targetFps > 0 ? std::min(q.targetFps, 45) : 0;
        break;
    case QualityPreset::High:
        q.name = "High";
        // Su mobile il preset alza la qualita' che COSTA POCO (ombre, oggetti
        // visibili) e lascia stare quella che costa moltissimo per pixel.
        if (!q.mobile) {
            q.msaaSamples = std::max(q.msaaSamples, 4);
            q.renderScale = 1.f;
        }
        q.shadowOccluders = 8;
        break;
    case QualityPreset::Ultra:
        q.name = "Ultra";
        if (!q.mobile) {
            q.msaaSamples = 8;
            q.renderScale = 1.f;
        }
        q.maxDrawDistance = std::max(q.maxDrawDistance, 1000.f);
        q.maxVisibleParts = std::max(q.maxVisibleParts, 10000);
        q.shadows = true;
        q.shadowMapSize = std::max(q.shadowMapSize, 4096);
        q.shadowOccluders = 8;
        q.postFx = true;
        q.targetFps = 0;
        break;
    default: break;
    }
    return q;
}
