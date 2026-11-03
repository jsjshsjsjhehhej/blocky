#pragma once

// Mobs: animali e mostri stile Minecraft alpha + drop a terra.
// Serve Terrain attaccato per gravita e collisione sui blocchi.

#include "systems/Terrain.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

class Mobs {
public:
    struct Ent {
        std::string name;
        std::string kind;
        float x = 0, y = 0, z = 0;
        float vx = 0, vy = 0, vz = 0;
        float hp = 10, maxHp = 10;
        float age = 0;
        float yaw = 0;
        bool hostile = false;
        std::string drop;
        int dropN = 1;
        float explodeIn = -1.f;
    };

    struct Body {
        glm::vec3 pos{0};
        glm::vec3 size{1};
        glm::vec3 color{0.8f, 0.5f, 0.4f};
    };

    static constexpr int kMax = 48;

    void reset() {
        ents_.clear();
        dmg_ = 0;
        nextId_ = 1;
    }

    void attach(Terrain* t) { terrain_ = t; }

    void setPlayer(float x, float y, float z, bool night) {
        px_ = x; py_ = y; pz_ = z;
        night_ = night;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.08f) dt = 0.08f;
        float bs = terrain_ ? terrain_->blockSize() : 3.f;
        std::vector<Ent> keep;
        keep.reserve(ents_.size());
        for (Ent& e : ents_) {
            e.age += dt;
            if (e.explodeIn >= 0.f) {
                e.explodeIn -= dt;
                if (e.explodeIn <= 0.f) {
                    if (terrain_) {
                        auto c = terrain_->worldToCell({e.x, e.y, e.z});
                        terrain_->explode(c.x, c.y, c.z, 3);
                    }
                    float dx = e.x - px_, dy = e.y - py_, dz = e.z - pz_;
                    if (dx * dx + dy * dy + dz * dz < (bs * 4) * (bs * 4))
                        dmg_ += 12.f;
                    continue;
                }
            }
            think(e, dt, bs);
            integrate(e, dt, bs);
            if (e.hp <= 0.f) {
                if (!e.drop.empty() && e.kind != "item" && (int)keep.size() < kMax) {
                    Ent it;
                    it.kind = "item";
                    it.name = "item" + std::to_string(nextId_++);
                    it.x = e.x; it.y = e.y + 0.4f; it.z = e.z;
                    it.hp = 1; it.maxHp = 1;
                    it.drop = e.drop;
                    it.dropN = e.dropN;
                    keep.push_back(it);
                }
                continue;
            }
            if (e.kind == "item" && e.age > 60.f) continue;
            keep.push_back(e);
        }
        ents_.swap(keep);
    }

    bool spawn(const std::string& kind, float x, float y, float z) {
        if ((int)ents_.size() >= kMax) return false;
        Ent e;
        e.kind = kind.empty() ? "pig" : kind;
        for (char& c : e.kind)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        e.name = e.kind + std::to_string(nextId_++);
        e.x = x; e.y = y; e.z = z;
        applyKind(e);
        ents_.push_back(e);
        return true;
    }

    bool spawnItem(float x, float y, float z, const std::string& id, int n) {
        if ((int)ents_.size() >= kMax || id.empty()) return false;
        Ent e;
        e.kind = "item";
        e.name = "item" + std::to_string(nextId_++);
        e.x = x; e.y = y; e.z = z;
        e.hp = 1; e.maxHp = 1;
        e.drop = id;
        e.dropN = n > 0 ? n : 1;
        e.hostile = false;
        ents_.push_back(e);
        return true;
    }

    bool hurt(const std::string& name, float amount) {
        Ent* e = find(name);
        if (!e || amount <= 0.f) return false;
        e->hp -= amount;
        return true;
    }

    std::string pickupNear(float x, float y, float z, float r) {
        r = r > 0.2f ? r : 1.8f;
        for (auto it = ents_.begin(); it != ents_.end(); ++it) {
            if (it->kind != "item") continue;
            float dx = it->x - x, dy = it->y - y, dz = it->z - z;
            if (dx * dx + dy * dy + dz * dz > r * r) continue;
            std::string id = it->drop + ":" + std::to_string(it->dropN);
            ents_.erase(it);
            return id;
        }
        return {};
    }

    float takePlayerDamage() {
        float d = dmg_;
        dmg_ = 0.f;
        return d;
    }

    int spawnAround(int n, bool hostile) {
        if (!terrain_) return 0;
        int made = 0;
        n = std::clamp(n, 1, 12);
        float bs = terrain_->blockSize();
        for (int i = 0; i < n * 4 && made < n; ++i) {
            int ox = (int)(px_ / bs) + (int)((hash(i + nextId_) % 17) - 8);
            int oz = (int)(pz_ / bs) + (int)((hash(i * 3 + nextId_) % 17) - 8);
            int oy = terrain_->surfaceY(ox, oz) + 1;
            if (hostile && terrain_->getLight(ox, oy, oz) > 7) continue;
            glm::vec3 w = terrain_->cellToWorld(ox, oy, oz);
            const char* k = hostile
                ? ((hash(i) % 3 == 0) ? "creeper" : (hash(i) % 2 ? "zombie" : "skeleton"))
                : ((hash(i) % 3 == 0) ? "pig" : (hash(i) % 2 ? "sheep" : "cow"));
            if (spawn(k, w.x, w.y, w.z)) ++made;
        }
        return made;
    }

    const std::vector<Ent>& all() const { return ents_; }
    int count() const { return (int)ents_.size(); }

    std::vector<Body> bodies() const {
        std::vector<Body> out;
        out.reserve(ents_.size());
        for (const Ent& e : ents_) {
            Body b;
            b.pos = {e.x, e.y + 0.7f, e.z};
            if (e.kind == "item") b.size = {0.4f, 0.4f, 0.4f};
            else if (e.kind == "chicken") b.size = {0.6f, 0.7f, 0.6f};
            else if (e.kind == "creeper") b.size = {0.7f, 1.8f, 0.7f};
            else b.size = {0.8f, 1.5f, 0.8f};
            b.color = colorOf(e.kind);
            out.push_back(b);
        }
        return out;
    }

