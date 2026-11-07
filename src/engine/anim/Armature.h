#pragma once

/**
 * Armature: lo scheletro di un rig, come in Blender.
 *
 * Ogni osso ha una posa di riposo (rest) rispetto al padre e punta lungo il
 * suo +Y per `length` stud: e' la convenzione di Blender, cosi' testa e coda
 * dell'osso si ricavano dalla trasformazione senza dati in piu'.
 *
 * Una Pose e' la deformazione SOPRA il riposo, osso per osso, nello spazio
 * locale dell'osso: posa vuota = scheletro a riposo. E' quello che le
 * animazioni registrano (Location/Rotation/Scale di Blender in Pose Mode).
 *
 *   globale(osso) = globale(padre) * riposo(osso) * posa(osso)
 *
 * Nessuna dipendenza da OpenGL o dal DataModel: si prova da sola nei test.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace blk {
namespace anim {

/** Posizione, rotazione e scala. Niente taglio (shear): basta per le ossa. */
struct Transform {
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f};

    glm::mat4 matrix() const {
        return glm::translate(glm::mat4(1.f), pos) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.f), scale);
    }

    glm::vec3 apply(const glm::vec3& p) const { return pos + rot * (scale * p); }

    bool isIdentity(float eps = 1e-5f) const {
        return glm::length(pos) < eps && std::fabs(std::fabs(rot.w) - 1.f) < eps
            && glm::length(scale - glm::vec3(1.f)) < eps;
    }
};

/** parent * child: prima il figlio, poi il padre. */
inline Transform compose(const Transform& parent, const Transform& child) {
    Transform r;
    r.pos = parent.apply(child.pos);
    r.rot = glm::normalize(parent.rot * child.rot);
    r.scale = parent.scale * child.scale;
    return r;
}

inline Transform inverse(const Transform& t) {
    Transform r;
    r.rot = glm::conjugate(t.rot);
    r.scale = glm::vec3(1.f) / glm::max(t.scale, glm::vec3(1e-6f));
    r.pos = -(r.rot * t.pos) * r.scale;
    return r;
}

inline Transform lerp(const Transform& a, const Transform& b, float t) {
    Transform r;
    r.pos = glm::mix(a.pos, b.pos, t);
    // slerp sul percorso corto: con q e -q (stessa rotazione) si gira dal lato giusto.
    glm::quat qb = b.rot;
    if (glm::dot(a.rot, qb) < 0.f) qb = -qb;
    r.rot = glm::normalize(glm::slerp(a.rot, qb, t));
    r.scale = glm::mix(a.scale, b.scale, t);
    return r;
}

