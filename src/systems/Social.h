#pragma once

// Account, marketplace e gruppi sul dispositivo. Le monete sono i B$ gia'
// nel portafoglio (Economy.h): non c'e' un pagamento vero.

#include "engine/Economy.h"
#include "engine/GameHub.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct SocialProduct {
    int id = 0;
    std::string name;
    int price = 0;
    bool pass = false;
};

struct SocialGroup {
    int id = 0;
    std::string name;
    std::string owner;
    struct Member { std::string name; int rank = 1; };
    std::vector<Member> members;
};

struct SocialState {
    std::string user = "Player";
    int userId = 1;
    std::vector<SocialProduct> catalog;
    std::vector<int> passes;
    std::vector<SocialGroup> groups;
    bool prompt = false;
    int promptId = 0;
    bool promptPass = false;
    std::string promptName;
    int promptPrice = 0;
    std::string promptError;
    bool receiptPending = false;
    int receiptProduct = 0;
};

inline SocialState& social() {
    static SocialState s;
    return s;
}

inline int socialHashId(const std::string& name) {
    uint32_t h = 2166136261u;
    for (unsigned char c : name) { h ^= c; h *= 16777619u; }
    int id = (int)(h & 0x7fffffffu);
    return id == 0 ? 1 : id;
}

inline std::string socialSafe(std::string s) {
    for (char& c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            c = '_';
    if (s.empty()) s = "Player";
    return s;
}

inline std::string socialEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') { o.push_back('\\'); o.push_back((char)c); }
        else if (c == '\n' || c == '\r') continue;
        else o.push_back((char)c);
    }
    return o;
}

