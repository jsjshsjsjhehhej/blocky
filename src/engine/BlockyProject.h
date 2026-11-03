#pragma once


#include "GameHub.h"
#include "SafeBytecode.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace BlockyProject {

inline void mkDir(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

inline std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char sep = '\\';
#ifndef _WIN32
    sep = '/';
#endif
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + sep + b;
}

inline std::string gamesInstallDir() {
    std::string root = blockyDataDir();
    mkDir(joinPath(root, "games"));
    return joinPath(root, "games");
}

inline std::string projectsDir() {
    std::string root = blockyDataDir();
    mkDir(joinPath(root, "projects"));
    return joinPath(root, "projects");
}

inline std::string escapeJson(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else o.push_back((char)c);
    }
    return o;
}

struct ProjectManifest {
    std::string format = "BlockyProject";
    int version = 1;
    std::string name = "Nuovo Gioco";
    std::string id;
    std::string creator;
    std::string description;
    std::string mainPlace = "places/main.place.json";
    std::string mainScript = "scripts/main.lua";
    int maxPlayers = 8;
    std::string engineMin = "1.0.0";
    bool blockyExclusive = true;
};

struct GamePackage {
    std::string format = "BlockyGame";
    int version = 1;
    std::string id;
    std::string title;
    std::string creator;
    std::string description;
    std::string placeFile = "place.json";
    std::string scriptFile = "scripts/main.lua";
    int maxPlayers = 8;
    bool blockyExclusive = true;
};

inline std::string slugify(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) c = '_';
    }
    while (!s.empty() && s.front() == '_') s.erase(s.begin());
    while (!s.empty() && s.back() == '_') s.pop_back();
    if (s.empty()) s = "game";
    return s;
}

inline std::string writeProjectJson(const ProjectManifest& m) {
    std::ostringstream j;
    j << "{\n"
      << "  \"format\": \"" << m.format << "\",\n"
      << "  \"version\": " << m.version << ",\n"
      << "  \"name\": \"" << escapeJson(m.name) << "\",\n"
      << "  \"id\": \"" << escapeJson(m.id) << "\",\n"
      << "  \"creator\": \"" << escapeJson(m.creator) << "\",\n"
      << "  \"description\": \"" << escapeJson(m.description) << "\",\n"
      << "  \"mainPlace\": \"" << escapeJson(m.mainPlace) << "\",\n"
      << "  \"mainScript\": \"" << escapeJson(m.mainScript) << "\",\n"
      << "  \"maxPlayers\": " << m.maxPlayers << ",\n"
      << "  \"engineMin\": \"" << escapeJson(m.engineMin) << "\",\n"
      << "  \"blockyExclusive\": true,\n"
      << "  \"runtime\": \"Blocky\",\n"
      << "  \"platforms\": [\"Blocky\"]\n"
      << "}\n";
    return j.str();
}

inline std::string writeGamePackageJson(const GamePackage& g) {
    std::ostringstream j;
    j << "{\n"
      << "  \"format\": \"" << g.format << "\",\n"
      << "  \"version\": " << g.version << ",\n"
      << "  \"id\": \"" << escapeJson(g.id) << "\",\n"
      << "  \"title\": \"" << escapeJson(g.title) << "\",\n"
      << "  \"creator\": \"" << escapeJson(g.creator) << "\",\n"
      << "  \"description\": \"" << escapeJson(g.description) << "\",\n"
      << "  \"placeFile\": \"" << escapeJson(g.placeFile) << "\",\n"
      << "  \"scriptFile\": \"" << escapeJson(g.scriptFile) << "\",\n"
      << "  \"maxPlayers\": " << g.maxPlayers << ",\n"
      << "  \"blockyExclusive\": true,\n"
      << "  \"runtime\": \"Blocky\",\n"
      << "  \"platforms\": [\"Blocky\"]\n"
      << "}\n";
    return j.str();
}

inline bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return (bool)f;
}

inline bool copyFileBinary(const std::string& from, const std::string& to) {
    std::ifstream in(from, std::ios::binary);
    if (!in) return false;
    // Crea la cartella padre se manca (assets/meshes/...).
    {
        std::error_code ec;
        const auto parent = std::filesystem::path(to).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent, ec);
    }
    std::ofstream out(to, std::ios::binary);
    if (!out) return false;
    out << in.rdbuf();
    return (bool)out;
}

