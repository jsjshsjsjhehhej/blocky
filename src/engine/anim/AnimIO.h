#pragma once

/**
 * Salvataggio (formato "Blocky Animation", JSON) e strumenti sulle pose:
 * libreria pose, copia/incolla, specchio sinistra/destra.
 *
 * File .blkanim:
 *   { "format": "blocky-anim", "version": 1,
 *     "armature": { "name", "bones": [ {name, parent, pos, rot[w,x,y,z], scale, length, deform, collection} ] },
 *     "actions":  [ {name, fps, start, end, loop, priority,
 *                    curves: [ {bone, channel, group, mute, locked,
 *                               keys: [[frame, value, interp, handles, inX, inY, outX, outY], ...]} ],
 *                    events: [ {frame, name, param} ], markers: [ {frame, name} ] } ],
 *     "poses":    [ {name, bones: { "Neck": [px,py,pz, w,x,y,z, sx,sy,sz] } } ] }
 *
 * Specchio: i nomi si scambiano (Left/Right, .L/.R, _L/_R) e la posa si
 * riflette sul piano YZ, come Mirror Pose di Blender. Funziona bene sui rig
 * simmetrici (come quelli dell'Auto Rig).
 */

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "engine/MiniJson.h"
#include "engine/anim/AnimCurve.h"
#include "engine/anim/BoneConstraints.h"

