#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "DataModel.h"
#include "Humanoid.h"
#include "LightingService.h"

#include "../platform/Platform.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <direct.h>
#else
#include <sys/stat.h>
static inline void _mkdir(const char* p) { mkdir(p, 0755); }
#endif

struct GameEntry {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string script;
    int playersOnline = 0;
    glm::vec3 accent{0.2f, 0.62f, 0.35f};
    bool isPublished = false;
    std::string placePath;
    std::string creator;
    int maxPlayers = 8;
    long long publishedAt = 0;
    ScriptType scriptType = ScriptType::Legacy;
    std::string iconPath; // vuoto = usa il colore accent come prima
    // Viene da un server e non da questo dispositivo: va scaricato prima di
    // poterci giocare.
    bool remote = false;
};

inline constexpr int kMaxPublishedGamesPerUser = 10;
inline constexpr long long kPublishQuotaWindowSecs = 60LL * 24 * 3600;

inline std::string blockyDataDir() {
    std::string dir = plat::dataDir();
    plat::makeDirs(dir + "/published");
    return dir;
}

inline std::string assetRoot() {
    return plat::selfDir();
}

inline std::string readTextFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::string jsonFindStr(const std::string& json, const std::string& key, size_t from = 0) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return {};
    auto q1 = json.find('"', json.find(':', k) + 1);
    if (q1 == std::string::npos) return {};
    auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

inline std::string jsonFindEscapedStr(const std::string& json, const std::string& key, size_t from = 0) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return {};
    auto c = json.find(':', k);
    if (c == std::string::npos) return {};
    auto q1 = json.find('"', c);
    if (q1 == std::string::npos) return {};
    std::string out;
    size_t i = q1 + 1;
    while (i < json.size() && json[i] != '"') {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char nc = json[i + 1];
            if (nc == 'n') { out += '\n'; i += 2; continue; }
            if (nc == '"' || nc == '\\') { out += nc; i += 2; continue; }
        }
        out += json[i];
        ++i;
    }
    return out;
}

inline int jsonFindInt(const std::string& json, const std::string& key, size_t from = 0) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return 0;
    auto c = json.find(':', k);
    if (c == std::string::npos) return 0;
    return std::atoi(json.c_str() + c + 1);
}

inline bool jsonFindBool(const std::string& json, const std::string& key, size_t from = 0, bool def = true) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return def;
    auto c = json.find(':', k);
    if (c == std::string::npos) return def;
    size_t i = c + 1;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
    if (json.compare(i, 4, "true") == 0) return true;
    if (json.compare(i, 5, "false") == 0) return false;
    return def;
}

inline bool jsonFindVec3(const std::string& json, const std::string& key, size_t from, glm::vec3& out) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return false;
    auto b = json.find('[', k);
    if (b == std::string::npos) return false;
    float x = 0, y = 0, z = 0;
    if (std::sscanf(json.c_str() + b + 1, "%f,%f,%f", &x, &y, &z) < 3) return false;
    out = {x, y, z};
    return true;
}

inline bool jsonFindFloat(const std::string& json, const std::string& key, size_t from, float& out) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return false;
    auto c = json.find(':', k);
    if (c == std::string::npos) return false;
    out = (float)std::atof(json.c_str() + c + 1);
    return true;
}

inline bool fileExistsPath(const std::string& path) {
    if (path.empty()) return false;
    if (FILE* f = std::fopen(path.c_str(), "rb")) { std::fclose(f); return true; }
    return false;
}

// Solo manipolazione di stringa, nessuna API di sistema: sta qui sopra il
// ramo #ifdef _WIN32 perche' la usano sia i dialoghi file di Windows (poco
// sotto) sia il resto del motore su ogni piattaforma.
inline void ensureBlockyPlaceExt(std::string& path) {
    if (path.empty()) return;
    auto lower = path;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    if (lower.size() >= 7 && lower.compare(lower.size() - 7, 7, ".blocky") == 0) return;
    if (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".json") == 0) return;
    path += ".blocky";
}

#ifdef _WIN32
inline bool wideToUtf8(const wchar_t* w, std::string& out) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return false;
    out.resize((size_t)n - 1);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return true;
}


