#pragma once

/**
 * DataStore — dati che sopravvivono alla chiusura del gioco (monete, livello,
 * progressi), l'equivalente del DataStoreService di Roblox. Prima di questo
 * file `game:GetService("DataStoreService")` restituiva una tabella vuota:
 * nessuno script poteva ricordarsi niente di un giocatore tra una sessione e
 * l'altra, ed e' il motivo per cui interi generi (simulator, tycoon,
 * progressione) erano fuori portata.
 *
 * Un file JSON per store in %APPDATA%/Blocky/datastores/, stesso schema
 * scritto a mano del resto del motore (published_games.json, shop.json...).
 *
 * SICUREZZA — gli script del place sono codice non fidato, quindi:
 *  - nome store e chiave passano da sanitizeKeyName(): solo [a-z0-9_-], mai
 *    barre o punti, quindi nessun modo di scrivere fuori da datastores/;
 *  - tetto al numero di chiavi per store e alla lunghezza delle stringhe,
 *    cosi' uno script in loop non riempie il disco.
 */

#include "GameHub.h"

#include <cstdio>
#include <map>
#include <string>

namespace blk::datastore {

inline constexpr size_t kMaxKeysPerStore = 5000;
inline constexpr size_t kMaxStringLen = 4096;

struct Value {
    bool isNumber = true;
    double number = 0.0;
    std::string text;
};

/** Solo [a-z0-9_-]: niente barre, punti o due punti — nessun path traversal. */
inline std::string sanitizeKeyName(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        const char l = (char)std::tolower((unsigned char)c);
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_' || l == '-')
            out.push_back(l);
        else if (l == ' ')
            out.push_back('_');
        if (out.size() >= 64) break;
    }
    return out;
}

inline std::string storesDir() { return blockyDataDir() + "/datastores"; }

inline std::string storePath(const std::string& safeName) {
    return storesDir() + "/" + safeName + ".json";
}

class Db {
public:
    /** Legge (una volta) e tiene in memoria lo store. */
    std::map<std::string, Value>& store(const std::string& safeName) {
        auto it = loaded_.find(safeName);
        if (it != loaded_.end()) return it->second;
        auto& s = loaded_[safeName];
        loadFromDisk(safeName, s);
        return s;
    }

    void save(const std::string& safeName) {
        auto it = loaded_.find(safeName);
        if (it == loaded_.end()) return;

        plat::makeDirs(storesDir());
        FILE* f = std::fopen(storePath(safeName).c_str(), "wb");
        if (!f) return;
        std::fputs("{\n", f);
        bool first = true;
        for (const auto& kv : it->second) {
            if (!first) std::fputs(",\n", f);
            first = false;
            if (kv.second.isNumber)
                std::fprintf(f, "  \"%s\": %.10g", kv.first.c_str(), kv.second.number);
            else
                std::fprintf(f, "  \"%s\": \"%s\"", kv.first.c_str(), escape(kv.second.text).c_str());
        }
        std::fputs("\n}\n", f);
        std::fclose(f);
    }

private:
    static std::string escape(const std::string& s) {
        std::string o;
        for (char c : s) {
            if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back(c); }
            else if (c == '\n') o += "\\n";
            else if ((unsigned char)c >= 0x20) o.push_back(c);
        }
        return o;
    }

    /** Parser piatto chiave/valore, stesso stile lenient di GameHub.h. */
    static void loadFromDisk(const std::string& safeName, std::map<std::string, Value>& out) {
        FILE* f = std::fopen(storePath(safeName).c_str(), "rb");
        if (!f) return;
        std::string json;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) json.append(buf, n);
        std::fclose(f);

        size_t pos = 0;
        while (out.size() < kMaxKeysPerStore) {
            const size_t q1 = json.find('"', pos);
            if (q1 == std::string::npos) break;
            const size_t q2 = json.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            const std::string key = json.substr(q1 + 1, q2 - q1 - 1);
            const size_t colon = json.find(':', q2);
            if (colon == std::string::npos) break;

            size_t i = colon + 1;
            while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
                ++i;
            if (i >= json.size()) break;

            Value v;
            if (json[i] == '"') {
                const size_t s2 = json.find('"', i + 1);
                if (s2 == std::string::npos) break;
                v.isNumber = false;
                v.text = json.substr(i + 1, s2 - i - 1);
                pos = s2 + 1;
            } else {
                v.isNumber = true;
                v.number = std::atof(json.c_str() + i);
                pos = json.find_first_of(",\n}", i);
                if (pos == std::string::npos) pos = json.size();
            }
            if (!key.empty()) out[key] = v;
        }
    }

    std::map<std::string, std::map<std::string, Value>> loaded_;
};

inline Db& db() {
    static Db instance;
    return instance;
}

} // namespace blk::datastore
