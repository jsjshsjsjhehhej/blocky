#pragma once

#include "StaticMesh.h"
#include "engine/Humanoid.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include "engine/GameHub.h"
#include "engine/R6WalkAnim.h"

enum class AvatarSlot : int {
    Head = 0,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Count
};

inline const char* avatarSlotName(AvatarSlot s) {
    switch (s) {
    case AvatarSlot::Head: return "Head";
    case AvatarSlot::Torso: return "Torso";
    case AvatarSlot::LeftArm: return "Left Arm";
    case AvatarSlot::RightArm: return "Right Arm";
    case AvatarSlot::LeftLeg: return "Left Leg";
    case AvatarSlot::RightLeg: return "Right Leg";
    default: return "?";
    }
}

inline const char* humanoidPartForSlot(AvatarSlot s) {
    switch (s) {
    case AvatarSlot::Head: return "Head";
    case AvatarSlot::Torso: return "Torso";
    case AvatarSlot::LeftArm: return "Left Arm";
    case AvatarSlot::RightArm: return "Right Arm";
    case AvatarSlot::LeftLeg: return "Left Leg";
    case AvatarSlot::RightLeg: return "Right Leg";
    default: return "Torso";
    }
}

enum class AccelTool { Move, Rotate, Scale };

struct AvatarAccessory {
    AvatarSlot slot = AvatarSlot::Head;
    std::string name = "Accessory";
    std::string sourcePath;
    StaticMesh mesh;
    glm::vec3 offset{0, 0, 0};
    glm::vec3 rotation{0, 0, 0};
    glm::vec3 scale{1, 1, 1};
    glm::vec3 tint{1.f, 1.f, 1.f};
    bool replacesBody = false;
};

struct AvatarCatalogState {
    std::vector<AvatarAccessory> items;
    int selected = -1;

    bool hideDefaultPart[(int)AvatarSlot::Count] = {};

    bool open = false;
    int wizardStep = 0;
    AvatarSlot pendingSlot = AvatarSlot::Head;
    bool pendingAsBodyReplace = false;
    AccelTool tool = AccelTool::Move;

    float camYaw = 25.f;
    float camPitch = -18.f;
    float camDist = 14.f;
    bool orbiting = false;

    std::string status;
    std::string pendingImportPath;

    std::vector<R6WalkClip> walkClips;
    int walkSelected = -1;
    std::string equippedWalkId;
    R6WalkClip pendingWalk;
    bool hasPendingWalk = false;
    char walkNameBuf[96] = {};
    char walkClipPrefBuf[96] = "WalkingAnimation";
    bool previewWalk = false;
    bool pendingAsWalkDefault = false;
    std::vector<std::string> pendingClipNames;
};

inline bool openModelFileDialog(std::string& outPath, void* hwndOwner = nullptr) {
#ifdef _WIN32
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFilter =
        L"Modelli 3D (*.glb;*.gltf;*.fbx;*.obj)\0*.glb;*.gltf;*.fbx;*.obj\0"
        L"Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Blocky — Aggiungi modello avatar";
    ofn.lpstrDefExt = L"glb";
    if (!GetOpenFileNameW(&ofn)) return false;

    int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return false;
    outPath.resize((size_t)n - 1);
    WideCharToMultiByte(CP_UTF8, 0, file, -1, outPath.data(), n, nullptr, nullptr);
    return true;
#else
    (void)outPath;
    (void)hwndOwner;
    return false;
#endif
}

inline glm::vec3 defaultOffsetForSlot(AvatarSlot s) {
    switch (s) {
    case AvatarSlot::Head: return {0, 1.1f, 0};
    case AvatarSlot::Torso: return {0, 0.15f, 0.85f};
    case AvatarSlot::LeftArm: return {-0.85f, 0.f, 0};
    case AvatarSlot::RightArm: return {0.85f, 0.f, 0};
    case AvatarSlot::LeftLeg: return {0, -0.2f, 0.55f};
    case AvatarSlot::RightLeg: return {0, -0.2f, 0.55f};
    default: return {0, 0, 0};
    }
}

inline glm::vec3 bodyReplaceOffsetForSlot(AvatarSlot) {
    return {0.f, 0.f, 0.f};
}

