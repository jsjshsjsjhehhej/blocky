#pragma once

/**
 * Il segreto condiviso che protegge le scritture sul server dei giochi.
 *
 * ── Perche' serve ─────────────────────────────────────────────────────────
 * blocky_hostd espone due rotte che ACCETTANO dati:
 *
 *     PUT /publish/<id>    un gioco intero, come archivio zip
 *     PUT /catalog         gli oggetti del catalogo, come archivio zip
 *
 * Finche' il server gira in casa dietro il CGNAT dell'operatore non succede
 * niente, perche' da fuori nessuno lo raggiunge. Su un VPS con IP pubblico
 * la situazione si ribalta: senza un controllo, CHIUNQUE su internet puo'
 * sostituire i giochi pubblicati, riempire il disco, o infilare quello che
 * vuole nel catalogo che poi tutti i client scaricano.
 *
 * Le rotte GET restano aperte: servono ai giocatori per vedere e scaricare i
 * giochi, ed e' esattamente cio' che il server deve fare.
 *
 * ── Come funziona ─────────────────────────────────────────────────────────
 * Un segreto solo, condiviso fra chi pubblica e il server, mandato
 * nell'intestazione X-Blocky-Token. Non e' un sistema di account: e' la
 * chiave di casa. Ed e' la cosa giusta qui, perche' chi pubblica sei tu.
 *
 * Al primo avvio, se non esiste, ne viene generato uno e scritto in
 * <root>/.token, stampandolo una volta sola. Cosi' il server non parte MAI
 * senza protezione per dimenticanza — che e' il modo in cui queste cose
 * finiscono davvero esposte.
 */

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace blk {
namespace servertoken {

namespace fs = std::filesystem;

/** Confronto a tempo costante: non rivela quanti caratteri erano giusti. */
inline bool equalsConstantTime(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

inline std::string generate() {
    static const char* kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    // random_device puo' essere finto su alcune piattaforme: si mescola con
    // l'orologio, cosi' anche nel caso peggiore il risultato non e' prevedibile.
    std::mt19937_64 gen(((std::uint64_t)rd() << 32) ^ (std::uint64_t)rd()
        ^ (std::uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> pick(0, 61);

    std::string out;
    out.reserve(40);
    for (int i = 0; i < 40; ++i) out.push_back(kAlphabet[pick(gen)]);
    return out;
}

inline std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) ++start;
    return s.substr(start);
}

/**
 * Carica il token, creandolo se manca.
 *
 * Ordine: quello passato a mano (riga di comando) vince, poi la variabile
 * d'ambiente, poi il file. Serve per i VPS, dove il token lo si mette nella
 * configurazione del servizio invece che in un file sul disco.
 */
inline std::string loadOrCreate(const std::string& rootDir,
                                const std::string& fromCommandLine,
                                bool& outCreated) {
    outCreated = false;

    const std::string cli = trimmed(fromCommandLine);
    if (!cli.empty()) return cli;

    if (const char* env = std::getenv("BLOCKY_TOKEN")) {
        const std::string e = trimmed(env);
        if (!e.empty()) return e;
    }

    std::error_code ec;
    fs::create_directories(rootDir, ec);
    const fs::path path = fs::path(rootDir) / ".token";

    {
        std::ifstream in(path);
        if (in) {
            std::string line;
            std::getline(in, line);
            const std::string t = trimmed(line);
            if (t.size() >= 16) return t;
        }
    }

    const std::string fresh = generate();
    std::ofstream out(path, std::ios::trunc);
    if (out) {
        out << fresh << "\n";
        outCreated = true;
    }
    return fresh;
}

} // namespace servertoken
} // namespace blk