inline std::string fileBaseName(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? path : path.substr(s + 1);
}

inline bool looksLikeAbsolutePath(const std::string& p) {
    if (p.empty()) return false;
    if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':') return true;
    return p[0] == '/' || p[0] == '\\';
}

/** Sostituisce il valore di una chiave JSON stringa (prima occorrenza da `from`). */
inline bool jsonReplaceStrValue(std::string& json, const std::string& key,
                                size_t from, const std::string& newVal) {
    const std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat, from);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t open = json.find('"', colon + 1);
    if (open == std::string::npos) return false;
    size_t close = open + 1;
    while (close < json.size()) {
        if (json[close] == '\\' && close + 1 < json.size()) { close += 2; continue; }
        if (json[close] == '"') break;
        ++close;
    }
    if (close >= json.size()) return false;
    json.replace(open + 1, close - open - 1, escapeJson(newVal));
    return true;
}

/**
 * Copia mesh/file referenziati dal place dentro assets/ e riscrive i path
 * relativi: cosi' GamesHost e il telefono trovano gli stessi file.
 */
inline std::string packPlaceAssets(const std::string& pkgDir, std::string placeJson) {
    mkDir(joinPath(pkgDir, "assets"));
    mkDir(joinPath(pkgDir, "assets\\meshes"));
    mkDir(joinPath(pkgDir, "assets\\textures"));
    mkDir(joinPath(pkgDir, "assets\\audio"));

    auto sanitizeBase = [](std::string base) {
        for (char& c : base) {
            if (!(std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-'))
                c = '_';
        }
        return base;
    };
    auto packOne = [&](const std::string& srcAbs, const std::string& key, size_t keyPos) {
        std::string base = sanitizeBase(fileBaseName(srcAbs));
        if (base.empty() || !fileExistsPath(srcAbs)) return;
        std::string sub = "meshes";
        auto dot = base.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = base.substr(dot + 1);
            for (char& c : ext) c = (char)std::tolower((unsigned char)c);
            if (ext == "wav" || ext == "mp3" || ext == "ogg") sub = "audio";
            else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp")
                sub = "textures";
        }
        const std::string destRel = "assets/" + sub + "/" + base;
        const std::string destAbs = joinPath(pkgDir, "assets\\" + sub + "\\" + base);
        if (!fileExistsPath(destAbs))
            copyFileBinary(srcAbs, destAbs);
        jsonReplaceStrValue(placeJson, key, keyPos, destRel);
    };

    auto extractPath = [&](const std::string& key, size_t keyPos) -> std::string {
        // Preferisci escaped; fallback grezzo (place vecchi con C:\...).
        std::string p = jsonFindEscapedStr(placeJson, key, keyPos);
        if (p.empty()) p = jsonFindStr(placeJson, key, keyPos);
        return p;
    };

    const char* keys[] = {"meshPath", "sourcePath", "soundPath", "iconPath", "texturePath"};
    for (const char* key : keys) {
        size_t pos = 0;
        const std::string pat = std::string("\"") + key + "\"";
        while ((pos = placeJson.find(pat, pos)) != std::string::npos) {
            const size_t keyPos = pos;
            const std::string oldPath = extractPath(key, keyPos);
            pos = keyPos + pat.size();
            if (oldPath.empty()) continue;
            if (oldPath.rfind("assets/", 0) == 0 || oldPath.rfind("assets\\", 0) == 0)
                continue;
            if (fileExistsPath(oldPath)) {
                packOne(oldPath, key, keyPos);
                continue;
            }
            // Prova forward-slash e models/
            std::string norm = oldPath;
            for (char& c : norm) if (c == '/') c = '\\';
            if (fileExistsPath(norm)) {
                packOne(norm, key, keyPos);
                continue;
            }
            const std::string alt = joinPath(blockyDataDir(),
                joinPath("models", fileBaseName(oldPath)));
            if (fileExistsPath(alt))
                packOne(alt, key, keyPos);
        }
    }
    return placeJson;
}

/** Elenco di tutti i file del pacchetto (path relativi con /). */
inline std::vector<std::string> listPackageFiles(const std::string& pkgDir) {
    std::vector<std::string> out;
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root(pkgDir);
    if (!fs::exists(root, ec)) return out;
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        fs::path rel = fs::relative(entry.path(), root, ec);
        if (ec) continue;
        std::string s = rel.generic_string();
        if (s.empty() || s[0] == '.') continue;
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline std::string writeFilesManifest(const std::string& pkgDir) {
    const auto files = listPackageFiles(pkgDir);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < files.size(); ++i) {
        if (i) j << ",";
        j << "\"" << escapeJson(files[i]) << "\"";
    }
    j << "]";
    const std::string body = j.str();
    writeTextFile(joinPath(pkgDir, "files.json"), body);
    return body;
}

/**
 * Per giochi gia' pubblicati: se place.json ha ancora path assoluti del PC,
 * li copia in assets/ e riscrive. Poi aggiorna files.json.
 */
inline void ensurePackageAssets(const std::string& pkgDir) {
    const std::string placePath = joinPath(pkgDir, "place.json");
    if (!fileExistsPath(placePath)) return;
    std::string json = readTextFile(placePath);
    if (json.empty()) return;
    std::string packed = packPlaceAssets(pkgDir, json);
    if (packed != json)
        writeTextFile(placePath, packed);
    writeFilesManifest(pkgDir);
}

inline bool createNewProject(const std::string& rootDir, ProjectManifest m,
                             const std::string& starterPlaceJson = {}) {
    if (m.id.empty()) m.id = slugify(m.name);
    m.blockyExclusive = true;

    mkDir(rootDir);
    mkDir(joinPath(rootDir, "places"));
    mkDir(joinPath(rootDir, "scripts"));
    mkDir(joinPath(rootDir, "assets"));
    mkDir(joinPath(rootDir, "assets\\meshes"));
    mkDir(joinPath(rootDir, "assets\\textures"));
    mkDir(joinPath(rootDir, "assets\\audio"));
    mkDir(joinPath(rootDir, "packages"));

    if (!writeTextFile(joinPath(rootDir, "project.blocky.json"), writeProjectJson(m)))
        return false;

    std::string place = starterPlaceJson;
    if (place.empty()) {
        place =
            "{\n"
            "  \"format\": \"BlockyPlace\",\n"
            "  \"version\": 1,\n"
            "  \"placeName\": \"" + escapeJson(m.name) + "\",\n"
            "  \"spawn\": {\"x\":0,\"y\":5,\"z\":0,\"yaw\":0},\n"
            "  \"baseplate\": {\"enabled\":true,\"size\":[64,1,64],\"color\":[0.35,0.72,0.42]},\n"
            "  \"parts\": [],\n"
            "  \"humanoidParts\": []\n"
            "}\n";
    }
    if (!writeTextFile(joinPath(rootDir, "places\\main.place.json"), place))
        return false;

    std::string lua =
        "-- " + m.name + " — script principale (solo runtime Blocky)\n"
        "print(\"[Blocky] Gioco avviato: " + m.name + "\")\n"
        "\n"
        "local part = Instance.new(\"Part\")\n"
        "part.Name = \"WelcomeBrick\"\n"
        "part.Size = Vector3.new(4, 1, 4)\n"
        "part.Position = Vector3.new(0, 2, -8)\n"
        "part.Color = Color3.fromRGB(40, 140, 255)\n"
        "part.Anchored = true\n"
        "workspace:Add(part)\n";
    if (!writeTextFile(joinPath(rootDir, "scripts\\main.lua"), lua))
        return false;

    writeTextFile(joinPath(rootDir, "README.txt"),
        "Progetto Blocky\n"
        "===============\n"
        "Questo gioco funziona SOLO su Blocky (Blocky.exe).\n"
        "Apri con Blocky Game Engine, pubblica nel hub Blocky.\n");

    return true;
}

inline std::string publishGamePackage(const GamePackage& pkgIn,
                                      const std::string& placeJson,
                                      const std::string& scriptSource = {}) {
    GamePackage pkg = pkgIn;
    pkg.blockyExclusive = true;
    if (pkg.id.empty()) pkg.id = slugify(pkg.title);
    if (pkg.maxPlayers < 1) pkg.maxPlayers = 1;
    if (pkg.maxPlayers > 32) pkg.maxPlayers = 32;

    std::string dir = joinPath(gamesInstallDir(), pkg.id);
    mkDir(dir);
    mkDir(joinPath(dir, "scripts"));
    mkDir(joinPath(dir, "assets"));
    mkDir(joinPath(dir, "assets\\meshes"));

    // Mesh/suoni/texture del place → assets/ con path relativi.
    const std::string packedPlace = packPlaceAssets(dir, placeJson);

    if (!writeTextFile(joinPath(dir, "game.blocky.json"), writeGamePackageJson(pkg)))
        return {};
    if (!writeTextFile(joinPath(dir, "place.json"), packedPlace))
        return {};
    {
        std::ostringstream info;
        const std::string desc = pkg.description.empty()
            ? (pkg.creator.empty() ? "Pubblicato su Blocky" : ("di " + pkg.creator))
            : pkg.description;
        info << "{\"id\":\"" << escapeJson(pkg.id)
             << "\",\"name\":\"" << escapeJson(pkg.title)
             << "\",\"title\":\"" << escapeJson(pkg.title)
             << "\",\"description\":\"" << escapeJson(desc)
             << "\",\"place\":\"place.json\",\"thumbnail\":\"\"}\n";
        writeTextFile(joinPath(dir, "info.json"), info.str());
    }
    if (!scriptSource.empty()) {
        writeTextFile(joinPath(dir, "scripts\\main.lua"), scriptSource);
        std::string bbcPath = joinPath(dir, "scripts\\main.bbc");
        std::string err;
        if (SafeBytecode::compileSourceToSealedFile(scriptSource, pkg.id, bbcPath, err)) {
            pkg.scriptFile = "scripts/main.bbc";
            writeTextFile(joinPath(dir, "game.blocky.json"), writeGamePackageJson(pkg));
        } else {
            std::fprintf(stderr, "SafeBytecode compile: %s\n", err.c_str());
            pkg.scriptFile = "scripts/main.lua";
        }
    }

    writeFilesManifest(dir);
    return dir;
}

inline std::vector<GamePackage> listInstalledGames() {
    std::vector<GamePackage> out;
    std::string root = gamesInstallDir();
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return out;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_directory(ec)) continue;
        const std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        std::string manifest = readTextFile((entry.path() / "game.blocky.json").string());
        if (manifest.empty()) continue;
        GamePackage g;
        g.id = jsonFindStr(manifest, "id");
        g.title = jsonFindStr(manifest, "title");
        g.creator = jsonFindStr(manifest, "creator");
        g.description = jsonFindStr(manifest, "description");
        g.placeFile = jsonFindStr(manifest, "placeFile");
        g.scriptFile = jsonFindStr(manifest, "scriptFile");
        g.maxPlayers = jsonFindInt(manifest, "maxPlayers");
        if (g.maxPlayers <= 0) g.maxPlayers = 8;
        if (g.id.empty()) g.id = name;
        if (g.title.empty()) g.title = g.id;
        if (g.placeFile.empty()) g.placeFile = "place.json";
        out.push_back(g);
    }
    std::sort(out.begin(), out.end(), [](const GamePackage& a, const GamePackage& b) {
        return a.title < b.title;
    });
    return out;
}

inline GameEntry toGameEntry(const GamePackage& g) {
    GameEntry e;
    e.id = g.id;
    e.title = g.title;
    e.subtitle = g.creator.empty() ? "Blocky Game" : ("di " + g.creator);
    e.creator = g.creator;
    e.isPublished = true;
    e.maxPlayers = g.maxPlayers;
    std::string dir = joinPath(gamesInstallDir(), g.id);
    e.placePath = joinPath(dir, g.placeFile.empty() ? "place.json" : g.placeFile);
    e.accent = {0.12f, 0.48f, 0.92f};
    std::string bbc = joinPath(dir, "scripts\\main.bbc");
    std::string lua = joinPath(dir, g.scriptFile.empty() ? "scripts/main.lua" : g.scriptFile);
    if (fileExistsPath(bbc) && SafeBytecode::looksLikeBbcMagic(bbc))
        e.script = bbc;
    else if (fileExistsPath(lua))
        e.script = lua;
    else
        e.script = "scripts/baseplate.lua";
    return e;
}

}