inline int avatarSlotIndexForPartName(const std::string& name) {
    if (name == "Head") return (int)AvatarSlot::Head;
    if (name == "Torso") return (int)AvatarSlot::Torso;
    if (name == "Left Arm") return (int)AvatarSlot::LeftArm;
    if (name == "Right Arm") return (int)AvatarSlot::RightArm;
    if (name == "Left Leg") return (int)AvatarSlot::LeftLeg;
    if (name == "Right Leg") return (int)AvatarSlot::RightLeg;
    return -1;
}

inline bool catalogHidesBodyPart(const AvatarCatalogState& cat, const std::string& partName) {
    int si = avatarSlotIndexForPartName(partName);
    if (si < 0) return false;
    if (cat.hideDefaultPart[si]) return true;
    for (const auto& a : cat.items) {
        if ((int)a.slot == si && a.replacesBody)
            return true;
    }
    return false;
}

inline void fillBodyHideMask(const AvatarCatalogState& cat, bool out[(int)AvatarSlot::Count]) {
    for (int i = 0; i < (int)AvatarSlot::Count; ++i) {
        out[i] = cat.hideDefaultPart[i];
        for (const auto& a : cat.items) {
            if ((int)a.slot == i && a.replacesBody) {
                out[i] = true;
                break;
            }
        }
    }
}

inline glm::vec3 findPartCenter(const Humanoid& h, const char* name, float bob = 0.f) {
    for (const auto& p : h.parts) {
        if (p.name == name) {
            glm::vec3 c = p.offset;
            if (p.name == "Torso" || p.name == "Head") c.y += bob;
            return c;
        }
    }
    return {0, 3, 0};
}

inline glm::mat4 accessoryPartMatrix(const AvatarAccessory& a, const Humanoid& h,
                                     float bob, float armL, float armR, float legL, float legR) {
    const char* partName = humanoidPartForSlot(a.slot);
    glm::vec3 partCenter = findPartCenter(h, partName, bob);
    glm::vec3 pivot{0};
    float rx = 0.f;
    for (const auto& p : h.parts) {
        if (p.name != partName) continue;
        pivot = p.pivot;
        if (p.name == "Left Arm") rx = armL;
        else if (p.name == "Right Arm") rx = armR;
        else if (p.name == "Left Leg") rx = legL;
        else if (p.name == "Right Leg") rx = legR;
        break;
    }
    glm::mat4 part = glm::translate(glm::mat4(1.f), partCenter + pivot);
    if (rx != 0.f) part = glm::rotate(part, glm::radians(rx), {1, 0, 0});
    part = glm::translate(part, -pivot);
    return part;
}

inline glm::mat4 accessoryLocalMatrix(const AvatarAccessory& a) {
    glm::mat4 local = glm::translate(glm::mat4(1.f), a.offset);
    local = glm::rotate(local, glm::radians(a.rotation.y), {0, 1, 0});
    local = glm::rotate(local, glm::radians(a.rotation.x), {1, 0, 0});
    local = glm::rotate(local, glm::radians(a.rotation.z), {0, 0, 1});
    local = glm::scale(local, a.scale);
    return local;
}

inline glm::mat4 accessoryWorldMatrix(const AvatarAccessory& a, const Humanoid& h,
                                      float bob, float armL, float armR, float legL, float legR) {
    return accessoryPartMatrix(a, h, bob, armL, armR, legL, legR) * accessoryLocalMatrix(a);
}

inline std::string avatarJsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else o.push_back((char)c);
    }
    return o;
}

