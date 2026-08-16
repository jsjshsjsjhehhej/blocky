#pragma once

/**
 * Player Animation R6 — un solo formato per walk / jump / idle / emote / tool.
 *
 * File: %APPDATA%/Blocky/anims/<id>.r6anim.json  (legacy: .walk.json)
 * Schema BlockyPlayerAnim v2:
 *   kind: walk|jump|idle|emote|tool
 *   timeMode: phase (t 0..1) | seconds (t in secondi, tipico jump)
 *   keys: [{t, la, ra, ll, rl}]  // rotX gradi
 *
 * Workflow Blender:
 *  1. Anima R6 (Left/Right Arm/Leg)
 *  2. Esporta GLB/FBX → Avatar Catalog → importa → assegna kind → Rilascia
 *  3. Script: game.Animator:SetWalkAnim("Id") / SetJumpAnim("Id") / Play("Id")
 */

#include "GameHub.h"
#include "Humanoid.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

struct R6WalkKey {
    float t = 0.f; // 0..1 sul ciclo (phase) OPPURE secondi se timeMode=seconds
    float leftArm = 0.f;
    float rightArm = 0.f;
    float leftLeg = 0.f;
    float rightLeg = 0.f;
};

/** kind: walk | jump | idle | emote | tool — un solo file per tutte le anim player. */
enum class R6AnimKind : int {
    Walk = 0,
    Jump,
    Idle,
    Emote,
    Tool
};

inline const char* r6AnimKindName(R6AnimKind k) {
    switch (k) {
    case R6AnimKind::Jump: return "jump";
    case R6AnimKind::Idle: return "idle";
    case R6AnimKind::Emote: return "emote";
    case R6AnimKind::Tool: return "tool";
    default: return "walk";
    }
}

inline R6AnimKind r6AnimKindFromName(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "jump" || s == "fall") return R6AnimKind::Jump;
    if (s == "idle") return R6AnimKind::Idle;
    if (s == "emote" || s == "wave" || s == "dance") return R6AnimKind::Emote;
    if (s == "tool" || s == "shoot" || s == "gun") return R6AnimKind::Tool;
    return R6AnimKind::Walk;
}

struct R6WalkClip {
    std::string id;            // slug file
    std::string clipName;      // es. WalkingAnimation (nome Blender)
    std::string displayName;   // nome rilasciato (scelto dall'utente)
    std::string sourcePath;    // FBX/GLB originale (solo path corto)
    float duration = 1.f;      // secondi
    bool looped = true;
    R6AnimKind kind = R6AnimKind::Walk;
    /** false = t in 0..1 (walk); true = t in secondi (jump). */
    bool timeInSeconds = false;
    std::vector<R6WalkKey> keys;
    bool valid() const { return keys.size() >= 2; }
};

inline std::string r6AnimsDir() {
    std::string d = blockyDataDir() + "/anims";
#ifdef _WIN32
    _mkdir(d.c_str());
#else
    mkdir(d.c_str(), 0755);
#endif
    return d;
}

inline std::string r6WalkLibraryPath() {
    return blockyDataDir() + "/walk_anims.json";
}