private:
    static glm::vec3 colorOf(const std::string& k) {
        if (k == "pig") return {0.92f, 0.55f, 0.55f};
        if (k == "sheep") return {0.92f, 0.92f, 0.90f};
        if (k == "cow") return {0.45f, 0.32f, 0.22f};
        if (k == "chicken") return {0.95f, 0.92f, 0.80f};
        if (k == "zombie") return {0.35f, 0.55f, 0.28f};
        if (k == "skeleton") return {0.88f, 0.88f, 0.82f};
        if (k == "creeper") return {0.22f, 0.62f, 0.22f};
        if (k == "spider") return {0.22f, 0.12f, 0.12f};
        if (k == "item") return {0.95f, 0.82f, 0.25f};
        return {0.8f, 0.5f, 0.4f};
    }

    void applyKind(Ent& e) {
        e.hostile = e.kind == "zombie" || e.kind == "skeleton" || e.kind == "creeper"
                 || e.kind == "spider";
        if (e.kind == "pig") { e.hp = 10; e.drop = "pork"; e.dropN = 1; }
        else if (e.kind == "sheep") { e.hp = 8; e.drop = "wool"; e.dropN = 1; }
        else if (e.kind == "cow") { e.hp = 10; e.drop = "beef"; e.dropN = 1; }
        else if (e.kind == "chicken") { e.hp = 4; e.drop = "feather"; e.dropN = 1; }
        else if (e.kind == "zombie") { e.hp = 20; e.drop = "dirt"; e.dropN = 0; }
        else if (e.kind == "skeleton") { e.hp = 20; e.drop = "coal"; e.dropN = 1; }
        else if (e.kind == "creeper") { e.hp = 20; e.drop = "dirt"; e.dropN = 0; }
        else if (e.kind == "spider") { e.hp = 16; e.drop = "string"; e.dropN = 1; }
        e.maxHp = e.hp;
    }

    void think(Ent& e, float dt, float bs) {
        if (e.kind == "item") return;
        float dx = px_ - e.x, dz = pz_ - e.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        bool chase = e.hostile && (night_ || dist < bs * 12.f);
        if (e.kind == "creeper" && dist < bs * 2.2f && e.explodeIn < 0.f)
            e.explodeIn = 1.2f;
        if (chase && dist > 0.4f) {
            e.vx = (dx / dist) * 4.2f;
            e.vz = (dz / dist) * 4.2f;
            if (e.kind == "spider") { e.vx *= 1.3f; e.vz *= 1.3f; }
            if (dist < bs * 1.4f && e.kind != "creeper")
                dmg_ += 4.f * dt;
        } else {
            e.yaw += dt * ((hash((int)e.age) % 5) - 2);
            e.vx = std::sin(e.yaw) * 1.6f;
            e.vz = std::cos(e.yaw) * 1.6f;
        }
    }

    void integrate(Ent& e, float dt, float bs) {
        e.vy -= 32.f * dt;
        e.x += e.vx * dt;
        e.z += e.vz * dt;
        e.y += e.vy * dt;
        if (!terrain_) {
            if (e.y < 0.f) { e.y = 0.f; e.vy = 0.f; }
            return;
        }
        auto c = terrain_->worldToCell({e.x, e.y, e.z});
        if (terrain_->solid(terrain_->get(c.x, c.y, c.z))) {
            e.y = terrain_->cellAabbMax(c.x, c.y, c.z).y + 0.02f;
            e.vy = 0.f;
        }
        if (terrain_->solid(terrain_->get(c.x, c.y + 1, c.z))) {
            e.x -= e.vx * dt;
            e.z -= e.vz * dt;
        }
        (void)bs;
    }

    Ent* find(const std::string& name) {
        for (Ent& e : ents_)
            if (e.name == name) return &e;
        return nullptr;
    }
    static uint32_t hash(int v) {
        uint32_t h = (uint32_t)v * 2654435761u;
        return h ^ (h >> 16);
    }

    std::vector<Ent> ents_;
    Terrain* terrain_ = nullptr;
    float px_ = 0, py_ = 0, pz_ = 0;
    bool night_ = false;
    float dmg_ = 0;
    int nextId_ = 1;
};
