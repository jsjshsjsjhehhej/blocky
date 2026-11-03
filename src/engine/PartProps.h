#pragma once

// Il "generatore" delle proprieta' delle part: ogni riga di PartProps.def
// diventa codice qui (salvataggio/caricamento), in PartPropsLua.h (Lua) e in
// PartPropsUi.h (Studio). Aggiungere una proprieta' = aggiungere una riga.

#include "DataModel.h"

#include <algorithm>
#include <cstdlib>
#include <ostream>
#include <string>

namespace partprops {

inline bool clampProp(bool v, float, float) { return v; }
inline float clampProp(float v, float mn, float mx) {
    if (!(v == v)) return mn;   // NaN da uno script o da un file rovinato
    return std::clamp(v, mn, mx);
}

inline void writeJson(std::ostream& o, bool v) { o << (v ? "true" : "false"); }
inline void writeJson(std::ostream& o, float v) { o << v; }

// Cerca la chiave solo dentro l'oggetto di QUESTA part [from, limit): una
// chiave che manca (file salvato prima che la proprieta' esistesse) deve
// dare il default, non il valore della part successiva.
inline size_t findValue(const std::string& json, const char* key, size_t from, size_t limit) {
    const std::string k = std::string("\"") + key + "\"";
    size_t at = json.find(k, from);
    if (at == std::string::npos || at >= limit) return std::string::npos;
    size_t c = json.find(':', at + k.size());
    if (c == std::string::npos || c >= limit) return std::string::npos;
    ++c;
    while (c < limit && (json[c] == ' ' || json[c] == '\t')) ++c;
    return c;
}

inline void readJson(const std::string& json, size_t at, bool& out) {
    if (json.compare(at, 4, "true") == 0) out = true;
    else if (json.compare(at, 5, "false") == 0) out = false;
}
inline void readJson(const std::string& json, size_t at, float& out) {
    char* end = nullptr;
    float v = std::strtof(json.c_str() + at, &end);
    if (end != json.c_str() + at) out = v;
}

// ,"anchored":true,"canCollide":true,... (con la virgola davanti a ognuna)
inline void writeAll(std::ostream& o, const WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) \
    o << ",\"" jsonKey "\":"; writeJson(o, p.field);
#include "PartProps.def"
#undef PART_PROP
}

inline void readAll(const std::string& json, size_t from, size_t limit, WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help)             \
    {                                                                         \
        size_t at = findValue(json, jsonKey, from, limit);                    \
        if (at != std::string::npos) readJson(json, at, p.field);            \
        p.field = clampProp(p.field, mn, mx);                                 \
    }
#include "PartProps.def"
#undef PART_PROP
}

} // namespace partprops