inline std::string avatarPathSlash(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

inline std::string avatarSaveFilePath() {
    return blockyDataDir() + "/avatar.json";
}

inline std::string blockyModelsDir() {
    std::string d = blockyDataDir() + "/models";
    _mkdir(d.c_str());
    return d;
}

inline std::string copyModelToLibrary(const std::string& srcPath, const std::string& displayName) {
    if (srcPath.empty()) return {};
    std::string base;
    for (char c : displayName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            base.push_back(c);
        else if (c == ' ')
            base.push_back('_');
    }
    if (base.empty()) base = "model";
    if (base.size() > 40) base.resize(40);

    std::string ext = ".glb";
    auto dot = srcPath.find_last_of('.');
    if (dot != std::string::npos)
        ext = srcPath.substr(dot);

    std::string dest = blockyModelsDir() + "/" + base + ext;
#ifdef _WIN32
    std::string src = srcPath;
    for (char& c : src) if (c == '/') c = '\\';
    std::string dstWin = dest;
    for (char& c : dstWin) if (c == '/') c = '\\';
    if (src != dstWin)
        CopyFileA(src.c_str(), dstWin.c_str(), FALSE);
#else
    return srcPath;
#endif
    return avatarPathSlash(dest);
}

inline bool saveEquippedAvatar(const AvatarCatalogState& cat) {
    std::ostringstream j;
    j << "{\n  \"hideDefaultPart\":[";
    for (int i = 0; i < (int)AvatarSlot::Count; ++i) {
        if (i) j << ",";
        j << (cat.hideDefaultPart[i] ? "true" : "false");
    }
    j << "],\n  \"accessories\": [\n";
    bool first = true;
    for (const auto& a : cat.items) {
        if (!first) j << ",\n";
        first = false;
        j << "    {"
          << "\"name\":\"" << avatarJsonEscape(a.name) << "\","
          << "\"sourcePath\":\"" << avatarJsonEscape(avatarPathSlash(a.sourcePath)) << "\","
          << "\"slot\":" << (int)a.slot << ","
          << "\"replacesBody\":" << (a.replacesBody ? "true" : "false") << ","
          << "\"offset\":[" << a.offset.x << "," << a.offset.y << "," << a.offset.z << "],"
          << "\"rotation\":[" << a.rotation.x << "," << a.rotation.y << "," << a.rotation.z << "],"
          << "\"scale\":[" << a.scale.x << "," << a.scale.y << "," << a.scale.z << "],"
          << "\"tint\":[" << a.tint.x << "," << a.tint.y << "," << a.tint.z << "]"
          << "}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(avatarSaveFilePath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool loadEquippedAvatar(AvatarCatalogState& cat) {
    std::string json = readTextFile(avatarSaveFilePath());
    if (json.empty()) return false;

    cat.items.clear();
    cat.selected = -1;
    for (int i = 0; i < (int)AvatarSlot::Count; ++i)
        cat.hideDefaultPart[i] = false;

    size_t hideKey = json.find("\"hideDefaultPart\"");
    if (hideKey != std::string::npos) {
        size_t arr = json.find('[', hideKey);
        size_t arrEnd = json.find(']', arr);
        if (arr != std::string::npos && arrEnd != std::string::npos) {
            int idx = 0;
            size_t p = arr;
            while (idx < (int)AvatarSlot::Count && p < arrEnd) {
                size_t t = json.find("true", p);
                size_t f = json.find("false", p);
                if (t != std::string::npos && t < arrEnd && (f == std::string::npos || t < f)) {
                    cat.hideDefaultPart[idx++] = true;
                    p = t + 4;
                } else if (f != std::string::npos && f < arrEnd) {
                    cat.hideDefaultPart[idx++] = false;
                    p = f + 5;
                } else break;
            }
        }
    }

    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
        AvatarAccessory a;
        a.name = jsonFindStr(json, "name", pos);
        a.sourcePath = jsonFindStr(json, "sourcePath", pos);
        a.slot = (AvatarSlot)jsonFindInt(json, "slot", pos);
        a.replacesBody = jsonFindBool(json, "replacesBody", pos, false);
        glm::vec3 off{}, rot{}, sc{1, 1, 1}, tint{1.f, 1.f, 1.f};
        jsonFindVec3(json, "offset", pos, off);
        jsonFindVec3(json, "rotation", pos, rot);
        jsonFindVec3(json, "scale", pos, sc);
        jsonFindVec3(json, "tint", pos, tint);
        a.offset = off;
        a.rotation = rot;
        a.scale = sc;
        a.tint = tint;
        if (a.scale.x == 0 && a.scale.y == 0 && a.scale.z == 0)
            a.scale = {1, 1, 1};

        if (!a.sourcePath.empty() && a.mesh.load(a.sourcePath)) {
            cat.items.push_back(std::move(a));
        } else if (!a.name.empty()) {
            std::fprintf(stderr, "[Blocky] Avatar item non caricato: %s\n", a.sourcePath.c_str());
        }
        pos += 6;
    }
    if (!cat.items.empty())
        cat.selected = 0;
    return true;
}