inline std::string r6NormName(std::string s) {
    std::string o;
    for (unsigned char c : s) {
        if (c >= 'A' && c <= 'Z') o.push_back((char)(c - 'A' + 'a'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o.push_back((char)c);
    }
    return o;
}

inline int r6LimbFromBoneName(const std::string& name) {
    std::string n = r6NormName(name);
    // 0=LA 1=RA 2=LL 3=RL  -1=no
    // Accetta: Left Arm, Left_Arm, LeftArm, mixamorigRightArm, UpperArmL, ArmL, ...
    auto has = [&](const char* a) { return n.find(a) != std::string::npos; };

    // Ordine: match espliciti left/right prima
    if (has("leftarm") || has("leftupperarm") || has("upperarml") || has("arml")
        || has("shoulderl") || n == "la")
        return 0;
    if (has("rightarm") || has("rightupperarm") || has("upperarmr") || has("armr")
        || has("shoulderr") || n == "ra")
        return 1;
    if (has("leftleg") || has("leftupperleg") || has("leftthigh") || has("upperlegl")
        || has("legl") || has("thighl") || n == "ll")
        return 2;
    if (has("rightleg") || has("rightupperleg") || has("rightthigh") || has("upperlegr")
        || has("legr") || has("thighr") || n == "rl")
        return 3;

    // Fallback Blender: "Arm.R" → armt? no → "armr" già sopra. "HandR" no.
    // Solo "arm" + suffisso L/R senza left/right nel nome
    if (has("arm") && !has("leg") && !has("farm")) {
        if (!has("right") && (n.size() >= 4 && (n.back() == 'l' || has("left"))))
            return 0;
        if (!has("left") && (n.size() >= 4 && (n.back() == 'r' || has("right"))))
            return 1;
    }
    return -1;
}

/** Quaternion → rotazione X in gradi (swing arti R6). */
inline float r6QuatToRotXDeg(const aiQuaternion& q) {
    glm::quat gq(q.w, q.x, q.y, q.z);
    // Euler XYZ: interessa pitch (X) per swing avanti/indietro
    glm::vec3 e = glm::eulerAngles(gq);
    float deg = glm::degrees(e.x);
    // Clamp soft
    if (deg > 180.f) deg -= 360.f;
    if (deg < -180.f) deg += 360.f;
    return deg;
}

inline float r6Lerp(float a, float b, float t) { return a + (b - a) * t; }

/**
 * Sample clip.
 * - timeInSeconds=false: phase01 in 0..1 (walk loop)
 * - timeInSeconds=true:  timeSec in secondi (jump timeline)
 */
inline R6WalkKey r6SampleClip(const R6WalkClip& clip, float phaseOrSec) {
    R6WalkKey out{};
    if (!clip.valid()) return out;
    const auto& keys = clip.keys;
    float t = phaseOrSec;
    if (!clip.timeInSeconds) {
        t = phaseOrSec - std::floor(phaseOrSec);
        if (t < 0.f) t += 1.f;
    } else if (clip.looped && clip.duration > 1e-4f) {
        t = std::fmod(std::max(0.f, phaseOrSec), clip.duration);
        if (t < 0.f) t += clip.duration;
    } else {
        t = std::max(0.f, phaseOrSec);
    }
    if (t <= keys.front().t) return keys.front();
    if (t >= keys.back().t) return keys.back();
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].t && t <= keys[i + 1].t) {
            float span = keys[i + 1].t - keys[i].t;
            float u = span > 1e-6f ? (t - keys[i].t) / span : 0.f;
            if (clip.timeInSeconds)
                u = u * u * (3.f - 2.f * u); // smoothstep jump
            out.t = t;
            out.leftArm = r6Lerp(keys[i].leftArm, keys[i + 1].leftArm, u);
            out.rightArm = r6Lerp(keys[i].rightArm, keys[i + 1].rightArm, u);
            out.leftLeg = r6Lerp(keys[i].leftLeg, keys[i + 1].leftLeg, u);
            out.rightLeg = r6Lerp(keys[i].rightLeg, keys[i + 1].rightLeg, u);
            return out;
        }
    }
    return keys.back();
}

inline void r6ApplyKeyToHumanoid(Humanoid& h, const R6WalkKey& k, float w = 1.f) {
    w = std::clamp(w, 0.f, 1.f);
    for (auto& p : h.parts) {
        if (p.name == "Left Leg") p.rotX = k.leftLeg * w;
        else if (p.name == "Right Leg") p.rotX = k.rightLeg * w;
        else if (p.name == "Left Arm") { p.rotX = k.leftArm * w; p.rotZ = 0.f; }
        else if (p.name == "Right Arm") { p.rotX = k.rightArm * w; p.rotZ = 0.f; }
    }
}

/** Pack R6 — sine fluido (meno robotico), leggero punch agli estremi. */
inline float r6PackWalkSharp(float sinVal) {
    float a = std::fabs(sinVal);
    return std::copysign(std::pow(a, 0.94f), sinVal);
}

