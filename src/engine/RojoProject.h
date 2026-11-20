#pragma once

/**
 * Progetti Rojo in Blocky.
 *
 * Un progetto Rojo e' una cartella con default.project.json e i file .luau:
 * si scrive il codice con l'editor che si vuole (VS Code, un'AI...) e lo Studio
 * lo tiene allineato da solo, come il plugin Rojo in Roblox Studio.
 *
 * Stessa forma dei progetti Rojo veri ("tree", "$path", "$className"), stesse
 * regole sui nomi dei file:
 *   nome.server.luau  -> Script        (anche .lua)
 *   nome.client.luau  -> LocalScript
 *   nome.luau         -> ModuleScript
 *   cartella/init.server.luau | init.client.luau | init.luau
 *                     -> la cartella diventa quello script (con i figli dentro)
 *   nome.json         -> ModuleScript che restituisce i dati
 *   cartella          -> Folder
 *
 * Servizi: ReplicatedStorage (anche ReplicatedFirst), ServerStorage,
 * ServerScriptService. StarterPlayer.StarterPlayerScripts /
 * StarterCharacterScripts e StarterGui finiscono in una cartella con quel nome
 * dentro ReplicatedStorage: in Blocky i LocalScript partono ovunque siano.
 * Workspace, Lighting, Players e gli altri servizi del motore: se nel
 * progetto sono vuoti non danno avviso. I figli script o cartelle entrano
 * come cartella con quel nome. I file .rbxm / .model.json restano un avviso.
 *
 * Nessuna dipendenza grafica: lo prova tools/test_rojo.cpp.
 */

#include "engine/BlockyLuauDefs.h"
#include "engine/DataModel.h"
#include "engine/MiniJson.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace blk {
namespace rojo {

namespace fs = std::filesystem;

struct Entry {
    SharedService service = SharedService::ReplicatedStorage;
    std::string className = "Folder";
    std::string name;
    int parent = -1;        // indice in Project::entries; -1 = radice del servizio
    std::string file;       // file del sorgente (vuoto per Folder / Remote)
};

struct Project {
    std::string name;
    std::string projectFile;   // percorso del default.project.json
    std::string dir;           // cartella del progetto
    std::vector<Entry> entries;
    std::vector<std::string> warnings;
};

namespace detail {

inline bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

inline std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/** Classe e nome dal nome del file, regole di Rojo. Vuoto = file da ignorare. */
inline bool classifyFile(const std::string& fileName, std::string& cls, std::string& name) {
    const std::string low = lower(fileName);
    struct Rule { const char* suffix; const char* cls; };
    static const Rule rules[] = {
        {".server.luau", "Script"}, {".server.lua", "Script"},
        {".client.luau", "LocalScript"}, {".client.lua", "LocalScript"},
        {".luau", "ModuleScript"}, {".lua", "ModuleScript"},
    };
    if (endsWith(low, ".meta.json") || endsWith(low, ".project.json")) return false;
    for (const auto& r : rules) {
        if (endsWith(low, r.suffix)) {
            cls = r.cls;
            name = fileName.substr(0, fileName.size() - std::strlen(r.suffix));
            return true;
        }
    }
    if (endsWith(low, ".json")) {
        cls = "ModuleScript";   // dati: si legge con require()
        name = fileName.substr(0, fileName.size() - 5);
        return true;
    }
    return false;
}

inline bool isInitFile(const std::string& fileName, std::string& cls) {
    const std::string low = lower(fileName);
    if (low == "init.server.luau" || low == "init.server.lua") { cls = "Script"; return true; }
    if (low == "init.client.luau" || low == "init.client.lua") { cls = "LocalScript"; return true; }
    if (low == "init.luau" || low == "init.lua") { cls = "ModuleScript"; return true; }
    return false;
}

inline bool unsupportedFile(const std::string& fileName) {
    const std::string low = lower(fileName);
    return endsWith(low, ".rbxm") || endsWith(low, ".rbxmx") || endsWith(low, ".model.json")
        || endsWith(low, ".csv") || endsWith(low, ".txt");
}

/** File e cartelle in ordine di nome: stesso ordine a ogni lettura. */
inline std::vector<fs::directory_entry> sortedDir(const fs::path& dir) {
    std::vector<fs::directory_entry> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) out.push_back(e);
    std::sort(out.begin(), out.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() < b.path().filename().string();
    });
    return out;
}