inline bool openPlaceFileDialog(std::string& outPath, void* hwndOwner = nullptr) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFilter =
        L"Blocky Place (*.blocky)\0*.blocky\0"
        L"Blocky Place legacy (*.json)\0*.json\0"
        L"Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Blocky — Apri Place";
    ofn.lpstrDefExt = L"blocky";
    if (!GetOpenFileNameW(&ofn)) return false;
    return wideToUtf8(file, outPath);
}

inline bool savePlaceFileDialog(std::string& outPath, void* hwndOwner = nullptr,
                                const char* suggestedName = "place.blocky") {
    wchar_t file[MAX_PATH] = {};
    if (suggestedName && suggestedName[0]) {
        MultiByteToWideChar(CP_UTF8, 0, suggestedName, -1, file, MAX_PATH);
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFilter =
        L"Blocky Place (*.blocky)\0*.blocky\0"
        L"JSON (*.json)\0*.json\0"
        L"Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Blocky — Salva Place (.blocky)";
    ofn.lpstrDefExt = L"blocky";
    if (!GetSaveFileNameW(&ofn)) return false;
    if (!wideToUtf8(file, outPath)) return false;
    ensureBlockyPlaceExt(outPath);
    return true;
}
inline bool openImageFileDialog(std::string& outPath, void* hwndOwner = nullptr) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFilter =
        L"Immagini (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0"
        L"Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Blocky — Scegli icona del gioco";
    if (!GetOpenFileNameW(&ofn)) return false;
    return wideToUtf8(file, outPath);
}
#else
inline bool openPlaceFileDialog(std::string&, void* = nullptr) { return false; }
inline bool savePlaceFileDialog(std::string&, void* = nullptr, const char* = nullptr) { return false; }
inline bool openImageFileDialog(std::string&, void* = nullptr) { return false; }
#endif

