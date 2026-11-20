#pragma once


#include "TargetPlatform.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
    // Low: stud sempre visibili ma leggeri (texture 64x64, spariscono prima con la distanza).
    bool  studLowDetail   = false;
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

    // Auto: LOD distanza + frustum + occlusion (soft-Z) + part piccole.
    // I flag singoli restano per debug; con autoLod=true sono tutti forzati.
    bool  autoLod         = true;
    bool  partLod         = true;
    bool  frustumCull     = true;
    bool  occlusionCull   = true;
    bool  cullSmallParts  = true;
    int   maxVisibleModels = 96;
    // Tetto vertici workspace models per frame (GLB/FBX pesanti). Oltre:
    // i piu' lontani/piccoli saltano. 0 = illimitato.
    int   maxModelVerts    = 2'500'000;
    // Preferisci part grandi/vicine quando si sfora maxVisibleParts.
    bool  priorityPartCull = true;

    /** Lag intenso: abbassa qualita' automaticamente (poi recupera). */
    bool  autoLagDefense   = true;
    /** NaN / coordinate assurde / fisica esplosa: riparazione a runtime. */
    bool  runtimeWorldGuard = true;
};

inline QualityProfile profileFor(TargetPlatform p) {
    QualityProfile q;
    switch (p) {
    case TargetPlatform::PC:
        q.name = "PC High";
        q.msaaSamples = 4;
        q.renderScale = 1.f;
        // Come la vista di Roblox su PC: si vede la citta' intorno, non solo
        // poche centinaia di parti. 64000 e' il tetto, non quante se ne
        // disegnano sempre: sotto, il frustum le scarta. Piu' su inizia a pesare.
        q.maxDrawDistance = 2000.f;
        q.maxVisibleParts = 64000;
        q.vsync = true;
        q.targetFps = 0; // PC: niente tetto, solo vsync — l'utente ha gia' il controllo
        q.studTextures = true;
        // Ombre sole: flag + occluders (prima solo occluders contava).
        q.shadows = true;
        q.shadowMapSize = 2048;
        q.shadowOccluders = 8;
        q.postFx = false;
        q.physicsHz = 72.f;
        q.luaBudgetUs = 8000;
        q.touchUi = false;
        q.uiScale = 1.f;
        q.textureBudgetMb = 2048;
        q.maxVisibleModels = 320;
        q.maxModelVerts = 6'000'000;
        q.priorityPartCull = true;
        q.autoLod = true;
        q.frustumCull = true;
        q.occlusionCull = true;
        q.autoLagDefense = true;
        q.runtimeWorldGuard = true;
        break;

    case TargetPlatform::MobileAndroid:
        q.name = "Android Balanced";
        q.mobile = true;
        q.msaaSamples = 0;
        // Scale un filo piu' basso per sostenere 60 fps su GPU mid-range.
        q.renderScale = 0.72f;
        q.maxDrawDistance = 260.f;
        q.maxVisibleParts = 2800;
        q.vsync = false; // evita incastri 120Hz→40fps; il cap e' targetFps
        q.targetFps = 60;
        q.studTextures = true;
        q.postFx = false;
        q.shadows = false;
        q.shadowOccluders = 0;
        q.skinningGpu = true;
        q.physicsHz = 60.f;
        q.luaBudgetUs = 1800;
        q.touchUi = true;
        q.uiScale = 1.25f;
        q.textureBudgetMb = 384;
        q.aggressiveGc = true;
        q.maxVisibleModels = 72;
        q.maxModelVerts = 1'200'000;
        q.priorityPartCull = true;
        q.autoLagDefense = true;
        q.runtimeWorldGuard = true;
        break;

    case TargetPlatform::MobileIOS:
        q.name = "iOS Balanced";
        q.mobile = true;
        q.msaaSamples = 2;
        q.renderScale = 0.85f;
        q.maxDrawDistance = 320.f;
        q.maxVisibleParts = 3600;
        q.vsync = true;
        q.targetFps = 60;
        q.studTextures = true;
        q.shadowOccluders = 4;
        q.physicsHz = 60.f;
        q.luaBudgetUs = 2200;
        q.touchUi = true;
        q.uiScale = 1.15f;
        q.textureBudgetMb = 512;
        q.maxVisibleModels = 96;
        q.maxModelVerts = 1'800'000;
        q.priorityPartCull = true;
        break;

    case TargetPlatform::PlayStation5:
        q.name = "PS5 Performance";
        q.msaaSamples = 4;
        q.renderScale = 1.f;
        q.maxDrawDistance = 1600.f;
        q.maxVisibleParts = 36000;
        q.vsync = true;
        q.targetFps = 0;
        q.studTextures = true;
        q.shadows = true;
        q.shadowMapSize = 4096;
        q.shadowOccluders = 8;
        q.postFx = true;
        q.physicsHz = 60.f;
        q.luaBudgetUs = 5000;
        q.touchUi = false;
        q.uiScale = 1.f;
        q.textureBudgetMb = 3072;
        q.maxVisibleModels = 400;
        q.maxModelVerts = 8'000'000;
        break;

    default:
        q.name = "Safe Fallback";
        q.msaaSamples = 0;
        q.renderScale = 0.75f;
        q.maxDrawDistance = 180.f;
        q.maxVisibleParts = 1200;
        q.shadowOccluders = 0;
        q.targetFps = 30;
        q.maxVisibleModels = 40;
        q.maxModelVerts = 600'000;
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
        q.renderScale = std::min(q.renderScale, 0.65f);
        q.maxDrawDistance *= 0.5f;
        q.maxVisibleParts = std::min(q.maxVisibleParts, q.mobile ? 900 : 2500);
        q.studTextures = true;    // gli stud restano: sono il look di Blocky
        q.studLowDetail = true;   // ...ma poco dettagliati
        q.shadows = false;
        q.shadowOccluders = 0;
        q.postFx = false;
        q.physicsHz = 30.f;
        // Low NON blocca piu' a 30 fps (sul telefono di prova: 15 ms di lavoro
        // e 18 ms di attesa). A 30 si scende solo con la difesa anti-lag.
        // La fisica resta a 30 Hz: il disegno interpola, niente scatti.
        q.autoLod = true;
        q.partLod = true;
        q.frustumCull = true;
        q.occlusionCull = true;
        q.cullSmallParts = true;
        q.priorityPartCull = true;
        q.maxVisibleModels = std::min(q.maxVisibleModels, q.mobile ? 36 : 96);
        q.maxModelVerts = std::min(q.maxModelVerts, q.mobile ? 400'000 : 1'500'000);
        break;
    case QualityPreset::Medium:
        q.name = "Medium";
        q.msaaSamples = std::min(q.msaaSamples, 2);
        q.renderScale = std::min(q.renderScale, q.mobile ? 0.8f : 0.9f);
        q.maxDrawDistance *= 0.75f;
        q.shadowOccluders = std::min(std::max(q.shadowOccluders, 1), 4);
        q.shadows = q.shadowOccluders > 0;
        // Medium su mobile: 60 fps (non piu' tetto a 45).
        if (q.mobile) q.targetFps = 60;
        else if (q.targetFps > 0) q.targetFps = std::min(q.targetFps, 60);
        break;
    case QualityPreset::High:
        q.name = "High";
        // Su mobile il preset alza la qualita' che COSTA POCO (ombre, oggetti
        // visibili) e lascia stare quella che costa moltissimo per pixel.
        if (!q.mobile) {
            q.msaaSamples = std::max(q.msaaSamples, 4);
            q.renderScale = 1.f;
            q.maxVisibleParts = std::max(q.maxVisibleParts, 32000);
            q.maxVisibleModels = std::max(q.maxVisibleModels, 360);
            q.maxModelVerts = std::max(q.maxModelVerts, 7'000'000);
            q.maxDrawDistance = std::max(q.maxDrawDistance, 1500.f);
        } else {
            q.targetFps = 60;
            q.renderScale = std::min(q.renderScale, 0.9f);
            q.maxVisibleParts = std::max(q.maxVisibleParts, 3200);
            q.maxVisibleModels = std::max(q.maxVisibleModels, 88);
        }
        q.shadowOccluders = 8;
        q.shadows = true;
        break;
    case QualityPreset::Ultra:
        q.name = "Ultra";
        if (!q.mobile) {
            q.msaaSamples = 8;
            q.renderScale = 1.f;
        }
        q.maxDrawDistance = std::max(q.maxDrawDistance, 2000.f);
        q.maxVisibleParts = std::max(q.maxVisibleParts, 48000);
        q.maxVisibleModels = std::max(q.maxVisibleModels, 512);
        q.maxModelVerts = std::max(q.maxModelVerts, 12'000'000);
        q.shadows = true;
        q.shadowMapSize = std::max(q.shadowMapSize, 4096);
        q.shadowOccluders = 8;
        q.postFx = true;
        q.targetFps = 0;
        q.luaBudgetUs = std::max(q.luaBudgetUs, 10000);
        break;
    default: break;
    }
    return q;
}
