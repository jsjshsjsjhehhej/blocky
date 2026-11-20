#pragma once

#include "../engine/LightingService.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

/** Look “AAA-like” applicabile in un clic: grade post + fog/luce + bloom. */
struct LookPack {
    bool enabled = false;
    char presetName[48] = "Default";

    float exposure = 1.f;
    float contrast = 1.f;
    float saturation = 1.f;
    float vignette = 0.f;
    float bloomIntensity = 1.6f;
    float bloomThLow = 0.95f;
    float bloomThHigh = 1.5f;
    float sharpen = 0.55f;
    bool wantPostFx = true;
    bool wantShadows = true;

    void resetNeutral() {
        enabled = false;
        std::snprintf(presetName, sizeof(presetName), "Default");
        exposure = 1.f;
        contrast = 1.f;
        saturation = 1.f;
        vignette = 0.f;
        bloomIntensity = 1.6f;
        bloomThLow = 0.95f;
        bloomThHigh = 1.5f;
        sharpen = 0.55f;
        wantPostFx = false;
        wantShadows = true;
    }

    void applyLighting(LightingService& L) const {
        // I preset toccano Lighting; Default non resetta il place dell'utente.
        if (!enabled) return;
        L.clampAll();
    }
};

enum class LookPreset : int {
    Default = 0,
    Forest = 1,
    Cinematic = 2,
    GoldenHour = 3,
    NightNeon = 4,
    CleanAaa = 5,
    Misty = 6,
    Realistic = 7,
    Count
};

inline const char* lookPresetLabel(LookPreset p) {
    switch (p) {
    case LookPreset::Forest: return "Forest (bosco)";
    case LookPreset::Cinematic: return "Cinematic";
    case LookPreset::GoldenHour: return "Golden Hour";
    case LookPreset::NightNeon: return "Night Neon";
    case LookPreset::CleanAaa: return "Clean AAA";
    case LookPreset::Misty: return "Misty / Atmosphere";
    case LookPreset::Realistic: return "Realistico";
    default: return "Default (off)";
    }
}

