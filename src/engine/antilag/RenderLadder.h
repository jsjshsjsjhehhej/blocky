#pragma once

// La scala di risoluzione si muove a gradini, non al numero continuo del
// piano. Un frame chiede 0.91 e il successivo 1.0: senza questo, la
// risoluzione interna cambia ogni volta e il bloom rialloca. Si scende
// subito (al massimo due gradini per frame) e si risale un gradino alla
// volta, dopo una manciata di frame in cui il piano e' gia' piu' alto.

#include "Math.h"

namespace antilag {

class RenderLadder {
public:
    static constexpr int kSteps = 6;

    static float stepValue(int index) {
        static const float k[kSteps] = { 1.f, 0.85f, 0.75f, 0.67f, 0.58f, 0.50f };
        if (index < 0) index = 0;
        if (index >= kSteps) index = kSteps - 1;
        return k[index];
    }

    // Primo gradino che non supera `wanted`. La tabella e' decrescente,
    // quindi e' la scala piu' alta ancora concessa dal piano.
    static int indexFor(float wanted) {
        if (wanted >= 0.97f) return 0;
        for (int i = 0; i < kSteps; ++i) {
            if (stepValue(i) <= wanted + 0.001f) return i;
        }
        return kSteps - 1;
    }

    void reset() {
        index_ = 0;
        hold_ = 0;
    }

    int index() const { return index_; }
    float current() const { return stepValue(index_); }

    float follow(float wanted) {
        if (wanted < 0.5f) wanted = 0.5f;
        if (wanted > 1.f) wanted = 1.f;
        int target = indexFor(wanted);
        if (target > index_) {
            int drop = target - index_;
            if (drop > 2) drop = 2;
            index_ += drop;
            hold_ = 10;
        } else if (target < index_) {
            if (hold_ > 0) {
                --hold_;
            } else {
                --index_;
                hold_ = 6;
            }
        }
        float v = stepValue(index_);
        // Se il piano e' gia' piu' basso del gradino, si scende subito:
        // l'attesa vale solo per la risalita, non per tenere pixel di troppo
        // mentre il frame e' ancora lungo.
        if (v > wanted) {
            index_ = target;
            hold_ = 10;
            v = stepValue(index_);
        }
        return v;
    }

private:
    int index_ = 0;
    int hold_ = 0;
};

}
