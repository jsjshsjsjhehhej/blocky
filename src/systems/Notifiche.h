#pragma once

#include <algorithm>
#include <string>
#include <vector>

class Notifiche {
public:
    struct Toast {
        std::string text;
        float life = 2.5f;
    };

    static constexpr int kMax = 8;

    void reset() { items_.clear(); }

    void update(float dt) {
        for (Toast& t : items_) t.life -= dt;
        items_.erase(std::remove_if(items_.begin(), items_.end(),
                                    [](const Toast& t) { return t.life <= 0.f; }),
                     items_.end());
    }

    void push(const std::string& text, float seconds) {
        if (text.empty()) return;
        Toast t;
        t.text = text.substr(0, 80);
        t.life = seconds > 0.2f ? seconds : 2.5f;
        items_.insert(items_.begin(), t);
        if ((int)items_.size() > kMax) items_.pop_back();
    }

    const std::vector<Toast>& items() const { return items_; }

private:
    std::vector<Toast> items_;
};