inline void applyLookPreset(LookPreset p, LookPack& look, LightingService& L) {
    look.resetNeutral();
    if (p == LookPreset::Default) {
        look.enabled = false;
        return;
    }
    look.enabled = true;
    look.wantPostFx = true;
    look.wantShadows = true;
    std::snprintf(look.presetName, sizeof(look.presetName), "%s", lookPresetLabel(p));

    switch (p) {
    case LookPreset::Forest:
        look.exposure = 1.08f;
        look.contrast = 1.12f;
        look.saturation = 1.18f;
        look.vignette = 0.28f;
        look.bloomIntensity = 1.35f;
        look.bloomThLow = 0.88f;
        look.bloomThHigh = 1.45f;
        L.clockTime = 11.2f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 11.2f;
        L.brightness = 2.35f;
        L.ambient = 0.48f;
        L.outdoorAmbient = {0.32f, 0.48f, 0.36f};
        L.fogColor = {0.55f, 0.68f, 0.58f};
        L.fogStart = 55.f;
        L.fogEnd = 220.f;
        L.globalShadows = true;
        break;
    case LookPreset::Cinematic:
        look.exposure = 1.02f;
        look.contrast = 1.22f;
        look.saturation = 0.92f;
        look.vignette = 0.42f;
        look.bloomIntensity = 2.05f;
        look.bloomThLow = 0.82f;
        look.bloomThHigh = 1.35f;
        look.sharpen = 0.7f;
        L.clockTime = 17.4f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 17.4f;
        L.brightness = 2.6f;
        L.ambient = 0.32f;
        L.outdoorAmbient = {0.42f, 0.38f, 0.48f};
        L.fogColor = {0.42f, 0.48f, 0.58f};
        L.fogStart = 80.f;
        L.fogEnd = 380.f;
        L.globalShadows = true;
        break;
    case LookPreset::GoldenHour:
        look.exposure = 1.12f;
        look.contrast = 1.15f;
        look.saturation = 1.22f;
        look.vignette = 0.32f;
        look.bloomIntensity = 1.9f;
        look.bloomThLow = 0.78f;
        look.bloomThHigh = 1.3f;
        L.clockTime = 18.6f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 18.6f;
        L.brightness = 2.8f;
        L.ambient = 0.38f;
        L.outdoorAmbient = {0.72f, 0.42f, 0.28f};
        L.fogColor = {0.85f, 0.55f, 0.35f};
        L.fogStart = 40.f;
        L.fogEnd = 260.f;
        L.globalShadows = true;
        break;
    case LookPreset::NightNeon:
        look.exposure = 0.95f;
        look.contrast = 1.18f;
        look.saturation = 1.25f;
        look.vignette = 0.48f;
        look.bloomIntensity = 2.6f;
        look.bloomThLow = 0.55f;
        look.bloomThHigh = 1.1f;
        L.clockTime = 22.5f;
        L.dayNight = DayNightMode::NightOnly;
        L.fixedNightClock = 22.5f;
        L.brightness = 1.6f;
        L.ambient = 0.22f;
        L.outdoorAmbient = {0.12f, 0.16f, 0.32f};
        L.fogColor = {0.08f, 0.10f, 0.18f};
        L.fogStart = 30.f;
        L.fogEnd = 160.f;
        L.globalShadows = true;
        break;
    case LookPreset::CleanAaa:
        look.exposure = 1.05f;
        look.contrast = 1.08f;
        look.saturation = 1.05f;
        look.vignette = 0.18f;
        look.bloomIntensity = 1.55f;
        look.bloomThLow = 0.9f;
        look.bloomThHigh = 1.4f;
        look.sharpen = 0.65f;
        L.clockTime = 14.f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 14.f;
        L.brightness = 2.4f;
        L.ambient = 0.4f;
        L.outdoorAmbient = {0.4f, 0.45f, 0.55f};
        L.fogColor = {0.62f, 0.72f, 0.85f};
        L.fogStart = 120.f;
        L.fogEnd = 520.f;
        L.globalShadows = true;
        break;
    case LookPreset::Misty:
        look.exposure = 1.0f;
        look.contrast = 1.05f;
        look.saturation = 0.88f;
        look.vignette = 0.22f;
        look.bloomIntensity = 1.25f;
        look.bloomThLow = 0.92f;
        look.bloomThHigh = 1.5f;
        L.clockTime = 9.5f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 9.5f;
        L.brightness = 1.9f;
        L.ambient = 0.55f;
        L.outdoorAmbient = {0.5f, 0.55f, 0.6f};
        L.fogColor = {0.72f, 0.76f, 0.8f};
        L.fogStart = 18.f;
        L.fogEnd = 110.f;
        L.globalShadows = true;
        break;
    case LookPreset::Realistic:
        // Pomeriggio chiaro: sole a circa 45 gradi, cielo blu, nebbia lunga,
        // bloom solo sulle punte. Niente vignetta da film.
        look.exposure = 1.0f;
        look.contrast = 1.0f;
        look.saturation = 1.0f;
        look.vignette = 0.f;
        look.bloomIntensity = 0.15f;
        look.bloomThLow = 1.2f;
        look.bloomThHigh = 1.8f;
        look.sharpen = 0.15f;
        L.clockTime = 15.2f;
        L.dayNight = DayNightMode::DayOnly;
        L.fixedDayClock = 15.2f;
        L.brightness = 2.f;
        L.ambient = 0.2f;
        L.outdoorAmbient = {0.96f, 0.97f, 0.98f};
        L.fogColor = {0.78f, 0.82f, 0.86f};
        L.fogStart = 140.f;
        L.fogEnd = 900.f;
        L.atmosphereEnabled = true;
        L.atmosphereDensity = 0.08f;
        L.atmosphereHaze = 0.04f;
        L.sunRaysEnabled = false;
        L.sunRaysIntensity = 0.f;
        L.globalShadows = true;
        L.ccEnabled = false;
        L.bloomFx = false;
        break;
    default:
        break;
    }
    L.clampAll();
}
