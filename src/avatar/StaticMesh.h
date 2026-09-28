#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/config.h>

#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class StaticMesh {
public:
    struct SubMesh {
        GLuint vao = 0, vbo = 0;
        int vertexCount = 0;
        GLuint texId = 0;
        GLuint emissiveTex = 0;
        glm::vec3 baseColor{1.f};
        glm::vec3 emissive{0.f};
        float alpha = 1.f;
        float metallic = 0.f;
        float roughness = 0.85f;
        bool twoSided = false;
        bool ownsTex = false;
        bool ownsEmissiveTex = false;
        std::string name;
        std::string matName;
        std::string missingTex;
    };

    std::vector<SubMesh> subs;
    bool loaded = false;
    bool hasBones = false;
    glm::vec3 boundsMin{0}, boundsMax{0};
    std::string path;
    int vertexCount = 0;
    bool exactImport = false;

    GLuint vao = 0, vbo = 0;

    void clear() {
        for (auto& s : subs) {
            if (s.vbo) glDeleteBuffers(1, &s.vbo);
            if (s.vao) glDeleteVertexArrays(1, &s.vao);
            if (s.ownsTex && s.texId) glDeleteTextures(1, &s.texId);
            if (s.ownsEmissiveTex && s.emissiveTex) glDeleteTextures(1, &s.emissiveTex);
            s = {};
        }
        subs.clear();
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
        vertexCount = 0;
        loaded = false;
        hasBones = false;
        path.clear();
    }

    ~StaticMesh() { clear(); }

    StaticMesh() = default;
    StaticMesh(const StaticMesh&) = delete;
    StaticMesh& operator=(const StaticMesh&) = delete;
    StaticMesh(StaticMesh&& o) noexcept { *this = std::move(o); }
    StaticMesh& operator=(StaticMesh&& o) noexcept {
        if (this == &o) return *this;
        clear();
        subs = std::move(o.subs);
        loaded = o.loaded;
        hasBones = o.hasBones;
        boundsMin = o.boundsMin;
        boundsMax = o.boundsMax;
        path = std::move(o.path);
        vertexCount = o.vertexCount;
        vao = o.vao;
        vbo = o.vbo;
        o.vao = o.vbo = 0;
        o.vertexCount = 0;
        o.loaded = false;
        o.hasBones = false;
        o.subs.clear();
        return *this;
    }

    static std::string extensionOf(const std::string& file) {
        auto dot = file.find_last_of('.');
        if (dot == std::string::npos) return {};
        std::string e = file.substr(dot);
        for (char& c : e)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        return e;
    }

    static std::string dirOf(const std::string& file) {
        auto s = file.find_last_of("\\/");
        return s == std::string::npos ? std::string(".") : file.substr(0, s);
    }

    static std::string toLower(std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    static bool isJunkMeshName(const std::string& raw) {
        std::string n = toLower(raw);
        static const char* junk[] = {
            "collision", "collider", "colmesh", "col_", "_col",
            "hitbox", "hurtbox", "physic", "physics", "convex", "hull",
            "ucx_", "ubx_", "usx_", "mdc_",
            "lod1", "lod2", "lod3", "lod4", "_lod", "lod_",
            "helper", "dummy", "nullobject", "empty.",
            "shadow", "occluder", "occlusion",
            "armature", "rig_", "ik_",
            "camera", "light", "lamp",
        };
        for (const char* j : junk) {
            if (n.find(j) != std::string::npos)
                return true;
        }
        return false;
    }

    static bool readFileBytes(const std::string& file, std::vector<char>& out) {
        out.clear();
#ifdef _WIN32
        int n = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
        if (n > 0) {
            std::wstring w(n - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, w.data(), n);
            FILE* f = _wfopen(w.c_str(), L"rb");
            if (f) {
                std::fseek(f, 0, SEEK_END);
                long sz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                if (sz > 0) {
                    out.resize((size_t)sz);
                    size_t rd = std::fread(out.data(), 1, out.size(), f);
                    std::fclose(f);
                    return rd == out.size();
                }
                std::fclose(f);
            }
        }
#endif
        FILE* f = std::fopen(file.c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0) { std::fclose(f); return false; }
        out.resize((size_t)sz);
        size_t rd = std::fread(out.data(), 1, out.size(), f);
        std::fclose(f);
        return rd == out.size();
    }

    static glm::mat4 aiMat(const aiMatrix4x4& m) {
        return glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4);
    }

    static GLuint uploadRgba(const unsigned char* px, int w, int h, bool flipY) {
        if (!px || w <= 0 || h <= 0) return 0;
        std::vector<unsigned char> flipped;
        const unsigned char* src = px;
        if (flipY) {
            flipped.resize((size_t)w * h * 4);
            for (int y = 0; y < h; ++y)
                std::memcpy(flipped.data() + (size_t)(h - 1 - y) * w * 4,
                            px + (size_t)y * w * 4, (size_t)w * 4);
            src = flipped.data();
        }
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return id;
    }

    static GLuint loadTextureFile(const std::string& texPath, bool quiet = false) {
        stbi_set_flip_vertically_on_load(false);
        int w = 0, h = 0, ch = 0;
        unsigned char* px = stbi_load(texPath.c_str(), &w, &h, &ch, 4);
        if (!px) {
#ifdef _WIN32
            int n = MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(), -1, nullptr, 0);
            if (n > 0) {
                std::wstring wpath(n - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(), -1, wpath.data(), n);
                FILE* f = _wfopen(wpath.c_str(), L"rb");
                if (f) {
                    std::fseek(f, 0, SEEK_END);
                    long sz = std::ftell(f);
                    std::fseek(f, 0, SEEK_SET);
                    if (sz > 0) {
                        std::vector<unsigned char> buf((size_t)sz);
                        std::fread(buf.data(), 1, buf.size(), f);
                        std::fclose(f);
                        px = stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &ch, 4);
                    } else std::fclose(f);
                }
            }