inline bool loadPlaceFromJson(const std::string& path, DataModel& dm,
                              LightingService* lighting = nullptr) {
    std::string json = readTextFile(path);
    if (json.empty()) return false;

    dm.clear();
    dm.placeName = jsonFindStr(json, "placeName");
    if (dm.placeName.empty()) dm.placeName = "Published";

    if (lighting) {
        auto lg = json.find("\"lighting\"");
        if (lg != std::string::npos) {
            int mode = jsonFindInt(json, "dayNight", lg);
            if (mode < 0 || mode > 2) mode = 0;
            lighting->dayNight = (DayNightMode)mode;
            jsonFindFloat(json, "dayLengthMinutes", lg, lighting->dayLengthMinutes);
            jsonFindFloat(json, "fixedDayClock", lg, lighting->fixedDayClock);
            jsonFindFloat(json, "fixedNightClock", lg, lighting->fixedNightClock);
            jsonFindFloat(json, "clockTime", lg, lighting->clockTime);
            jsonFindFloat(json, "brightness", lg, lighting->brightness);
            jsonFindFloat(json, "ambient", lg, lighting->ambient);
            lighting->globalShadows = jsonFindBool(json, "globalShadows", lg, true);
            lighting->clampAll();
        }
    }

    auto sp = json.find("\"spawn\"");
    if (sp != std::string::npos) {
        size_t spEnd = json.find('}', sp);
        if (spEnd == std::string::npos) spEnd = sp + 80;
        if (!jsonFindVec3(json, "pos", sp, dm.spawn.position)) {
            float x = 0, y = 0, z = 0;
            jsonFindFloat(json, "x", sp, x);
            jsonFindFloat(json, "y", sp, y);
            jsonFindFloat(json, "z", sp, z);
            dm.spawn.position = {x, y, z};
        }
        jsonFindFloat(json, "yaw", sp, dm.spawn.yaw);
        (void)spEnd;
    }

    // Formato legacy: la Baseplate non era una Part ma un blob a parte nel
    // JSON. I place vecchi la portano ancora cosi' — la convertiamo in una
    // Part vera al volo, cosi' risulta editabile come tutte le altre.
    auto bp = json.find("\"baseplate\"");
    if (bp != std::string::npos && jsonFindBool(json, "enabled", bp, true)) {
        glm::vec3 bpSize{120, 2, 120}, bpColor{0.55f, 0.56f, 0.55f};
        jsonFindVec3(json, "size", bp, bpSize);
        jsonFindVec3(json, "color", bp, bpColor);
        dm.addBaseplate(bpSize, bpColor);
    }

    size_t partsKey = json.find("\"parts\"");
    if (partsKey == std::string::npos) return true;
    size_t arr = json.find('[', partsKey);
    if (arr == std::string::npos) return true;
    int depth = 0;
    size_t arrEnd = arr;
    for (size_t i = arr; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']') {
            --depth;
            if (depth == 0) { arrEnd = i; break; }
        }
    }

    size_t pos = arr;
    while ((pos = json.find('{', pos)) != std::string::npos && pos < arrEnd) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos || end > arrEnd) break;
        WorkspacePart p;
        p.name = jsonFindStr(json, "name", pos);
        if (p.name.empty()) p.name = "Part";
        jsonFindVec3(json, "pos", pos, p.position);
        jsonFindVec3(json, "size", pos, p.size);
        jsonFindVec3(json, "rot", pos, p.rotation);
        jsonFindVec3(json, "color", pos, p.color);
        p.anchored = jsonFindBool(json, "anchored", pos, true);
        p.canCollide = jsonFindBool(json, "canCollide", pos, true);
        {
            std::string sh = jsonFindStr(json, "shape", pos);
            if (!sh.empty())
                p.shape = partShapeFromName(sh);
        }
        {
            std::string mat = jsonFindStr(json, "material", pos);
            if (!mat.empty())
                p.setMaterial(partMaterialFromName(mat));
            else if (jsonFindBool(json, "neon", pos, false))
                p.setMaterial(PartMaterial::Neon);
        }
        jsonFindFloat(json, "emissive", pos, p.emissive);
        p.pointLight = jsonFindBool(json, "pointLight", pos, false);
        jsonFindFloat(json, "lightRange", pos, p.lightRange);
        jsonFindFloat(json, "lightBrightness", pos, p.lightBrightness);
        p.canDestroy = jsonFindBool(json, "canDestroy", pos, true);
        jsonFindFloat(json, "health", pos, p.health);
        jsonFindFloat(json, "maxHealth", pos, p.maxHealth);
        p.script = jsonFindEscapedStr(json, "script", pos);
        {
            int st = jsonFindInt(json, "scriptType", pos);
            p.scriptType = (st == 1) ? ScriptType::Server
                          : (st == 2) ? ScriptType::Client
                          : ScriptType::Legacy;
        }
        dm.parts.push_back(std::move(p));
        pos = end + 1;
    }

    size_t labKey = json.find("\"labels\"");
    if (labKey != std::string::npos) {
        size_t larr = json.find('[', labKey);
        if (larr != std::string::npos) {
            int depth = 0;
            size_t larrEnd = larr;
            for (size_t i = larr; i < json.size(); ++i) {
                if (json[i] == '[') ++depth;
                else if (json[i] == ']') {
                    --depth;
                    if (depth == 0) { larrEnd = i; break; }
                }
            }
            size_t lpos = larr;
            while ((lpos = json.find('{', lpos)) != std::string::npos && lpos < larrEnd) {
                size_t end = json.find('}', lpos);
                if (end == std::string::npos || end > larrEnd) break;
                WorkspaceLabel lab;
                lab.name = jsonFindStr(json, "name", lpos);
                lab.text = jsonFindStr(json, "text", lpos);
                lab.className = jsonFindStr(json, "className", lpos);
                if (lab.name.empty()) lab.name = "TextLabel";
                if (lab.text.empty()) lab.text = lab.name;
                if (lab.className.empty()) lab.className = "TextLabel";
                glm::vec3 anch{0.5f, 0.12f, 0.f};
                if (jsonFindVec3(json, "anchor", lpos, anch)) {
                    lab.anchorX = anch.x;
                    lab.anchorY = anch.y;
                } else {
                    jsonFindFloat(json, "anchorX", lpos, lab.anchorX);
                    jsonFindFloat(json, "anchorY", lpos, lab.anchorY);
                }
                glm::vec3 off{0, 0, 0};
                if (jsonFindVec3(json, "offset", lpos, off)) {
                    lab.offsetX = off.x;
                    lab.offsetY = off.y;
                }
                glm::vec3 sz{0, 0, 0};
                if (jsonFindVec3(json, "size", lpos, sz)) {
                    lab.sizeW = sz.x;
                    lab.sizeH = sz.y;
                }
                jsonFindVec3(json, "color", lpos, lab.color);
                jsonFindFloat(json, "fontScale", lpos, lab.fontScale);
                if (!jsonFindVec3(json, "bgColor", lpos, lab.bgColor))
                    lab.bgColor = {0.f, 0.f, 0.f};
                if (!jsonFindFloat(json, "bgTransparency", lpos, lab.bgTransparency)) {
                    bool oldBg = jsonFindBool(json, "bg", lpos, true);
                    lab.bgTransparency = oldBg ? 0.35f : 1.f;
                }
                lab.visible = jsonFindBool(json, "visible", lpos, true);
                dm.labels.push_back(std::move(lab));
                lpos = end + 1;
            }
        }
    }

    size_t bpKey = json.find("\"backpack\"");
    if (bpKey != std::string::npos) {
        size_t barr = json.find('[', bpKey);
        if (barr != std::string::npos) {
            int depth = 0;
            size_t barrEnd = barr;
            for (size_t i = barr; i < json.size(); ++i) {
                if (json[i] == '[') ++depth;
                else if (json[i] == ']') {
                    --depth;
                    if (depth == 0) { barrEnd = i; break; }
                }
            }
            size_t bpos = barr;
            while ((bpos = json.find('{', bpos)) != std::string::npos && bpos < barrEnd) {
                size_t end = json.find('}', bpos);
                if (end == std::string::npos || end > barrEnd) break;
                BackpackItem it;
                it.id = jsonFindStr(json, "id", bpos);
                it.name = jsonFindStr(json, "name", bpos);
                it.iconHue = jsonFindInt(json, "iconHue", bpos);
                it.tool = (BackpackTool)jsonFindInt(json, "tool", bpos);
                it.meshPath = jsonFindStr(json, "meshPath", bpos);
                it.animId = jsonFindStr(json, "animId", bpos);
                jsonFindVec3(json, "gripOffset", bpos, it.gripOffset);
                jsonFindVec3(json, "gripRotation", bpos, it.gripRotation);
                jsonFindFloat(json, "gripScale", bpos, it.gripScale);
                if (it.name.empty()) it.name = "Item";
                if (it.id.empty()) it.id = "bp_" + std::to_string(dm.backpack.size());
                if (it.tool == BackpackTool::None) {
                    std::string low = it.name;
                    for (char& c : low) c = (char)std::tolower((unsigned char)c);
                    if (low.find("sword") != std::string::npos || low.find("spada") != std::string::npos)
                        it.tool = BackpackTool::Sword;
                    else if (low.find("pistol") != std::string::npos || low.find("pistola") != std::string::npos
                             || low.find("gun") != std::string::npos || low.find("fucile") != std::string::npos)
                        it.tool = BackpackTool::Pistol;
                }
                dm.backpack.push_back(std::move(it));
                bpos = end + 1;
            }
        }
    }
    dm.equippedBackpack = jsonFindInt(json, "equippedBackpack");
    if (dm.equippedBackpack < -1 || dm.equippedBackpack >= DataModel::kHotbarSlots)
        dm.equippedBackpack = -1;

    size_t frKey = json.find("\"friends\"");
    if (frKey != std::string::npos) {
        size_t farr = json.find('[', frKey);
        if (farr != std::string::npos) {
            int depth = 0;
            size_t farrEnd = farr;
            for (size_t i = farr; i < json.size(); ++i) {
                if (json[i] == '[') ++depth;
                else if (json[i] == ']') {
                    --depth;
                    if (depth == 0) { farrEnd = i; break; }
                }
            }
            size_t fpos = farr;
            while ((fpos = json.find('{', fpos)) != std::string::npos && fpos < farrEnd) {
                size_t end = json.find('}', fpos);
                if (end == std::string::npos || end > farrEnd) break;
                WorkspaceFriend fr;
                fr.name = jsonFindStr(json, "name", fpos);
                if (fr.name.empty()) fr.name = "Amico";
                jsonFindVec3(json, "pos", fpos, fr.position);
                jsonFindFloat(json, "yaw", fpos, fr.yaw);
                dm.friends.push_back(std::move(fr));
                fpos = end + 1;
            }
        }
    }

    return true;
}

