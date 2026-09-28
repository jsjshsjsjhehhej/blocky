#pragma once


#include "R6WalkAnim.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct AnimLibrary {
    std::map<std::string, R6WalkClip> byId;
    std::string defaultWalkId;
    std::string defaultJumpId;

    bool has(const std::string& id) const {
        return byId.find(id) != byId.end() && byId.at(id).valid();
    }

    const R6WalkClip* get(const std::string& id) const {
        auto it = byId.find(id);
        if (it == byId.end() || !it->second.valid()) return nullptr;
        return &it->second;
    }

    R6WalkClip* getMut(const std::string& id) {
        auto it = byId.find(id);
        if (it == byId.end() || !it->second.valid()) return nullptr;
        return &it->second;
    }

    static std::string resolveClipPath(const std::string& id, const std::string& stored) {
        if (!stored.empty()) {
            R6WalkClip tmp;
            if (r6LoadClipFile(stored, tmp)) return stored;
        }
        std::string modern = r6ClipFilePath(id);
        R6WalkClip c;
        if (r6LoadClipFile(modern, c)) return modern;
        std::string legacy = r6AnimsDir() + "/" + id + ".walk.json";
        if (r6LoadClipFile(legacy, c)) return legacy;
        return modern;
    }

    void addAliases(const R6WalkClip& clip, const std::string& animId) {
        byId[clip.id] = clip;
        if (clip.id != animId)
            byId[animId] = byId[clip.id];
        if (!clip.clipName.empty() && clip.clipName != animId && clip.clipName != clip.id)
            byId[clip.clipName] = byId[clip.id];
        std::string clipSan = sanitizeGameId(clip.clipName);
        if (!clipSan.empty() && clipSan != clip.id)
            byId[clipSan] = byId[clip.id];
    }

    bool registerClip(const std::string& animId, R6WalkClip clip) {
        if (animId.empty() || !clip.valid()) return false;
        clip.id = sanitizeGameId(animId);
        if (clip.displayName.empty()) clip.displayName = animId;
        if (clip.kind == R6AnimKind::Walk) {
            R6AnimKind guessed = r6AnimKindFromName(animId + " " + clip.clipName + " " + clip.displayName);
            if (guessed != R6AnimKind::Walk) {
                clip.kind = guessed;
                if (guessed == R6AnimKind::Jump) clip.timeInSeconds = true;
            }
        }
        std::string path;
        if (!r6SaveClipFile(clip, path)) return false;
        clip.sourcePath = path;
        addAliases(clip, animId);
        persistIndex();
        return true;
    }

    const R6WalkClip* resolve(const std::string& raw) const {
        if (raw.empty()) return nullptr;
        if (const R6WalkClip* c = get(raw)) return c;
        std::string san = sanitizeGameId(raw);
        if (const R6WalkClip* c = get(san)) return c;
        auto pipe = raw.find('|');
        if (pipe != std::string::npos) {
            std::string a = raw.substr(0, pipe);
            std::string b = raw.substr(pipe + 1);
            if (const R6WalkClip* c = get(a)) return c;
            if (const R6WalkClip* c = get(b)) return c;
            if (const R6WalkClip* c = get(sanitizeGameId(a))) return c;
            if (const R6WalkClip* c = get(sanitizeGameId(b))) return c;
            if (const R6WalkClip* c = get(sanitizeGameId(a + b))) return c;
        }
        for (const auto& kv : byId) {
            if (kv.second.clipName == raw || kv.second.displayName == raw)
                return &kv.second;
            if (!kv.second.clipName.empty()
                && kv.second.clipName.find(raw) != std::string::npos)
                return &kv.second;
        }
        return nullptr;
    }

    void setDefaultWalk(const std::string& animId) {
        if (has(animId) || has(sanitizeGameId(animId)))
            defaultWalkId = has(animId) ? animId : sanitizeGameId(animId);
        persistIndex();
    }

    void setDefaultJump(const std::string& animId) {
        if (has(animId) || has(sanitizeGameId(animId)))
            defaultJumpId = has(animId) ? animId : sanitizeGameId(animId);
        persistIndex();
    }

    void seedBuiltins() {
        bool dirty = false;
        auto upsert = [&](R6WalkClip clip) {
            std::string path;
            if (!r6SaveClipFile(clip, path)) return;
            clip.sourcePath = path;
            byId[clip.id] = clip;
            dirty = true;
        };
        upsert(r6MakeClassicWalkClip());
        upsert(r6MakeClassicJumpClip());
        if (defaultJumpId.empty() || defaultJumpId == "classic_jump") {
            defaultJumpId = "classic_jump";
            dirty = true;
        }
        if (defaultWalkId.empty()) {
            defaultWalkId = "classic_walk";
            dirty = true;
        }
        if (dirty) persistIndex();
    }

    void persistIndex() const {
        std::ostringstream j;
        j << "{\n  \"defaultWalkId\": \"" << r6Escape(defaultWalkId) << "\",\n"
          << "  \"defaultJumpId\": \"" << r6Escape(defaultJumpId) << "\",\n"
          << "  \"anims\": [\n";
        bool first = true;
        std::map<std::string, bool> seen;
        for (const auto& kv : byId) {
            if (seen[kv.second.id]) continue;
            seen[kv.second.id] = true;
            std::string path = kv.second.sourcePath;
            if (path.empty() || path.size() > 400)
                path = r6ClipFilePath(kv.second.id);
            if (!first) j << ",\n";
            first = false;
            j << "    {\"id\":\"" << r6Escape(kv.second.id)
              << "\",\"displayName\":\"" << r6Escape(kv.second.displayName)
              << "\",\"kind\":\"" << r6AnimKindName(kv.second.kind)
              << "\",\"path\":\"" << r6Escape(path) << "\"}";
        }
        j << "\n  ]\n}\n";
        std::ofstream f(blockyDataDir() + "/anim_library.json");
        if (f) f << j.str();
    }

    bool loadIndex() {
        byId.clear();
        defaultWalkId.clear();
        defaultJumpId.clear();
        std::string json = readTextFile(blockyDataDir() + "/anim_library.json");
        if (json.empty()) {
            std::vector<R6WalkClip> legacy;
            std::string eq;
            if (r6LoadLibrary(legacy, eq)) {
                for (auto& c : legacy)
                    byId[c.id] = c;
                defaultWalkId = eq;
            }
            seedBuiltins();
            if (!byId.empty()) persistIndex();
            return !byId.empty();
        }
        defaultWalkId = jsonFindStr(json, "defaultWalkId");
        defaultJumpId = jsonFindStr(json, "defaultJumpId");
        size_t pos = 0;
        while ((pos = json.find("\"path\"", pos)) != std::string::npos) {
            std::string path = r6Unescape(jsonFindStr(json, "path", pos));
            std::string id = jsonFindStr(json, "id", pos > 40 ? pos - 40 : 0);
            std::string kindStr = jsonFindStr(json, "kind", pos > 80 ? pos - 80 : 0);
            R6WalkClip c;
            std::string tryPath = resolveClipPath(id, path);
            if (!tryPath.empty() && r6LoadClipFile(tryPath, c)) {
                if (!id.empty()) c.id = id;
                if (!kindStr.empty()) c.kind = r6AnimKindFromName(kindStr);
                c.sourcePath = tryPath;
                byId[c.id] = c;
            }
            pos += 6;
        }
        seedBuiltins();
        return !byId.empty();
    }

    std::vector<std::string> listIds() const {
        std::vector<std::string> ids;
        std::map<std::string, bool> seen;
        for (const auto& kv : byId) {
            if (seen[kv.second.id]) continue;
            seen[kv.second.id] = true;
            ids.push_back(kv.second.displayName.empty() ? kv.second.id : kv.second.displayName);
        }
        return ids;
    }
};

inline AnimLibrary& animLibrary() {
    static AnimLibrary lib;
    return lib;
}
