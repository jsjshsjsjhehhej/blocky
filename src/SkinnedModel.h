#pragma once

#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <limits>

static const int MAX_BONES = 100;

static inline glm::mat4 aiToGlm(const aiMatrix4x4& m) {
    glm::mat4 r;
    r[0][0]=m.a1; r[1][0]=m.a2; r[2][0]=m.a3; r[3][0]=m.a4;
    r[0][1]=m.b1; r[1][1]=m.b2; r[2][1]=m.b3; r[3][1]=m.b4;
    r[0][2]=m.c1; r[1][2]=m.c2; r[2][2]=m.c3; r[3][2]=m.c4;
    r[0][3]=m.d1; r[1][3]=m.d2; r[2][3]=m.d3; r[3][3]=m.d4;
    return r;
}
static inline glm::vec3 aiToGlm(const aiVector3D& v) { return { v.x, v.y, v.z }; }
static inline glm::quat aiToGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

struct KeyPosition { glm::vec3 position; float time; };
struct KeyRotation { glm::quat orientation; float time; };
struct KeyScale    { glm::vec3 scale;    float time; };

struct Bone {
    std::string name;
    std::vector<KeyPosition> positions;
    std::vector<KeyRotation> rotations;
    std::vector<KeyScale>    scales;
    glm::mat4 localTransform{ 1.0f };

    static float scaleFactor(float last, float next, float t) {
        float diff = next - last;
        return diff == 0.0f ? 0.0f : (t - last) / diff;
    }
    template <typename T>
    static int keyIndex(const std::vector<T>& keys, float t) {
        for (size_t i = 0; i + 1 < keys.size(); ++i)
            if (t < keys[i + 1].time) return (int)i;
        return (int)keys.size() - 2;
    }

    glm::mat4 interpPosition(float t) const {
        if (positions.size() == 1) return glm::translate(glm::mat4(1.0f), positions[0].position);
        int i = keyIndex(positions, t), n = i + 1;
        float f = scaleFactor(positions[i].time, positions[n].time, t);
        return glm::translate(glm::mat4(1.0f), glm::mix(positions[i].position, positions[n].position, f));
    }
    glm::mat4 interpRotation(float t) const {
        if (rotations.size() == 1) return glm::mat4_cast(glm::normalize(rotations[0].orientation));
        int i = keyIndex(rotations, t), n = i + 1;
        float f = scaleFactor(rotations[i].time, rotations[n].time, t);
        return glm::mat4_cast(glm::normalize(glm::slerp(rotations[i].orientation, rotations[n].orientation, f)));
    }
    glm::mat4 interpScale(float t) const {
        if (scales.size() == 1) return glm::scale(glm::mat4(1.0f), scales[0].scale);
        int i = keyIndex(scales, t), n = i + 1;
        float f = scaleFactor(scales[i].time, scales[n].time, t);
        return glm::scale(glm::mat4(1.0f), glm::mix(scales[i].scale, scales[n].scale, f));
    }
    void update(float t) {
        localTransform = interpPosition(t) * interpRotation(t) * interpScale(t);
    }

    void sample(float t, glm::vec3& outPos, glm::quat& outRot, glm::vec3& outScale) const {
        if (positions.empty()) outPos = glm::vec3(0.0f);
        else if (positions.size() == 1) outPos = positions[0].position;
        else {
            int i = keyIndex(positions, t), n = i + 1;
            outPos = glm::mix(positions[i].position, positions[n].position,
                              scaleFactor(positions[i].time, positions[n].time, t));
        }

        if (rotations.empty()) outRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        else if (rotations.size() == 1) outRot = glm::normalize(rotations[0].orientation);
        else {
            int i = keyIndex(rotations, t), n = i + 1;
            outRot = glm::normalize(glm::slerp(rotations[i].orientation, rotations[n].orientation,
                                               scaleFactor(rotations[i].time, rotations[n].time, t)));
        }

        if (scales.empty()) outScale = glm::vec3(1.0f);
        else if (scales.size() == 1) outScale = scales[0].scale;
        else {
            int i = keyIndex(scales, t), n = i + 1;
            outScale = glm::mix(scales[i].scale, scales[n].scale,
                                scaleFactor(scales[i].time, scales[n].time, t));
        }
    }
};