inline std::string publishedManifestPath() {
    return blockyDataDir() + "/published_games.json";
}

inline long long jsonFindLongLong(const std::string& json, const std::string& key,
                                  size_t from = 0) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return 0;
    auto c = json.find(':', k);
    if (c == std::string::npos) return 0;
    long long v = 0;
    if (std::sscanf(json.c_str() + c + 1, "%lld", &v) == 1) return v;
    return 0;
}

inline std::vector<GameEntry> loadPublishedGames() {
    std::string json = readTextFile(publishedManifestPath());
    std::vector<GameEntry> out;
    if (json.empty()) return out;

    size_t pos = 0;
    while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
        GameEntry e;
        e.id = jsonFindStr(json, "id", pos);
        e.title = jsonFindStr(json, "title", pos);
        e.subtitle = jsonFindStr(json, "subtitle", pos);
        e.creator = jsonFindStr(json, "creator", pos);
        e.placePath = jsonFindStr(json, "placePath", pos);
        e.iconPath = jsonFindStr(json, "iconPath", pos);
        e.playersOnline = jsonFindInt(json, "players", pos);
        e.maxPlayers = jsonFindInt(json, "maxPlayers", pos);
        e.publishedAt = jsonFindLongLong(json, "publishedAt", pos);
        if (e.maxPlayers < 1) e.maxPlayers = 8;
        if (e.maxPlayers > 32) e.maxPlayers = 32;
        e.script = "scripts/baseplate.lua";
        e.isPublished = true;
        e.accent = {0.15f, 0.48f, 0.92f};
        if (!e.id.empty()) out.push_back(e);
        pos += 4;
    }
    return out;
}

