#pragma once


#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "engine/AssetAudit.h"
#include "engine/GameIndex.h"
#include "engine/ZipArchive.h"

namespace GamePublish {

namespace fs = std::filesystem;

struct BuildResult {
    bool ok = false;
    GameIndex::Index index;
    std::string json;
    std::vector<std::string> errors;
};


inline bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return out.good();
}

inline std::string readTextFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::string slugify(std::string s) {
    for (char& c : s) {
        if (std::isalnum((unsigned char)c)) c = (char)std::tolower((unsigned char)c);
        else c = '_';
    }
    return s.empty() ? "game" : s;
}

inline bool loadPlaceInto(const std::string& path, DataModel& world) {
    const std::string text = readTextFile(path);
    if (text.empty()) return false;

    auto findString = [&](const char* key) -> std::string {
        const std::string needle = std::string("\"") + key + "\"";
        size_t p = text.find(needle);
        if (p == std::string::npos) return {};
        p = text.find(':', p);
        if (p == std::string::npos) return {};
        p = text.find('"', p);
        if (p == std::string::npos) return {};
        const size_t end = text.find('"', p + 1);
        if (end == std::string::npos) return {};
        return text.substr(p + 1, end - p - 1);
    };

    world.placeName = findString("placeName");
    if (world.placeName.empty()) world.placeName = "Untitled";

    auto countBlock = [&](const char* blockKey) -> int {
        const std::string bneedle = std::string("\"") + blockKey + "\"";
        const size_t b = text.find(bneedle);
        if (b == std::string::npos) return 0;
        const size_t blockEnd = text.find("]", b);
        if (blockEnd == std::string::npos) return 0;
        int n = 0;
        size_t p = b;
        while ((p = text.find("\"name\"", p)) != std::string::npos && p < blockEnd) {
            ++n;
            p += 6;
        }
        return n;
    };

    const int partCount = countBlock("parts");
    for (int i = 0; i < partCount; ++i) {
        WorkspacePart p;
        p.name = "Part" + std::to_string(i + 1);
        world.parts.push_back(p);
    }
    return true;
}

inline std::string findPlaceFile(const std::string& gameDir) {
    for (const char* name : {"place.json", "game.blocky.json", "place1.json"}) {
        fs::path p = fs::path(gameDir) / name;
        std::error_code ec;
        if (fs::exists(p, ec)) return p.string();
    }
    std::error_code ec;
    fs::path placesDir = fs::path(gameDir) / "places";
    if (fs::exists(placesDir, ec)) {
        for (const auto& entry : fs::directory_iterator(placesDir, ec)) {
            if (entry.path().extension() == ".json") return entry.path().string();
        }
    }
    return {};
}


inline BuildResult buildIndexForFolder(const std::string& gameDir,
                                       std::string gameId, std::string title) {
    BuildResult r;

    std::error_code ec;
    if (!fs::exists(gameDir, ec) || !fs::is_directory(gameDir, ec)) {
        r.errors.push_back("cartella non trovata: " + gameDir);
        return r;
    }

    const std::string placePath = findPlaceFile(gameDir);
    if (placePath.empty()) {
        r.errors.push_back("nessun place.json / game.blocky.json — non è un gioco pubblicato");
        return r;
    }

    DataModel world;
    world.clear();
    if (!loadPlaceInto(placePath, world)) {
        r.errors.push_back("impossibile leggere " + placePath);
        return r;
    }
    if (title.empty()) title = world.placeName;
    if (gameId.empty()) gameId = slugify(world.placeName);

    const fs::path assetsDir = fs::path(gameDir) / "assets";
    AssetAudit::Report audit;
    if (fs::exists(assetsDir, ec)) {
        audit = AssetAudit::run(assetsDir.string());
    }

    if (audit.errors > 0) {
        for (const auto& f : audit.findings) {
            if (f.severity == AssetAudit::Severity::Error)
                r.errors.push_back("asset " + f.path + " — " + f.rule + ": " + f.detail);
        }
        return r;
    }

    GameIndex::Index idx = GameIndex::buildFromWorld(world, LightingService{}, gameId, title);

    idx.assets.reserve(audit.assets.size());
    for (const AssetAudit::AssetRecord& rec : audit.assets) {
        GameIndex::AssetEntry e;
        e.id = rec.id;
        e.format = rec.format;
        e.kind = GameIndex::kindFromFormat(rec.format);
        e.bytes = rec.bytes;
        e.checksum = GameIndex::toHex(rec.checksum);
        idx.assets.push_back(std::move(e));
    }

    GameIndex::finalizeHash(idx);

    r.ok = true;
    r.index = std::move(idx);
    r.json = GameIndex::toJson(r.index);
    return r;
}


struct PublishResult {
    bool ok = false;
    std::string gameId;
    std::string message;
    BuildResult build;
};

inline std::string randomSuffix() {
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream o;
    o << std::hex << rng();
    return o.str();
}

inline PublishResult publishZipBytes(const std::vector<std::uint8_t>& zipBytes,
                                     const std::string& requestedId,
                                     const std::string& hostedRoot) {
    PublishResult pr;
    const std::string id = slugify(requestedId.empty() ? "game" : requestedId);
    pr.gameId = id;

    std::error_code ec;
    fs::create_directories(hostedRoot, ec);

    const fs::path staging = fs::path(hostedRoot) / ".staging" / (id + "_" + randomSuffix());
    fs::create_directories(staging, ec);
    if (ec) {
        pr.message = "impossibile creare la cartella di staging";
        return pr;
    }

    const ZipArchive::ExtractResult unz = ZipArchive::extractFromMemory(zipBytes, staging.string());
    if (!unz.ok) {
        fs::remove_all(staging, ec);
        pr.message = "archivio non valido: " + unz.error;
        return pr;
    }
    if (unz.entries == 0) {
        fs::remove_all(staging, ec);
        pr.message = "archivio vuoto";
        return pr;
    }

    fs::path contentRoot = staging;
    {
        int childCount = 0;
        fs::path onlyChild;
        for (const auto& entry : fs::directory_iterator(staging, ec)) {
            ++childCount;
            onlyChild = entry.path();
        }
        if (childCount == 1 && fs::is_directory(onlyChild, ec)) contentRoot = onlyChild;
    }

    pr.build = buildIndexForFolder(contentRoot.string(), id, id);
    if (!pr.build.ok) {
        fs::remove_all(staging, ec);
        pr.message = "indicizzazione fallita";
        for (const auto& e : pr.build.errors) pr.message += "\n  - " + e;
        return pr;
    }

    if (!writeTextFile((contentRoot / "game_index.json").string(), pr.build.json)) {
        fs::remove_all(staging, ec);
        pr.message = "scrittura di game_index.json fallita";
        return pr;
    }

    const fs::path finalDir = fs::path(hostedRoot) / id;
    const fs::path backupDir = fs::path(hostedRoot) / (".old_" + id);

    fs::remove_all(backupDir, ec);
    if (fs::exists(finalDir, ec)) fs::rename(finalDir, backupDir, ec);

    fs::rename(contentRoot, finalDir, ec);
    if (ec) {
        std::error_code ec2;
        fs::rename(backupDir, finalDir, ec2);
        fs::remove_all(staging, ec2);
        pr.message = "scambio della cartella pubblicata fallito: " + ec.message();
        return pr;
    }

    fs::remove_all(backupDir, ec);
    fs::remove_all(staging, ec);

    pr.ok = true;
    pr.message = "pubblicato";
    return pr;
}

}