struct SkinnedClip {
    std::string name;
    float duration = 0.0f;
    float ticksPerSecond = 25.0f;
    std::vector<Bone> bones;

    bool valid() const { return duration > 0.0f && !bones.empty(); }
};

struct NodeData {
    glm::mat4 transformation;
    std::string name;
    std::vector<NodeData> children;
};

class AnimatedModel {
public:
    struct SubMesh {
        GLuint vao = 0;
        GLsizei count = 0;
        glm::vec3 color{ 0.7f };
        std::vector<std::string> boneNames;
        std::vector<glm::mat4>   boneOffsets;
    };

    std::vector<SubMesh> meshes;
    glm::mat4 globalInverse{ 1.0f };

    std::vector<SkinnedClip> clips;
    NodeData rootNode;
    float currentTime = 0.0f;

    float blendSeconds = 0.18f;

    std::map<std::string, glm::mat4> nodeGlobal;
    glm::vec3 bbMin{ 0.0f }, bbMax{ 0.0f };
    glm::mat4 correction = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
    bool loaded = false;

    bool load(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_LimitBoneWeights
        );
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            std::fprintf(stderr, "[AnimatedModel] Errore: %s\n", importer.GetErrorString());
            return false;
        }

        globalInverse = glm::inverse(aiToGlm(scene->mRootNode->mTransformation));

        std::vector<std::vector<SVert>> cpuVerts;

        for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* mesh = scene->mMeshes[m];
            std::vector<SVert> verts;
            processMesh(scene, mesh, verts);
            cpuVerts.push_back(std::move(verts));
        }
        if (meshes.empty()) {
            std::fprintf(stderr, "[AnimatedModel] Nessuna mesh in '%s'\n", path.c_str());
            return false;
        }

        copyHierarchy(rootNode, scene->mRootNode);
        loadAnimation(scene);
        computeBindBounds(cpuVerts);

        loaded = true;

        std::string clipList;
        for (const SkinnedClip& c : clips) {
            if (!clipList.empty()) clipList += ", ";
            clipList += c.name;
        }
        std::printf("[AnimatedModel] '%s': %zu mesh, %d clip [%s] | attiva '%s' | bbox y[%.1f..%.1f]\n",
                    path.c_str(), meshes.size(), clipCount(),
                    clipList.c_str(), activeClipName().c_str(), bbMin.y, bbMax.y);
        return true;
    }

    glm::mat4 placement(float targetHeight, float groundY, glm::vec3 offset) const {
        glm::vec3 dim = bbMax - bbMin;
        float scale = targetHeight / glm::max(dim.y, 1e-4f);
        glm::vec3 center = (bbMin + bbMax) * 0.5f;
        glm::mat4 t(1.0f);
        t = glm::translate(t, offset + glm::vec3(0.0f, groundY - bbMin.y * scale, 0.0f));
        t = glm::scale(t, glm::vec3(scale));
        t = glm::translate(t, glm::vec3(-center.x, 0.0f, -center.z));
        t = t * correction;
        return t;
    }


    int clipCount() const { return (int)clips.size(); }

    std::vector<std::string> clipNames() const {
        std::vector<std::string> n;
        n.reserve(clips.size());
        for (const SkinnedClip& c : clips) n.push_back(c.name);
        return n;
    }

    const SkinnedClip* activeClip() const {
        if (activeIndex < 0 || activeIndex >= (int)clips.size()) return nullptr;
        return &clips[(size_t)activeIndex];
    }

    const std::string& activeClipName() const {
        static const std::string none;
        const SkinnedClip* c = activeClip();
        return c ? c->name : none;
    }

    int findClip(const std::string& needle) const {
        const std::string want = toLowerCopy(needle);
        int loose = -1;
        for (size_t i = 0; i < clips.size(); ++i) {
            const std::string have = toLowerCopy(clips[i].name);
            if (have == want) return (int)i;
            if (loose < 0 && have.find(want) != std::string::npos) loose = (int)i;
        }
        return loose;
    }

    bool setClip(const std::string& name, bool restart = true) {
        const int idx = findClip(name);
        if (idx < 0) return false;
        return setClip(idx, restart);
    }

    bool setClip(int index, bool restart = true) {
        if (index < 0 || index >= (int)clips.size()) return false;
        if (index == activeIndex) return true;

        previousIndex = activeIndex;
        previousTime = currentTime;
        blendLeft = (previousIndex >= 0) ? blendSeconds : 0.0f;

        activeIndex = index;
        if (restart) currentTime = 0.0f;
        return true;
    }

    void update(float dt) {
        if (!loaded) return;

        if (const SkinnedClip* c = activeClip()) {
            if (c->valid()) {
                currentTime = fmodf(currentTime + c->ticksPerSecond * dt, c->duration);
            }
        }

        if (blendLeft > 0.0f) {
            blendLeft = std::max(0.0f, blendLeft - dt);
            if (const SkinnedClip* p = previousClip()) {
                if (p->valid())
                    previousTime = fmodf(previousTime + p->ticksPerSecond * dt, p->duration);
            }
            if (blendLeft <= 0.0f) previousIndex = -1;
        }

        nodeGlobal.clear();
        buildPose(&rootNode, glm::mat4(1.0f));
    }

    void draw(GLuint prog) const {
        GLint locBones = glGetUniformLocation(prog, "uFinalBones");
        GLint locColor = glGetUniformLocation(prog, "uColor");

        for (const SubMesh& sm : meshes) {
            std::vector<glm::mat4> mats;
            if (sm.boneNames.empty()) {
                mats.push_back(glm::mat4(1.0f));
            } else {
                mats.resize(sm.boneNames.size());
                for (size_t i = 0; i < sm.boneNames.size(); ++i) {
                    auto it = nodeGlobal.find(sm.boneNames[i]);
                    glm::mat4 g = (it != nodeGlobal.end()) ? it->second : glm::mat4(1.0f);
                    mats[i] = globalInverse * g * sm.boneOffsets[i];
                }
            }
            glUniformMatrix4fv(locBones, (GLsizei)mats.size(), GL_FALSE, glm::value_ptr(mats[0]));
            glUniform3fv(locColor, 1, glm::value_ptr(sm.color));
            glBindVertexArray(sm.vao);
            glDrawElements(GL_TRIANGLES, sm.count, GL_UNSIGNED_INT, 0);
        }
    }