inline bool savePublishedGamesList(const std::vector<GameEntry>& all) {
    std::ostringstream j;
    j << "{\n  \"games\": [\n";
    bool first = true;
    for (const auto& e : all) {
        if (!e.isPublished) continue;
        if (!first) j << ",\n";
        first = false;
        j << "    {\"id\":\"" << e.id << "\",\"title\":\"" << e.title
          << "\",\"subtitle\":\"" << e.subtitle << "\",\"creator\":\"" << e.creator
          << "\",\"placePath\":\"" << e.placePath
          << "\",\"iconPath\":\"" << e.iconPath
          << "\",\"players\":" << e.playersOnline
          << ",\"maxPlayers\":" << e.maxPlayers
          << ",\"publishedAt\":" << e.publishedAt << "}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(publishedManifestPath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline std::string normalizeCreatorKey(const std::string& c) {
    std::string o;
    o.reserve(c.size());
    for (unsigned char ch : c) {
        char l = (char)std::tolower(ch);
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_' || l == '-')
            o.push_back(l);
    }
    return o.empty() ? "builder" : o;
}

inline int countUserPublishedInWindow(const std::vector<GameEntry>& games,
                                      const std::string& creator,
                                      long long nowSecs) {
    std::string key = normalizeCreatorKey(creator);
    int n = 0;
    for (const auto& g : games) {
        if (!g.isPublished) continue;
        if (normalizeCreatorKey(g.creator) != key) continue;
        long long at = g.publishedAt > 0 ? g.publishedAt : nowSecs;
        if (nowSecs - at <= kPublishQuotaWindowSecs)
            ++n;
    }
    return n;
}

inline bool canPublishGame(const std::vector<GameEntry>& games,
                           const std::string& gameId,
                           const std::string& creator,
                           long long nowSecs,
                           int* outUsed = nullptr) {
    std::string key = normalizeCreatorKey(creator);
    for (const auto& g : games) {
        if (g.isPublished && g.id == gameId && normalizeCreatorKey(g.creator) == key) {
            if (outUsed)
                *outUsed = countUserPublishedInWindow(games, creator, nowSecs);
            return true;
        }
    }
    int used = countUserPublishedInWindow(games, creator, nowSecs);
    if (outUsed) *outUsed = used;
    return used < kMaxPublishedGamesPerUser;
}

inline std::vector<GameEntry> loadGameManifest() {
    std::vector<std::string> paths = {
        assetRoot() + "/assets/places/manifest.json",
        "assets/places/manifest.json",
    };
    std::string json;
    for (const auto& p : paths) {
        json = readTextFile(p);
        if (!json.empty()) break;
    }

    std::vector<GameEntry> games;
    if (json.empty()) {
        // Solo Baseplate: e la lista di ripiego se assets/places/manifest.json
        // manca o non si legge. Deve rispecchiare il manifest, altrimenti un
        // asset mancante farebbe ricomparire giochi che non ci sono piu.
        games.push_back({"baseplate", "Baseplate", "Costruisci libero", "scripts/baseplate.lua", 1247, {0.2f, 0.62f, 0.35f}});
    } else {
        size_t pos = 0;
        while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
            GameEntry e;
            e.id = jsonFindStr(json, "id", pos);
            e.title = jsonFindStr(json, "title", pos);
            e.subtitle = jsonFindStr(json, "subtitle", pos);
            e.script = jsonFindStr(json, "script", pos);
            {
                int st = jsonFindInt(json, "scriptType", pos);
                e.scriptType = (st == 1) ? ScriptType::Server
                              : (st == 2) ? ScriptType::Client
                              : ScriptType::Legacy;
            }
            e.iconPath = jsonFindStr(json, "iconPath", pos);
            e.playersOnline = jsonFindInt(json, "players", pos);
            e.maxPlayers = jsonFindInt(json, "maxPlayers", pos);
            if (e.maxPlayers < 1) e.maxPlayers = 8;
            if (e.maxPlayers > 32) e.maxPlayers = 32;
            auto k = json.find("\"accent\"", pos);
            if (k != std::string::npos) {
                auto b = json.find('[', k);
                if (b != std::string::npos) {
                    float r = 0.2f, g = 0.62f, bl = 0.35f;
                    std::sscanf(json.c_str() + b + 1, "%f,%f,%f", &r, &g, &bl);
                    e.accent = {r, g, bl};
                }
            }
            if (!e.id.empty()) games.push_back(e);
            pos += 4;
        }
        if (games.empty()) {
            // Stessa lista del ripiego sopra: un solo gioco.
            games = {
                {"baseplate", "Baseplate", "Costruisci libero", "scripts/baseplate.lua", 1247, {0.2f, 0.62f, 0.35f}},
            };
        }
    }

    auto pub = loadPublishedGames();
    for (auto& e : pub) {
        bool exists = false;
        for (auto& g : games) {
            if (g.id == e.id) {
                g = e;
                exists = true;
                break;
            }
        }
        if (!exists) games.push_back(e);
    }

    return games;
}