/** Rotazione minima che porta la direzione `from` su `to` (entrambe normalizzate). */
inline glm::quat rotationBetween(const glm::vec3& from, const glm::vec3& to) {
    const float d = glm::clamp(glm::dot(from, to), -1.f, 1.f);
    if (d > 0.99999f) return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (d < -0.99999f) {
        glm::vec3 axis = glm::cross(glm::vec3(1.f, 0.f, 0.f), from);
        if (glm::length(axis) < 1e-4f) axis = glm::cross(glm::vec3(0.f, 0.f, 1.f), from);
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::normalize(glm::cross(from, to));
    return glm::angleAxis(std::acos(d), axis);
}

struct Bone {
    std::string name;
    int parent = -1;
    Transform rest;          // rispetto al padre (al mondo del rig se radice)
    float length = 1.f;      // lungo il +Y locale
    bool deform = true;      // influenza lo skinning (le ossa di controllo no)
    std::string collection;  // Bone Collections di Blender: gruppi da mostrare/nascondere
};

enum class Mode { Edit, Pose, Animate };

inline const char* modeName(Mode m) {
    return m == Mode::Edit ? "Edit Mode" : m == Mode::Pose ? "Pose Mode" : "Animate Mode";
}

/** Deformazione sopra il riposo, un Transform per osso (spazio locale dell'osso). */
struct Pose {
    std::vector<Transform> local;

    void reset(size_t boneCount) { local.assign(boneCount, Transform{}); }
    Transform get(int i) const { return i >= 0 && i < (int)local.size() ? local[(size_t)i] : Transform{}; }
};

class Armature {
public:
    std::string name = "Armature";
    std::vector<Bone> bones;

    int find(const std::string& n) const {
        for (size_t i = 0; i < bones.size(); ++i)
            if (bones[i].name == n) return (int)i;
        return -1;
    }

    bool valid(int i) const { return i >= 0 && i < (int)bones.size(); }

    std::vector<int> children(int i) const {
        std::vector<int> out;
        for (size_t c = 0; c < bones.size(); ++c)
            if (bones[c].parent == i) out.push_back((int)c);
        return out;
    }

    /** a e' padre, nonno, ... di b? */
    bool isAncestor(int a, int b) const {
        for (int p = valid(b) ? bones[(size_t)b].parent : -1, guard = 0; p >= 0 && guard < 4096; ++guard) {
            if (p == a) return true;
            p = bones[(size_t)p].parent;
        }
        return false;
    }

    /** Nome libero: "Arm", poi "Arm.001", "Arm.002" come Blender. */
    std::string uniqueName(const std::string& base) const {
        const std::string b = base.empty() ? "Bone" : base;
        if (find(b) < 0) return b;
        for (int n = 1; n < 10000; ++n) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), ".%03d", n);
            if (find(b + buf) < 0) return b + buf;
        }
        return b + ".x";
    }

    int addBone(const std::string& n, int parent, const Transform& rest, float length = 1.f) {
        Bone b;
        b.name = uniqueName(n);
        b.parent = valid(parent) ? parent : -1;
        b.rest = rest;
        b.length = std::max(0.01f, length);
        bones.push_back(std::move(b));
        return (int)bones.size() - 1;
    }

    bool renameBone(int i, const std::string& n) {
        if (!valid(i) || n.empty()) return false;
        const int other = find(n);
        if (other >= 0 && other != i) return false;
        bones[(size_t)i].name = n;
        return true;
    }

    /**
     * Cambia padre. keepWorld = l'osso resta dov'e' nel mondo (Blender: "Keep
     * Offset"). Rifiuta i cicli: un osso non puo' finire sotto un suo figlio.
     */
    bool setParent(int i, int newParent, bool keepWorld = true) {
        if (!valid(i)) return false;
        if (newParent == i || (valid(newParent) && isAncestor(i, newParent))) return false;
        if (!valid(newParent)) newParent = -1;
        if (keepWorld) {
            std::vector<Transform> g;
            restGlobals(g);
            const Transform parentG = newParent >= 0 ? g[(size_t)newParent] : Transform{};
            bones[(size_t)i].rest = compose(inverse(parentG), g[(size_t)i]);
        }
        bones[(size_t)i].parent = newParent;
        return true;
    }

    /**
     * Toglie un osso. I figli passano al nonno restando dove sono (come
     * Blender). Gli indici dopo `i` scalano di uno.
     */
    bool removeBone(int i) {
        if (!valid(i)) return false;
        std::vector<Transform> g;
        restGlobals(g);
        const int grand = bones[(size_t)i].parent;
        const Transform grandG = grand >= 0 ? g[(size_t)grand] : Transform{};
        for (size_t c = 0; c < bones.size(); ++c) {
            if (bones[c].parent != i) continue;
            bones[c].parent = grand;
            bones[c].rest = compose(inverse(grandG), g[c]);
        }
        bones.erase(bones.begin() + i);
        for (auto& b : bones)
            if (b.parent > i) --b.parent;
        return true;
    }

    /** Copia dell'osso (e dei figli se withChildren) sotto lo stesso padre. */
    int duplicateBone(int i, bool withChildren = false) {
        if (!valid(i)) return -1;
        std::vector<int> src = {i};
        if (withChildren) {
            for (size_t k = 0; k < src.size(); ++k)
                for (int c : children(src[k])) src.push_back(c);
        }
        std::vector<int> map(bones.size(), -1);
        for (int s : src) {
            Bone b = bones[(size_t)s];
            b.name = uniqueName(b.name);
            const int np = map[(size_t)std::max(0, b.parent)];
            if (s != i && b.parent >= 0 && np >= 0) b.parent = np;
            bones.push_back(b);
            map.resize(bones.size(), -1);
            map[(size_t)s] = (int)bones.size() - 1;
        }
        return map[(size_t)i];
    }

    /** Ordine con i padri sempre prima dei figli (dopo setParent non vale l'ordine degli indici). */
    std::vector<int> order() const {
        std::vector<int> out;
        std::vector<char> done(bones.size(), 0);
        out.reserve(bones.size());
        for (size_t start = 0; start < bones.size(); ++start) {
            std::vector<int> chain;
            for (int b = (int)start, guard = 0; b >= 0 && !done[(size_t)b] && guard < 4096; ++guard) {
                chain.push_back(b);
                b = bones[(size_t)b].parent;
            }
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                if (done[(size_t)*it]) continue;
                done[(size_t)*it] = 1;
                out.push_back(*it);
            }
        }
        return out;
    }

    void restGlobals(std::vector<Transform>& out) const {
        out.assign(bones.size(), Transform{});
        for (int i : order()) {
            const Bone& b = bones[(size_t)i];
            out[(size_t)i] = b.parent >= 0 ? compose(out[(size_t)b.parent], b.rest) : b.rest;
        }
    }

    void poseGlobals(const Pose& pose, std::vector<Transform>& out) const {
        out.assign(bones.size(), Transform{});
        for (int i : order()) {
            const Bone& b = bones[(size_t)i];
            const Transform local = compose(b.rest, pose.get(i));
            out[(size_t)i] = b.parent >= 0 ? compose(out[(size_t)b.parent], local) : local;
        }
    }

    /** Ricalcola solo l'osso i e i suoi discendenti (dopo aver toccato la posa di i). */
    void updateSubtree(const Pose& pose, int i, std::vector<Transform>& globals) const {
        if (!valid(i) || globals.size() != bones.size()) return;
        std::vector<int> stack = {i};
        while (!stack.empty()) {
            const int b = stack.back();
            stack.pop_back();
            const Bone& bone = bones[(size_t)b];
            const Transform local = compose(bone.rest, pose.get(b));
            globals[(size_t)b] = bone.parent >= 0 ? compose(globals[(size_t)bone.parent], local) : local;
            for (int c : children(b)) stack.push_back(c);
        }
    }

    static glm::vec3 head(const Transform& g) { return g.pos; }
    glm::vec3 tail(int i, const Transform& g) const {
        return g.apply(glm::vec3(0.f, valid(i) ? bones[(size_t)i].length : 1.f, 0.f));
    }

    /**
     * Imposta la rotazione GLOBALE dell'osso i cambiando solo la sua posa
     * (serve a IK e vincoli, che ragionano nel mondo). globals deve essere
     * aggiornato: lo resta anche dopo, sottoalbero compreso.
     */
    void setGlobalRotation(Pose& pose, std::vector<Transform>& globals, int i, const glm::quat& globalRot) const {
        if (!valid(i)) return;
        const Bone& b = bones[(size_t)i];
        const glm::quat parentRot = b.parent >= 0 ? globals[(size_t)b.parent].rot : glm::quat(1.f, 0.f, 0.f, 0.f);
        // globale = padre * riposo * posa  =>  posa = (padre * riposo)^-1 * globale
        const glm::quat poseRot = glm::conjugate(parentRot * b.rest.rot) * globalRot;
        if (pose.local.size() < bones.size()) pose.local.resize(bones.size());
        pose.local[(size_t)i].rot = glm::normalize(poseRot);
        updateSubtree(pose, i, globals);
    }

    /** Come sopra per la posizione globale della testa dell'osso. */
    void setGlobalPosition(Pose& pose, std::vector<Transform>& globals, int i, const glm::vec3& globalPos) const {
        if (!valid(i)) return;
        const Bone& b = bones[(size_t)i];
        const Transform parentG = b.parent >= 0 ? globals[(size_t)b.parent] : Transform{};
        // testa = padre * (riposo.pos + riposo.rot*(riposo.scale*posa.pos))
        const glm::vec3 inParent = inverse(parentG).apply(globalPos);
        const glm::vec3 inRest = glm::conjugate(b.rest.rot) * (inParent - b.rest.pos) / glm::max(b.rest.scale, glm::vec3(1e-6f));
        if (pose.local.size() < bones.size()) pose.local.resize(bones.size());
        pose.local[(size_t)i].pos = inRest;
        updateSubtree(pose, i, globals);
    }
};

} // namespace anim
} // namespace blk