private:
    struct SVert {
        float px, py, pz;
        float nx, ny, nz;
        int   b[4]  = { -1, -1, -1, -1 };
        float w[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    void addWeight(SVert& v, int boneId, float weight) {
        if (weight <= 0.0f) return;
        for (int i = 0; i < 4; ++i)
            if (v.b[i] < 0) { v.b[i] = boneId; v.w[i] = weight; return; }
    }

    void processMesh(const aiScene* scene, const aiMesh* mesh, std::vector<SVert>& verts) {
        verts.assign(mesh->mNumVertices, SVert{});
        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D& p = mesh->mVertices[i];
            aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
            verts[i].px = p.x; verts[i].py = p.y; verts[i].pz = p.z;
            verts[i].nx = n.x; verts[i].ny = n.y; verts[i].nz = n.z;
        }

        SubMesh sm;
        // uFinalBones nello shader ha solo MAX_BONES slot: un mesh con piu'
        // ossa di cosi' (rig complessi, tante dita/spina dorsale in un
        // unico sottomesh) sforerebbe l'array uniform — undefined behavior
        // che si vede come mesh deformata/rotta. Le ossa oltre il limite
        // vengono ignorate invece di corrompere la memoria della GPU.
        unsigned numBones = std::min(mesh->mNumBones, (unsigned)MAX_BONES);
        if (mesh->mNumBones > (unsigned)MAX_BONES) {
            std::fprintf(stderr,
                "[SkinnedModel] '%s': %u ossa oltre il limite di %d — le ossa in eccesso sono ignorate\n",
                mesh->mName.C_Str(), mesh->mNumBones, MAX_BONES);
        }
        sm.boneNames.resize(numBones);
        sm.boneOffsets.resize(numBones);

        for (unsigned b = 0; b < numBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            sm.boneNames[b]   = bone->mName.C_Str();
            sm.boneOffsets[b] = aiToGlm(bone->mOffsetMatrix);
            for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                unsigned vid = bone->mWeights[w].mVertexId;
                if (vid < verts.size())
                    addWeight(verts[vid], (int)b, bone->mWeights[w].mWeight);
            }
        }

        std::vector<unsigned> idx;
        idx.reserve(mesh->mNumFaces * 3);
        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            for (unsigned j = 0; j < mesh->mFaces[f].mNumIndices; ++j)
                idx.push_back(mesh->mFaces[f].mIndices[j]);

        sm.color = glm::vec3(0.7f);
        if (scene->HasMaterials()) {
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiColor3D c(0.7f, 0.7f, 0.7f);
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS)
                sm.color = glm::vec3(c.r, c.g, c.b);
        }

        GLuint vao, vbo, ebo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SVert), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SVert), (void*)offsetof(SVert, px));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SVert), (void*)offsetof(SVert, nx));
        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(2, 4, GL_INT, sizeof(SVert), (void*)offsetof(SVert, b));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SVert), (void*)offsetof(SVert, w));
        glEnableVertexAttribArray(3);
        glBindVertexArray(0);

        sm.vao = vao;
        sm.count = (GLsizei)idx.size();
        meshes.push_back(std::move(sm));
    }

    void copyHierarchy(NodeData& dest, const aiNode* src) {
        dest.name = src->mName.C_Str();
        dest.transformation = aiToGlm(src->mTransformation);
        dest.children.reserve(src->mNumChildren);
        for (unsigned i = 0; i < src->mNumChildren; ++i) {
            NodeData child;
            copyHierarchy(child, src->mChildren[i]);
            dest.children.push_back(std::move(child));
        }
    }

    void buildBind(const NodeData* node, const glm::mat4& parent,
                   std::map<std::string, glm::mat4>& out) const {
        glm::mat4 global = parent * node->transformation;
        out[node->name] = global;
        for (const NodeData& child : node->children)
            buildBind(&child, global, out);
    }

    void computeBindBounds(const std::vector<std::vector<SVert>>& cpuVerts) {
        std::map<std::string, glm::mat4> gb;
        buildBind(&rootNode, glm::mat4(1.0f), gb);

        bbMin = glm::vec3( std::numeric_limits<float>::max());
        bbMax = glm::vec3(-std::numeric_limits<float>::max());

        for (size_t m = 0; m < meshes.size(); ++m) {
            const SubMesh& sm = meshes[m];
            std::vector<glm::mat4> fm(sm.boneNames.size());
            for (size_t i = 0; i < sm.boneNames.size(); ++i) {
                auto it = gb.find(sm.boneNames[i]);
                glm::mat4 g = (it != gb.end()) ? it->second : glm::mat4(1.0f);
                fm[i] = globalInverse * g * sm.boneOffsets[i];
            }
            for (const SVert& v : cpuVerts[m]) {
                glm::mat4 skin(0.0f);
                float tot = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    if (v.b[k] < 0) continue;
                    skin += fm[v.b[k]] * v.w[k];
                    tot += v.w[k];
                }
                if (tot < 1e-4f) skin = glm::mat4(1.0f);
                glm::vec3 p = glm::vec3(correction * skin * glm::vec4(v.px, v.py, v.pz, 1.0f));
                bbMin = glm::min(bbMin, p);
                bbMax = glm::max(bbMax, p);
            }
        }
    }

    void loadAnimation(const aiScene* scene) {
        if (scene->mNumAnimations == 0) return;

        clips.reserve(scene->mNumAnimations);

        for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
            const aiAnimation* anim = scene->mAnimations[a];

            SkinnedClip clip;
            clip.name = anim->mName.C_Str();
            if (clip.name.empty()) clip.name = "clip" + std::to_string(a);
            clip.duration = (float)anim->mDuration;
            clip.ticksPerSecond = anim->mTicksPerSecond != 0.0 ? (float)anim->mTicksPerSecond : 25.0f;

            clip.bones.reserve(anim->mNumChannels);
            for (unsigned c = 0; c < anim->mNumChannels; ++c) {
                const aiNodeAnim* ch = anim->mChannels[c];
                Bone bone;
                bone.name = ch->mNodeName.C_Str();
                for (unsigned k = 0; k < ch->mNumPositionKeys; ++k)
                    bone.positions.push_back({ aiToGlm(ch->mPositionKeys[k].mValue), (float)ch->mPositionKeys[k].mTime });
                for (unsigned k = 0; k < ch->mNumRotationKeys; ++k)
                    bone.rotations.push_back({ aiToGlm(ch->mRotationKeys[k].mValue), (float)ch->mRotationKeys[k].mTime });
                for (unsigned k = 0; k < ch->mNumScalingKeys; ++k)
                    bone.scales.push_back({ aiToGlm(ch->mScalingKeys[k].mValue), (float)ch->mScalingKeys[k].mTime });
                clip.bones.push_back(std::move(bone));
            }
            clips.push_back(std::move(clip));
        }

        int chosen = 0, bestScore = -1;
        for (size_t i = 0; i < clips.size(); ++i) {
            const std::string nm = toLowerCopy(clips[i].name);
            int score = clips[i].valid() ? (int)clips[i].bones.size() : -1;
            if (nm.find("walk") != std::string::npos) score += 1000;
            if (score > bestScore) { bestScore = score; chosen = (int)i; }
        }
        activeIndex = chosen;
    }

    static std::string toLowerCopy(std::string s) {
        for (char& c : s) c = (char)tolower((unsigned char)c);
        return s;
    }

    const SkinnedClip* previousClip() const {
        if (previousIndex < 0 || previousIndex >= (int)clips.size()) return nullptr;
        return &clips[(size_t)previousIndex];
    }

    static const Bone* findBoneIn(const SkinnedClip* clip, const std::string& name) {
        if (!clip) return nullptr;
        for (const Bone& b : clip->bones)
            if (b.name == name) return &b;
        return nullptr;
    }

    void buildPose(const NodeData* node, const glm::mat4& parent) {
        glm::mat4 local = node->transformation;

        if (const Bone* b = findBoneIn(activeClip(), node->name)) {
            glm::vec3 pos, scl;
            glm::quat rot;
            b->sample(currentTime, pos, rot, scl);

            if (blendLeft > 0.0f && blendSeconds > 0.0f) {
                if (const Bone* prev = findBoneIn(previousClip(), node->name)) {
                    glm::vec3 p2, s2;
                    glm::quat r2;
                    prev->sample(previousTime, p2, r2, s2);
                    const float w = 1.0f - (blendLeft / blendSeconds);
                    pos = glm::mix(p2, pos, w);
                    rot = glm::normalize(glm::slerp(r2, rot, w));
                    scl = glm::mix(s2, scl, w);
                }
            }

            local = glm::translate(glm::mat4(1.0f), pos)
                  * glm::mat4_cast(rot)
                  * glm::scale(glm::mat4(1.0f), scl);
        }

        glm::mat4 global = parent * local;
        nodeGlobal[node->name] = global;
        for (const NodeData& child : node->children)
            buildPose(&child, global);
    }

private:
    int activeIndex = -1;
    int previousIndex = -1;
    float previousTime = 0.0f;
    float blendLeft = 0.0f;
};
