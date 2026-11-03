#pragma once

// Hitbox avanzate: box ruotata (yaw), sfera, capsula. Gruppi e maschere
// (player/hurt/hit/proj/trigger). Overlap, raggio, vita.

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

class Hitbox {
public:
    enum class Shape : uint8_t { Box = 0, Sphere, Capsule };
    static constexpr uint32_t GWorld = 1u;
    static constexpr uint32_t GPlayer = 2u;
    static constexpr uint32_t GHurt = 4u;
    static constexpr uint32_t GHit = 8u;
    static constexpr uint32_t GProj = 16u;
    static constexpr uint32_t GTrigger = 32u;
    static constexpr uint32_t GAll = 0xFFFFFFFFu;
    static constexpr int kMax = 96;

    struct Box {
        std::string name;
        Shape shape = Shape::Box;
        float x = 0, y = 0, z = 0;
        float hx = 1, hy = 1, hz = 1; // half extents; per sfera hx=raggio; capsula hx=r, hy=halfH
        float yaw = 0.f;
        uint32_t group = GWorld;
        uint32_t mask = GAll;
        float life = -1.f;
        bool enabled = true;
    };

    struct Hit {
        std::string a, b;
        float x = 0, y = 0, z = 0;
    };

    struct RayHit {
        std::string name;
        float t = 0, x = 0, y = 0, z = 0;
        bool ok = false;
    };

    void reset() {
        boxes_.clear();
        hits_.clear();
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        std::vector<Box> keep;
        keep.reserve(boxes_.size());
        for (Box& b : boxes_) {
            if (b.life >= 0.f) {
                b.life -= dt;
                if (b.life <= 0.f) continue;
            }
            keep.push_back(b);
        }
        boxes_.swap(keep);
        hits_.clear();
        for (int i = 0; i < (int)boxes_.size(); ++i) {
            for (int j = i + 1; j < (int)boxes_.size(); ++j) {
                const Box& a = boxes_[(size_t)i];
                const Box& b = boxes_[(size_t)j];
                if (!a.enabled || !b.enabled) continue;
                if ((a.group & b.mask) == 0 || (b.group & a.mask) == 0) continue;
                glm::vec3 p;
                if (overlap(a, b, p)) {
                    Hit h;
                    h.a = a.name; h.b = b.name;
                    h.x = p.x; h.y = p.y; h.z = p.z;
                    hits_.push_back(h);
                }
            }
        }
    }

    static uint32_t parseGroup(const std::string& s) {
        if (s == "player") return GPlayer;
        if (s == "hurt") return GHurt;
        if (s == "hit") return GHit;
        if (s == "proj" || s == "projectile") return GProj;
        if (s == "trigger") return GTrigger;
        if (s == "all") return GAll;
        return GWorld;
    }

    bool addBox(const std::string& name, float x, float y, float z,
                float hx, float hy, float hz, float yaw, uint32_t group) {
        Box b;
        if (!prep(name, b)) return false;
        b.shape = Shape::Box;
        b.x = x; b.y = y; b.z = z;
        b.hx = std::max(0.05f, hx); b.hy = std::max(0.05f, hy); b.hz = std::max(0.05f, hz);
        b.yaw = yaw;
        b.group = group ? group : GWorld;
        boxes_.push_back(b);
        return true;
    }

    bool addSphere(const std::string& name, float x, float y, float z, float r, uint32_t group) {
        Box b;
        if (!prep(name, b)) return false;
        b.shape = Shape::Sphere;
        b.x = x; b.y = y; b.z = z;
        b.hx = std::max(0.05f, r);
        b.group = group ? group : GWorld;
        boxes_.push_back(b);
        return true;
    }

    bool addCapsule(const std::string& name, float x, float y, float z,
                    float r, float halfH, float yaw, uint32_t group) {
        Box b;
        if (!prep(name, b)) return false;
        b.shape = Shape::Capsule;
        b.x = x; b.y = y; b.z = z;
        b.hx = std::max(0.05f, r);
        b.hy = std::max(0.05f, halfH);
        b.yaw = yaw;
        b.group = group ? group : GWorld;
        boxes_.push_back(b);
        return true;
    }

    bool move(const std::string& name, float x, float y, float z, float yaw) {
        Box* b = find(name);
        if (!b) return false;
        b->x = x; b->y = y; b->z = z; b->yaw = yaw;
        return true;
    }

    bool setLife(const std::string& name, float seconds) {
        Box* b = find(name);
        if (!b) return false;
        b->life = seconds;
        return true;
    }

    bool setMask(const std::string& name, uint32_t mask) {
        Box* b = find(name);
        if (!b) return false;
        b->mask = mask ? mask : GAll;
        return true;
    }

    bool setEnabled(const std::string& name, bool on) {
        Box* b = find(name);
        if (!b) return false;
        b->enabled = on;
        return true;
    }

    bool remove(const std::string& name) {
        for (auto it = boxes_.begin(); it != boxes_.end(); ++it) {
            if (it->name == name) { boxes_.erase(it); return true; }
        }
        return false;
    }

