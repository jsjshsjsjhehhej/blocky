#pragma once

/**
 * Curve di animazione (F-Curve di Blender) e Action.
 *
 * Una FCurve anima UN numero di un osso: Location X, Rotation Z, Scale Y...
 * Nove canali per osso, come il Graph Editor di Blender. La rotazione si
 * anima in gradi Eulero XYZ: e' quello che si legge e si trascina in un curve
 * editor (con i quaternioni le curve non dicono niente a nessuno). Le chiavi
 * di rotazione vengono "srotolate" quando si inseriscono: da 179 a -179 si
 * passa per 180, non facendo il giro al contrario.
 *
 * Interpolazione per chiave, verso la successiva: Constant (scalino), Linear,
 * Bezier con maniglie. Le maniglie "Auto" si ricalcolano da sole come le
 * Auto Clamped di Blender: niente sforamenti sopra un massimo o sotto un
 * minimo.
 *
 * Una Action raccoglie le curve di un'animazione, gli eventi (Footstep,
 * PlaySound...) e i marker. Il tempo e' in frame, con fps della Action.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "engine/anim/Armature.h"

namespace blk {
namespace anim {

enum class Channel : int { LocX, LocY, LocZ, RotX, RotY, RotZ, ScaleX, ScaleY, ScaleZ, Count };

inline const char* channelName(Channel c) {
    static const char* n[] = {"Location X", "Location Y", "Location Z", "Rotation X", "Rotation Y",
                              "Rotation Z", "Scale X", "Scale Y", "Scale Z"};
    return (int)c >= 0 && (int)c < (int)Channel::Count ? n[(int)c] : "?";
}

inline const char* channelId(Channel c) {
    static const char* n[] = {"loc.x", "loc.y", "loc.z", "rot.x", "rot.y", "rot.z", "scale.x", "scale.y", "scale.z"};
    return (int)c >= 0 && (int)c < (int)Channel::Count ? n[(int)c] : "?";
}

inline bool isRotation(Channel c) { return c >= Channel::RotX && c <= Channel::RotZ; }
inline float channelDefault(Channel c) { return c >= Channel::ScaleX ? 1.f : 0.f; }

// Elastic / Bounce / Back = gli EasingStyle "Out" di Roblox (rimbalzo e
// sforamento all'arrivo). I numeri restano quelli salvati nei .blkanim.
enum class Interp : int { Constant, Linear, Bezier, Elastic, Bounce, Back };
constexpr int kInterpCount = 6;
enum class HandleType : int { Auto, Free, Vector };

inline const char* interpName(Interp i) {
    switch (i) {
    case Interp::Constant: return "Constant";
    case Interp::Linear: return "Linear";
    case Interp::Elastic: return "Elastic";
    case Interp::Bounce: return "Bounce";
    case Interp::Back: return "Back";
    default: return "Bezier";
    }
}

/** Curve di easing "Out" (0..1 -> 0..1; Elastic e Back sforano un po' l'arrivo). */
inline float easeOut(Interp i, float u) {
    if (u <= 0.f) return 0.f;
    if (u >= 1.f) return 1.f;
    switch (i) {
    case Interp::Elastic: {
        const float c4 = 2.f * 3.14159265f / 3.f;
        return std::pow(2.f, -10.f * u) * std::sin((u * 10.f - 0.75f) * c4) + 1.f;
    }
    case Interp::Bounce: {
        const float n1 = 7.5625f, d1 = 2.75f;
        if (u < 1.f / d1) return n1 * u * u;
        if (u < 2.f / d1) { u -= 1.5f / d1; return n1 * u * u + 0.75f; }
        if (u < 2.5f / d1) { u -= 2.25f / d1; return n1 * u * u + 0.9375f; }
        u -= 2.625f / d1;
        return n1 * u * u + 0.984375f;
    }
    case Interp::Back: {
        const float c1 = 1.70158f, c3 = c1 + 1.f, v = u - 1.f;
        return 1.f + c3 * v * v * v + c1 * v * v;
    }
    default: return u;
    }
}