/** Clip walk R6 — gambe ±48°, braccia ±68°, ciclo fluido ~0.56s. */
inline R6WalkClip r6MakeClassicWalkClip() {
    R6WalkClip c;
    c.id = "classic_walk";
    c.clipName = "R6PackWalk";
    c.displayName = "R6 Walk";
    c.kind = R6AnimKind::Walk;
    c.timeInSeconds = false;
    c.duration = 0.56f;
    c.looped = true;
    for (int i = 0; i <= 24; ++i) {
        float t = i / 24.f;
        float ph = t * 6.2831853f;
        float leg = r6PackWalkSharp(std::sin(ph)) * 48.f;
        // braccia un filo in ritardo = meno meccanico
        float arm = r6PackWalkSharp(std::sin(ph + 0.18f)) * 68.f;
        R6WalkKey k;
        k.t = t;
        k.leftLeg = leg;
        k.rightLeg = -leg * 0.97f;
        k.leftArm = -arm;
        k.rightArm = arm * 1.03f;
        c.keys.push_back(k);
    }
    return c;
}

/**
 * Jump pack R6 (video + anim ufficiale): braccia SU + gambe a split in aria.
 */
inline const float* r6PackJumpKeys(int& outCount) {
    // t, LA, RA, LL, RL  (rotX°; negativo = braccio su)
    // Gambe quasi ferme (stile R6 classico: solo braccia su)
    static const float k[][5] = {
        { 0.00f,   -8.f,  -12.f,    6.f,   -4.f }, // crouch leggero
        { 0.05f,  -55.f,  -70.f,    4.f,   -2.f }, // launch
        { 0.11f, -125.f, -145.f,    2.f,   -2.f }, // rise
        { 0.18f, -168.f, -178.f,    0.f,    0.f }, // quasi peak
        { 0.26f, -180.f, -180.f,    0.f,    0.f }, // peak braccia dritte
        { 0.36f, -165.f, -160.f,    0.f,    0.f }, // hold
        { 0.48f, -105.f,  -98.f,    2.f,   -2.f }, // fall start
        { 0.60f,  -52.f,  -45.f,    2.f,   -2.f }, // fall
        { 0.75f,  -22.f,  -18.f,    2.f,   -2.f }, // pre-land
        { 0.92f,   -8.f,   -8.f,    0.f,    0.f },
    };
    outCount = 10;
    return &k[0][0];
}

inline R6WalkClip r6MakeClassicJumpClip() {
    R6WalkClip c;
    c.id = "classic_jump";
    c.clipName = "R6PackJump";
    c.displayName = "R6 Jump";
    c.kind = R6AnimKind::Jump;
    c.timeInSeconds = true;
    c.looped = false;
    c.duration = 0.92f;
    int n = 0;
    const float* flat = r6PackJumpKeys(n);
    for (int i = 0; i < n; ++i) {
        const float* row = flat + i * 5;
        R6WalkKey key;
        key.t = row[0];
        key.leftArm = row[1];
        key.rightArm = row[2];
        key.leftLeg = row[3];
        key.rightLeg = row[4];
        c.keys.push_back(key);
    }
    return c;
}

/**
 * Importa da FBX/GLB/GLTF (Blender / Maya / ecc.).
 * preferredClip: nome Action/clip (es. WalkingAnimation, Shoot, FancyWalk).
 * Se vuoto: preferisce WalkingAnimation, poi "walk", poi la prima.
 * Ritorna anche la lista nomi clip trovate in outClipNames (opzionale).
 */
