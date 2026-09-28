#pragma once


#include "GameHub.h"
#include "SafeBytecode.h"

#include <algorithm>
#include <cstdio>
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
    std::ofstream out(to, std::ios::binary);
    if (!out) return false;
    out << in.rdbuf();
    return (bool)out;
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

    if (!writeTextFile(joinPath(dir, "game.blocky.json"), writeGamePackageJson(pkg)))
        return {};
    if (!writeTextFile(joinPath(dir, "place.json"), placeJson))
        return {};
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

    return dir;
}

inline std::vector<GamePackage> listInstalledGames() {
    std::vector<GamePackage> out;
    std::string root = gamesInstallDir();
#ifdef _WIN32
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA((root + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == '.') continue;
        std::string folder = joinPath(root, fd.cFileName);
        std::string manifest = readTextFile(joinPath(folder, "game.blocky.json"));
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
        if (g.id.empty()) g.id = fd.cFileName;
        if (g.title.empty()) g.title = g.id;
        if (g.placeFile.empty()) g.placeFile = "place.json";
        out.push_back(g);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
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