struct Keyframe {
    float frame = 0.f;
    float value = 0.f;
    Interp interp = Interp::Bezier;    // verso la chiave dopo
    HandleType handles = HandleType::Auto;
    glm::vec2 hIn{0.f}, hOut{0.f};     // maniglie assolute (frame, valore)
    bool selected = false;
};

struct FCurve {
    std::string bone;
    Channel channel = Channel::LocX;
    std::string group;   // cartella nel dope sheet (di solito il nome dell'osso)
    std::vector<Keyframe> keys;
    bool mute = false;   // non anima
    bool locked = false; // non si modifica dall'editor

    void sort() {
        std::stable_sort(keys.begin(), keys.end(), [](const Keyframe& a, const Keyframe& b) { return a.frame < b.frame; });
    }

    int findKey(float frame, float eps = 0.01f) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (std::fabs(keys[i].frame - frame) <= eps) return (int)i;
        return -1;
    }

    /**
     * Inserisce o sostituisce la chiave a quel frame. Le rotazioni si
     * srotolano verso la chiave precedente (niente salti di 360 gradi).
     */
    int insert(float frame, float value, Interp interp = Interp::Bezier) {
        if (isRotation(channel)) {
            const Keyframe* prev = nullptr;
            for (const auto& k : keys)
                if (k.frame < frame - 0.01f) prev = &k;
            if (prev) {
                while (value - prev->value > 180.f) value -= 360.f;
                while (value - prev->value < -180.f) value += 360.f;
            }
        }
        const int existing = findKey(frame);
        if (existing >= 0) {
            keys[(size_t)existing].value = value;
            recalcHandles();
            return existing;
        }
        Keyframe k;
        k.frame = frame;
        k.value = value;
        k.interp = interp;
        keys.push_back(k);
        sort();
        recalcHandles();
        return findKey(frame);
    }

    /**
     * Maniglie Auto Clamped: pendenza dai vicini, lunghe un terzo del tratto;
     * su un massimo/minimo (o agli estremi) piatte, cosi' la curva non sfora.
     */
    void recalcHandles() {
        const size_t n = keys.size();
        for (size_t i = 0; i < n; ++i) {
            Keyframe& k = keys[i];
            const float dPrev = i > 0 ? k.frame - keys[i - 1].frame : 0.f;
            const float dNext = i + 1 < n ? keys[i + 1].frame - k.frame : 0.f;
            if (k.handles == HandleType::Free) continue;
            if (k.handles == HandleType::Vector) {
                k.hIn = i > 0 ? glm::mix(glm::vec2(k.frame, k.value), glm::vec2(keys[i - 1].frame, keys[i - 1].value), 1.f / 3.f)
                              : glm::vec2(k.frame - 1.f, k.value);
                k.hOut = i + 1 < n ? glm::mix(glm::vec2(k.frame, k.value), glm::vec2(keys[i + 1].frame, keys[i + 1].value), 1.f / 3.f)
                                   : glm::vec2(k.frame + 1.f, k.value);
                continue;
            }
            float slope = 0.f;
            if (i > 0 && i + 1 < n) {
                const float a = keys[i - 1].value, b = keys[i + 1].value;
                const bool extremum = (k.value >= a && k.value >= b) || (k.value <= a && k.value <= b);
                if (!extremum) slope = (b - a) / std::max(1e-4f, keys[i + 1].frame - keys[i - 1].frame);
            }
            const float lin = dPrev > 0.f ? dPrev / 3.f : 1.f;
            const float lout = dNext > 0.f ? dNext / 3.f : 1.f;
            k.hIn = {k.frame - lin, k.value - slope * lin};
            k.hOut = {k.frame + lout, k.value + slope * lout};
        }
    }

    float eval(float frame) const {
        if (keys.empty()) return channelDefault(channel);
        if (frame <= keys.front().frame) return keys.front().value;
        if (frame >= keys.back().frame) return keys.back().value;
        size_t i = 1;
        while (i < keys.size() && keys[i].frame < frame) ++i;
        const Keyframe& a = keys[i - 1];
        const Keyframe& b = keys[i];
        const float span = b.frame - a.frame;
        if (span <= 1e-5f) return b.value;
        if (a.interp == Interp::Constant) return a.value;
        const float u = (frame - a.frame) / span;
        if (a.interp == Interp::Linear) return a.value + (b.value - a.value) * u;
        if (a.interp == Interp::Bezier) return evalBezier(a, b, frame);
        return a.value + (b.value - a.value) * easeOut(a.interp, u);
    }

    /**
     * Bezier cubica (frame, valore). Le maniglie si tengono dentro il tratto
     * in orizzontale, cosi' il frame cresce sempre lungo la curva e a ogni
     * frame corrisponde un solo valore; poi si cerca il parametro per bisezione.
     */
    static float evalBezier(const Keyframe& a, const Keyframe& b, float frame) {
        const glm::vec2 p0(a.frame, a.value), p3(b.frame, b.value);
        glm::vec2 p1 = a.hOut, p2 = b.hIn;
        p1.x = glm::clamp(p1.x, p0.x, p3.x);
        p2.x = glm::clamp(p2.x, p0.x, p3.x);
        auto at = [&](float s) {
            const float m = 1.f - s;
            return m * m * m * p0 + 3.f * m * m * s * p1 + 3.f * m * s * s * p2 + s * s * s * p3;
        };
        float lo = 0.f, hi = 1.f;
        for (int it = 0; it < 40; ++it) {
            const float mid = 0.5f * (lo + hi);
            if (at(mid).x < frame) lo = mid; else hi = mid;
        }
        return at(0.5f * (lo + hi)).y;
    }
};

