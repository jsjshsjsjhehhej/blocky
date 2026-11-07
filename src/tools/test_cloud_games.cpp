// Catalogo giochi online, dal lato del client (engine/RemoteGames.h +
// engine/WebRequest.h) contro un backend vero (backend/, npm start).
//
//   BLOCKY_TEST_BACKEND  es. http://127.0.0.1:8095   (senza: il test si salta)
//   BLOCKY_TEST_TOKEN    token di un creatore (POST /v1/admin/publishers)
//
// Pubblica un pacchetto (con file binari), lo ritrova nell'elenco, lo scarica
// e confronta i byte, lo ripubblica e controlla che si riscarichi la versione
// nuova, poi prova un token sbagliato. Lavora in una cartella dati sua
// (APPDATA va impostato dal chiamante su una cartella di prova).

#include "engine/RemoteGames.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

static int g_fail = 0;
static void check(bool ok, const std::string& what) {
    std::printf("[cloud] %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

static void writeBin(const std::string& path, const std::string& data) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f << data;
}
static std::string readBin(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

/** Aspetta il messaggio di publishToCloud. */
static std::string waitUpload(int seconds = 40) {
    for (int i = 0; i < seconds * 20; ++i) {
        const std::string m = blk::remotegames::takeUploadMessage();
        if (!m.empty()) return m;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return "(nessun esito)";
}

static bool waitDownload(int seconds = 40) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (int i = 0; i < seconds * 20; ++i) {
        if (!blk::remotegames::isDownloading()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}


int main() {
    const char* url = std::getenv("BLOCKY_TEST_BACKEND");
    const char* token = std::getenv("BLOCKY_TEST_TOKEN");
    if (!url || !url[0] || !token || !token[0]) {
        std::printf("[cloud] saltato: servono BLOCKY_TEST_BACKEND e BLOCKY_TEST_TOKEN\n");
        return 0;
    }
    const std::string server = url;
    const std::string id = "test_cloud_lab";

    // ---- Il pacchetto, come lo scrive lo Studio pubblicando ----
    const std::string pkg = blockyDataDir() + "/games_src/" + id;
    std::filesystem::remove_all(pkg);
    const std::string place1 = "{\"format\":\"BlockyPlace\",\"version\":2,\"placeName\":\"Lab\",\"parts\":[]}";
    std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + std::string("\0\r\n\xff\x00binario", 12);
    std::string mesh;
    for (int i = 0; i < 70000; ++i) mesh += (char)(i * 31 % 256);   // ~70 KB, tutti i byte
    writeBin(pkg + "/place.json", place1);
    writeBin(pkg + "/thumbnail.png", png);
    writeBin(pkg + "/scripts/main.lua", "print(\"ciao\")\r\n");
    writeBin(pkg + "/assets/meshes/auto.glb", mesh);
    writeBin(pkg + "/info.json", "{\"title\":\"Lab\"}");

    // ---- Pubblica ----
    blk::remotegames::publishToCloud(server, token, id, pkg, "Laboratorio Cloud", "prova", 6);
    std::string msg = waitUpload();
    check(msg.find("Pubblicato online") != std::string::npos, "pubblicazione: " + msg);
    check(msg.find("versione 1") != std::string::npos, "prima versione");

    // ---- Elenco ----
    std::vector<GameEntry> list = blk::remotegames::fetch(server);
    bool found = false;
    for (const auto& g : list) found = found || (g.id == id && g.title == "Laboratorio Cloud" && g.remote);
    check(found, "il gioco e' nell'elenco del catalogo (" + std::to_string(list.size()) + " giochi)");
    check(blk::remotegames::filesFor(id).version == 1, "l'elenco porta la versione 1");
    check(!blk::remotegames::thumbnailPath(id).empty()
              && readBin(blk::remotegames::thumbnailPath(id)) == png,
          "anteprima scaricata identica byte per byte");

    // ---- Download ----
    blk::remotegames::downloadGame(server, id, false);
    check(waitDownload(), "download finito");
    const std::string dl = blk::remotegames::localGameDir(id);
    check(readBin(dl + "/place.json") == place1, "place.json identico");
    check(readBin(dl + "/assets/meshes/auto.glb") == mesh, "mesh binaria identica (70 KB, tutti i byte)");
    check(readBin(dl + "/scripts/main.lua") == "print(\"ciao\")\r\n", "script identico (anche il \\r\\n)");
    check(readBin(dl + "/.version") == "1", "ricorda la versione scaricata");

    // ---- Ripubblica: il client riscarica la versione nuova ----
    const std::string place2 = "{\"format\":\"BlockyPlace\",\"version\":2,\"placeName\":\"Lab v2\",\"parts\":[]}";
    writeBin(pkg + "/place.json", place2);
    blk::remotegames::publishToCloud(server, token, id, pkg, "Laboratorio Cloud", "prova", 6);
    msg = waitUpload();
    check(msg.find("versione 2") != std::string::npos, "ripubblicazione: " + msg);
    blk::remotegames::fetch(server);
    check(blk::remotegames::filesFor(id).version == 2, "l'elenco porta la versione 2");
    blk::remotegames::downloadGame(server, id, false);
    check(waitDownload(), "secondo download finito");
    check(readBin(dl + "/place.json") == place2, "il client ha il place NUOVO (non quello in cache)");
    check(readBin(dl + "/.version") == "2", "versione locale aggiornata");

    // ---- Token sbagliato ----
    blk::remotegames::publishToCloud(server, "blk_sbagliato", id, pkg, "Furto", "", 6);
    msg = waitUpload();
    check(msg.find("non pubblicato") != std::string::npos, "token sbagliato rifiutato: " + msg);

    // ---- Un nome di file che il catalogo non accetta: errore chiaro, niente invio ----
    writeBin(pkg + "/assets/file con spazi.png", "x");
    blk::remotegames::publishToCloud(server, token, id, pkg, "Laboratorio Cloud", "prova", 6);
    msg = waitUpload();
    check(msg.find("nome di file non accettato") != std::string::npos, "nome di file con spazi: " + msg);

    if (g_fail) {
        std::printf("[cloud] %d controlli falliti\n", g_fail);
        return 1;
    }
    std::printf("ALL CLOUD TESTS PASSED\n");
    return 0;
}