struct Builder {
    Project& p;
    SharedService service;

    int add(const std::string& cls, const std::string& name, int parent, const std::string& file) {
        Entry e;
        e.service = service;
        e.className = cls;
        e.name = name;
        e.parent = parent;
        e.file = file;
        p.entries.push_back(std::move(e));
        return (int)p.entries.size() - 1;
    }

    /** Contenuto di una cartella dentro `parent`. */
    void addDirChildren(const fs::path& dir, int parent, int depth) {
        if (depth > 32) { p.warnings.push_back("cartelle troppo profonde: " + dir.string()); return; }
        for (const auto& e : sortedDir(dir)) {
            const std::string fn = e.path().filename().string();
            if (fn.empty() || fn[0] == '.') continue;   // .git, .vscode...
            std::string cls;
            if (e.is_directory()) {
                addPath(e.path(), fn, parent, depth + 1);
            } else if (isInitFile(fn, cls)) {
                continue;   // gia' usato dalla cartella
            } else {
                std::string name;
                if (classifyFile(fn, cls, name)) add(cls, name, parent, e.path().string());
                else if (unsupportedFile(fn)) p.warnings.push_back("non ancora supportato, saltato: " + e.path().string());
            }
        }
    }

    /** Un $path: file singolo o cartella (con init.* la cartella diventa script). */
    int addPath(const fs::path& path, const std::string& name, int parent, int depth) {
        std::error_code ec;
        if (fs::is_directory(path, ec)) {
            std::string initCls, initFile;
            for (const auto& e : sortedDir(path)) {
                std::string cls;
                if (!e.is_directory() && isInitFile(e.path().filename().string(), cls)) {
                    initCls = cls;
                    initFile = e.path().string();
                    break;
                }
            }
            const int me = add(initCls.empty() ? "Folder" : initCls, name, parent, initFile);
            addDirChildren(path, me, depth);
            return me;
        }
        if (fs::is_regular_file(path, ec)) {
            std::string cls, fileName;
            if (classifyFile(path.filename().string(), cls, fileName)) return add(cls, name, parent, path.string());
            p.warnings.push_back("file non supportato: " + path.string());
            return -1;
        }
        p.warnings.push_back("$path non trovato: " + path.string());
        return -1;
    }

    /** Un nodo del "tree" (dentro un servizio). */
    void addNode(const std::string& name, const json::Value& node, int parent, int depth) {
        if (!node.isObject() || depth > 32) return;
        int me = -1;
        const json::Value* path = node.get("$path");
        const json::Value* cls = node.get("$className");
        if (path && path->type == json::Value::Type::String) {
            me = addPath(fs::path(p.dir) / path->str, name, parent, depth);
        } else {
            std::string c = cls && cls->type == json::Value::Type::String ? cls->str : "Folder";
            static const char* ok[] = {"Folder", "RemoteEvent", "RemoteFunction", "ModuleScript", "Script", "LocalScript"};
            bool known = false;
            for (const char* k : ok) known = known || c == k;
            if (!known) {
                p.warnings.push_back("classe \"" + c + "\" non supportata per \"" + name + "\": diventa Folder");
                c = "Folder";
            }
            me = add(c, name, parent, "");
        }
        if (me < 0) return;
        for (const auto& kv : node.obj) {
            if (!kv.first.empty() && kv.first[0] == '$') continue;
            addNode(kv.first, kv.second, me, depth + 1);
        }
    }

    /** Figli di un servizio (o di un nodo che diventa cartella radice). */
    void addServiceChildren(const json::Value& node, int parent) {
        const json::Value* path = node.get("$path");
        if (path && path->type == json::Value::Type::String) {
            const fs::path pp = fs::path(p.dir) / path->str;
            std::error_code ec;
            if (fs::is_directory(pp, ec)) addDirChildren(pp, parent, 0);
            else p.warnings.push_back("$path di un servizio deve essere una cartella: " + pp.string());
        }
        for (const auto& kv : node.obj) {
            if (!kv.first.empty() && kv.first[0] == '$') continue;
            addNode(kv.first, kv.second, parent, 0);
        }
    }
};