struct AnimEvent {
    float frame = 0.f;
    std::string name;   // Footstep, Jump, Land, Attack, PlaySound, SpawnParticle, ...
    std::string param;  // es. id del suono
};

struct Marker {
    float frame = 0.f;
    std::string name;
};

/** Riferimento a una chiave nell'Action, per le selezioni del dope sheet. */
struct KeyRef {
    int curve = -1;
    int key = -1;
    bool operator==(const KeyRef& o) const { return curve == o.curve && key == o.key; }
};

struct Action {
    std::string name = "Action";
    float fps = 30.f;
    float frameStart = 0.f;
    float frameEnd = 60.f;
    bool loop = true;
    float priority = 0.f; // Core/Idle/Movement/Action di Roblox, come numero
    std::vector<FCurve> curves;
    std::vector<AnimEvent> events;
    std::vector<Marker> markers;

    int findCurve(const std::string& bone, Channel ch) const {
        for (size_t i = 0; i < curves.size(); ++i)
            if (curves[i].bone == bone && curves[i].channel == ch) return (int)i;
        return -1;
    }

    FCurve& curve(const std::string& bone, Channel ch) {
        const int i = findCurve(bone, ch);
        if (i >= 0) return curves[(size_t)i];
        FCurve c;
        c.bone = bone;
        c.channel = ch;
        c.group = bone;
        curves.push_back(std::move(c));
        return curves.back();
    }

    int keyCount() const {
        int n = 0;
        for (const auto& c : curves) n += (int)c.keys.size();
        return n;
    }

    float durationSeconds() const { return std::max(0.f, frameEnd - frameStart) / std::max(1.f, fps); }

    /** Frame della Action a `seconds` dall'inizio, ripetendo se loop. */
    float frameAt(float seconds) const {
        const float len = frameEnd - frameStart;
        float f = seconds * fps;
        if (len <= 0.f) return frameStart;
        if (loop) {
            f = std::fmod(f, len);
            if (f < 0.f) f += len;
        } else {
            f = glm::clamp(f, 0.f, len);
        }
        return frameStart + f;
    }