inline void mergeGamePackages(std::vector<GameEntry>& games,
                              const std::vector<GameEntry>& packaged) {
    for (const auto& e : packaged) {
        bool exists = false;
        for (auto& g : games) {
            if (g.id == e.id) {
                g = e;
                exists = true;
                break;
            }
        }
        if (!exists) games.push_back(e);
    }
}

inline void applyGamePlace(const GameEntry& game, DataModel& dm,
                           LightingService* lighting = nullptr) {
    if (game.isPublished && !game.placePath.empty()) {
        if (loadPlaceFromJson(game.placePath, dm, lighting)) {
            dm.placeName = game.title;
            return;
        }
    }
    // Garden e Obby non sono piu' giochi dell'Hub, quindi qui non arriva piu'
    // nessuno con quegli id. Le loro mappe NON sono state cancellate: restano
    // come modelli di partenza in Studio (Mappa -> Garden / Obby), che e' una
    // cosa diversa dall'essere un gioco giocabile dall'Hub.
    dm.loadBaseplatePlace();
    dm.placeName = game.title;
}

inline std::string resolveScriptPath(const std::string& rel) {
    if (rel.empty()) return {};
    if (fileExistsPath(rel)) return rel;
    std::vector<std::string> bases = { assetRoot() + "/", "" };
    for (const auto& base : bases) {
        std::string p = base + "assets/" + rel;
        if (fileExistsPath(p)) return p;
        p = base + rel;
        if (fileExistsPath(p)) return p;
    }
    return assetRoot() + "/assets/" + rel;
}

inline std::string sanitizeGameId(const std::string& title) {
    std::string id;
    for (char c : title) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            id.push_back((char)std::tolower((unsigned char)c));
        else if (c == ' ' || c == '-' || c == '_')
            id.push_back('_');
    }
    if (id.empty()) id = "game";
    if (id.size() > 40) id.resize(40);
    return id;
}
