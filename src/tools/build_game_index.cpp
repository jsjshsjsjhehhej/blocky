
#include <cstdio>
#include <cstring>
#include <string>

#include "engine/GamePublish.h"

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

    const GamePublish::BuildResult r = GamePublish::buildIndexForFolder(gameDir, gameId, title);

    if (!r.ok) {
        std::fprintf(stderr, "\n  Non pronto:\n");
        for (const auto& e : r.errors) std::fprintf(stderr, "    - %s\n", e.c_str());
        std::fprintf(stderr, "\n");
        return 1;
    }

    const std::string outPath = gameDir + "/game_index.json";
    if (!GamePublish::writeTextFile(outPath, r.json)) {
        std::fprintf(stderr, "  Scrittura fallita: %s\n", outPath.c_str());
        return 1;
    }

    std::printf("\n  %s\n", outPath.c_str());
    std::printf("  gameId       %s\n", r.index.gameId.c_str());
    std::printf("  contentHash  %s\n", r.index.contentHash.c_str());
    std::printf("  parts        %d\n", r.index.entities.parts);
    std::printf("  collider     %zu\n", r.index.colliders.size());
    std::printf("  script       %zu\n", r.index.scripts.size());
    std::printf("  asset        %zu (%.2f MB)\n\n",
                r.index.assets.size(), (double)r.index.totalBytes() / (1024.0 * 1024.0));
    return 0;
}