#endif
        }
        if (!px) {
            if (!quiet)
                std::fprintf(stderr, "[StaticMesh] Texture mancante: %s\n", texPath.c_str());
            return 0;
        }
        GLuint id = uploadRgba(px, w, h, false);
        stbi_image_free(px);
        return id;
    }

    static GLuint loadEmbeddedTexture(const aiScene* sc, const char* pathHint) {
        if (!sc || !pathHint || pathHint[0] != '*') return 0;
        int idx = std::atoi(pathHint + 1);
        if (idx < 0 || idx >= (int)sc->mNumTextures) return 0;
        const aiTexture* t = sc->mTextures[idx];
        if (!t) return 0;

        if (t->mHeight == 0) {
            int w = 0, h = 0, ch = 0;
            stbi_set_flip_vertically_on_load(false);
            unsigned char* px = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(t->pcData),
                (int)t->mWidth, &w, &h, &ch, 4);
            if (!px) return 0;
            GLuint id = uploadRgba(px, w, h, false);
            stbi_image_free(px);
            return id;
        }

        std::vector<unsigned char> rgba((size_t)t->mWidth * t->mHeight * 4);
        for (unsigned i = 0; i < t->mWidth * t->mHeight; ++i) {
            const aiTexel& tx = t->pcData[i];
            rgba[i * 4 + 0] = tx.r;
            rgba[i * 4 + 1] = tx.g;
            rgba[i * 4 + 2] = tx.b;
            rgba[i * 4 + 3] = tx.a;
        }
        return uploadRgba(rgba.data(), (int)t->mWidth, (int)t->mHeight, false);
    }

    GLuint loadTextureOfTypes(const aiScene* sc, const aiMaterial* mat,
                              const std::string& modelDir,
                              const aiTextureType* types, int nTypes,
                              std::string* outMissing = nullptr) {
        if (!mat) return 0;
        aiString texPath;
        for (int t = 0; t < nTypes; ++t) {
            aiTextureType ty = types[t];
            if (mat->GetTextureCount(ty) == 0) continue;
            if (mat->GetTexture(ty, 0, &texPath) != AI_SUCCESS) continue;
            const char* p = texPath.C_Str();
            if (!p || !p[0]) continue;
            if (p[0] == '*') {
                GLuint id = loadEmbeddedTexture(sc, p);
                if (id) return id;
            } else {
                if (const aiTexture* emb = sc->GetEmbeddedTexture(p)) {
                    for (unsigned i = 0; i < sc->mNumTextures; ++i) {
                        if (sc->mTextures[i] != emb) continue;
                        char hint[16];
                        std::snprintf(hint, sizeof(hint), "*%u", i);
                        if (GLuint id = loadEmbeddedTexture(sc, hint)) return id;
                        break;
                    }
                }
                std::string just = p;
                auto slash = just.find_last_of("\\/");
                if (slash != std::string::npos)
                    just = just.substr(slash + 1);
                std::string parent = modelDir;
                auto up = parent.find_last_of("\\/");
                if (up != std::string::npos) parent = parent.substr(0, up);
                const char* subdirs[] = { "", "textures/", "Textures/", "tex/",
                                          "maps/", "Materials/", "materials/",
                                          "source/", "images/" };
                std::vector<std::string> candidates = { modelDir + "/" + p, std::string(p) };
                for (const char* sd : subdirs) {
                    candidates.push_back(modelDir + "/" + sd + just);
                    candidates.push_back(parent + "/" + sd + just);
                }
                for (const auto& c : candidates) {
                    GLuint id = loadTextureFile(c, true);
                    if (id) return id;
                }
                if (outMissing && outMissing->empty()) *outMissing = just;
                std::fprintf(stderr, "[StaticMesh] texture non trovata: %s\n", p);
            }
        }
        return 0;
    }

    GLuint loadMaterialTexture(const aiScene* sc, const aiMaterial* mat,
                               const std::string& modelDir, std::string* outMissing = nullptr) {
        static const aiTextureType types[] = {
            aiTextureType_BASE_COLOR,
            aiTextureType_DIFFUSE,
            aiTextureType_UNKNOWN,
        };
        return loadTextureOfTypes(sc, mat, modelDir, types, 3, outMissing);
    }

    GLuint loadEmissiveTexture(const aiScene* sc, const aiMaterial* mat,
                               const std::string& modelDir) {
        static const aiTextureType types[] = {
            aiTextureType_EMISSIVE,
            aiTextureType_EMISSION_COLOR,
        };
        return loadTextureOfTypes(sc, mat, modelDir, types, 2, nullptr);
    }

    struct Gathered {
        const aiMesh* mesh = nullptr;
        glm::mat4 xform{1.f};
        std::string name;
        float volume = 0.f;
    };

    void collectMeshes(const aiScene* sc, const aiNode* node, const glm::mat4& parent,
                       std::vector<Gathered>& out) {
        if (!node) return;
        glm::mat4 xform = parent * aiMat(node->mTransformation);
        for (unsigned i = 0; i < node->mNumMeshes; ++i) {
            unsigned mi = node->mMeshes[i];
            if (mi >= sc->mNumMeshes) continue;
            const aiMesh* mesh = sc->mMeshes[mi];
            if (!mesh || !mesh->HasPositions() || mesh->mNumFaces == 0) continue;

            std::string name = mesh->mName.C_Str();
            if (name.empty()) name = node->mName.C_Str();
            if (!exactImport
                && (isJunkMeshName(name) || isJunkMeshName(node->mName.C_Str())))
                continue;

            glm::vec3 bmin(1e9f), bmax(-1e9f);
            for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
                glm::vec4 wp = xform * glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y,
                                                 mesh->mVertices[v].z, 1.f);
                bmin = glm::min(bmin, glm::vec3(wp));
                bmax = glm::max(bmax, glm::vec3(wp));
            }
            glm::vec3 sz = bmax - bmin;
            float vol = std::max(sz.x, 0.001f) * std::max(sz.y, 0.001f) * std::max(sz.z, 0.001f);

            Gathered g;
            g.mesh = mesh;
            g.xform = xform;
            g.name = name;
            g.volume = vol;
            out.push_back(g);
        }
        for (unsigned c = 0; c < node->mNumChildren; ++c)
            collectMeshes(sc, node->mChildren[c], xform, out);
    }

    void buildSubmesh(const Gathered& g, const aiScene* sc, const std::string& modelDir,
                      const glm::mat4& xformWorld, std::vector<SubMesh>& outSubs,
                      glm::vec3& bmin, glm::vec3& bmax) {
        const aiMesh* mesh = g.mesh;
        if (!mesh || !mesh->mVertices || mesh->mNumFaces == 0) return;
        glm::mat4 xform = xformWorld * g.xform;
        glm::mat3 nmat = glm::mat3(glm::transpose(glm::inverse(xform)));

        const int kStride = 11;
        const bool hasVCol = mesh->HasVertexColors(0);

        std::vector<float> v;
        v.reserve((size_t)mesh->mNumFaces * 3 * kStride);

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (unsigned k = 0; k < 3; ++k) {
                unsigned idx = face.mIndices[k];
                if (idx >= mesh->mNumVertices) continue;
                aiVector3D p = mesh->mVertices[idx];
                glm::vec4 wp = xform * glm::vec4(p.x, p.y, p.z, 1.f);
                aiVector3D an = mesh->HasNormals() ? mesh->mNormals[idx] : aiVector3D(0, 1, 0);
                glm::vec3 wn = nmat * glm::vec3(an.x, an.y, an.z);
                float len = glm::length(wn);
                if (len > 1e-6f) wn /= len; else wn = {0, 1, 0};
                float u = 0.f, vv = 0.f;
                if (mesh->HasTextureCoords(0)) {
                    u = mesh->mTextureCoords[0][idx].x;
                    vv = mesh->mTextureCoords[0][idx].y;
                }
                glm::vec3 vc{1.f};
                if (hasVCol) {
                    const aiColor4D& cc = mesh->mColors[0][idx];
                    vc = {cc.r, cc.g, cc.b};
                }
                v.push_back(wp.x); v.push_back(wp.y); v.push_back(wp.z);
                v.push_back(wn.x); v.push_back(wn.y); v.push_back(wn.z);
                v.push_back(u); v.push_back(vv);
                v.push_back(vc.r); v.push_back(vc.g); v.push_back(vc.b);
                bmin = glm::min(bmin, glm::vec3(wp));
                bmax = glm::max(bmax, glm::vec3(wp));
            }
        }
        if (v.empty()) return;

        SubMesh sm;
        sm.name = g.name;
        sm.vertexCount = (int)(v.size() / kStride);
        sm.baseColor = {1.f, 1.f, 1.f};

        if (sc->HasMaterials() && mesh->mMaterialIndex < sc->mNumMaterials) {
            const aiMaterial* mat = sc->mMaterials[mesh->mMaterialIndex];
            aiString mname;
            if (mat->Get(AI_MATKEY_NAME, mname) == AI_SUCCESS)
                sm.matName = mname.C_Str();

            aiColor4D c(1, 1, 1, 1);
            if (mat->Get(AI_MATKEY_BASE_COLOR, c) == AI_SUCCESS
                || mat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS) {
                sm.baseColor = {c.r, c.g, c.b};
                sm.alpha = c.a;
            }
            float opacity = 1.f;
            if (mat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
                sm.alpha *= opacity;
            if (sm.alpha <= 0.f || sm.alpha > 1.f) sm.alpha = 1.f;

            aiColor4D em(0, 0, 0, 1);
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, em) == AI_SUCCESS)
                sm.emissive = {em.r, em.g, em.b};

            float mf = 0.f, rf = 0.f;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR, mf) == AI_SUCCESS)
                sm.metallic = std::clamp(mf, 0.f, 1.f);
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, rf) == AI_SUCCESS)
                sm.roughness = std::clamp(rf, 0.f, 1.f);
            else {
                float shin = 0.f;
                if (mat->Get(AI_MATKEY_SHININESS, shin) == AI_SUCCESS && shin > 0.f)
                    sm.roughness = std::clamp(1.f - std::sqrt(shin / 1000.f), 0.05f, 1.f);
            }

            int twoSided = 0;
            if (mat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
                sm.twoSided = twoSided != 0;

            sm.texId = loadMaterialTexture(sc, mat, modelDir, &sm.missingTex);
            sm.ownsTex = (sm.texId != 0);
            sm.emissiveTex = loadEmissiveTexture(sc, mat, modelDir);
            sm.ownsEmissiveTex = (sm.emissiveTex != 0);
        }

        glGenVertexArrays(1, &sm.vao);
        glGenBuffers(1, &sm.vbo);
        glBindVertexArray(sm.vao);
        glBindBuffer(GL_ARRAY_BUFFER, sm.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
        const GLsizei stride = kStride * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glBindVertexArray(0);

        outSubs.push_back(std::move(sm));
    }

    bool load(const std::string& file, float targetMaxDim = 1.6f) {
        clear();
        Assimp::Importer imp;
        imp.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
            aiPrimitiveType_POINT | aiPrimitiveType_LINE);
        imp.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
        imp.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 250000);
        imp.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 500000);

        const unsigned flags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_FlipUVs |
            aiProcess_SplitLargeMeshes;

        const aiScene* sc = nullptr;
        try {
            sc = imp.ReadFile(file, flags);
            if (!sc || !sc->mRootNode) {
                std::vector<char> bytes;
                if (!readFileBytes(file, bytes)) {
                    std::fprintf(stderr, "StaticMesh cannot open: %s\n", file.c_str());
                    return false;
                }
                if (bytes.size() > 120u * 1024u * 1024u) {
                    std::fprintf(stderr, "StaticMesh troppo grande: %s\n", file.c_str());
                    return false;
                }
                std::string hint = extensionOf(file);
                sc = imp.ReadFileFromMemory(
                    bytes.data(), bytes.size(), flags, hint.empty() ? nullptr : hint.c_str());
            }
        } catch (...) {
            std::fprintf(stderr, "StaticMesh exception: %s\n", file.c_str());
            clear();
            return false;
        }
        if (!sc || !sc->mRootNode || sc->mNumMeshes == 0) {
            std::fprintf(stderr, "StaticMesh load fail: %s (%s)\n", file.c_str(), imp.GetErrorString());
            return false;
        }

        hasBones = false;
        if (sc->mNumAnimations > 0) hasBones = true;
        for (unsigned i = 0; i < sc->mNumMeshes && !hasBones; ++i) {
            if (sc->mMeshes[i] && sc->mMeshes[i]->mNumBones > 0)
                hasBones = true;
        }

        std::vector<Gathered> gathered;
        collectMeshes(sc, sc->mRootNode, glm::mat4(1.f), gathered);
        if (gathered.empty()) {
            for (unsigned mi = 0; mi < sc->mNumMeshes; ++mi) {
                Gathered g;
                g.mesh = sc->mMeshes[mi];
                g.name = g.mesh ? g.mesh->mName.C_Str() : "";
                g.xform = glm::mat4(1.f);
                g.volume = 1.f;
                if (g.mesh && g.mesh->HasPositions())
                    gathered.push_back(g);
            }
        }
        if (gathered.empty()) {
            std::fprintf(stderr, "StaticMesh empty: %s\n", file.c_str());
            return false;
        }

        std::vector<Gathered> keep;
        if (exactImport) {
            keep = gathered;
        } else {
            float maxVol = 0.f;
            for (const auto& g : gathered) maxVol = std::max(maxVol, g.volume);
            for (const auto& g : gathered) {
                if (maxVol > 0.f && g.volume < maxVol * 0.008f) {
                    std::printf("[StaticMesh] skip tiny '%s'\n", g.name.c_str());
                    continue;
                }
                keep.push_back(g);
            }
            if (keep.empty()) keep = gathered;
        }

        std::string ext = extensionOf(file);
        glm::mat4 axisCorr(1.f);
        if (ext == ".fbx")
            axisCorr = glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1, 0, 0));

        glm::vec3 rawMin(1e9f), rawMax(-1e9f);
        for (const auto& g : keep) {
            glm::mat4 x = axisCorr * g.xform;
            for (unsigned vi = 0; vi < g.mesh->mNumVertices; ++vi) {
                const auto& p = g.mesh->mVertices[vi];
                glm::vec4 wp = x * glm::vec4(p.x, p.y, p.z, 1.f);
                rawMin = glm::min(rawMin, glm::vec3(wp));
                rawMax = glm::max(rawMax, glm::vec3(wp));
            }
        }
        glm::vec3 size = rawMax - rawMin;
        float maxDim = std::max(size.x, std::max(size.y, size.z));
        if (maxDim < 1e-5f) maxDim = 1.f;
        float scl = targetMaxDim / maxDim;
        glm::vec3 center = (rawMin + rawMax) * 0.5f;
        glm::mat4 norm = glm::scale(glm::mat4(1.f), glm::vec3(scl))
                       * glm::translate(glm::mat4(1.f), -center)
                       * axisCorr;

        std::string modelDir = dirOf(file);
        boundsMin = glm::vec3(1e9f);
        boundsMax = glm::vec3(-1e9f);
        try {
            int builtVerts = 0;
            for (const auto& g : keep) {
                if (!g.mesh) continue;
                if (builtVerts > 400000) {
                    std::fprintf(stderr, "StaticMesh: troppo denso, stop early\n");
                    break;
                }
                size_t before = subs.size();
                buildSubmesh(g, sc, modelDir, norm, subs, boundsMin, boundsMax);
                if (subs.size() > before)
                    builtVerts += subs.back().vertexCount;
            }
        } catch (...) {
            std::fprintf(stderr, "StaticMesh build exception: %s\n", file.c_str());
            clear();
            return false;
        }

        if (subs.empty()) {
            std::fprintf(stderr, "StaticMesh no usable faces: %s\n", file.c_str());
            return false;
        }

        vertexCount = 0;
        int withTex = 0;
        for (const auto& sm : subs) {
            vertexCount += sm.vertexCount;
            if (sm.texId) ++withTex;
        }
        vao = subs[0].vao;
        vbo = subs[0].vbo;
        path = file;
        loaded = true;
        std::printf("[StaticMesh] OK '%s' subs=%d verts=%d textured=%d/%d mancanti=%d size=(%.2f,%.2f,%.2f)\n",
                    file.c_str(), (int)subs.size(), vertexCount, withTex, (int)subs.size(),
                    missingTextureCount(),
                    boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z);
        return true;
    }

    int missingTextureCount() const {
        int n = 0;
        for (const auto& s : subs)
            if (!s.missingTex.empty()) ++n;
        return n;
    }

    std::string firstMissingTexture() const {
        for (const auto& s : subs)
            if (!s.missingTex.empty()) return s.missingTex;
        return {};
    }

    void draw() const {
        if (!loaded) return;
        for (const auto& sm : subs) {
            if (!sm.vao || sm.vertexCount <= 0) continue;
            glBindVertexArray(sm.vao);
            glDrawArrays(GL_TRIANGLES, 0, sm.vertexCount);
        }
        glBindVertexArray(0);
    }

    void drawTextured(GLuint prog, const glm::vec3& tintMul, float alpha = 1.f) const {
        if (!loaded) return;
        GLint locColor = glGetUniformLocation(prog, "uColor");
        GLint locUseTex = glGetUniformLocation(prog, "uUseTex");
        GLint locTex = glGetUniformLocation(prog, "uTex");
        GLint locAlpha = glGetUniformLocation(prog, "uAlpha");
        GLint locEmCol = glGetUniformLocation(prog, "uEmissiveCol");
        GLint locEmTex = glGetUniformLocation(prog, "uEmissiveTex");
        GLint locUseEm = glGetUniformLocation(prog, "uUseEmissiveTex");
        GLint locSpec = glGetUniformLocation(prog, "uSpecular");
        GLint locRough = glGetUniformLocation(prog, "uRoughness");
        if (locTex >= 0) glUniform1i(locTex, 0);
        if (locEmTex >= 0) glUniform1i(locEmTex, 1);

        const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        bool cullOff = false;

        for (const auto& sm : subs) {
            if (!sm.vao || sm.vertexCount <= 0) continue;

            if (sm.twoSided && !cullOff) { glDisable(GL_CULL_FACE); cullOff = true; }
            else if (!sm.twoSided && cullOff) {
                if (cullWas) glEnable(GL_CULL_FACE);
                cullOff = false;
            }

            glm::vec3 col = sm.baseColor * tintMul;
            if (locColor >= 0) glUniform3fv(locColor, 1, glm::value_ptr(col));
            if (locAlpha >= 0) glUniform1f(locAlpha, alpha * sm.alpha);
            if (locEmCol >= 0) glUniform3fv(locEmCol, 1, glm::value_ptr(sm.emissive));
            if (locSpec >= 0)
                glUniform1f(locSpec, (0.04f + sm.metallic * 0.60f)
                                     * (1.f - sm.roughness * 0.85f));
            if (locRough >= 0) glUniform1f(locRough, sm.roughness);

            bool use = sm.texId != 0;
            if (locUseTex >= 0) glUniform1i(locUseTex, use ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, use ? sm.texId : 0);

            bool useEm = sm.emissiveTex != 0;
            if (locUseEm >= 0) glUniform1i(locUseEm, useEm ? 1 : 0);
            if (locEmTex >= 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, useEm ? sm.emissiveTex : 0);
                glActiveTexture(GL_TEXTURE0);
            }

            glBindVertexArray(sm.vao);
            glDrawArrays(GL_TRIANGLES, 0, sm.vertexCount);
        }

        if (cullOff && cullWas) glEnable(GL_CULL_FACE);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};
