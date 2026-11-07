// Test del client comandato dal server: parser JSON, flag con il tipo giusto,
// cache su disco, risposta delle versioni. Con un argomento host:porta prova
// anche un backend vero (backend/, `npm run dev`) gia' in esecuzione.

#include "engine/ClientSettings.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cs = blk::clientsettings;
namespace json = blk::json;

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("[settings] %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main(int argc, char** argv) {
    // ---- JSON ----
    {
        json::Value v;
        check(json::parse(R"({"a":[1,2.5,-3e2,true,false,null],"b":{"c":"x\"y\\z\u00e8\ud83d\ude00"}})", v)
              && v.get("a")->arr.size() == 6 && v.get("a")->arr[0].isInteger && !v.get("a")->arr[1].isInteger
              && v.get("b")->get("c")->str == "x\"y\\z\xC3\xA8\xF0\x9F\x98\x80",
              "JSON annidato, escape, unicode e surrogati");
        check(!json::parse("{\"a\":1,}", v), "virgola finale rifiutata");
        check(!json::parse("{\"a\":1} x", v), "roba dopo la fine rifiutata");
        check(!json::parse("{\"a\":\"riga\nnuova\"}", v), "a capo crudo in stringa rifiutato");
        check(!json::parse("[01]", v), "zero iniziale rifiutato");
        check(!json::parse("\"\\ud800\"", v), "surrogato spaiato rifiutato");
        std::string deep(40, '[');
        deep += std::string(40, ']');
        check(!json::parse(deep, v), "profondita' oltre 32 rifiutata");
        check(json::parse(json::quote("a\"b\\c\n\x01"), v) && v.str == "a\"b\\c\n\x01", "quote e' rileggibile");
    }

    // ---- Flag ----
    {
        cs::FlagMap f;
        check(cs::parseSettings(R"({"flags":{"FFlagA":true,"FIntB":42,"FStringC":"ciao","FIntD":1.5,"FFlagE":"si","Altro":1}})", f)
              && f.size() == 3 && f["FFlagA"].b && f["FIntB"].i == 42 && f["FStringC"].s == "ciao",
              "flag del tipo giusto tenute, le altre scartate");
        check(!cs::parseSettings(R"({"altro":{}})", f), "senza \"flags\" la risposta non vale");

        cs::FlagMap back;
        check(cs::parseSettings(cs::settingsToJson(f), back) && back.size() == 3 && back["FStringC"].s == "ciao",
              "cache: scrivi e rileggi");

        const std::string path = "test_client_settings_cache.json";
        cs::resetForTests();
        check(cs::getBool("FFlagA", false) == false && cs::getInt("FIntB", 7) == 7, "senza niente: i default nel codice");
        check(cs::saveCache(path, f) && cs::loadCache(path), "cache su disco");
        check(cs::getBool("FFlagA", false) && cs::getInt("FIntB", 7) == 42 && cs::getString("FStringC", "") == "ciao",
              "dopo la cache: i valori del server");
        check(cs::getInt("FFlagA", 9) == 9, "tipo sbagliato chiesto: default");
        check(cs::snapshot().source == cs::Source::Cache, "fonte: cache");
        std::remove(path.c_str());
        check(!cs::loadCache(path), "cache mancante: falso, nessun crash");
    }

    // ---- Versione ----
    {
        cs::VersionInfo v;
        check(cs::parseVersion(R"({"latest":"0.2.0","minSupported":"0.2.0","updateAvailable":true,"mustUpdate":true,"url":"https://x/b.zip","sha256":"ab","signature":"","notes":"n"})", v)
              && v.mustUpdate && v.latest == "0.2.0" && v.url == "https://x/b.zip", "risposta versione");
        check(cs::parseVersion(R"({"latest":"0.2.0","url":"file:///C:/x.exe"})", v) && v.url.empty(),
              "url non web scartato");
    }

    // ---- Backend vero (facoltativo) ----
    if (argc > 1) {
        const std::string addr = argv[1];
        const size_t colon = addr.rfind(':');
        const std::string host = addr.substr(0, colon);
        const int port = std::atoi(addr.c_str() + colon + 1);
        cs::resetForTests();
        cs::startFetch(host, port, "test-device-1", "test_client_settings_live.json");
        for (int i = 0; i < 100 && cs::isFetching(); ++i) {
#ifdef _WIN32
            Sleep(50);
#endif
        }
        const auto snap = cs::snapshot();
        std::printf("[settings] backend: fonte=%s errore='%s' flag=%d ultima=%s mustUpdate=%d\n",
                    cs::sourceName(snap.source), snap.lastError.c_str(), (int)snap.flags.size(),
                    snap.version.latest.c_str(), (int)snap.version.mustUpdate);
        check(snap.source == cs::Source::Server, "impostazioni ricevute dal backend");
        for (const auto& kv : snap.flags) std::printf("[settings]   %s\n", kv.first.c_str());
        std::remove("test_client_settings_live.json");
    }

    std::printf(g_fail ? "[settings] %d FALLITI\n" : "[settings] TUTTO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
