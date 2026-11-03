#pragma once

// Inventario: stack di oggetti per il giocatore locale. Gli script usano
// Inventario.Add / Remove / Count. Max 32 slot, stack fino a 99.

#include <algorithm>
#include <string>
#include <vector>

class Inventario {
public:
    static constexpr int kMaxSlots = 32;
    static constexpr int kMaxStack = 99;

    struct Slot {
        std::string id;
        std::string name;
        int count = 0;
    };

    void reset() {
        slots_.clear();
        dirty_ = false;
        selected_ = 0;
    }

    void update(float) {}

    static bool validId(const std::string& s) {
        if (s.empty() || s.size() > 24) return false;
        for (char c : s)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-'))
                return false;
        return true;
    }

    int count(const std::string& id) const {
        int n = 0;
        for (const Slot& s : slots_)
            if (s.id == id) n += s.count;
        return n;
    }

    bool has(const std::string& id, int n = 1) const { return count(id) >= n; }

    int add(const std::string& id, const std::string& name, int n) {
        if (!validId(id) || n <= 0) return 0;
        int left = n;
        for (Slot& s : slots_) {
            if (s.id != id) continue;
            const int room = kMaxStack - s.count;
            const int take = std::min(room, left);
            s.count += take;
            left -= take;
            if (left == 0) break;
        }
        while (left > 0 && (int)slots_.size() < kMaxSlots) {
            Slot s;
            s.id = id;
            s.name = name.empty() ? id : name;
            s.count = std::min(kMaxStack, left);
            left -= s.count;
            slots_.push_back(s);
        }
        dirty_ = true;
        return n - left;
    }

    int remove(const std::string& id, int n) {
        if (n <= 0) return 0;
        int need = n;
        for (int i = (int)slots_.size() - 1; i >= 0 && need > 0; --i) {
            if (slots_[(size_t)i].id != id) continue;
            const int take = std::min(slots_[(size_t)i].count, need);
            slots_[(size_t)i].count -= take;
            need -= take;
            if (slots_[(size_t)i].count <= 0)
                slots_.erase(slots_.begin() + i);
        }
        if (need != n) dirty_ = true;
        return n - need;
    }

    const std::vector<Slot>& slots() const { return slots_; }
    int slotCount() const { return (int)slots_.size(); }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    void select(int i) {
        if (slots_.empty()) { selected_ = 0; return; }
        int n = std::min(9, (int)slots_.size());
        selected_ = ((i % n) + n) % n;
    }
    int selected() const { return selected_; }
    const Slot* selectedSlot() const {
        if (selected_ < 0 || selected_ >= (int)slots_.size()) return nullptr;
        return &slots_[(size_t)selected_];
    }
    const char* selectedId() const {
        const Slot* s = selectedSlot();
        return s ? s->id.c_str() : "";
    }
    const Slot* slotAt(int i) const {
        if (i < 0 || i >= (int)slots_.size()) return nullptr;
        return &slots_[(size_t)i];
    }

private:
    std::vector<Slot> slots_;
    bool dirty_ = false;
    int selected_ = 0;
};
