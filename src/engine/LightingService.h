#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

/** Come scorre il tempo nel place (scelto nel Blocky Game Engine). */
enum class DayNightMode : int { Cycle = 0, DayOnly = 1, NightOnly = 2 };

inline const char* dayNightModeName(DayNightMode m) {
    switch (m) {
    case DayNightMode::DayOnly:   return "Solo giorno";
    case DayNightMode::NightOnly: return "Solo notte";
    default:                      return "Ciclo giorno/notte";
    }
}

/** Lighting stile Roblox — personalizzabile da Explorer / Lua. */
struct LightingService {
    float brightness = 2.f;       // 0..10
    float ambient = 0.42f;        // 0..1 (moltiplicatore ambient)
    float clockTime = 14.f;       // 0..24
    float geographicLatitude = 0.f;
    glm::vec3 outdoorAmbient{0.38f, 0.42f, 0.52f};
    glm::vec3 colorShiftTop{0.f, 0.f, 0.f};
    glm::vec3 fogColor{0.55f, 0.65f, 0.78f};
    float fogStart = 200.f;
    float fogEnd = 800.f;
    bool globalShadows = true; // ombre proiettate da sole/luna (box occlusori)

    // —— Ciclo giorno / notte ——
    DayNightMode dayNight = DayNightMode::Cycle;
    float dayLengthMinutes = 8.f; // durata di un giro completo 0→24h
    float fixedDayClock = 14.f;   // ora usata in "Solo giorno"
    float fixedNightClock = 0.f;  // ora usata in "Solo notte"

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
    }

    /**
     * Applica la modalità. `advance` = false in Studio (fuori dal Test): l'ora
     * resta ferma su quella scelta, così mentre costruisci la luce non cambia.
     * Il ciclo scorre solo in Test / Play.
     */
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

    /** Direzione VERSO il sole: 6h = alba a est, 12h = zenit, 18h = tramonto a ovest. */
    glm::vec3 toSun() const {
        float ang = (clockTime - 6.f) * (3.1415927f / 12.f);
        return glm::normalize(glm::vec3(-std::cos(ang), std::sin(ang), 0.28f));
    }

    /** La luna sta all'opposto del sole. */
    glm::vec3 toMoon() const { return -toSun(); }

    bool sunIsUp() const { return toSun().y > 0.f; }

    /**
     * Astro che illumina la scena: il sole finché è sopra l'orizzonte, poi la luna.
     * Serve per avere sempre una direzione d'ombra sensata.
     */
    glm::vec3 toSkyLight() const {
        glm::vec3 s = toSun();
        return s.y > 0.f ? s : -s;
    }

    /** 0 = pieno giorno, 1 = piena notte (transizione morbida all'orizzonte). */
    float nightAmount() const {
        float elev = toSun().y;           // -1 .. 1
        return std::clamp(0.5f - elev * 2.2f, 0.f, 1.f);
    }

    /** Vettore luce per gli shader (L = -uSun). */
    glm::vec3 sunDirection() const { return -toSkyLight(); }

    glm::vec3 sunTint() const {
        // alba/tramonto più caldi
        float h = clockTime;
        float dusk = 0.f;
        if (h < 6.f) dusk = 1.f - h / 6.f;
        else if (h > 18.f) dusk = (h - 18.f) / 6.f;
        dusk = std::clamp(dusk, 0.f, 1.f);
        glm::vec3 day{1.f, 0.97f, 0.92f};
        glm::vec3 warm{1.f, 0.55f, 0.28f};
        return glm::mix(day, warm, dusk * 0.85f) * std::clamp(brightness * 0.45f, 0.35f, 2.2f);
    }

    /** Colore del cielo (zenit / orizzonte) per l'ora corrente. */
    void skyColors(glm::vec3& zenith, glm::vec3& horizon) const {
        float n = nightAmount();
        glm::vec3 dayZ{0.42f, 0.65f, 0.95f}, dayH{0.69f, 0.84f, 0.95f};
        glm::vec3 nightZ{0.03f, 0.05f, 0.12f}, nightH{0.10f, 0.12f, 0.22f};
        // vicino all'orizzonte il sole scalda il cielo basso
        float dusk = std::clamp(1.f - std::fabs(toSun().y) * 4.f, 0.f, 1.f) * (1.f - n);
        zenith = glm::mix(dayZ, nightZ, n);
        horizon = glm::mix(glm::mix(dayH, nightH, n), glm::vec3(1.f, 0.62f, 0.35f), dusk * 0.55f);
    }
};
