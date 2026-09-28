#pragma once

/**
 * Feature flag esterni: assets/config/config.json, un oggetto piatto
 * "nome": true/false. Si legge una volta sola (lazy, alla prima chiamata di
 * configFlags()) e resta in memoria per tutta la sessione — non è pensato
 * per cambiare a runtime, solo per accendere/spegnere cose senza ricompilare.
 *
 * File assente, vuoto o malformato = tutti i flag tornano il loro default:
 * mai un motivo per cui il gioco non parta.
 */

#include "../engine/GameHub.h"

#include <fstream>
#include <map>
#include <string>

struct ConfigFlags {
    std::map<std::string, bool> flags;

    bool get(const std::string& name, bool def) const {
        auto it = flags.find(name);
        return it != flags.end() ? it->second : def;
    }
};

// Parser volutamente semplice, stesso stile degli altri jsonFind* del
// motore (GameHub.h): scorre l'intero testo cercando "chiave": true/false,
// senza capire la struttura ad oggetti — sufficiente per un file piatto di
// soli booleani, e non si rompe su virgole/spazi/ordine diverso.
inline void parseConfigFlags(const std::string& json, std::map<std::string, bool>& out) {
    size_t pos = 0;
    while (true) {
        size_t q1 = json.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string key = json.substr(q1 + 1, q2 - q1 - 1);
        size_t c = json.find(':', q2);
        if (c == std::string::npos) break;
        size_t i = c + 1;
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
            ++i;
        if (json.compare(i, 4, "true") == 0) { out[key] = true; pos = i + 4; }
        else if (json.compare(i, 5, "false") == 0) { out[key] = false; pos = i + 5; }
        else pos = q2 + 1; // valore non booleano (stringa/numero/oggetto): salta la chiave
    }
}

inline ConfigFlags& configFlags() {
    static ConfigFlags cf;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string path = resolveScriptPath("config/config.json");
        std::ifstream f(path, std::ios::binary);
        if (f) {
            std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            parseConfigFlags(json, cf.flags);
            std::fprintf(stdout, "[Config] %s: %d flag caricati\n", path.c_str(), (int)cf.flags.size());
        }
    }
    return cf;
}