    /** Valore dei 9 canali di un osso (canali senza curva = valore neutro). */
    static void channelsOf(const Transform& t, float out[9]) {
        const glm::vec3 e = glm::degrees(glm::eulerAngles(t.rot));
        out[0] = t.pos.x; out[1] = t.pos.y; out[2] = t.pos.z;
        out[3] = e.x; out[4] = e.y; out[5] = e.z;
        out[6] = t.scale.x; out[7] = t.scale.y; out[8] = t.scale.z;
    }

    static Transform fromChannels(const float v[9]) {
        Transform t;
        t.pos = {v[0], v[1], v[2]};
        t.rot = glm::quat(glm::radians(glm::vec3(v[3], v[4], v[5])));
        t.scale = {v[6], v[7], v[8]};
        return t;
    }

    /** Posa dello scheletro a quel frame. Le ossa senza curve restano a riposo. */
    Pose sample(const Armature& arm, float frame) const {
        Pose p;
        p.reset(arm.bones.size());
        std::vector<float> vals(arm.bones.size() * 9);
        std::vector<char> touched(arm.bones.size(), 0);
        for (size_t b = 0; b < arm.bones.size(); ++b)
            for (int c = 0; c < 9; ++c) vals[b * 9 + (size_t)c] = channelDefault((Channel)c);
        for (const auto& c : curves) {
            if (c.mute || c.keys.empty()) continue;
            const int b = arm.find(c.bone);
            if (b < 0) continue;
            vals[(size_t)b * 9 + (size_t)c.channel] = c.eval(frame);
            touched[(size_t)b] = 1;
        }
        for (size_t b = 0; b < arm.bones.size(); ++b)
            if (touched[b]) p.local[b] = fromChannels(&vals[b * 9]);
        return p;
    }

    enum KeyMask { KeyLoc = 1, KeyRot = 2, KeyScale = 4, KeyAll = 7 };

    /** Tasto I: chiavi della posa attuale di un osso a quel frame. */
    void insertKeys(const std::string& bone, const Transform& pose, float frame, int mask = KeyAll,
                    Interp interp = Interp::Bezier) {
        float v[9];
        channelsOf(pose, v);
        for (int c = 0; c < 9; ++c) {
            const bool on = (c < 3 && (mask & KeyLoc)) || (c >= 3 && c < 6 && (mask & KeyRot)) || (c >= 6 && (mask & KeyScale));
            if (!on) continue;
            FCurve& fc = curve(bone, (Channel)c);
            if (fc.locked) continue;
            fc.insert(frame, v[c], interp);
        }
    }

    // ---- Operazioni del dope sheet su una selezione di chiavi ----

    std::vector<KeyRef> selected() const {
        std::vector<KeyRef> out;
        for (size_t c = 0; c < curves.size(); ++c)
            for (size_t k = 0; k < curves[c].keys.size(); ++k)
                if (curves[c].keys[k].selected) out.push_back({(int)c, (int)k});
        return out;
    }

    void selectAll(bool on) {
        for (auto& c : curves)
            for (auto& k : c.keys) k.selected = on;
    }

    /** Seleziona le chiavi in un rettangolo di frame (box select del dope sheet). */
    void selectFrames(float from, float to, const std::vector<int>& curveIdx, bool extend) {
        if (!extend) selectAll(false);
        if (from > to) std::swap(from, to);
        for (int ci : curveIdx) {
            if (ci < 0 || ci >= (int)curves.size()) continue;
            for (auto& k : curves[(size_t)ci].keys)
                if (k.frame >= from && k.frame <= to) k.selected = true;
        }
    }