    bool hits(const std::string& a, const std::string& b) const {
        for (const Hit& h : hits_)
            if ((h.a == a && h.b == b) || (h.a == b && h.b == a)) return true;
        const Box *ba = findConst(a), *bb = findConst(b);
        if (!ba || !bb || !ba->enabled || !bb->enabled) return false;
        if ((ba->group & bb->mask) == 0 || (bb->group & ba->mask) == 0) return false;
        glm::vec3 p;
        return overlap(*ba, *bb, p);
    }

    std::vector<std::string> overlapNames(const std::string& name) const {
        std::vector<std::string> out;
        const Box* a = findConst(name);
        if (!a) return out;
        glm::vec3 p;
        for (const Box& b : boxes_) {
            if (b.name == name || !b.enabled) continue;
            if ((a->group & b.mask) == 0 || (b.group & a->mask) == 0) continue;
            if (overlap(*a, b, p)) out.push_back(b.name);
        }
        return out;
    }

    RayHit ray(float ox, float oy, float oz, float dx, float dy, float dz,
               float maxDist, uint32_t mask) const {
        RayHit best;
        glm::vec3 o(ox, oy, oz), d(dx, dy, dz);
        float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len < 1e-6f) return best;
        d.x /= len; d.y /= len; d.z /= len;
        float bestT = maxDist > 0.f ? maxDist : 500.f;
        for (const Box& b : boxes_) {
            if (!b.enabled) continue;
            if ((b.group & mask) == 0) continue;
            float t = 0.f;
            glm::vec3 hit;
            if (!raycast(b, o, d, bestT, t, hit)) continue;
            if (t < 0.f || t > bestT) continue;
            bestT = t;
            best.ok = true;
            best.t = t;
            best.name = b.name;
            best.x = hit.x; best.y = hit.y; best.z = hit.z;
        }
        return best;
    }

    const std::vector<Hit>& frameHits() const { return hits_; }
    const std::vector<Box>& all() const { return boxes_; }
    int count() const { return (int)boxes_.size(); }

    static bool overlap(const Box& a, const Box& b, glm::vec3& out) {
        if (a.shape == Shape::Sphere && b.shape == Shape::Sphere) {
            float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            float d2 = dx * dx + dy * dy + dz * dz;
            float r = a.hx + b.hx;
            if (d2 > r * r) return false;
            out = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
            return true;
        }
        if (a.shape == Shape::Sphere)
            return sphereVs(a, b, out);
        if (b.shape == Shape::Sphere)
            return sphereVs(b, a, out);
        if (a.shape == Shape::Capsule || b.shape == Shape::Capsule)
            return capsulePair(a, b, out);
        return boxVsBox(a, b, out);
    }

