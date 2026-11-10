#pragma once

// Orologio del frame. Il resto del motore clampa dt a 50 ms per non far
// esplodere la fisica: quel clamp NASCONDE lo scatto proprio a chi dovrebbe
// reagire. Qui si registra il tempo vero, prima del clamp.

#include "Math.h"

#include <array>
#include <cstdint>

namespace antilag {

enum class HitchKind : uint8_t {
    None = 0,
    Mild,       // un filo sopra il target
    Heavy,      // scatto visibile
    Stall       // frame lunghissimo: disco, shader, script infinito
};

inline const char* hitchName(HitchKind k) {
    switch (k) {
    case HitchKind::None:  return "niente";
    case HitchKind::Mild:  return "lieve";
    case HitchKind::Heavy: return "scatto";
    case HitchKind::Stall: return "stallo";
    }
    return "?";
}

struct FrameSample {
    float ms = 0.f;
    HitchKind hitch = HitchKind::None;
};

class FrameClock {
public:
    static constexpr int kWindow = 120;

    void reset() {
        head_ = 0;
        count_ = 0;
        sum_ = 0.f;
        stalls_ = 0;
        heavies_ = 0;
        milds_ = 0;
        for (auto& s : window_) s = {};
        for (auto& b : hist_) b = 0;
    }

    void push(float seconds, float targetMs) {
        float ms = seconds * 1000.f;
        if (ms < 0.f) ms = 0.f;
        if (ms > 2000.f) ms = 2000.f;
        HitchKind kind = classify(ms, targetMs);
        if (count_ == kWindow) {
            forget(window_[head_]);
        } else {
            ++count_;
        }
        window_[head_] = {ms, kind};
        remember(window_[head_]);
        head_ = (head_ + 1) % kWindow;
    }

    int count() const { return count_; }
    int stalls() const { return stalls_; }
    int heavies() const { return heavies_; }
    int milds() const { return milds_; }

    float latestMs() const {
        if (count_ <= 0) return 0.f;
        int i = head_ - 1;
        if (i < 0) i = kWindow - 1;
        return window_[i].ms;
    }

    float averageMs() const {
        if (count_ <= 0) return 0.f;
        return sum_ / (float)count_;
    }

    // Percentile sulla finestra, p in 0..1. Copia e ordina: 120 float,
    // niente da ottimizzare, serve un numero stabile.
    float percentile(float p) const {
        if (count_ <= 0) return 0.f;
        p = clampf(p, 0.f, 1.f);
        float tmp[kWindow];
        int n = 0;
        for (int i = 0; i < count_; ++i) tmp[n++] = sampleAt(i).ms;
        for (int i = 1; i < n; ++i) {
            float v = tmp[i];
            int j = i;
            while (j > 0 && tmp[j - 1] > v) {
                tmp[j] = tmp[j - 1];
                --j;
            }
            tmp[j] = v;
        }
        float idx = p * (float)(n - 1);
        int i0 = (int)idx;
        int i1 = std::min(n - 1, i0 + 1);
        float f = idx - (float)i0;
        return tmp[i0] * (1.f - f) + tmp[i1] * f;
    }

    int badFrames(float limitMs) const {
        int n = 0;
        for (int i = 0; i < count_; ++i)
            if (sampleAt(i).ms > limitMs) ++n;
        return n;
    }

    int goodTail(float limitMs) const {
        int n = 0;
        for (int i = count_ - 1; i >= 0; --i) {
            if (sampleAt(i).ms > limitMs) break;
            ++n;
        }
        return n;
    }

    int badTail(float limitMs) const {
        int n = 0;
        for (int i = count_ - 1; i >= 0; --i) {
            if (sampleAt(i).ms <= limitMs) break;
            ++n;
        }
        return n;
    }

    static HitchKind classify(float ms, float targetMs) {
        if (targetMs < 1.f) targetMs = 16.7f;
        if (ms >= 80.f || ms >= targetMs * 4.f) return HitchKind::Stall;
        if (ms >= targetMs * 1.8f) return HitchKind::Heavy;
        if (ms >= targetMs * 1.15f) return HitchKind::Mild;
        return HitchKind::None;
    }

    // Istogramma a 32 secchi da 0 a 64 ms, piu' un secchio per il resto.
    // Serve al governor per distinguere "un po' sopra i 16 ms" da "meta'
    // dei frame a 40".
    static constexpr int kHist = 33;
    int histogram(int bucket) const {
        if (bucket < 0 || bucket >= kHist) return 0;
        return hist_[bucket];
    }

private:
    FrameSample sampleAt(int oldestFirst) const {
        if (count_ < kWindow) return window_[oldestFirst];
        int i = (head_ + oldestFirst) % kWindow;
        return window_[i];
    }

    static int bucketOf(float ms) {
        if (ms < 0.f) ms = 0.f;
        int b = (int)(ms / 2.f);
        if (b >= kHist) b = kHist - 1;
        return b;
    }

    void remember(const FrameSample& s) {
        sum_ += s.ms;
        ++hist_[bucketOf(s.ms)];
        if (s.hitch == HitchKind::Stall) ++stalls_;
        else if (s.hitch == HitchKind::Heavy) ++heavies_;
        else if (s.hitch == HitchKind::Mild) ++milds_;
    }

    void forget(const FrameSample& s) {
        sum_ -= s.ms;
        int b = bucketOf(s.ms);
        if (hist_[b] > 0) --hist_[b];
        if (s.hitch == HitchKind::Stall && stalls_ > 0) --stalls_;
        else if (s.hitch == HitchKind::Heavy && heavies_ > 0) --heavies_;
        else if (s.hitch == HitchKind::Mild && milds_ > 0) --milds_;
    }

    std::array<FrameSample, kWindow> window_{};
    std::array<int, kHist> hist_{};
    int head_ = 0;
    int count_ = 0;
    float sum_ = 0.f;
    int stalls_ = 0;
    int heavies_ = 0;
    int milds_ = 0;
};

}