namespace blk {
namespace anim {

struct NamedPose {
    std::string name;
    std::map<std::string, Transform> bones; // posa per osso (per nome: vale anche su rig simili)
};

/** Posa attuale -> libreria. onlyBones vuoto = tutte le ossa. */
inline NamedPose capturePose(const Armature& arm, const Pose& pose, const std::string& name,
                             const std::vector<int>& onlyBones = {}) {
    NamedPose np;
    np.name = name;
    if (onlyBones.empty()) {
        for (size_t i = 0; i < arm.bones.size(); ++i) np.bones[arm.bones[i].name] = pose.get((int)i);
    } else {
        for (int i : onlyBones)
            if (arm.valid(i)) np.bones[arm.bones[(size_t)i].name] = pose.get(i);
    }
    return np;
}

/** Libreria -> posa, mescolando con `blendW` (1 = sostituisce). Ossa mancanti: invariate. */
inline void applyNamedPose(const Armature& arm, Pose& pose, const NamedPose& np, float blendW = 1.f) {
    if (pose.local.size() < arm.bones.size()) pose.local.resize(arm.bones.size());
    for (const auto& kv : np.bones) {
        const int i = arm.find(kv.first);
        if (i >= 0) pose.local[(size_t)i] = lerp(pose.local[(size_t)i], kv.second, glm::clamp(blendW, 0.f, 1.f));
    }
}

// ---- Specchio ----

inline std::string mirrorName(const std::string& n) {
    struct Pair { const char* a; const char* b; };
    static const Pair pairs[] = {{"Left", "Right"}, {"left", "right"}, {"LEFT", "RIGHT"}};
    for (const auto& p : pairs) {
        for (int dir = 0; dir < 2; ++dir) {
            const std::string from = dir ? p.b : p.a, to = dir ? p.a : p.b;
            const size_t at = n.find(from);
            if (at != std::string::npos) return n.substr(0, at) + to + n.substr(at + from.size());
        }
    }
    // Suffissi/prefissi di Blender: Arm.L, Arm_L, L_Arm
    auto swapSuffix = [&](const char* l, const char* r) -> std::string {
        const std::string sl = l, sr = r;
        if (n.size() > sl.size() && n.compare(n.size() - sl.size(), sl.size(), sl) == 0) return n.substr(0, n.size() - sl.size()) + sr;
        if (n.size() > sr.size() && n.compare(n.size() - sr.size(), sr.size(), sr) == 0) return n.substr(0, n.size() - sr.size()) + sl;
        return std::string();
    };
    for (const auto& s : {std::make_pair(".L", ".R"), std::make_pair("_L", "_R"), std::make_pair(".l", ".r")}) {
        const std::string m = swapSuffix(s.first, s.second);
        if (!m.empty()) return m;
    }
    if (n.rfind("L_", 0) == 0) return "R_" + n.substr(2);
    if (n.rfind("R_", 0) == 0) return "L_" + n.substr(2);
    return n;
}

/** Riflessione sul piano YZ di una posa locale: x della posizione e rotazioni Y/Z cambiano segno. */
inline Transform mirrorTransform(const Transform& t) {
    Transform m = t;
    m.pos.x = -t.pos.x;
    m.rot = glm::quat(t.rot.w, t.rot.x, -t.rot.y, -t.rot.z);
    return m;
}

/** Mirror Pose: ogni osso prende la posa specchiata del suo gemello. */
inline Pose mirrorPose(const Armature& arm, const Pose& pose) {
    Pose out;
    out.reset(arm.bones.size());
    for (size_t i = 0; i < arm.bones.size(); ++i) {
        const int twin = arm.find(mirrorName(arm.bones[i].name));
        const int src = twin >= 0 ? twin : (int)i;
        out.local[i] = mirrorTransform(pose.get(src));
    }
    return out;
}

/** Mirror Animation: curve dei gemelli scambiate, canali LocX/RotY/RotZ col segno opposto. */
inline Action mirrorAction(const Action& a) {
    Action m = a;
    m.name = a.name + " (specchio)";
    for (auto& c : m.curves) {
        c.bone = mirrorName(c.bone);
        c.group = mirrorName(c.group);
        const bool flip = c.channel == Channel::LocX || c.channel == Channel::RotY || c.channel == Channel::RotZ;
        if (!flip) continue;
        for (auto& k : c.keys) {
            k.value = -k.value;
            k.hIn.y = -k.hIn.y;
            k.hOut.y = -k.hOut.y;
        }
    }
    return m;
}

// ---- JSON ----

namespace io {

inline std::string num(float v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.6g", (double)v);
    return b;
}

inline std::string vec(const glm::vec3& v) { return "[" + num(v.x) + "," + num(v.y) + "," + num(v.z) + "]"; }
inline std::string quatJ(const glm::quat& q) { return "[" + num(q.w) + "," + num(q.x) + "," + num(q.y) + "," + num(q.z) + "]"; }

inline std::string transformArr(const Transform& t) {
    return "[" + num(t.pos.x) + "," + num(t.pos.y) + "," + num(t.pos.z) + "," + num(t.rot.w) + "," + num(t.rot.x) + ","
         + num(t.rot.y) + "," + num(t.rot.z) + "," + num(t.scale.x) + "," + num(t.scale.y) + "," + num(t.scale.z) + "]";
}

inline float f(const json::Value* v, float def = 0.f) { return v && v->type == json::Value::Type::Number ? (float)v->num : def; }
inline bool b(const json::Value* v, bool def = false) { return v && v->type == json::Value::Type::Bool ? v->b : def; }
inline std::string s(const json::Value* v) { return v && v->type == json::Value::Type::String ? v->str : std::string(); }

inline bool arrN(const json::Value* v, float* out, size_t n) {
    if (!v || v->type != json::Value::Type::Array || v->arr.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        if (v->arr[i].type != json::Value::Type::Number) return false;
        out[i] = (float)v->arr[i].num;
    }
    return true;
}

inline Channel channelFromId(const std::string& id, bool& ok) {
    for (int c = 0; c < (int)Channel::Count; ++c)
        if (id == channelId((Channel)c)) { ok = true; return (Channel)c; }
    ok = false;
    return Channel::LocX;
}

} // namespace io

inline std::string toJson(const Armature& arm, const std::vector<Action>& actions, const std::vector<NamedPose>& poses = {}) {
    using namespace io;
    std::ostringstream o;
    o << "{\"format\":\"blocky-anim\",\"version\":1,\"armature\":{\"name\":" << json::quote(arm.name) << ",\"bones\":[";
    for (size_t i = 0; i < arm.bones.size(); ++i) {
        const Bone& bn = arm.bones[i];
        o << (i ? "," : "") << "{\"name\":" << json::quote(bn.name) << ",\"parent\":" << bn.parent << ",\"pos\":" << vec(bn.rest.pos)
          << ",\"rot\":" << quatJ(bn.rest.rot) << ",\"scale\":" << vec(bn.rest.scale) << ",\"length\":" << num(bn.length)
          << ",\"deform\":" << (bn.deform ? "true" : "false") << ",\"collection\":" << json::quote(bn.collection) << "}";
    }
    o << "]},\"actions\":[";
    for (size_t ai = 0; ai < actions.size(); ++ai) {
        const Action& a = actions[ai];
        o << (ai ? "," : "") << "{\"name\":" << json::quote(a.name) << ",\"fps\":" << num(a.fps) << ",\"start\":" << num(a.frameStart)
          << ",\"end\":" << num(a.frameEnd) << ",\"loop\":" << (a.loop ? "true" : "false") << ",\"priority\":" << num(a.priority)
          << ",\"curves\":[";
        for (size_t ci = 0; ci < a.curves.size(); ++ci) {
            const FCurve& c = a.curves[ci];
            o << (ci ? "," : "") << "{\"bone\":" << json::quote(c.bone) << ",\"channel\":\"" << channelId(c.channel) << "\",\"group\":"
              << json::quote(c.group) << ",\"mute\":" << (c.mute ? "true" : "false") << ",\"locked\":" << (c.locked ? "true" : "false")
              << ",\"keys\":[";
            for (size_t k = 0; k < c.keys.size(); ++k) {
                const Keyframe& kf = c.keys[k];
                o << (k ? "," : "") << "[" << num(kf.frame) << "," << num(kf.value) << "," << (int)kf.interp << "," << (int)kf.handles << ","
                  << num(kf.hIn.x) << "," << num(kf.hIn.y) << "," << num(kf.hOut.x) << "," << num(kf.hOut.y) << "]";
            }
            o << "]}";
        }
        o << "],\"events\":[";
        for (size_t e = 0; e < a.events.size(); ++e)
            o << (e ? "," : "") << "{\"frame\":" << num(a.events[e].frame) << ",\"name\":" << json::quote(a.events[e].name)
              << ",\"param\":" << json::quote(a.events[e].param) << "}";
        o << "],\"markers\":[";
        for (size_t m = 0; m < a.markers.size(); ++m)
            o << (m ? "," : "") << "{\"frame\":" << num(a.markers[m].frame) << ",\"name\":" << json::quote(a.markers[m].name) << "}";
        o << "]}";
    }
    o << "],\"poses\":[";
    for (size_t p = 0; p < poses.size(); ++p) {
        o << (p ? "," : "") << "{\"name\":" << json::quote(poses[p].name) << ",\"bones\":{";
        bool first = true;
        for (const auto& kv : poses[p].bones) {
            o << (first ? "" : ",") << json::quote(kv.first) << ":" << transformArr(kv.second);
            first = false;
        }
        o << "}}";
    }
    o << "]}";
    return o.str();
}

/** Falso con il motivo in `error` se il file non e' un .blkanim valido. */
inline bool fromJson(const std::string& text, Armature& arm, std::vector<Action>& actions, std::vector<NamedPose>* poses,
                     std::string& error) {
    using namespace io;
    json::Value root;
    if (!json::parse(text, root) || !root.isObject()) { error = "JSON non valido"; return false; }
    if (s(root.get("format")) != "blocky-anim") { error = "non e' un file Blocky Animation"; return false; }
    if (f(root.get("version"), 0.f) > 1.f) { error = "versione del file piu' nuova di questo Blocky"; return false; }

    Armature a;
    const json::Value* ja = root.get("armature");
    if (!ja || !ja->isObject()) { error = "manca l'armature"; return false; }
    a.name = s(ja->get("name"));
    const json::Value* jb = ja->get("bones");
    if (jb && jb->type == json::Value::Type::Array) {
        for (const auto& b0 : jb->arr) {
            Bone bn;
            bn.name = s(b0.get("name"));
            bn.parent = (int)f(b0.get("parent"), -1.f);
            float p[3] = {0, 0, 0}, q[4] = {1, 0, 0, 0}, sc[3] = {1, 1, 1};
            arrN(b0.get("pos"), p, 3);
            arrN(b0.get("rot"), q, 4);
            arrN(b0.get("scale"), sc, 3);
            bn.rest.pos = {p[0], p[1], p[2]};
            bn.rest.rot = glm::normalize(glm::quat(q[0], q[1], q[2], q[3]));
            bn.rest.scale = {sc[0], sc[1], sc[2]};
            bn.length = std::max(0.01f, f(b0.get("length"), 1.f));
            bn.deform = b(b0.get("deform"), true);
            bn.collection = s(b0.get("collection"));
            if (bn.name.empty()) { error = "osso senza nome"; return false; }
            a.bones.push_back(std::move(bn));
        }
    }
    for (size_t i = 0; i < a.bones.size(); ++i) {
        const int p = a.bones[i].parent;
        if (p >= (int)a.bones.size() || p == (int)i) { error = "padre di un osso non valido"; return false; }
        if (p < -1) a.bones[i].parent = -1;
    }
    for (size_t i = 0; i < a.bones.size(); ++i)
        if (a.isAncestor((int)i, (int)i)) { error = "ossa in cerchio (ciclo di padri)"; return false; }

    std::vector<Action> acts;
    const json::Value* jacts = root.get("actions");
    if (jacts && jacts->type == json::Value::Type::Array) {
        for (const auto& j : jacts->arr) {
            Action act;
            act.name = s(j.get("name"));
            act.fps = std::max(1.f, f(j.get("fps"), 30.f));
            act.frameStart = f(j.get("start"), 0.f);
            act.frameEnd = f(j.get("end"), 60.f);
            act.loop = b(j.get("loop"), true);
            act.priority = f(j.get("priority"), 0.f);
            const json::Value* jc = j.get("curves");
            if (jc && jc->type == json::Value::Type::Array) {
                for (const auto& c0 : jc->arr) {
                    FCurve c;
                    bool ok = false;
                    c.bone = s(c0.get("bone"));
                    c.channel = channelFromId(s(c0.get("channel")), ok);
                    if (!ok) continue;
                    c.group = s(c0.get("group"));
                    c.mute = b(c0.get("mute"));
                    c.locked = b(c0.get("locked"));
                    const json::Value* jk = c0.get("keys");
                    if (jk && jk->type == json::Value::Type::Array) {
                        for (const auto& k0 : jk->arr) {
                            float v[8];
                            if (!arrN(&k0, v, 8)) continue;
                            Keyframe kf;
                            kf.frame = v[0];
                            kf.value = v[1];
                            kf.interp = (Interp)glm::clamp((int)v[2], 0, kInterpCount - 1);
                            kf.handles = (HandleType)glm::clamp((int)v[3], 0, 2);
                            kf.hIn = {v[4], v[5]};
                            kf.hOut = {v[6], v[7]};
                            c.keys.push_back(kf);
                        }
                    }
                    c.sort();
                    c.recalcHandles();
                    act.curves.push_back(std::move(c));
                }
            }
            const json::Value* je = j.get("events");
            if (je && je->type == json::Value::Type::Array)
                for (const auto& e : je->arr) act.events.push_back({f(e.get("frame")), s(e.get("name")), s(e.get("param"))});
            const json::Value* jm = j.get("markers");
            if (jm && jm->type == json::Value::Type::Array)
                for (const auto& m : jm->arr) act.markers.push_back({f(m.get("frame")), s(m.get("name"))});
            acts.push_back(std::move(act));
        }
    }
    if (poses) {
        poses->clear();
        const json::Value* jp = root.get("poses");
        if (jp && jp->type == json::Value::Type::Array) {
            for (const auto& p0 : jp->arr) {
                NamedPose np;
                np.name = s(p0.get("name"));
                const json::Value* bones = p0.get("bones");
                if (bones && bones->isObject()) {
                    for (const auto& kv : bones->obj) {
                        float v[10];
                        if (!arrN(&kv.second, v, 10)) continue;
                        Transform t;
                        t.pos = {v[0], v[1], v[2]};
                        t.rot = glm::normalize(glm::quat(v[3], v[4], v[5], v[6]));
                        t.scale = {v[7], v[8], v[9]};
                        np.bones[kv.first] = t;
                    }
                }
                poses->push_back(std::move(np));
            }
        }
    }
    arm = std::move(a);
    actions = std::move(acts);
    return true;
}

inline bool saveFile(const std::string& path, const Armature& arm, const std::vector<Action>& actions,
                     const std::vector<NamedPose>& poses = {}) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << toJson(arm, actions, poses);
        if (!f) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

inline bool loadFile(const std::string& path, Armature& arm, std::vector<Action>& actions, std::vector<NamedPose>* poses,
                     std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "file non trovato"; return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    return fromJson(ss.str(), arm, actions, poses, error);
}

} // namespace anim
} // namespace blk