/** JSON -> tabella Luau (per i moduli .json). */
inline void jsonToLua(const json::Value& v, std::string& out, int depth = 0) {
    using T = json::Value::Type;
    if (depth > 64) { out += "nil"; return; }
    switch (v.type) {
    case T::Null: out += "nil"; break;
    case T::Bool: out += v.b ? "true" : "false"; break;
    case T::Number: {
        std::ostringstream o;
        o.precision(17);
        o << v.num;
        out += o.str();
        break;
    }
    case T::String: {
        out += '"';
        for (char c : v.str) {
            if (c == '"' || c == '\\') { out += '\\'; out += c; }
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c;
        }
        out += '"';
        break;
    }
    case T::Array:
        out += "{";
        for (size_t i = 0; i < v.arr.size(); ++i) {
            if (i) out += ", ";
            jsonToLua(v.arr[i], out, depth + 1);
        }
        out += "}";
        break;
    case T::Object: {
        out += "{";
        bool first = true;
        for (const auto& kv : v.obj) {
            if (!first) out += ", ";
            first = false;
            json::Value key;
            key.type = T::String;
            key.str = kv.first;
            out += "[";
            jsonToLua(key, out, depth + 1);
            out += "] = ";
            jsonToLua(kv.second, out, depth + 1);
        }
        out += "}";
        break;
    }
    }
}

} // namespace detail