inline bool r6ImportWalkFromFile(const std::string& path, R6WalkClip& out, std::string& err,
                                 const std::string& preferredClip = {},
                                 std::vector<std::string>* outClipNames = nullptr) {
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights);
    if (!scene || !scene->mRootNode) {
        err = imp.GetErrorString() ? imp.GetErrorString() : "file non valido";
        return false;
    }
    if (scene->mNumAnimations == 0) {
        err = "Nessuna animazione nel file. In Blender/Maya: crea Action/clip ed esporta con animazioni.";
        return false;
    }

    std::string preferLow = preferredClip;
    for (char& c : preferLow)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');

    int chosen = -1;
    int bestScore = -1;
    std::string chosenName;
    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* an = scene->mAnimations[a];
        std::string nm = an->mName.C_Str();
        if (outClipNames) outClipNames->push_back(nm.empty() ? ("Clip" + std::to_string(a)) : nm);
        std::string low = nm;
        for (char& c : low)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        int score = (int)an->mNumChannels;
        if (!preferLow.empty() && (low == preferLow || low.find(preferLow) != std::string::npos))
            score += 50000;
        if (low == "walkinganimation" || low.find("walkinganimation") != std::string::npos)
            score += 10000;
        else if (low.find("walk") != std::string::npos)
            score += 1000;
        else if (low.find("shoot") != std::string::npos || low.find("pistol") != std::string::npos
                 || low.find("fire") != std::string::npos || low.find("gun") != std::string::npos)
            score += 800;
        if (score > bestScore) {
            bestScore = score;
            chosen = (int)a;
            chosenName = nm.empty() ? "WalkingAnimation" : nm;
        }
    }
    if (chosen < 0) {
        err = "Nessuna clip utilizzabile";
        return false;
    }

    const aiAnimation* anim = scene->mAnimations[chosen];
    float tps = anim->mTicksPerSecond != 0.0 ? (float)anim->mTicksPerSecond : 25.f;
    float durTicks = (float)anim->mDuration;
    float durSec = std::max(0.1f, durTicks / tps);

    // Canali arti
    const aiNodeAnim* ch[4] = { nullptr, nullptr, nullptr, nullptr };
    std::vector<std::string> channelNames;
    channelNames.reserve(anim->mNumChannels);
    for (unsigned c = 0; c < anim->mNumChannels; ++c) {
        const char* nm = anim->mChannels[c]->mNodeName.C_Str();
        channelNames.push_back(nm ? nm : "?");
        int limb = r6LimbFromBoneName(nm ? nm : "");
        if (limb >= 0 && !ch[limb])
            ch[limb] = anim->mChannels[c];
    }
    int mapped = (ch[0] ? 1 : 0) + (ch[1] ? 1 : 0) + (ch[2] ? 1 : 0) + (ch[3] ? 1 : 0);

    // Pistola / shoot: spesso anima solo 1 braccio — ok
    std::string clipLow = chosenName;
    for (char& c : clipLow)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    const bool shootLike = clipLow.find("shoot") != std::string::npos
        || clipLow.find("pistol") != std::string::npos
        || clipLow.find("pistola") != std::string::npos
        || clipLow.find("gun") != std::string::npos
        || clipLow.find("fire") != std::string::npos
        || clipLow.find("aim") != std::string::npos
        || clipLow.find("reload") != std::string::npos;

    // Se zero arti ma c'è 1 canale con rotazioni → assegna a Right Arm (tipico sparo)
    if (mapped == 0 && anim->mNumChannels >= 1) {
        const aiNodeAnim* best = nullptr;
        unsigned bestKeys = 0;
        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* na = anim->mChannels[c];
            if (na && na->mNumRotationKeys > bestKeys) {
                bestKeys = na->mNumRotationKeys;
                best = na;
            }
        }
        if (best && bestKeys > 0) {
            ch[1] = best; // Right Arm
            mapped = 1;
        }
    }

    if (mapped < 1) {
        std::string list;
        for (size_t i = 0; i < channelNames.size() && i < 12; ++i) {
            if (i) list += ", ";
            list += channelNames[i];
        }
        if (channelNames.size() > 12) list += ", ...";
        err = "Clip \"" + chosenName + "\": nessun arto R6 trovato. "
              "In Blender rinomina gli object/bone in: Right Arm (per pistola), "
              "Left Arm, Left Leg, Right Leg. Canali nel file: " + list;
        return false;
    }
    // Walk meglio con >=2, ma 1 arto (pistola) e' accettato
    (void)shootLike;

    auto sampleLimb = [](const aiNodeAnim* node, float tick) -> float {
        if (!node || node->mNumRotationKeys == 0) return 0.f;
        if (node->mNumRotationKeys == 1)
            return r6QuatToRotXDeg(node->mRotationKeys[0].mValue);
        // Trova chiave
        unsigned i = 0;
        while (i + 1 < node->mNumRotationKeys
               && (float)node->mRotationKeys[i + 1].mTime < tick)
            ++i;
        if (i + 1 >= node->mNumRotationKeys)
            return r6QuatToRotXDeg(node->mRotationKeys[node->mNumRotationKeys - 1].mValue);
        float t0 = (float)node->mRotationKeys[i].mTime;
        float t1 = (float)node->mRotationKeys[i + 1].mTime;
        float u = (t1 > t0) ? (tick - t0) / (t1 - t0) : 0.f;
        aiQuaternion q;
        aiQuaternion::Interpolate(q, node->mRotationKeys[i].mValue,
                                  node->mRotationKeys[i + 1].mValue, u);
        q.Normalize();
        return r6QuatToRotXDeg(q);
    };

    out = R6WalkClip{};
    out.clipName = chosenName;
    out.displayName = chosenName;
    out.sourcePath = path;
    out.duration = durSec;
    out.looped = !shootLike; // walk/run loop; shoot/pistol one-shot
    if (mapped == 1 && !shootLike) {
        // Un solo arto senza nome shoot → comunque one-shot (es. wave/fire)
        std::string idHint = path;
        for (char& c : idHint)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (idHint.find("pistol") != std::string::npos
            || idHint.find("pistola") != std::string::npos
            || idHint.find("gun") != std::string::npos
            || idHint.find("shoot") != std::string::npos)
            out.looped = false;
    }

    const int samples = 16;
    for (int s = 0; s <= samples; ++s) {
        float u = s / (float)samples;
        float tick = u * durTicks;
        R6WalkKey k;
        k.t = u;
        k.leftArm = sampleLimb(ch[0], tick);
        k.rightArm = sampleLimb(ch[1], tick);
        k.leftLeg = sampleLimb(ch[2], tick);
        k.rightLeg = sampleLimb(ch[3], tick);
        out.keys.push_back(k);
    }

    // id da nome file
    auto slash = path.find_last_of("\\/");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    out.id = sanitizeGameId(base.empty() ? chosenName : base);
    err.clear();
    return out.valid();
}