    int deleteSelected() {
        int n = 0;
        for (auto& c : curves) {
            if (c.locked) continue;
            const size_t before = c.keys.size();
            c.keys.erase(std::remove_if(c.keys.begin(), c.keys.end(), [](const Keyframe& k) { return k.selected; }), c.keys.end());
            n += (int)(before - c.keys.size());
            c.recalcHandles();
        }
        curves.erase(std::remove_if(curves.begin(), curves.end(), [](const FCurve& c) { return c.keys.empty(); }), curves.end());
        return n;
    }

    /** Sposta le chiavi selezionate di df frame (G nel dope sheet). */
    void moveSelected(float df) {
        for (auto& c : curves) {
            if (c.locked) continue;
            bool any = false;
            for (auto& k : c.keys)
                if (k.selected) {
                    k.frame += df;
                    k.hIn.x += df;
                    k.hOut.x += df;
                    any = true;
                }
            if (any) { c.sort(); mergeDuplicates(c); c.recalcHandles(); }
        }
    }

    /** Scala nel tempo attorno a `pivot` (S nel dope sheet). */
    void scaleSelected(float factor, float pivot) {
        if (factor <= 0.f) return;
        for (auto& c : curves) {
            if (c.locked) continue;
            bool any = false;
            for (auto& k : c.keys)
                if (k.selected) {
                    k.frame = pivot + (k.frame - pivot) * factor;
                    any = true;
                }
            if (any) { c.sort(); mergeDuplicates(c); c.recalcHandles(); }
        }
    }

    /** Duplica le chiavi selezionate spostate di df; la selezione passa alle copie. */
    int duplicateSelected(float df) {
        int n = 0;
        for (auto& c : curves) {
            if (c.locked) continue;
            std::vector<Keyframe> copies;
            for (auto& k : c.keys)
                if (k.selected) {
                    Keyframe d = k;
                    d.frame += df;
                    d.hIn.x += df;
                    d.hOut.x += df;
                    copies.push_back(d);
                    k.selected = false;
                }
            for (auto& d : copies) c.keys.push_back(d);
            n += (int)copies.size();
            if (!copies.empty()) { c.sort(); mergeDuplicates(c); c.recalcHandles(); }
        }
        return n;
    }

    void setInterpolationSelected(Interp i) {
        for (auto& c : curves)
            for (auto& k : c.keys)
                if (k.selected && !c.locked) k.interp = i;
    }

    /**
     * Ease In-Out (Cubic di Roblox): Bezier con maniglie piatte, cosi' il
     * movimento parte e arriva piano. `autoHandles` = torna alle maniglie Auto.
     */
    void setEaseSelected(bool autoHandles) {
        for (auto& c : curves) {
            if (c.locked) continue;
            bool any = false;
            for (auto& k : c.keys)
                if (k.selected) {
                    k.interp = Interp::Bezier;
                    k.handles = autoHandles ? HandleType::Auto : HandleType::Free;
                    any = true;
                }
            if (!any) continue;
            c.recalcHandles();   // lunghezze giuste (un terzo del tratto)
            if (!autoHandles)
                for (auto& k : c.keys)
                    if (k.selected) { k.hIn.y = k.value; k.hOut.y = k.value; }
        }
    }