private:
    bool prep(const std::string& name, Box& b) {
        if (name.empty() || name.size() > 32 || (int)boxes_.size() >= kMax) return false;
        remove(name);
        b.name = name;
        return true;
    }
    Box* find(const std::string& name) {
        for (Box& b : boxes_)
            if (b.name == name) return &b;
        return nullptr;
    }
    const Box* findConst(const std::string& name) const {
        for (const Box& b : boxes_)
            if (b.name == name) return &b;
        return nullptr;
    }

    static glm::vec3 toLocal(const Box& b, float wx, float wy, float wz) {
        float dx = wx - b.x, dy = wy - b.y, dz = wz - b.z;
        float c = std::cos(-b.yaw * 0.017453292f), s = std::sin(-b.yaw * 0.017453292f);
        return {dx * c - dz * s, dy, dx * s + dz * c};
    }
    static glm::vec3 toWorld(const Box& b, float lx, float ly, float lz) {
        float c = std::cos(b.yaw * 0.017453292f), s = std::sin(b.yaw * 0.017453292f);
        return {b.x + lx * c + lz * s, b.y + ly, b.z + -lx * s + lz * c};
    }

    static glm::vec3 closestOnSegment(glm::vec3 a, glm::vec3 b, glm::vec3 p) {
        glm::vec3 ab = b - a;
        float d = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        if (d < 1e-8f) return a;
        float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y + (p.z - a.z) * ab.z) / d;
        t = std::clamp(t, 0.f, 1.f);
        return a + ab * t;
    }

    static glm::vec3 capsuleSegA(const Box& b) {
        return toWorld(b, 0.f, -b.hy, 0.f);
    }
    static glm::vec3 capsuleSegB(const Box& b) {
        return toWorld(b, 0.f, b.hy, 0.f);
    }

    static bool sphereVs(const Box& s, const Box& o, glm::vec3& out) {
        glm::vec3 c(s.x, s.y, s.z);
        glm::vec3 p;
        if (o.shape == Shape::Box) {
            glm::vec3 l = toLocal(o, c.x, c.y, c.z);
            l.x = std::clamp(l.x, -o.hx, o.hx);
            l.y = std::clamp(l.y, -o.hy, o.hy);
            l.z = std::clamp(l.z, -o.hz, o.hz);
            p = toWorld(o, l.x, l.y, l.z);
        } else {
            p = closestOnSegment(capsuleSegA(o), capsuleSegB(o), c);
        }
        float dx = c.x - p.x, dy = c.y - p.y, dz = c.z - p.z;
        float r = s.hx + (o.shape == Shape::Capsule ? o.hx : 0.f);
        if (o.shape == Shape::Box) r = s.hx;
        if (dx * dx + dy * dy + dz * dz > r * r) return false;
        out = {(c.x + p.x) * 0.5f, (c.y + p.y) * 0.5f, (c.z + p.z) * 0.5f};
        return true;
    }

    static bool capsulePair(const Box& a, const Box& b, glm::vec3& out) {
        Box sa = a, sb = b;
        if (a.shape != Shape::Capsule) {
            sa.shape = Shape::Sphere;
            return sphereVs(sa, b, out);
        }
        if (b.shape != Shape::Capsule) {
            sb.shape = Shape::Sphere;
            return sphereVs(sb, a, out);
        }
        glm::vec3 a0 = capsuleSegA(a), a1 = capsuleSegB(a);
        glm::vec3 b0 = capsuleSegA(b), b1 = capsuleSegB(b);
        glm::vec3 pa = closestOnSegment(a0, a1, (b0 + b1) * 0.5f);
        glm::vec3 pb = closestOnSegment(b0, b1, pa);
        pa = closestOnSegment(a0, a1, pb);
        float dx = pa.x - pb.x, dy = pa.y - pb.y, dz = pa.z - pb.z;
        float r = a.hx + b.hx;
        if (dx * dx + dy * dy + dz * dz > r * r) return false;
        out = {(pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f, (pa.z + pb.z) * 0.5f};
        return true;
    }

    static bool boxVsBox(const Box& a, const Box& b, glm::vec3& out) {
        if (std::fabs(a.y - b.y) > a.hy + b.hy) return false;
        const float deg = 0.017453292f;
        float ac = std::cos(a.yaw * deg), as = std::sin(a.yaw * deg);
        float bc = std::cos(b.yaw * deg), bs = std::sin(b.yaw * deg);
        glm::vec2 aX(ac, -as), aZ(as, ac), bX(bc, -bs), bZ(bs, bc);
        glm::vec2 d(b.x - a.x, b.z - a.z);
        auto sep = [&](glm::vec2 axis) {
            float n = std::sqrt(axis.x * axis.x + axis.y * axis.y);
            if (n < 1e-8f) return false;
            axis.x /= n; axis.y /= n;
            float pa = std::fabs(aX.x * axis.x + aX.y * axis.y) * a.hx
                     + std::fabs(aZ.x * axis.x + aZ.y * axis.y) * a.hz;
            float pb = std::fabs(bX.x * axis.x + bX.y * axis.y) * b.hx
                     + std::fabs(bZ.x * axis.x + bZ.y * axis.y) * b.hz;
            float pd = std::fabs(d.x * axis.x + d.y * axis.y);
            return pd > pa + pb + 1e-5f;
        };
        if (sep(aX) || sep(aZ) || sep(bX) || sep(bZ)) return false;
        out = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
        return true;
    }

    static bool raycast(const Box& b, glm::vec3 o, glm::vec3 d, float maxT, float& tOut, glm::vec3& hit) {
        if (b.shape == Shape::Sphere) {
            glm::vec3 oc(o.x - b.x, o.y - b.y, o.z - b.z);
            float qb = oc.x * d.x + oc.y * d.y + oc.z * d.z;
            float qc = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - b.hx * b.hx;
            float disc = qb * qb - qc;
            if (disc < 0.f) return false;
            float t = -qb - std::sqrt(disc);
            if (t < 0.f) t = -qb + std::sqrt(disc);
            if (t < 0.f || t > maxT) return false;
            tOut = t;
            hit = o + d * t;
            return true;
        }
        glm::vec3 lo = toLocal(b, o.x, o.y, o.z);
        glm::vec3 worldB = o + d;
        glm::vec3 lb = toLocal(b, worldB.x, worldB.y, worldB.z);
        glm::vec3 ld(lb.x - lo.x, lb.y - lo.y, lb.z - lo.z);
        float hx = b.shape == Shape::Capsule ? b.hx : b.hx;
        float hy = b.shape == Shape::Capsule ? (b.hy + b.hx) : b.hy;
        float hz = b.shape == Shape::Capsule ? b.hx : b.hz;
        float tmin = 0.f, tmax = maxT;
        for (int a = 0; a < 3; ++a) {
            float oA = (&lo.x)[a], dA = (&ld.x)[a], hA = a == 0 ? hx : (a == 1 ? hy : hz);
            if (std::fabs(dA) < 1e-8f) {
                if (oA < -hA || oA > hA) return false;
                continue;
            }
            float t1 = (-hA - oA) / dA, t2 = (hA - oA) / dA;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
        if (tmin < 0.f || tmin > maxT) return false;
        tOut = tmin;
        hit = o + d * tmin;
        return true;
    }

    std::vector<Box> boxes_;
    std::vector<Hit> hits_;
};