inline std::string r6Escape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c >= 32) o.push_back((char)c);
    }
    return o;
}

/**
 * Inverso di r6Escape.
 *
 * Serve perche' jsonFindStr restituisce la stringa GREZZA: senza questo, un
 * path Windows salvato come "C:\\Users" torna dentro con due backslash, al
 * salvataggio successivo diventa quattro, poi otto. Un file di clip qui e'
 * arrivato a 2^28 backslash (256 MB) in 28 cicli salva/rileggi.
 */
inline std::string r6Unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) { o.push_back(s[i]); continue; }
        char n = s[++i];
        if (n == 'n') o.push_back('\n');
        else if (n == 't') o.push_back('\t');
        else o.push_back(n);          // \\ -> \   e   \" -> "
    }
    return o;
}

inline std::string r6ClipFilePath(const std::string& id) {
    return r6AnimsDir() + "/" + id + ".r6anim.json";
}

/** sourcePath solo se path corto (evita JSON da 100MB se qualcuno ci mette binari). */
inline std::string r6SafeSourcePath(const std::string& p) {
    if (p.size() > 400) return {};
    return p;
}

inline bool r6SaveClipFile(const R6WalkClip& clip, std::string& outPath) {
    if (!clip.valid()) return false;
    std::string id = clip.id.empty() ? sanitizeGameId(clip.displayName) : clip.id;
    outPath = r6ClipFilePath(id);
    const char* timeMode = clip.timeInSeconds ? "seconds" : "phase";
    std::string src = r6SafeSourcePath(clip.sourcePath);
    std::ostringstream j;
    j << std::fixed;
    j << "{\n"
      << "  \"format\": \"BlockyPlayerAnim\",\n"
      << "  \"version\": 2,\n"
      << "  \"id\": \"" << r6Escape(id) << "\",\n"
      << "  \"kind\": \"" << r6AnimKindName(clip.kind) << "\",\n"
      << "  \"clipName\": \"" << r6Escape(clip.clipName) << "\",\n"
      << "  \"displayName\": \"" << r6Escape(clip.displayName) << "\",\n"
      << "  \"sourcePath\": \"" << r6Escape(src) << "\",\n"
      << "  \"duration\": " << clip.duration << ",\n"
      << "  \"looped\": " << (clip.looped ? "true" : "false") << ",\n"
      << "  \"timeMode\": \"" << timeMode << "\",\n"
      << "  \"keys\": [\n";
    for (size_t i = 0; i < clip.keys.size(); ++i) {
        const auto& k = clip.keys[i];
        j << "    {\"t\":" << k.t
          << ",\"la\":" << k.leftArm << ",\"ra\":" << k.rightArm
          << ",\"ll\":" << k.leftLeg << ",\"rl\":" << k.rightLeg << "}";
        if (i + 1 < clip.keys.size()) j << ",";
        j << "\n";
    }
    j << "  ]\n}\n";
    std::ofstream f(outPath);
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool r6LoadClipFile(const std::string& path, R6WalkClip& out) {
    std::string json = readTextFile(path);
    if (json.empty()) return false;
    out = R6WalkClip{};
    // Sempre r6Unescape sui campi stringa: senza, i backslash raddoppiano a
    // ogni ciclo salva/rileggi (vedi r6Unescape).
    out.id = r6Unescape(jsonFindStr(json, "id"));
    out.clipName = r6Unescape(jsonFindStr(json, "clipName"));
    out.displayName = r6Unescape(jsonFindStr(json, "displayName"));
    out.sourcePath = r6SafeSourcePath(r6Unescape(jsonFindStr(json, "sourcePath")));
    float dur = 1.f;
    jsonFindFloat(json, "duration", 0, dur);
    out.duration = dur > 0.05f ? dur : 1.f;
    out.looped = jsonFindBool(json, "looped", 0, true);

    std::string kindStr = jsonFindStr(json, "kind");
    if (!kindStr.empty())
        out.kind = r6AnimKindFromName(kindStr);
    else {
        // legacy BlockyWalkAnim / infer da nome
        std::string hint = out.id + " " + out.clipName + " " + out.displayName;
        out.kind = r6AnimKindFromName(hint);
    }

    std::string tm = jsonFindStr(json, "timeMode");
    for (char& c : tm)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (tm == "seconds" || tm == "sec" || tm == "time")
        out.timeInSeconds = true;
    else if (tm == "phase" || tm == "normalized")
        out.timeInSeconds = false;
    else
        out.timeInSeconds = (out.kind == R6AnimKind::Jump);

    size_t keysPos = json.find("\"keys\"");
    if (keysPos == std::string::npos) return false;
    size_t arr = json.find('[', keysPos);
    if (arr == std::string::npos) return false;
    size_t pos = arr;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        R6WalkKey k;
        jsonFindFloat(json, "t", pos, k.t);
        jsonFindFloat(json, "la", pos, k.leftArm);
        jsonFindFloat(json, "ra", pos, k.rightArm);
        jsonFindFloat(json, "ll", pos, k.leftLeg);
        jsonFindFloat(json, "rl", pos, k.rightLeg);
        out.keys.push_back(k);
        pos = end + 1;
        if (json.find(']', pos) < json.find('{', pos)) break;
    }
    if (out.displayName.empty())
        out.displayName = out.clipName.empty() ? r6AnimKindName(out.kind) : out.clipName;
    if (out.id.empty()) out.id = sanitizeGameId(out.displayName);
    return out.valid();
}

