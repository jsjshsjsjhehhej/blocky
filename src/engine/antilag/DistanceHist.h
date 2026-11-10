#pragma once

// Istogramma delle distanze delle part. Se il giocatore tiene 400 stud di
// vista ma il 95% del mondo sta entro 60, disegnare (e testare) fino a 400
// non aggiunge nulla: il tetto puo' chiudersi sulla distanza che contiene
// davvero le part che il budget riesce a mostrare.

#include "Math.h"

namespace antilag {

class DistanceHist {
public:
    static constexpr int kBins = 16;

    void clear() {
        for (int& b : bins_) b = 0;
        total_ = 0;
        sum_ = 0.f;
        farthest_ = 0.f;
    }

    void add(float distance) {
        if (distance < 0.f) distance = 0.f;
        if (distance > 4000.f) distance = 4000.f;
        int b = bucket(distance);
        bins_[b] += 1;
        total_ += 1;
        sum_ += distance;
        if (distance > farthest_) farthest_ = distance;
    }

    int total() const { return total_; }
    float mean() const { return total_ > 0 ? sum_ / (float)total_ : 0.f; }
    float farthest() const { return farthest_; }

    // Distanza entro cui cadono i primi `keep` campioni (i piu' vicini,
    // perche' i secchi sono ordinati dal vicino al lontano). 0 se vuoto.
    float reach(int keep) const {
        if (total_ <= 0) return 0.f;
        if (keep < 1) keep = 1;
        if (keep > total_) keep = total_;
        int seen = 0;
        for (int i = 0; i < kBins; ++i) {
            seen += bins_[i];
            if (seen >= keep) return edge(i);
        }
        return edge(kBins - 1);
    }

    // Quante part stanno oltre `distance`. Utile per sapere se il taglio
    // del governor sta davvero lasciando fuori qualcosa.
    int beyond(float distance) const {
        if (total_ <= 0) return 0;
        int b0 = bucket(distance);
        int n = 0;
        for (int i = b0 + 1; i < kBins; ++i) n += bins_[i];
        return n;
    }

    static float edge(int bucket) {
        if (bucket < 0) bucket = 0;
        if (bucket >= kBins) bucket = kBins - 1;
        // 4, 5.6, 7.8, ... fino a circa 900 stud.
        float e = 4.f;
        for (int i = 0; i < bucket; ++i) e *= 1.4f;
        return e;
    }

    static int bucket(float distance) {
        float e = 4.f;
        for (int i = 0; i < kBins - 1; ++i) {
            if (distance <= e) return i;
            e *= 1.4f;
        }
        return kBins - 1;
    }

private:
    int bins_[kBins] = {};
    int total_ = 0;
    float sum_ = 0.f;
    float farthest_ = 0.f;
};

// Non si scende sotto il piano gia' deciso se il mondo e' vuoto, e non si
// allunga: l'istogramma puo' solo chiudere vista sprecata.
inline float capDistanceToContent(float planned, const DistanceHist& hist, int keep) {
    if (hist.total() < 8) return planned;
    float reach = hist.reach(keep);
    if (reach <= 0.f) return planned;
    float cap = reach * 1.2f;
    if (cap < 24.f) cap = 24.f;
    if (cap < planned) return cap;
    return planned;
}

}
