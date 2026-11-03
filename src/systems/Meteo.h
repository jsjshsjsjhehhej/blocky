#pragma once

// Meteo: ora del giorno (0-24) e tempo atmosferico. Gli script possono
// fissare l'ora o lasciar scorrere il ciclo.

#include <algorithm>
#include <cmath>
#include <string>

class Meteo {
public:
    enum class Weather { Sole = 0, Nuvole, Pioggia, Neve };

    void reset() {
        hours_ = 12.f;
        speed_ = 0.f;
        weather_ = Weather::Sole;
    }

    void update(float dt) {
        if (speed_ == 0.f) return;
        hours_ += dt * speed_;
        while (hours_ >= 24.f) hours_ -= 24.f;
        while (hours_ < 0.f) hours_ += 24.f;
    }

    void setHours(float h) {
        hours_ = std::fmod(std::max(0.f, h), 24.f);
    }
    float hours() const { return hours_; }

    void setSpeed(float s) { speed_ = std::clamp(s, 0.f, 120.f); }
    float speed() const { return speed_; }

    void setWeather(Weather w) { weather_ = w; }
    Weather weather() const { return weather_; }

    static Weather parseWeather(const std::string& n) {
        if (n == "nuvole" || n == "cloud") return Weather::Nuvole;
        if (n == "pioggia" || n == "rain") return Weather::Pioggia;
        if (n == "neve" || n == "snow") return Weather::Neve;
        return Weather::Sole;
    }

    static const char* weatherName(Weather w) {
        switch (w) {
        case Weather::Nuvole: return "nuvole";
        case Weather::Pioggia: return "pioggia";
        case Weather::Neve: return "neve";
        default: return "sole";
        }
    }

    // 0 = mezzanotte, 1 = mezzogiorno. Per tinte cielo/luce.
    float dayFactor() const {
        const float ang = (hours_ / 24.f) * 6.2831853f;
        float s = std::sin(ang - 1.5708f);
        return std::clamp(s * 0.5f + 0.5f, 0.f, 1.f);
    }

    bool isNight() const { return hours_ < 6.f || hours_ >= 20.f; }

private:
    float hours_ = 12.f;
    float speed_ = 0.f;
    Weather weather_ = Weather::Sole;
};
