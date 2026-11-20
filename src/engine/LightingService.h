#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

enum class DayNightMode : int { Cycle = 0, DayOnly = 1, NightOnly = 2 };

inline const char* dayNightModeName(DayNightMode m) {
    switch (m) {
    case DayNightMode::DayOnly:   return "Solo giorno";
    case DayNightMode::NightOnly: return "Solo notte";
    default:                      return "Ciclo giorno/notte";
    }
}

struct LightingService {
    float brightness = 2.f;
    float ambient = 0.42f;
    float clockTime = 14.f;
    float geographicLatitude = 0.f;
    glm::vec3 outdoorAmbient{0.38f, 0.42f, 0.52f};
    glm::vec3 colorShiftTop{0.f, 0.f, 0.f};
    glm::vec3 fogColor{0.55f, 0.65f, 0.78f};
    float fogStart = 200.f;
    float fogEnd = 800.f;
    bool globalShadows = true;

    // Studio 2020-ish Atmosphere / SunRays (applicati a fog + look).
    bool  atmosphereEnabled = false;
    float atmosphereDensity = 0.35f;   // 0..1 → stringe fogEnd
    float atmosphereHaze = 0.15f;      // tinge horizon/fog
    float atmosphereOffset = 0.f;
    bool  sunRaysEnabled = false;
    float sunRaysIntensity = 0.25f;
    float sunRaysSpread = 1.f;

    // Figli di Lighting, come in Studio: cambiano l'immagine finale.
    bool  ccEnabled = false;
    float ccBrightness = 0.f;     // -1..1
    float ccContrast = 0.f;       // -1..1
    float ccSaturation = 0.f;     // -1..1
    glm::vec3 ccTint{1.f, 1.f, 1.f};
    bool  bloomFx = false;
    float bloomFxIntensity = 1.f;
    float bloomFxThreshold = 0.8f;
    bool  blurFx = false;
    float blurFxSize = 0.f;       // 0..56
    bool  dofFx = false;
    float dofFar = 0.4f;          // 0..1, sfoca il quadro (non un obiettivo vero)

    DayNightMode dayNight = DayNightMode::Cycle;
    float dayLengthMinutes = 8.f;
    float fixedDayClock = 14.f;
    float fixedNightClock = 0.f;

    void clampAll() {
        brightness = std::clamp(brightness, 0.f, 10.f);
        ambient = std::clamp(ambient, 0.f, 1.f);
        clockTime = std::fmod(std::max(0.f, clockTime), 24.f);
        if (clockTime < 0.f) clockTime += 24.f;
        fogStart = std::max(0.f, fogStart);
        fogEnd = std::max(fogStart + 1.f, fogEnd);
        dayLengthMinutes = std::clamp(dayLengthMinutes, 0.25f, 120.f);
        fixedDayClock = std::clamp(fixedDayClock, 7.f, 17.f);
        fixedNightClock = std::clamp(fixedNightClock, 0.f, 24.f);
        atmosphereDensity = std::clamp(atmosphereDensity, 0.f, 1.f);
        atmosphereHaze = std::clamp(atmosphereHaze, 0.f, 1.f);
        sunRaysIntensity = std::clamp(sunRaysIntensity, 0.f, 2.f);
        sunRaysSpread = std::clamp(sunRaysSpread, 0.1f, 4.f);
        ccBrightness = std::clamp(ccBrightness, -1.f, 1.f);
        ccContrast = std::clamp(ccContrast, -1.f, 1.f);
        ccSaturation = std::clamp(ccSaturation, -1.f, 1.f);
        ccTint = glm::clamp(ccTint, glm::vec3(0.f), glm::vec3(1.f));
        bloomFxIntensity = std::clamp(bloomFxIntensity, 0.f, 4.f);
        bloomFxThreshold = std::clamp(bloomFxThreshold, 0.f, 2.f);
        blurFxSize = std::clamp(blurFxSize, 0.f, 56.f);
        dofFar = std::clamp(dofFar, 0.f, 1.f);
    }

