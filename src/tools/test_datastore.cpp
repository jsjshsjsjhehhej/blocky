// Test del DataStore: la cosa che conta non e' "la funzione ritorna un
// numero", ma che il dato **sopravviva alla chiusura del gioco**. Quindi il
// test crea un runtime, scrive, lo distrugge, ne crea un ALTRO e rilegge —
// se il valore torna, la persistenza e' vera (non memoria di processo).
// Verifica anche che una chiave malevola non possa scrivere fuori cartella.

#include "engine/LuaRuntime.h"
#include "engine/DataStore.h"

#include <cstdio>
#include <string>

static bool g_ok = true;

static void check(bool cond, const char* what) {
    std::printf("%-58s %s\n", what, cond ? "OK" : "FALLITO");
    if (!cond) g_ok = false;
}

static bool runScript(const char* code, std::string& err) {
    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    if (!lua.init(&dm)) { err = lua.lastError; return false; }
    if (!lua.runString(code)) { err = lua.lastError; return false; }
    return true;
}

int main() {
    const std::string file = blk::datastore::storePath("testsoldi");
    std::remove(file.c_str());

    std::string err;

    // --- Sessione 1: scrive ---
    bool ok1 = runScript(R"(
        local store = game:GetService("DataStoreService"):GetDataStore("TestSoldi")
        store:SetAsync("walter", 100)
        local nuovo = store:IncrementAsync("walter", 50)
        assert(nuovo == 150, "IncrementAsync doveva tornare 150, ha tornato " .. tostring(nuovo))
        store:SetAsync("nome", "Walter")
    )", err);
    check(ok1, "sessione 1: SetAsync + IncrementAsync");
    if (!ok1) std::printf("   errore: %s\n", err.c_str());

    // --- Il file esiste davvero su disco? ---
    FILE* f = std::fopen(file.c_str(), "rb");
    check(f != nullptr, "il file dello store esiste su disco");
    if (f) std::fclose(f);

    // --- Sessione 2: runtime NUOVO, rilegge (la prova vera) ---
    bool ok2 = runScript(R"(
        local store = game:GetService("DataStoreService"):GetDataStore("TestSoldi")
        local soldi = store:GetAsync("walter")
        assert(soldi == 150, "dopo il riavvio i soldi dovevano essere 150, sono " .. tostring(soldi))
        local nome = store:GetAsync("nome")
        assert(nome == "Walter", "il nome doveva essere Walter, e' " .. tostring(nome))
        local vuoto = store:GetAsync("mai_scritto")
        assert(vuoto == nil, "una chiave mai scritta deve dare nil")
    )", err);
    check(ok2, "sessione 2 (runtime nuovo): il dato e' sopravvissuto");
    if (!ok2) std::printf("   errore: %s\n", err.c_str());

    // --- RemoveAsync ---
    bool ok3 = runScript(R"(
        local store = game:GetService("DataStoreService"):GetDataStore("TestSoldi")
        store:RemoveAsync("walter")
        assert(store:GetAsync("walter") == nil, "dopo RemoveAsync deve dare nil")
    )", err);
    check(ok3, "RemoveAsync cancella la chiave");
    if (!ok3) std::printf("   errore: %s\n", err.c_str());

    // --- Sicurezza: nomi con path traversal vengono ripuliti ---
    check(blk::datastore::sanitizeKeyName("../../evil") == "evil",
          "sanitize: '../../evil' -> 'evil' (niente barre)");
    check(blk::datastore::sanitizeKeyName("C:\\Windows\\system32") == "cwindowssystem32",
          "sanitize: percorso Windows neutralizzato");
    check(blk::datastore::sanitizeKeyName("Soldi Giocatore") == "soldi_giocatore",
          "sanitize: spazi -> underscore, minuscolo");

    bool ok4 = runScript(R"(
        local dss = game:GetService("DataStoreService")
        local store = dss:GetDataStore("../../fuori")
        store:SetAsync("x", 1)
    )", err);
    check(ok4, "uno store con nome malevolo non fa crashare");
    FILE* escaped = std::fopen((blk::datastore::storesDir() + "/../../fuori.json").c_str(), "rb");
    check(escaped == nullptr, "nessun file scritto fuori da datastores/");
    if (escaped) std::fclose(escaped);

    std::printf("\n%s\n", g_ok ? "TUTTO OK" : "QUALCOSA NON VA");
    return g_ok ? 0 : 1;
}