/** Legge default.project.json e la struttura delle cartelle (senza i sorgenti). */
inline bool loadProject(const std::string& projectFile, Project& out, std::string& err) {
    out = Project{};
    out.projectFile = projectFile;
    std::error_code ec;
    out.dir = fs::path(projectFile).parent_path().string();
    std::ifstream f(projectFile, std::ios::binary);
    if (!f) { err = "non trovo " + projectFile; return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    json::Value root;
    if (!json::parse(ss.str(), root) || !root.isObject()) { err = "default.project.json non e' JSON valido"; return false; }
    const json::Value* name = root.get("name");
    out.name = name && name->type == json::Value::Type::String ? name->str : fs::path(out.dir).filename().string();
    const json::Value* tree = root.get("tree");
    if (!tree || !tree->isObject()) { err = "manca \"tree\" nel progetto"; return false; }

    for (const auto& kv : tree->obj) {
        const std::string& svcName = kv.first;
        if (!svcName.empty() && svcName[0] == '$') continue;
        const json::Value& node = kv.second;
        detail::Builder b{out, SharedService::ReplicatedStorage};
        if (svcName == "ReplicatedStorage" || svcName == "ReplicatedFirst") {
            b.service = SharedService::ReplicatedStorage;
            b.addServiceChildren(node, -1);
        } else if (svcName == "ServerStorage") {
            b.service = SharedService::ServerStorage;
            b.addServiceChildren(node, -1);
        } else if (svcName == "ServerScriptService") {
            b.service = SharedService::ServerScriptService;
            b.addServiceChildren(node, -1);
        } else if (svcName == "StarterPlayer") {
            // StarterPlayerScripts / StarterCharacterScripts -> cartelle in ReplicatedStorage.
            b.service = SharedService::ReplicatedStorage;
            for (const auto& sub : node.obj) {
                if (!sub.first.empty() && sub.first[0] == '$') continue;
                const int folder = b.add("Folder", sub.first, -1, "");
                b.addServiceChildren(sub.second, folder);
            }
        } else if (svcName == "StarterGui") {
            b.service = SharedService::ReplicatedStorage;
            const int folder = b.add("Folder", "StarterGui", -1, "");
            b.addServiceChildren(node, folder);
        } else if (svcName == "Workspace" || svcName == "Lighting" || svcName == "Players"
                   || svcName == "SoundService" || svcName == "StarterPack" || svcName == "Teams"
                   || svcName == "Chat" || svcName == "TextChatService") {
            // Servizi che il motore ha già. Senza figli non c'è niente da importare.
            // Con figli (script, cartelle) li tiene una cartella con il nome del servizio.
            bool hasChild = false;
            if (node.isObject()) {
                const json::Value* props = node.get("$properties");
                if (props && props->isObject() && !props->obj.empty())
                    out.warnings.push_back("servizio \"" + svcName + "\": le proprieta' del progetto non si applicano ancora");
                for (const auto& sub : node.obj) {
                    if (!sub.first.empty() && sub.first[0] != '$') { hasChild = true; break; }
                }
            }
            if (hasChild) {
                b.service = SharedService::ReplicatedStorage;
                const int folder = b.add("Folder", svcName, -1, "");
                b.addServiceChildren(node, folder);
            }
        } else {
            out.warnings.push_back("servizio \"" + svcName + "\" non ancora supportato: saltato");
        }
    }
    return true;
}

/** Sorgente di una voce (file .luau, o tabella Luau per i .json). */
inline bool readSource(const Entry& e, std::string& src) {
    src.clear();
    if (e.file.empty()) return true;
    std::ifstream f(e.file, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    src = ss.str();
    if (src.size() >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF)
        src.erase(0, 3);   // BOM di Windows
    if (detail::endsWith(detail::lower(e.file), ".json")) {
        json::Value v;
        if (!json::parse(src, v)) return false;
        std::string lua = "return ";
        detail::jsonToLua(v, lua);
        src = lua + "\n";
    }
    return true;
}

/**
 * Mette il progetto nel place: toglie le Instance sincronizzate prima (o con
 * lo stesso nome nella stessa radice, per non avere doppioni dopo un
 * salvataggio) e aggiunge quelle del progetto. Restituisce quante.
 */
inline int applyToWorld(const Project& p, DataModel& w, std::vector<std::string>* errors = nullptr) {
    // Quali radici possiede il progetto (servizio + nome di primo livello).
    std::map<std::pair<int, std::string>, bool> ownedRoots;
    for (const auto& e : p.entries)
        if (e.parent < 0) ownedRoots[{(int)e.service, e.name}] = true;

    // 1) Le Instance che restano, con i parentIndex rinumerati.
    std::vector<char> drop(w.instances.size(), 0);
    for (size_t i = 0; i < w.instances.size(); ++i) {
        const auto& s = w.instances[i];
        if (!s.rojoFile.empty() || s.rojoSynced) drop[i] = 1;
        if (s.parentIndex < 0 && ownedRoots.count({(int)s.service, s.name})) drop[i] = 1;
    }
    // figli di una Instance tolta: via anche loro
    for (bool changed = true; changed;) {
        changed = false;
        for (size_t i = 0; i < w.instances.size(); ++i) {
            const int par = w.instances[i].parentIndex;
            if (!drop[i] && par >= 0 && par < (int)drop.size() && drop[(size_t)par]) { drop[i] = 1; changed = true; }
        }
    }
    std::vector<int> remap(w.instances.size(), -1);
    std::vector<SharedInstance> kept;
    for (size_t i = 0; i < w.instances.size(); ++i) {
        if (drop[i]) continue;
        remap[i] = (int)kept.size();
        kept.push_back(w.instances[i]);
    }
    for (auto& s : kept)
        if (s.parentIndex >= 0) s.parentIndex = s.parentIndex < (int)remap.size() ? remap[(size_t)s.parentIndex] : -1;

    // 2) Le voci del progetto.
    const int base = (int)kept.size();
    int n = 0;
    for (size_t i = 0; i < p.entries.size(); ++i) {
        const Entry& e = p.entries[i];
        SharedInstance s;
        s.name = e.name;
        s.className = e.className;
        s.service = e.service;
        s.parentIndex = e.parent >= 0 ? base + e.parent : -1;
        s.scriptType = e.className == "Script" ? ScriptType::Server
                     : e.className == "LocalScript" ? ScriptType::Client
                     : ScriptType::ModuleScript;
        s.rojoFile = e.file.empty() ? ("dir:" + e.name) : e.file;
        s.rojoSynced = true;
        if (!readSource(e, s.source) && errors) errors->push_back("non riesco a leggere " + e.file);
        kept.push_back(std::move(s));
        ++n;
    }
    w.instances = std::move(kept);
    return n;
}

/** Toglie dal place tutto quello che arriva dal progetto. */
inline int removeFromWorld(DataModel& w) {
    Project empty;
    const size_t before = w.instances.size();
    applyToWorld(empty, w);
    return (int)(before - w.instances.size());
}

/**
 * Sincronizzazione dal vivo: chiamare poll() ogni tanto (lo Studio lo fa ogni
 * mezzo secondo). Un file cambiato aggiorna solo il suo sorgente; un file o una
 * cartella aggiunti / tolti rifanno tutta la struttura.
 */
class Watcher {
public:
    bool connect(const std::string& projectFile, DataModel& w, std::string& err) {
        Project p;
        if (!loadProject(projectFile, p, err)) return false;
        project_ = std::move(p);
        errors_.clear();
        applyToWorld(project_, w, &errors_);
        stamp_ = takeStamp();
        connected_ = true;
        return true;
    }

    void disconnect(DataModel* w = nullptr) {
        if (w) removeFromWorld(*w);
        connected_ = false;
        project_ = Project{};
        stamp_.clear();
    }

    bool connected() const { return connected_; }
    const Project& project() const { return project_; }
    const std::vector<std::string>& errors() const { return errors_; }

    /**
     * Controlla i file. changed riceve i nomi degli script aggiornati.
     * Restituisce true se la STRUTTURA e' cambiata (place ricostruito).
     */
    bool poll(DataModel& w, std::vector<std::string>& changed) {
        changed.clear();
        if (!connected_) return false;
        Project fresh;
        std::string err;
        if (!loadProject(project_.projectFile, fresh, err)) { errors_ = {err}; return false; }
        if (structureKey(fresh) != structureKey(project_)) {
            project_ = std::move(fresh);
            errors_.clear();
            applyToWorld(project_, w, &errors_);
            stamp_ = takeStamp();
            changed.push_back("(struttura del progetto)");
            return true;
        }
        const auto now = takeStamp();
        for (const auto& kv : now) {
            const auto it = stamp_.find(kv.first);
            if (it != stamp_.end() && it->second == kv.second) continue;
            for (auto& s : w.instances) {
                if (s.rojoFile != kv.first) continue;
                Entry e;
                e.file = kv.first;
                std::string src;
                if (readSource(e, src)) { s.source = src; changed.push_back(s.name); }
                else errors_.push_back("non riesco a leggere " + kv.first);
            }
        }
        stamp_ = now;
        return false;
    }

private:
    Project project_;
    std::map<std::string, long long> stamp_;
    std::vector<std::string> errors_;
    bool connected_ = false;

    std::map<std::string, long long> takeStamp() const {
        std::map<std::string, long long> m;
        // Impronta del CONTENUTO: la data di modifica su Windows (MinGW) non e'
        // affidabile, e alcuni editor la lasciano uguale. Gli script sono
        // piccoli: rileggerli ogni mezzo secondo costa poco.
        for (const auto& e : project_.entries) {
            if (e.file.empty()) continue;
            std::ifstream f(e.file, std::ios::binary);
            if (!f) { m[e.file] = -1; continue; }
            unsigned long long h = 1469598103934665603ull;
            char buf[4096];
            while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
                const std::streamsize n = f.gcount();
                for (std::streamsize i = 0; i < n; ++i) { h ^= (unsigned char)buf[i]; h *= 1099511628211ull; }
                if (n < (std::streamsize)sizeof(buf)) break;
            }
            m[e.file] = (long long)h;
        }
        return m;
    }

    static std::string structureKey(const Project& p) {
        std::string k;
        for (const auto& e : p.entries)
            k += std::to_string((int)e.service) + "|" + e.className + "|" + e.name + "|" + std::to_string(e.parent) + "|" + e.file + "\n";
        return k;
    }
};

/**
 * Esporta gli script condivisi del place come progetto Rojo (per passare a
 * VS Code / AI un place fatto nello Studio). Restituisce quanti file scritti.
 */
inline int exportPlace(const DataModel& w, const std::string& dir, const std::string& projectName, std::string& err) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) { err = "non riesco a creare " + dir; return -1; }
    static const char* svcDir[] = {"ReplicatedStorage", "ServerStorage", "ServerScriptService"};
    auto safe = [](std::string s) {
        for (char& c : s)
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        return s.empty() ? std::string("Instance") : s;
    };
    // Percorso di una Instance: src/<Servizio>/<cartelle...>
    std::function<fs::path(int)> pathOf = [&](int i) -> fs::path {
        const auto& s = w.instances[(size_t)i];
        if (s.parentIndex < 0 || s.parentIndex >= (int)w.instances.size())
            return fs::path(dir) / "src" / svcDir[(int)s.service] / safe(s.name);
        return pathOf(s.parentIndex) / safe(s.name);
    };
    auto hasChildren = [&](int i) {
        for (const auto& s : w.instances) if (s.parentIndex == i) return true;
        return false;
    };
    auto suffix = [](const std::string& cls) {
        return cls == "Script" ? std::string(".server.luau") : cls == "LocalScript" ? std::string(".client.luau") : std::string(".luau");
    };
    int written = 0;
    std::vector<std::string> remotes[3];
    for (size_t i = 0; i < w.instances.size(); ++i) {
        const auto& s = w.instances[i];
        const fs::path p = pathOf((int)i);
        const bool script = s.className == "Script" || s.className == "LocalScript" || s.className == "ModuleScript";
        if (s.className == "Folder" || (script && hasChildren((int)i))) fs::create_directories(p, ec);
        if (!script) {
            if ((s.className == "RemoteEvent" || s.className == "RemoteFunction") && s.parentIndex < 0)
                remotes[(int)s.service].push_back(s.name + "|" + s.className);
            continue;
        }
        const fs::path file = hasChildren((int)i) ? p / ("init" + suffix(s.className)) : fs::path(p.string() + suffix(s.className));
        fs::create_directories(file.parent_path(), ec);
        std::ofstream f(file, std::ios::binary | std::ios::trunc);
        if (!f) { err = "non riesco a scrivere " + file.string(); return -1; }
        f << s.source;
        ++written;
    }
    for (int sv = 0; sv < 3; ++sv) fs::create_directories(fs::path(dir) / "src" / svcDir[sv], ec);
    std::ofstream pj(fs::path(dir) / "default.project.json", std::ios::binary | std::ios::trunc);
    pj << "{\n  \"name\": " << json::quote(projectName) << ",\n  \"tree\": {\n    \"$className\": \"DataModel\"";
    for (int sv = 0; sv < 3; ++sv) {
        pj << ",\n    \"" << svcDir[sv] << "\": {\n      \"$path\": \"src/" << svcDir[sv] << "\"";
        for (const auto& r : remotes[sv]) {
            const size_t bar = r.find('|');
            pj << ",\n      " << json::quote(r.substr(0, bar)) << ": { \"$className\": " << json::quote(r.substr(bar + 1)) << " }";
        }
        pj << "\n    }";
    }
    pj << "\n  }\n}\n";
    // Per VS Code (estensione luau-lsp) e per un'AI: l'API di Roblox la conosce
    // gia' luau-lsp, qui le API in piu' di Blocky.
    std::ofstream(fs::path(dir) / "blocky.d.luau", std::ios::binary | std::ios::trunc) << blockyLuauDefs();
    fs::create_directories(fs::path(dir) / ".vscode", ec);
    if (!fs::exists(fs::path(dir) / ".vscode" / "settings.json"))
        std::ofstream(fs::path(dir) / ".vscode" / "settings.json", std::ios::binary)
            << "{\n  \"luau-lsp.platform.type\": \"roblox\",\n"
            << "  \"luau-lsp.types.definitionFiles\": [\"blocky.d.luau\"]\n}\n";
    return written;
}

} // namespace rojo
} // namespace blk