    // Fog effettivo dopo Atmosphere (non modifica i campi salvati).
    float effectiveFogEnd() const {
        if (!atmosphereEnabled) return fogEnd;
        float shrink = 1.f - atmosphereDensity * 0.65f;
        return std::max(fogStart + 8.f, fogEnd * shrink);
    }
    glm::vec3 effectiveFogColor() const {
        if (!atmosphereEnabled) return fogColor;
        glm::vec3 haze{0.72f, 0.78f, 0.88f};
        return glm::mix(fogColor, haze, atmosphereHaze);
    }

    void tick(float dt, bool advance = true) {
        switch (dayNight) {
        case DayNightMode::DayOnly:
            clockTime = fixedDayClock;
            break;
        case DayNightMode::NightOnly:
            clockTime = fixedNightClock;
            break;
        case DayNightMode::Cycle:
            if (advance)
                clockTime += dt * (24.f / (std::max(0.25f, dayLengthMinutes) * 60.f));
            break;
        }
        clampAll();
    }

    glm::vec3 toSun() const {
        float ang = (clockTime - 6.f) * (3.1415927f / 12.f);
        return glm::normalize(glm::vec3(-std::cos(ang), std::sin(ang), 0.28f));
    }

    glm::vec3 toMoon() const { return -toSun(); }

    bool sunIsUp() const { return toSun().y > 0.f; }

    glm::vec3 toSkyLight() const {
        glm::vec3 s = toSun();
        return s.y > 0.f ? s : -s;
    }

    float nightAmount() const {
        float elev = toSun().y;
        return std::clamp(0.5f - elev * 2.2f, 0.f, 1.f);
    }

    glm::vec3 sunDirection() const { return -toSkyLight(); }

    // Di giorno: bianco caldo a mezzogiorno, arancio verso alba/tramonto.
    // Di notte la luce "direzionale" e' la luna (toSkyLight() punta gia' a
    // lei quando il sole e' sotto): prima qui restava il calcolo del
    // tramonto anche a notte fonda (luce calda invece che lunare fredda) —
    // ora sfuma verso un chiaro di luna freddo/tenue con nightAmount(),
    // niente scatto netto all'orizzonte.
    glm::vec3 sunTint() const {
        float h = clockTime;
        float dusk = 0.f;
        if (h < 6.f) dusk = 1.f - h / 6.f;
        else if (h > 18.f) dusk = (h - 18.f) / 6.f;
        dusk = std::clamp(dusk, 0.f, 1.f);
        glm::vec3 day{1.f, 0.97f, 0.92f};
        glm::vec3 warm{1.f, 0.55f, 0.28f};
        glm::vec3 daySide = glm::mix(day, warm, dusk * 0.85f)
                           * std::clamp(brightness * 0.45f, 0.35f, 2.2f);

        glm::vec3 moon{0.55f, 0.65f, 0.85f};
        glm::vec3 moonSide = moon * std::clamp(brightness * 0.18f, 0.08f, 0.5f);

        return glm::mix(daySide, moonSide, nightAmount());
    }

    // Tinta del fill ambientale: a slider Ambient=0 resta bianco neutro
    // (identico al vecchio comportamento fisso), via via che sale prende
    // la tonalita' di OutdoorAmbient mantenendo la stessa luminosita'
    // (cosi' non puo' mai scurire/schiarire tutto, solo tingere).
    glm::vec3 ambientTint() const {
        float lum = 0.2126f * outdoorAmbient.r + 0.7152f * outdoorAmbient.g + 0.0722f * outdoorAmbient.b;
        glm::vec3 hueOnly = lum > 0.001f ? outdoorAmbient / lum : glm::vec3(1.f);
        return glm::mix(glm::vec3(1.f), hueOnly, std::clamp(ambient, 0.f, 1.f));
    }

    void skyColors(glm::vec3& zenith, glm::vec3& horizon) const {
        float n = nightAmount();
        glm::vec3 dayZ{0.42f, 0.65f, 0.95f}, dayH{0.69f, 0.84f, 0.95f};
        glm::vec3 nightZ{0.03f, 0.05f, 0.12f}, nightH{0.10f, 0.12f, 0.22f};
        float dusk = std::clamp(1.f - std::fabs(toSun().y) * 4.f, 0.f, 1.f) * (1.f - n);
        zenith = glm::mix(dayZ, nightZ, n);
        horizon = glm::mix(glm::mix(dayH, nightH, n), glm::vec3(1.f, 0.62f, 0.35f), dusk * 0.55f);
    }
};
