#pragma once

// Cassa: chest 27 slot su una cella voxel, come Minecraft alpha/infdev.

#include <algorithm>
#include <string>
#include <vector>

class Cassa {
public:
    static constexpr int kSlots = 27;
    static constexpr int kMaxStack = 64;
    static constexpr int kMaxChests = 64;

    struct Slot {
        std::string id;
        std::string name;
        int count = 0;
    };
    struct Box {
        int x = 0, y = 0, z = 0;
        Slot slots[kSlots];
    };

    void reset() { boxes_.clear(); }
    void update(float) {}

    bool open(int x, int y, int z) {
        if (find(x, y, z)) return true;
        if ((int)boxes_.size() >= kMaxChests) return false;
        Box b;
        b.x = x;
        b.y = y;
        b.z = z;
        boxes_.push_back(b);
        return true;
    }

    bool close(int x, int y, int z) {
        for (int i = 0; i < (int)boxes_.size(); ++i) {
            if (boxes_[(size_t)i].x != x || boxes_[(size_t)i].y != y || boxes_[(size_t)i].z != z)
                continue;
            boxes_.erase(boxes_.begin() + i);
            return true;
        }
        return false;
    }

    int put(int x, int y, int z, const std::string& id, const std::string& name, int n) {
        Box* b = find(x, y, z);
        if (!b || id.empty() || n <= 0) return 0;
        int left = n;
        for (int i = 0; i < kSlots && left > 0; ++i) {
            Slot& s = b->slots[i];
            if (s.id != id) continue;
            int room = kMaxStack - s.count;
            int take = std::min(room, left);
            s.count += take;
            left -= take;
        }
        for (int i = 0; i < kSlots && left > 0; ++i) {
            Slot& s = b->slots[i];
            if (!s.id.empty()) continue;
            s.id = id;
            s.name = name.empty() ? id : name;
            s.count = std::min(kMaxStack, left);
            left -= s.count;
        }
        return n - left;
    }

    int take(int x, int y, int z, const std::string& id, int n) {
        Box* b = find(x, y, z);
        if (!b || n <= 0) return 0;
        int need = n;
        for (int i = kSlots - 1; i >= 0 && need > 0; --i) {
            Slot& s = b->slots[i];
            if (s.id != id) continue;
            int takeN = std::min(s.count, need);
            s.count -= takeN;
            need -= takeN;
            if (s.count <= 0) {
                s.id.clear();
                s.name.clear();
                s.count = 0;
            }
        }
        return n - need;
    }

    int count(int x, int y, int z, const std::string& id) const {
        const Box* b = findConst(x, y, z);
        if (!b) return 0;
        int n = 0;
        for (int i = 0; i < kSlots; ++i)
            if (b->slots[i].id == id) n += b->slots[i].count;
        return n;
    }

    const Box* get(int x, int y, int z) const { return findConst(x, y, z); }
    int chestCount() const { return (int)boxes_.size(); }
    const std::vector<Box>& all() const { return boxes_; }

private:
    Box* find(int x, int y, int z) {
        for (Box& b : boxes_)
            if (b.x == x && b.y == y && b.z == z) return &b;
        return nullptr;
    }
    const Box* findConst(int x, int y, int z) const {
        for (const Box& b : boxes_)
            if (b.x == x && b.y == y && b.z == z) return &b;
        return nullptr;
    }

    std::vector<Box> boxes_;
};
