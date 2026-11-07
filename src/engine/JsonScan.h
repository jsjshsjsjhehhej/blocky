#pragma once

/**
 * Due aiuti per leggere i file dei place senza un parser JSON completo, ma
 * senza farsi ingannare dal contenuto delle stringhe.
 *
 * Prima i caricatori cercavano la fine di una part con find('}') e le
 * sezioni con find("\"labels\""): uno script con una tabella Luau ({}) o con
 * la parola "labels" tra virgolette spezzava il place (part fantasma, sezioni
 * lette dal punto sbagliato). Qui le stringhe si saltano, con i loro escape.
 */

#include <string>

namespace blk {
namespace jsonscan {

/** Fine dell'oggetto o array che apre in json[open] ('{' o '['); npos se non c'e'. */
inline size_t matchingEnd(const std::string& json, size_t open) {
    if (open >= json.size() || (json[open] != '{' && json[open] != '[')) return std::string::npos;
    int depth = 0;
    bool inString = false;
    for (size_t i = open; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == '{' || c == '[') ++depth;
        else if ((c == '}' || c == ']') && --depth == 0) return i;
    }
    return std::string::npos;
}

/**
 * Posizione della chiave di primo livello (es. "parts") dell'oggetto radice,
 * fuori da qualsiasi stringa; npos se non c'e'. Punta alle virgolette aperte.
 */
inline size_t topKey(const std::string& json, const std::string& key) {
    int depth = 0;
    for (size_t i = 0; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '"') {
            const size_t start = i;
            std::string tok;
            size_t j = i + 1;
            for (; j < json.size() && json[j] != '"'; ++j) {
                if (json[j] == '\\' && j + 1 < json.size()) { tok += json[++j]; continue; }
                tok += json[j];
            }
            i = j;   // sulle virgolette di chiusura
            if (depth == 1 && tok == key) {
                size_t k = j + 1;
                while (k < json.size() && (json[k] == ' ' || json[k] == '\t' || json[k] == '\r' || json[k] == '\n')) ++k;
                if (k < json.size() && json[k] == ':') return start;
            }
            continue;
        }
        if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') --depth;
    }
    return std::string::npos;
}

/** Inizio ('[' o '{') del valore della chiave di primo livello `key`; npos se manca. */
inline size_t topValue(const std::string& json, const std::string& key, char open) {
    const size_t k = topKey(json, key);
    if (k == std::string::npos) return std::string::npos;
    const size_t colon = json.find(':', k + key.size() + 2);
    if (colon == std::string::npos) return std::string::npos;
    size_t v = colon + 1;
    while (v < json.size() && (json[v] == ' ' || json[v] == '\t' || json[v] == '\r' || json[v] == '\n')) ++v;
    return (v < json.size() && json[v] == open) ? v : std::string::npos;
}

} // namespace jsonscan
} // namespace blk
