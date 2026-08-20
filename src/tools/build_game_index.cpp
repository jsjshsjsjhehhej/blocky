/**
 * build_game_index — costruisce game_index.json per un gioco pubblicato.
 *
 *   build_game_index <cartella_gioco> [--id nome] [--title "Titolo"]
 *
 * La cartella deve avere place.json (o game.blocky.json) e una sottocartella
 * assets/. Lo strumento non tocca la mesh — usa AssetAudit per l'inventario
 * (hash, formato, peso) e legge il place per collider e script.
 *
 * Uscita 0 se scritto, 1 se la cartella non è quella di un gioco pubblicato:
 * così può stare in uno script di release e fermarlo prima del pacchetto,
 * come asset_audit.
 */

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "engine/AssetAudit.h"
#include "engine/GameIndex.h"

namespace fs = std::filesystem;

namespace {

/**
 * Legge solo i campi che servono da un place.json scritto da PlaceExporter,
 * senza un parser JSON completo: il formato è generato da noi, quindi i
 * pattern sono noti e stabili.
 */
bool loadPlaceInto(const std::string& path, DataModel& world) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();

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

    // Conta le occorrenze di "name" dentro il blocco "parts": basta a dare un
    // numero di entità plausibile senza analizzare la struttura ad albero.
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

std::string findPlaceFile(const std::string& gameDir) {
    for (const char* name : {"place.json", "game.blocky.json", "place1.json"}) {
        fs::path p = fs::path(gameDir) / name;
        std::error_code ec;
        if (fs::exists(p, ec)) return p.string();
    }
    // Fallback: primo *.place.json trovato dentro places/.
    std::error_code ec;
    fs::path placesDir = fs::path(gameDir) / "places";
    if (fs::exists(placesDir, ec)) {
        for (const auto& entry : fs::directory_iterator(placesDir, ec)) {
            if (entry.path().extension() == ".json") return entry.path().string();
        }
    }
    return {};
}

/** writeTextFile locale: evitiamo BlockyProject.h, che tira dentro Lua
 *  attraverso SafeBytecode.h — questo strumento non ne ha bisogno. */
bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return out.good();
}

std::string slugify(std::string s) {
    for (char& c : s) {
        if (std::isalnum((unsigned char)c)) c = (char)std::tolower((unsigned char)c);
        else c = '_';
    }
    return s.empty() ? "game" : s;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "uso: build_game_index <cartella_gioco> [--id nome] [--title \"Titolo\"]\n");
        return 1;
    }

    const std::string gameDir = argv[1];
    std::string gameId, title;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--id") == 0 && i + 1 < argc) gameId = argv[++i];
        else if (std::strcmp(argv[i], "--title") == 0 && i + 1 < argc) title = argv[++i];
    }

    std::error_code ec;
    if (!fs::exists(gameDir, ec) || !fs::is_directory(gameDir, ec)) {
        std::fprintf(stderr, "  Cartella non trovata: %s\n", gameDir.c_str());
        return 1;
    }

    const std::string placePath = findPlaceFile(gameDir);
    if (placePath.empty()) {
        std::fprintf(stderr,
            "  Nessun place.json / game.blocky.json in %s — non è una cartella di gioco pubblicato\n",
            gameDir.c_str());
        return 1;
    }

    DataModel world;
    world.clear();
    if (!loadPlaceInto(placePath, world)) {
        std::fprintf(stderr, "  Impossibile leggere %s\n", placePath.c_str());
        return 1;
    }
    if (title.empty()) title = world.placeName;
    if (gameId.empty()) gameId = slugify(world.placeName);

    // --- Asset: si riusa l'audit invece di rifare l'inventario ---
    const fs::path assetsDir = fs::path(gameDir) / "assets";
    AssetAudit::Report audit;
    if (fs::exists(assetsDir, ec)) {
        audit = AssetAudit::run(assetsDir.string());
    } else {
        std::fprintf(stderr,
            "  Nessuna cartella assets/ in %s (proseguo: gioco senza asset esterni)\n",
            gameDir.c_str());
    }

    if (audit.errors > 0) {
        std::fprintf(stderr, "  L'audit degli asset ha trovato %d errori: sistema quelli prima.\n",
                     audit.errors);
        for (const auto& f : audit.findings) {
            if (f.severity == AssetAudit::Severity::Error)
                std::fprintf(stderr, "    [ERRORE] %s — %s: %s\n",
                            f.path.c_str(), f.rule.c_str(), f.detail.c_str());
        }
        return 1;
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

    const fs::path outPath = fs::path(gameDir) / "game_index.json";
    const std::string json = GameIndex::toJson(idx);
    if (!writeTextFile(outPath.string(), json)) {
        std::fprintf(stderr, "  Scrittura fallita: %s\n", outPath.string().c_str());
        return 1;
    }

    std::printf("\n  %s\n", outPath.string().c_str());
    std::printf("  gameId       %s\n", idx.gameId.c_str());
    std::printf("  contentHash  %s\n", idx.contentHash.c_str());
    std::printf("  parts        %d\n", idx.entities.parts);
    std::printf("  collider     %zu\n", idx.colliders.size());
    std::printf("  script       %zu\n", idx.scripts.size());
    std::printf("  asset        %zu (%.2f MB)\n\n",
                idx.assets.size(), (double)idx.totalBytes() / (1024.0 * 1024.0));
    return 0;
}
