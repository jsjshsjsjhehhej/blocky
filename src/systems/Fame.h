#pragma once

// Fame: barra 0-20 come Minecraft alpha/beta, cibo e danno da fame.

#include <algorithm>
#include <string>

class Fame {
public:
    static constexpr float kMax = 20.f;

    void reset() {
        hunger_ = kMax;
        sat_ = 5.f;
        exhaust_ = 0.f;
        starve_ = 0.f;
        acc_ = 0.f;
        lastFood_ = 0.f;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.1f) dt = 0.1f;
        acc_ += dt;
        if (acc_ < 1.f) return;
        acc_ = 0.f;
        if (sat_ > 0.f) {
            sat_ = std::max(0.f, sat_ - 0.15f);
            return;
        }
        if (hunger_ > 0.f) {
            hunger_ = std::max(0.f, hunger_ - 0.25f);
            return;
        }
        starve_ += 1.f;
    }

    bool eat(const std::string& item) {
        int food = 0;
        float sat = 0.f;
        foodOf(item, food, sat);
        if (food <= 0) return false;
        if (hunger_ >= kMax && sat_ >= 8.f) return false;
        hunger_ = std::min(kMax, hunger_ + (float)food);
        sat_ = std::min(kMax, sat_ + sat);
        lastFood_ = (float)food;
        return true;
    }

    bool isFood(const std::string& item) const {
        int food = 0;
        float sat = 0.f;
        foodOf(item, food, sat);
        return food > 0;
    }

    int foodValue(const std::string& item) const {
        int food = 0;
        float sat = 0.f;
        foodOf(item, food, sat);
        return food;
    }

    void set(float h) { hunger_ = std::clamp(h, 0.f, kMax); }
    float hunger() const { return hunger_; }
    float saturation() const { return sat_; }
    bool starving() const { return hunger_ <= 0.f; }

    float takeStarveDamage() {
        float d = starve_;
        starve_ = 0.f;
        return d;
    }
    float takeFoodHeal() {
        float h = lastFood_;
        lastFood_ = 0.f;
        return h;
    }

private:
    static void foodOf(const std::string& item, int& food, float& sat) {
        std::string t = item;
        for (char& c : t)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (t == "apple" || t == "mela") { food = 4; sat = 2.4f; }
        else if (t == "bread" || t == "pane") { food = 5; sat = 6.f; }
        else if (t == "pork" || t == "rawpork") { food = 3; sat = 1.8f; }
        else if (t == "cookedpork" || t == "porkchop") { food = 8; sat = 12.8f; }
        else if (t == "beef" || t == "rawbeef") { food = 3; sat = 1.8f; }
        else if (t == "cookedbeef" || t == "steak") { food = 8; sat = 12.8f; }
        else if (t == "chicken" || t == "rawchicken") { food = 2; sat = 1.2f; }
        else if (t == "cookedchicken") { food = 6; sat = 7.2f; }
        else if (t == "mushroom" || t == "fungo") { food = 1; sat = 0.4f; }
        else if (t == "stew" || t == "mushroomstew") { food = 8; sat = 9.6f; }
        else if (t == "wheat" || t == "grano") { food = 1; sat = 0.2f; }
        else if (t == "cookie" || t == "biscotto") { food = 2; sat = 0.4f; }
        else if (t == "fish" || t == "pesce") { food = 2; sat = 0.4f; }
        else if (t == "cookedfish") { food = 5; sat = 6.f; }
        else if (t == "goldenapple" || t == "meladora") { food = 10; sat = 14.f; }
        else { food = 0; sat = 0.f; }
    }

    float hunger_ = kMax;
    float sat_ = 5.f;
    float exhaust_ = 0.f;
    float starve_ = 0.f;
    float acc_ = 0.f;
    float lastFood_ = 0.f;
};