    /** Inverte nel tempo le chiavi selezionate (dentro il loro intervallo): l'animazione va al contrario. */
    void reverseSelected() {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& c : curves)
            for (const auto& k : c.keys)
                if (k.selected) { lo = std::min(lo, k.frame); hi = std::max(hi, k.frame); }
        if (lo >= hi) return;
        for (auto& c : curves) {
            if (c.locked) continue;
            bool any = false;
            for (auto& k : c.keys)
                if (k.selected) {
                    k.frame = lo + hi - k.frame;
                    if (k.handles == HandleType::Free) {
                        const glm::vec2 in = k.hIn, out = k.hOut;
                        k.hIn = {lo + hi - out.x, out.y};
                        k.hOut = {lo + hi - in.x, in.y};
                    }
                    any = true;
                }
            if (any) { c.sort(); c.recalcHandles(); }
        }
    }

    /** Seleziona tutte le chiavi a quel frame (alla testina). */
    void selectAtFrame(float frame, bool extend) {
        if (!extend) selectAll(false);
        for (auto& c : curves) {
            const int k = c.findKey(frame);
            if (k >= 0) c.keys[(size_t)k].selected = true;
        }
    }

    // ---- Copia / incolla di chiavi (Ctrl+C / Ctrl+V nel dope sheet) ----
    struct KeyClip {
        std::string bone;
        Channel channel = Channel::LocX;
        Keyframe key;   // frame relativo alla prima chiave copiata
    };

    std::vector<KeyClip> copySelected() const {
        std::vector<KeyClip> out;
        float first = 1e9f;
        for (const auto& c : curves)
            for (const auto& k : c.keys)
                if (k.selected) first = std::min(first, k.frame);
        for (const auto& c : curves)
            for (const auto& k : c.keys)
                if (k.selected) {
                    KeyClip kc{c.bone, c.channel, k};
                    kc.key.frame -= first;
                    kc.key.hIn.x -= first;
                    kc.key.hOut.x -= first;
                    kc.key.selected = false;
                    out.push_back(kc);
                }
        return out;
    }

    /** Incolla con la prima chiave al frame `at`; le chiavi incollate restano selezionate. */
    int paste(const std::vector<KeyClip>& clip, float at) {
        selectAll(false);
        int n = 0;
        for (const auto& kc : clip) {
            FCurve& c = curve(kc.bone, kc.channel);
            if (c.locked) continue;
            const int k = c.insert(at + kc.key.frame, kc.key.value, kc.key.interp);
            if (k < 0) continue;
            Keyframe& dst = c.keys[(size_t)k];
            dst.interp = kc.key.interp;
            dst.handles = kc.key.handles;
            if (dst.handles == HandleType::Free) {
                dst.hIn = kc.key.hIn + glm::vec2(at, dst.value - kc.key.value);
                dst.hOut = kc.key.hOut + glm::vec2(at, dst.value - kc.key.value);
            }
            dst.selected = true;
            c.recalcHandles();
            ++n;
        }
        return n;
    }

    /** Frame (ordinati) in cui c'e' almeno una chiave: i rombi del dope sheet. */
    std::vector<float> keyedFrames(const std::string& boneFilter = "") const {
        std::vector<float> f;
        for (const auto& c : curves) {
            if (!boneFilter.empty() && c.bone != boneFilter) continue;
            for (const auto& k : c.keys) f.push_back(k.frame);
        }
        std::sort(f.begin(), f.end());
        f.erase(std::unique(f.begin(), f.end(), [](float a, float b) { return std::fabs(a - b) < 0.01f; }), f.end());
        return f;
    }

    /** Eventi attraversati andando da f0 (escluso) a f1 (incluso), anche passando il loop. */
    std::vector<const AnimEvent*> eventsBetween(float f0, float f1, bool wrapped) const {
        std::vector<const AnimEvent*> out;
        for (const auto& e : events) {
            const bool hit = wrapped ? (e.frame > f0 || e.frame <= f1) : (e.frame > f0 && e.frame <= f1);
            if (hit) out.push_back(&e);
        }
        return out;
    }

private:
    /** Due chiavi finite sullo stesso frame: vince l'ultima selezionata (quella spostata). */
    static void mergeDuplicates(FCurve& c) {
        for (size_t i = 1; i < c.keys.size();) {
            if (std::fabs(c.keys[i].frame - c.keys[i - 1].frame) < 0.01f) {
                const size_t drop = c.keys[i].selected && !c.keys[i - 1].selected ? i - 1 : i;
                c.keys.erase(c.keys.begin() + (long)drop);
            } else {
                ++i;
            }
        }
    }
};

} // namespace anim
} // namespace blk
