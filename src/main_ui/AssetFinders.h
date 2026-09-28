// Ricerca del file FBX del personaggio e del modello .glb di default sul
// disco. Non e' un header riusabile: usa direttamente assetRoot() e le
// altre utility di main.cpp. Compila solo se incluso da main.cpp
// esattamente in questo punto.

static std::string findFbx() {
    std::vector<std::string> paths = { assetRoot() + "/assets/character.fbx", "assets/character.fbx" };
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return {};
}

static std::string findNoobGlb() {
    std::vector<std::string> paths = {
        assetRoot() + "/assets/roblox-noob.glb",
        "assets/roblox-noob.glb",
        assetRoot() + "/roblox-noob.glb",
        "roblox-noob.glb",
    };
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return {};
}