inline void socialSaveCatalog() {
    std::ofstream f(blockyDataDir() + "/catalog.json");
    if (!f) return;
    f << "{\"items\":[\n";
    for (size_t i = 0; i < social().catalog.size(); ++i) {
        const auto& p = social().catalog[i];
        f << "{\"id\":" << p.id << ",\"name\":\"" << socialEscape(p.name)
          << "\",\"price\":" << p.price << ",\"pass\":" << (p.pass ? "true" : "false") << "}";
        if (i + 1 < social().catalog.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

inline void socialSavePasses() {
    std::ofstream f(blockyDataDir() + "/owns_" + socialSafe(social().user) + ".txt");
    if (!f) return;
    for (int id : social().passes) f << id << "\n";
}

inline void socialSaveGroups() {
    std::ofstream f(blockyDataDir() + "/groups.json");
    if (!f) return;
    f << "{\"groups\":[\n";
    for (size_t i = 0; i < social().groups.size(); ++i) {
        const auto& g = social().groups[i];
        f << "{\"id\":" << g.id << ",\"name\":\"" << socialEscape(g.name)
          << "\",\"owner\":\"" << socialEscape(g.owner) << "\",\"members\":[";
        for (size_t m = 0; m < g.members.size(); ++m) {
            f << "{\"name\":\"" << socialEscape(g.members[m].name) << "\",\"rank\":" << g.members[m].rank << "}";
            if (m + 1 < g.members.size()) f << ",";
        }
        f << "]}";
        if (i + 1 < social().groups.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

inline void socialLoadCatalog() {
    social().catalog.clear();
    std::string json = readTextFile(blockyDataDir() + "/catalog.json");
    size_t pos = 0;
    while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
        SocialProduct p;
        p.id = jsonFindInt(json, "id", pos);
        p.name = jsonFindStr(json, "name", pos);
        p.price = jsonFindInt(json, "price", pos);
        p.pass = jsonFindBool(json, "pass", pos, false);
        if (p.id > 0 && !p.name.empty()) social().catalog.push_back(p);
        pos += 4;
    }
}

inline void socialLoadPasses() {
    social().passes.clear();
    std::ifstream f(blockyDataDir() + "/owns_" + socialSafe(social().user) + ".txt");
    int id = 0;
    while (f >> id) if (id > 0) social().passes.push_back(id);
}

inline void socialLoadGroups() {
    social().groups.clear();
    std::string json = readTextFile(blockyDataDir() + "/groups.json");
    size_t pos = 0;
    while ((pos = json.find("\"owner\"", pos)) != std::string::npos) {
        SocialGroup g;
        g.id = jsonFindInt(json, "id", pos > 80 ? pos - 80 : 0);
        g.name = jsonFindStr(json, "name", pos > 80 ? pos - 80 : 0);
        g.owner = jsonFindStr(json, "owner", pos);
        size_t mend = json.find("]}", pos);
        size_t mp = pos;
        while (mend != std::string::npos && (mp = json.find("\"rank\"", mp)) != std::string::npos && mp < mend) {
            SocialGroup::Member mem;
            mem.name = jsonFindStr(json, "name", mp > 40 ? mp - 40 : 0);
            mem.rank = jsonFindInt(json, "rank", mp);
            if (!mem.name.empty()) g.members.push_back(mem);
            mp += 6;
        }
        if (g.id > 0 && !g.name.empty()) social().groups.push_back(g);
        pos += 7;
    }
}

inline void socialBind(const char* username) {
    social().user = (username && username[0]) ? username : "Player";
    social().userId = socialHashId(social().user);
    socialLoadCatalog();
    socialLoadPasses();
    socialLoadGroups();
}

inline const SocialProduct* socialFind(int id) {
    for (const auto& p : social().catalog)
        if (p.id == id) return &p;
    return nullptr;
}

inline bool socialOwns(int passId) {
    for (int id : social().passes)
        if (id == passId) return true;
    return false;
}

inline bool socialRegister(int id, const std::string& name, int price, bool pass) {
    if (id <= 0 || name.empty() || price < 0) return false;
    for (auto& p : social().catalog) {
        if (p.id == id) {
            p.name = name.substr(0, 32);
            p.price = price;
            p.pass = pass;
            socialSaveCatalog();
            return true;
        }
    }
    if ((int)social().catalog.size() >= 64) return false;
    SocialProduct p;
    p.id = id;
    p.name = name.substr(0, 32);
    p.price = price;
    p.pass = pass;
    social().catalog.push_back(p);
    socialSaveCatalog();
    return true;
}

inline void socialPrompt(int id, bool pass) {
    auto& s = social();
    s.prompt = true;
    s.promptId = id;
    s.promptPass = pass;
    s.promptError.clear();
    if (const SocialProduct* p = socialFind(id)) {
        s.promptName = p->name;
        s.promptPrice = p->price;
        s.promptPass = p->pass;
    } else {
        s.promptName = pass ? "Game Pass" : "Prodotto";
        s.promptPrice = 0;
        s.promptError = "Non e' in catalogo. Registralo prima.";
    }
}

inline bool socialBuy() {
    auto& s = social();
    if (!s.prompt) return false;
    const SocialProduct* p = socialFind(s.promptId);
    if (!p) { s.promptError = "Non e' in catalogo."; return false; }
    if (p->pass && socialOwns(p->id)) { s.promptError = "Ce l'hai gia'."; return false; }
    if (p->price > 0 && !ecoSpend(s.user.c_str(), p->price, p->name.c_str())) {
        s.promptError = economy().lastError.empty() ? "B$ insufficienti" : economy().lastError;
        return false;
    }
    if (p->pass) {
        s.passes.push_back(p->id);
        socialSavePasses();
    } else {
        s.receiptPending = true;
        s.receiptProduct = p->id;
    }
    s.prompt = false;
    return true;
}

inline int socialCreateGroup(const std::string& name) {
    if (name.empty() || name.size() > 24) return 0;
    auto& s = social();
    int id = 1;
    for (const auto& g : s.groups) id = std::max(id, g.id + 1);
    SocialGroup g;
    g.id = id;
    g.name = name;
    g.owner = s.user;
    g.members.push_back({s.user, 255});
    s.groups.push_back(g);
    socialSaveGroups();
    return id;
}

inline bool socialJoin(int id) {
    auto& s = social();
    for (auto& g : s.groups) {
        if (g.id != id) continue;
        for (const auto& m : g.members)
            if (m.name == s.user) return true;
        g.members.push_back({s.user, 1});
        socialSaveGroups();
        return true;
    }
    return false;
}

inline bool socialLeave(int id) {
    auto& s = social();
    for (auto& g : s.groups) {
        if (g.id != id) continue;
        if (g.owner == s.user) return false;
        g.members.erase(std::remove_if(g.members.begin(), g.members.end(),
            [&](const SocialGroup::Member& m) { return m.name == s.user; }), g.members.end());
        socialSaveGroups();
        return true;
    }
    return false;
}

inline int socialRank(int groupId) {
    const auto& s = social();
    for (const auto& g : s.groups) {
        if (g.id != groupId) continue;
        for (const auto& m : g.members)
            if (m.name == s.user) return m.rank;
    }
    return 0;
}

inline const SocialGroup* socialGroup(int id) {
    for (const auto& g : social().groups)
        if (g.id == id) return &g;
    return nullptr;
}