inline bool r6SaveLibrary(const std::vector<R6WalkClip>& clips, const std::string& equippedId) {
    std::ostringstream j;
    j << "{\n  \"equippedId\": \"" << r6Escape(equippedId) << "\",\n  \"clips\": [\n";
    for (size_t i = 0; i < clips.size(); ++i) {
        std::string path;
        if (!r6SaveClipFile(clips[i], path)) continue;
        if (i) j << ",\n";
        j << "    {\"id\":\"" << r6Escape(clips[i].id)
          << "\",\"displayName\":\"" << r6Escape(clips[i].displayName)
          << "\",\"path\":\"" << r6Escape(path) << "\"}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(r6WalkLibraryPath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool r6LoadLibrary(std::vector<R6WalkClip>& clips, std::string& equippedId) {
    clips.clear();
    equippedId.clear();
    std::string json = readTextFile(r6WalkLibraryPath());
    if (json.empty()) return false;
    equippedId = jsonFindStr(json, "equippedId");
    size_t pos = 0;
    while ((pos = json.find("\"path\"", pos)) != std::string::npos) {
        std::string path = jsonFindStr(json, "path", pos);
        R6WalkClip c;
        if (!path.empty() && r6LoadClipFile(path, c))
            clips.push_back(std::move(c));
        pos += 6;
    }
    return !clips.empty();
}
