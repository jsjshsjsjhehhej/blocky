#pragma once

// Mondo: spawn, sonno, giorno e nome — il guscio survival di un alpha.

#include "systems/Terrain.h"

#include <algorithm>
#include <string>

class Mondo {
public:
    void reset() {
        spawnX_ = 0;
        spawnY_ = 12;
        spawnZ_ = 0;
        day_ = 1;
        name_ = "mondo";
        slept_ = false;
        terrain_ = nullptr;
        playTime_ = 0.f;
    }

    void attach(Terrain* t) { terrain_ = t; }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        playTime_ += dt;
    }

    void setSpawn(float x, float y, float z) {
        spawnX_ = x;
        spawnY_ = y;
        spawnZ_ = z;
    }

    void spawnFromTerrain() {
        if (!terrain_) return;
        float y = (terrain_->surfaceY(0, 0) + 1.15f) * terrain_->blockSize();
        spawnX_ = 0.f;
        spawnY_ = y;
        spawnZ_ = 0.f;
    }

    bool sleep() {
        slept_ = true;
        day_ += 1;
        return true;
    }

    bool takeSleep() {
        bool s = slept_;
        slept_ = false;
        return s;
    }

    bool setName(const std::string& n) {
        if (n.empty() || n.size() > 24) return false;
        for (char c : n)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-'))
                return false;
        name_ = n;
        return true;
    }

    float spawnX() const { return spawnX_; }
    float spawnY() const { return spawnY_; }
    float spawnZ() const { return spawnZ_; }
    int day() const { return day_; }
    const std::string& name() const { return name_; }
    float playTime() const { return playTime_; }
    int seed() const { return terrain_ ? terrain_->seed() : 0; }

private:
    float spawnX_ = 0, spawnY_ = 12, spawnZ_ = 0;
    int day_ = 1;
    std::string name_ = "mondo";
    bool slept_ = false;
    Terrain* terrain_ = nullptr;
    float playTime_ = 0.f;
};
