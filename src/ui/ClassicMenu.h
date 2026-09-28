#pragma once


#include <imgui.h>
#include <imgui_internal.h>

#include "engine/GameHub.h"
#include "engine/PoolConfig.h"
#include "engine/RemoteGames.h"
#include "engine/RemoteCatalog.h"
#include "engine/ServerBrowser.h"
#include "engine/Economy.h"
#include "engine/GoogleAuth.h"
#include "engine/AccountVault.h"
#include "engine/Purchase.h"
#include "moderation/ContentFilter.h"
#include "ui/Loc.h"
#include "ui/UiTheme.h"
#include "ui/UiMetrics.h"
#include "render/Texture2D.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

enum class HubTab { Games, Shop, Chat };

struct ClassicMenuState {
    int selectedGame = 0;
    int hoveredGame = -1;
    float charSpin = 0.f;
    float uiTime = 0.f;
    float loginAppear = 0.f;
    float tabSlide = 0.f;
    float playPulse = 0.f;
    bool wantPlay = false;
    bool wantBuild = false;
    bool wantCatalog = false;
    bool wantImportModel = false;
    bool wantQuit = false;
    int launchGame = -1;

    char joinIp[64] = "127.0.0.1";
    int  joinPort = 27015;
    bool wantJoinIp = false;

    // Server dei giochi pubblicati (blocky_hostd). Vuoto = solo giochi locali.
    char gameServer[80] = "";
    bool wantRefreshGameServer = false;
    bool wantPublishCatalog = false;
    bool wantSyncGames = false;
    bool wantServerList = false;
    bool showServerList = false;
    int  joinServerIndex = -1;

    char joinCode[16] = "";
    bool wantJoinCode = false;

    bool showLogin = true;
    HubTab tab = HubTab::Games;
    bool loggedIn = false;
    char username[64] = "";
    char password[64] = "";
    char googleEmail[128] = "";
    bool googleLoginPending = false;
    bool loggedInWithGoogle = false;
    bool createPasskeyPending = false;
    bool unlockPasskeyPending = false;
    char passkeyBuf[32] = {};
    char passkeyConfirmBuf[32] = {};
    char unlockTargetUser[64] = {};
    char chatInput[128] = "";
    std::vector<std::string> chatLines;
    Moderation::Session mod;
    int shopCoins = 500;
    std::string loginError;

    struct ShopItem {
        std::string name;
        std::string seller = "Blocky";
        std::string sourcePath;
        int slot = 0;
        int kind = 0;
        float offset[3] = {0, 0, 0};
        float rotation[3] = {0, 0, 0};
        float scale[3] = {1, 1, 1};
        float tint[3] = {1.f, 1.f, 1.f};
        int price = 50;
        bool owned = false;
        bool fromPublisher = false;
        bool replacesBody = false;
        bool equipped = false;
    };
    std::vector<ShopItem> shopItems;
    int wantEquipShop = -1;
    int wantUnequipShop = -1;
    int wantUnequipEquipped = -1;
    bool shopSeeded = false;

    char netStatus[96] = "";

    struct EquippedInfo {
        std::string name;
        int slot = 0;
        std::string sourcePath;
    };
    std::vector<EquippedInfo> equippedList;

    std::vector<std::string> userFriends; // account reali (LAN), non i companion NPC di Studio
    bool showPlayersPanel = false;
    int reportTargetPlayer = -1; // indice in g_net.remotes, -1 = nessuno
    bool reportGamePending = false;
    char reportReasonBuf[160] = {};
};

inline std::string blockyProfilePath() {
    return plat::dataDir() + "/profile.txt";
}

inline void ecoSyncMenu(ClassicMenuState& st, float nowTime = 0.f) {
    st.shopCoins = ecoBalance(st.username, nowTime);
}

// Lista amici reali (account visti su LAN), un file per utente locale —
// NON va confusa con WorkspaceFriend/g_friends, che sono i companion NPC
// piazzabili in Studio: due concetti separati, stesso nome per comodita'
// dell'utente ma niente a che fare l'uno con l'altro nel codice.
inline std::string blockyFriendsPath(const std::string& username) {
    std::string key = normalizeCreatorKey(username);
    // Stessa radice di tutto il resto dei dati utente: il ripiego
    // relativo di prima su Android finiva in "/", di sola lettura, e la
    // lista amici non veniva mai salvata.
    return plat::dataDir() + "/friends_" + key + ".txt";
}

inline std::vector<std::string> loadFriendsList(const std::string& username) {
    std::vector<std::string> out;
    if (username.empty()) return out;
    std::ifstream f(blockyFriendsPath(username));
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

inline void saveFriendsList(const std::string& username, const std::vector<std::string>& list) {
    if (username.empty()) return;
    std::ofstream f(blockyFriendsPath(username));
    for (const auto& n : list) f << n << "\n";
}

inline bool isFriend(const ClassicMenuState& st, const std::string& name) {
    std::string key = normalizeCreatorKey(name);
    for (const auto& f : st.userFriends)
        if (normalizeCreatorKey(f) == key) return true;
    return false;
}

inline void addFriend(ClassicMenuState& st, const std::string& name) {
    if (name.empty() || normalizeCreatorKey(name) == normalizeCreatorKey(st.username)) return;
    if (isFriend(st, name)) return;
    st.userFriends.push_back(name);
    saveFriendsList(st.username, st.userFriends);
}

inline void removeFriend(ClassicMenuState& st, const std::string& name) {
    std::string key = normalizeCreatorKey(name);
    auto& v = st.userFriends;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const std::string& f) { return normalizeCreatorKey(f) == key; }), v.end());
    saveFriendsList(st.username, v);
}

inline void saveUserProfile(const ClassicMenuState& st) {
    if (st.username[0] == '\0') return;
    ClassicMenuState& mut = const_cast<ClassicMenuState&>(st);
    ecoVerify(st.username, 0.f);
    mut.shopCoins = economy().coins;
    uint32_t sig = ecoDiskSignature(st.username, mut.shopCoins);
    FILE* f = std::fopen(blockyProfilePath().c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "%s\n%d\n%d\n%08X\n", st.username, mut.shopCoins,
                 st.loggedIn ? 1 : 0, (unsigned)sig);
    std::fclose(f);
    ecoSaveIntegrity(st.username);
}

inline bool loadUserProfile(ClassicMenuState& st) {
    loadSavedAccounts();
    dedupeSavedAccounts();
    FILE* f = std::fopen(blockyProfilePath().c_str(), "rb");
    if (!f) {
        ecoBoot(st.username, EconomyState::kStartCoins);
        st.shopCoins = EconomyState::kStartCoins;
        return false;
    }
    char line[128] = {};
    if (!std::fgets(line, sizeof(line), f)) { std::fclose(f); return false; }
    size_t n = std::strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (n == 0 || n >= sizeof(st.username)) { std::fclose(f); return false; }
    std::snprintf(st.username, sizeof(st.username), "%s", line);

    int coins = EconomyState::kStartCoins;
    char coinsLine[64] = {};
    if (std::fgets(coinsLine, sizeof(coinsLine), f))
        coins = std::atoi(coinsLine);

    char autoLine[32] = {};
    bool autoLogin = false;
    if (std::fgets(autoLine, sizeof(autoLine), f))
        autoLogin = std::atoi(autoLine) != 0;

    char sigLine[64] = {};
    bool hasSigLine = std::fgets(sigLine, sizeof(sigLine), f) != nullptr;
    std::fclose(f);

    bool hasSig = false;
    bool sigOk = false;
    if (hasSigLine) {
        unsigned v = 0;
        size_t sl = std::strlen(sigLine);
        while (sl > 0 && (sigLine[sl - 1] == '\n' || sigLine[sl - 1] == '\r')) sigLine[--sl] = '\0';
        if (sl >= 6 && std::sscanf(sigLine, "%x", &v) == 1) {
            hasSig = true;
            sigOk = ecoCheckDiskSignature(st.username, coins, (uint32_t)v);
        }
    }

    bool diskCheat = false;
    coins = ecoSanitizeLoadedCoins(st.username, coins, hasSig, sigOk, &diskCheat);

    ecoBoot(st.username, coins);

    if (diskCheat && !economy().banned) {
        ecoReportConfirmedCheat(st.username, EcoCheatKind::DiskForgery, 1000.f,
                                "profile signature mismatch");
    }

    st.shopCoins = economy().coins;

    if (economy().banned) {
        st.loggedIn = false;
        st.showLogin = true;
        st.loginError = "Account BANNATO per cheat economia (permanente)";
    } else if (autoLogin && st.username[0]) {
        const SavedAccount* acc = findSavedAccount(st.username);
        // Se su questo dispositivo la passkey e' gia' stata verificata di
        // recente, non si richiede: sarebbe solo fastidio, non sicurezza in piu'.
        if (acc && acc->hasPasskey && !isDeviceTrustedFor(st.username)) {
            st.loggedIn = false;
            st.showLogin = true;
            st.unlockPasskeyPending = true;
            std::snprintf(st.unlockTargetUser, sizeof(st.unlockTargetUser), "%s", st.username);
        } else {
            st.loggedIn = true;
            st.showLogin = false;
            st.userFriends = loadFriendsList(st.username);
        }
    }

    saveUserProfile(st);
    return st.username[0] != '\0';
}

inline std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else o.push_back((char)c);
    }
    return o;
}

inline std::string pathForJson(std::string p) {
    for (char& c : p)
        if (c == '\\') c = '/';
    return p;
}

inline std::string shopSavePath() {
    return blockyDataDir() + "/shop.json";
}

inline std::string avatarSavePath() {
    return blockyDataDir() + "/avatar.json";
}

inline bool saveShopItems(const ClassicMenuState& st) {
    std::ostringstream j;
    j << "{\n  \"items\": [\n";
    bool first = true;
    for (const auto& it : st.shopItems) {
        if (!first) j << ",\n";
        first = false;
        j << "    {"
          << "\"name\":\"" << jsonEscape(it.name) << "\","
          << "\"seller\":\"" << jsonEscape(it.seller) << "\","
          << "\"sourcePath\":\"" << jsonEscape(pathForJson(it.sourcePath)) << "\","
          << "\"slot\":" << it.slot << ","
          << "\"offset\":[" << it.offset[0] << "," << it.offset[1] << "," << it.offset[2] << "],"
          << "\"rotation\":[" << it.rotation[0] << "," << it.rotation[1] << "," << it.rotation[2] << "],"
          << "\"scale\":[" << it.scale[0] << "," << it.scale[1] << "," << it.scale[2] << "],"
          << "\"tint\":[" << it.tint[0] << "," << it.tint[1] << "," << it.tint[2] << "],"
          << "\"price\":" << it.price << ","
          << "\"kind\":" << it.kind << ","
          << "\"owned\":" << (it.owned ? "true" : "false") << ","
          << "\"fromPublisher\":" << (it.fromPublisher ? "true" : "false") << ","
          << "\"replacesBody\":" << (it.replacesBody ? "true" : "false")
          << "}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(shopSavePath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool jsonFindFloat3(const std::string& json, const std::string& key, size_t from, float out[3]) {
    auto k = json.find("\"" + key + "\"", from);
    if (k == std::string::npos) return false;
    auto b = json.find('[', k);
    if (b == std::string::npos) return false;
    return std::sscanf(json.c_str() + b + 1, "%f,%f,%f", &out[0], &out[1], &out[2]) >= 3;
}

inline bool loadShopItems(ClassicMenuState& st) {
    std::string json = readTextFile(shopSavePath());
    if (json.empty()) return false;

    st.shopItems.clear();
    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
        ClassicMenuState::ShopItem it;
        it.name = jsonFindStr(json, "name", pos);
        it.seller = jsonFindStr(json, "seller", pos);
        it.sourcePath = jsonFindStr(json, "sourcePath", pos);
        it.slot = jsonFindInt(json, "slot", pos);
        jsonFindFloat3(json, "offset", pos, it.offset);
        jsonFindFloat3(json, "rotation", pos, it.rotation);
        jsonFindFloat3(json, "scale", pos, it.scale);
        jsonFindFloat3(json, "tint", pos, it.tint);
        it.price = jsonFindInt(json, "price", pos);
        it.kind = jsonFindInt(json, "kind", pos);
        it.owned = jsonFindBool(json, "owned", pos, false);
        it.fromPublisher = jsonFindBool(json, "fromPublisher", pos, false);
        it.replacesBody = jsonFindBool(json, "replacesBody", pos, false);
        if (it.scale[0] == 0 && it.scale[1] == 0 && it.scale[2] == 0) {
            it.scale[0] = it.scale[1] = it.scale[2] = 1.f;
        }
        if (!it.name.empty())
            st.shopItems.push_back(std::move(it));
        pos += 6;
    }
    st.shopSeeded = true;
    return !st.shopItems.empty();
}


/**
 * Gli item scaricati dal server, aggiunti a quelli locali.
 *
 * Si fondono per NOME invece di sostituire tutto: chi ha gia' i suoi item non
 * deve vederseli spazzare via perche' un altro dispositivo ha pubblicato il
 * proprio catalogo. Se un nome esiste gia' si tiene quello locale, che e'
 * quello su cui l'utente sta lavorando.
 *
 * I sourcePath arrivano come nomi di file nudi: qui diventano il percorso
 * della copia scaricata, altrimenti punterebbero a una cartella che esiste
 * solo sul computer di chi li ha creati.
 */
inline int mergeRemoteCatalog(ClassicMenuState& st, const std::string& json) {
    if (json.empty()) return 0;

    int added = 0;
    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
        ClassicMenuState::ShopItem it;
        it.name = jsonFindStr(json, "name", pos);
        it.seller = jsonFindStr(json, "seller", pos);
        const std::string meshFile = jsonFindStr(json, "sourcePath", pos);
        it.slot = jsonFindInt(json, "slot", pos);
        jsonFindFloat3(json, "offset", pos, it.offset);
        jsonFindFloat3(json, "rotation", pos, it.rotation);
        jsonFindFloat3(json, "scale", pos, it.scale);
        jsonFindFloat3(json, "tint", pos, it.tint);
        it.price = jsonFindInt(json, "price", pos);
        it.kind = jsonFindInt(json, "kind", pos);
        // Un catalogo condiviso e una VETRINA, non un regalo: gli item
        // arrivano da vedere, non gia posseduti. Prima si copiava owned dal
        // PC che li aveva pubblicati, e sul telefono comparivano come roba
        // gia tua senza averla mai presa.
        it.owned = false;
        it.equipped = false;
        it.fromPublisher = jsonFindBool(json, "fromPublisher", pos, false);
        it.replacesBody = jsonFindBool(json, "replacesBody", pos, false);
        pos += 6;

        if (it.name.empty()) continue;
        if (it.scale[0] == 0 && it.scale[1] == 0 && it.scale[2] == 0)
            it.scale[0] = it.scale[1] = it.scale[2] = 1.f;

        bool already = false;
        for (const auto& existing : st.shopItems)
            if (existing.name == it.name) { already = true; break; }
        if (already) continue;

        if (!meshFile.empty())
            it.sourcePath = blk::remotecatalog::localMeshDir() + "/" + meshFile;

        st.shopItems.push_back(std::move(it));
        ++added;
    }
    if (added > 0) saveShopItems(st);
    return added;
}

/**
 * Manda il catalogo locale al server.
 *
 * I sourcePath vengono ridotti al solo nome del file: e' quello che l'altro
 * capo chiedera' a GET /catalog/<file>, e un percorso assoluto di questo
 * computer non gli servirebbe a niente.
 */
inline void publishCatalogToServer(const ClassicMenuState& st, const std::string& hostPort) {
    std::ostringstream j;
    j << "{\n  \"items\": [\n";
    std::vector<std::string> meshes;
    bool first = true;
    for (const auto& it : st.shopItems) {
        std::string meshName;
        if (!it.sourcePath.empty()) {
            meshes.push_back(it.sourcePath);
            meshName = std::filesystem::path(it.sourcePath).filename().string();
        }
        if (!first) j << ",\n";
        first = false;
        j << "    {"
          << "\"name\":\"" << jsonEscape(it.name) << "\","
          << "\"seller\":\"" << jsonEscape(it.seller) << "\","
          << "\"sourcePath\":\"" << jsonEscape(meshName) << "\","
          << "\"slot\":" << it.slot << ","
          << "\"offset\":[" << it.offset[0] << "," << it.offset[1] << "," << it.offset[2] << "],"
          << "\"rotation\":[" << it.rotation[0] << "," << it.rotation[1] << "," << it.rotation[2] << "],"
          << "\"scale\":[" << it.scale[0] << "," << it.scale[1] << "," << it.scale[2] << "],"
          << "\"tint\":[" << it.tint[0] << "," << it.tint[1] << "," << it.tint[2] << "],"
          << "\"price\":" << it.price << ","
          << "\"kind\":" << it.kind << ","
          << "\"owned\":" << (it.owned ? "true" : "false") << ","
          << "\"fromPublisher\":" << (it.fromPublisher ? "true" : "false") << ","
          << "\"replacesBody\":" << (it.replacesBody ? "true" : "false")
          << "}";
    }
    j << "\n  ]\n}\n";
    blk::remotecatalog::publish(hostPort, j.str(), meshes);
}

inline void seedDefaultShop(ClassicMenuState& st) {
    if (st.shopSeeded) return;
    if (loadShopItems(st)) return;
    st.shopSeeded = true;
    auto add = [&](const char* name, int price, bool owned = false) {
        ClassicMenuState::ShopItem it;
        it.name = name;
        it.price = price;
        it.owned = owned;
        it.seller = "Blocky";
        st.shopItems.push_back(std::move(it));
    };
    add("T-Shirt Blu", 25);
    add("Pantaloni Verdi", 25);
    add("Face Smile", 50, true);
    add("Slingshot", 100);
    saveShopItems(st);
}

inline float uiEaseOutCubic(float t) {
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    float u = 1.f - t;
    return 1.f - u * u * u;
}

inline float uiLerp(float a, float b, float t) { return a + (b - a) * t; }

inline void uiSoftShadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, int layers = 4) {
    for (int i = layers; i >= 1; --i) {
        float o = (float)i * 2.2f;
        int al = 10 + i * 6;
        dl->AddRectFilled({a.x - o * 0.3f, a.y + o * 0.4f}, {b.x + o * 0.3f, b.y + o},
                          IM_COL32(8, 28, 70, al), rounding + o * 0.2f);
    }
}

// Definita in fondo al file, accanto alle altre finestre della hub.
inline void drawServerListWindow(ClassicMenuState& st);

inline void applyClassicRobloxTheme() {
    // Come applyStudioTheme() sotto: i valori vivono in assets/ui/theme.json
    // (chiavi "Hub..." — vedi UiTheme.h) invece che scritti qui a mano. Se il
    // file manca o e' malformato, loadHubTheme applica questi stessi valori
    // di sempre, quindi nessun comportamento cambia finche' non lo tocchi.
    loadHubTheme(ImGui::GetStyle());
    // Dopo il tema, mai prima: loadHubTheme riscrive lo stile da capo e
    // cancellerebbe le misure da touch.
    blk::ui::applyToStyle(ImGui::GetStyle());
}

inline void applyStudioTheme() {
    // I colori/valori vivono ora in assets/ui/theme.json (vedi UiTheme.h) —
    // cambiare un colore non richiede piu' ricompilare. Se il file manca o
    // e' malformato, loadUiTheme applica esattamente questi stessi valori
    // di prima, quindi nessun comportamento cambia finche' non lo tocchi.
    loadUiTheme(ImGui::GetStyle());
    blk::ui::applyToStyle(ImGui::GetStyle());
}

inline void drawBlueHubBackground(ImDrawList* dl, float w, float h, float time = 0.f) {
    float wave = 0.5f + 0.5f * std::sin(time * 0.35f);
    int r0 = (int)uiLerp(58, 88, wave);
    int g0 = (int)uiLerp(140, 175, wave);
    int b0 = (int)uiLerp(230, 250, wave);
    int r1 = (int)uiLerp(18, 32, wave);
    int g1 = (int)uiLerp(62, 90, wave);
    int b1 = (int)uiLerp(148, 175, wave);
    dl->AddRectFilledMultiColor({0, 0}, {w, h},
        IM_COL32(r0, g0, b0, 255), IM_COL32(r0 - 12, g0 - 14, b0 - 8, 255),
        IM_COL32(r1, g1, b1, 255), IM_COL32(r1 - 6, g1 - 10, b1 - 12, 255));

    for (int i = 0; i < 14; ++i) {
        float sp = 0.12f + (i % 5) * 0.04f;
        float x = fmodf(i * 173.f + time * (18.f + i * 2.5f) * (i % 2 ? 1.f : -1.f) + w, w + 120.f) - 60.f;
        float y = 90.f + (i % 5) * (h * 0.14f) + std::sin(time * sp + i) * 28.f;
        float r = 48.f + (i % 4) * 22.f + std::sin(time * 0.7f + i) * 8.f;
        int a = 10 + (i % 4) * 5;
        dl->AddCircleFilled({x, y}, r, IM_COL32(255, 255, 255, a));
        dl->AddCircleFilled({x + r * 0.2f, y - r * 0.15f}, r * 0.45f, IM_COL32(180, 220, 255, a / 2));
    }

    dl->AddRectFilledMultiColor({0, 0}, {w, h * 0.42f},
        IM_COL32(255, 255, 255, 36), IM_COL32(255, 255, 255, 22),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

    dl->AddRectFilled({0, 0}, {w, 68}, IM_COL32(12, 58, 138, 210));
    dl->AddRectFilledMultiColor({0, 0}, {w, 68},
        IM_COL32(255, 255, 255, 28), IM_COL32(255, 255, 255, 8),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    dl->AddRectFilledMultiColor({0, 66}, {w, 72},
        IM_COL32(140, 205, 255, 100), IM_COL32(90, 170, 245, 50),
        IM_COL32(90, 170, 245, 0), IM_COL32(140, 205, 255, 0));
}

inline bool hubPlayButton(const char* label, ImVec2 size, float pulse = 0.f) {
    float glow = 0.5f + 0.5f * std::sin(pulse * 3.2f);
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.06f, uiLerp(0.68f, 0.78f, glow), 0.36f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.12f, 0.88f, 0.48f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.04f, 0.55f, 0.28f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text,          {1.f, 1.f, 1.f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f);
    bool p = ImGui::Button(label, size);
    if (ImGui::IsItemHovered()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        dl->AddRect(a, b, IM_COL32(180, 255, 210, 90 + (int)(glow * 40)), 14.f, 0, 2.2f);
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return p;
}

inline bool hubTabButton(const char* label, bool active) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        {1.f, 1.f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.97f, 0.99f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          {0.06f, 0.32f, 0.76f, 1.f});
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.14f, 0.46f, 0.90f, 0.72f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f, 0.58f, 1.00f, 0.92f});
        ImGui::PushStyleColor(ImGuiCol_Text,          {1.f, 1.f, 1.f, 1.f});
    }
    bool p = ImGui::Button(label, {112, 36});
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return p;
}

inline void hubPanelHeader(const char* title) {
    ImGui::PushStyleColor(ImGuiCol_Text, {0.06f, 0.34f, 0.78f, 1.f});
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        {p.x, p.y}, {p.x + w, p.y + 2.f},
        IM_COL32(40, 130, 240, 180), IM_COL32(40, 130, 240, 40),
        IM_COL32(40, 130, 240, 0), IM_COL32(40, 130, 240, 0));
    ImGui::Dummy({0, 8});
}

inline bool completeHubLogin(ClassicMenuState& st, bool fromGoogle) {
    if (st.username[0] == '\0') {
        st.loginError = "Inserisci un username";
        return false;
    }
    auto mod = Moderation::checkUsername(st.username);
    if (mod.verdict != Moderation::Verdict::Ok) {
        st.loginError = "Username non consentito (moderazione)";
        return false;
    }
    // Nome gia di qualcun altro su questo dispositivo, e protetto da passkey:
    // non si entra riscrivendone il nome, bisogna dimostrare di essere lui.
    // Senza questo controllo bastava ridigitare lo stesso username per
    // ritrovarsi dentro un account non proprio.
    if (!st.unlockPasskeyPending) {
        if (const SavedAccount* taken = findSavedAccount(st.username)) {
            if (taken->hasPasskey && !isDeviceTrustedFor(st.username)) {
                st.unlockPasskeyPending = true;
                std::snprintf(st.unlockTargetUser, sizeof(st.unlockTargetUser), "%s",
                              taken->username.c_str());
                st.passkeyBuf[0] = 0;
                st.loginError = "Questo username ha gia un account: inserisci la passkey";
                st.showLogin = true;
                st.loggedIn = false;
                return false;
            }
        }
    }

    ecoLoadIntegrity(st.username);
    if (economy().banned) {
        st.loginError = "Account BANNATO per cheat economia (permanente)";
        st.loggedIn = false;
        st.showLogin = true;
        return false;
    }
    st.loggedIn = true;
    st.showLogin = false;
    st.loginError.clear();
    st.loginAppear = 0.f;
    st.googleLoginPending = false;
    st.unlockPasskeyPending = false;
    st.loggedInWithGoogle = fromGoogle;
    st.userFriends = loadFriendsList(st.username);
    rememberLoginAccount(st.username,
                         fromGoogle ? st.googleEmail : nullptr,
                         fromGoogle);
    const SavedAccount* acc = findSavedAccount(st.username);
    bool isGuest = false;
    {
        std::string u = st.username;
        for (char& c : u) c = (char)std::tolower((unsigned char)c);
        isGuest = u.find("ospite") == 0 || u.find("guest") == 0;
    }
    if (!isGuest && (!acc || !acc->hasPasskey)) {
        st.createPasskeyPending = true;
        st.passkeyBuf[0] = 0;
        st.passkeyConfirmBuf[0] = 0;
        st.showLogin = true;
        st.loggedIn = true;
    }
    if (economy().seal == 0)
        ecoBoot(st.username, EconomyState::kStartCoins);
    else
        ecoReseal(st.username);
    ecoSyncMenu(st, 0.f);
    st.chatLines.clear();
    if (fromGoogle) {
        st.chatLines.push_back("[Sistema] Accesso Google: benvenuto, "
            + std::string(st.username) + "!");
        if (st.googleEmail[0])
            st.chatLines.push_back(std::string("[Google] Account: ") + st.googleEmail);
    } else {
        st.chatLines.push_back("[Sistema] Benvenuto su Blocky, " + std::string(st.username) + "!");
    }
    st.chatLines.push_back("[Sistema] Account salvato su questo PC.");
    st.chatLines.push_back("[Sistema] Hai " + std::to_string(st.shopCoins) + " B$ (Blocky Bucks).");
    if (!isGuest && (!acc || !acc->hasPasskey))
        st.chatLines.push_back("[Sicurezza] Crea una passkey (PIN) per rientrare al volo.");
    else if (!isGuest)
        st.chatLines.push_back("[Sistema] Username salvato — al prossimo avvio usa la passkey.");
    else
        st.chatLines.push_back("[Sistema] Modalita ospite — account non protetto da passkey.");
    if (economy().cheatStrikes > 0)
        st.chatLines.push_back("[Sicurezza] Strike cheat: "
            + std::to_string(economy().cheatStrikes) + "/2 — al 2° ban permanente.");
    if (economy().tampered && !economy().lastError.empty())
        st.chatLines.push_back("[Sicurezza] " + economy().lastError);
    saveUserProfile(st);
    return true;
}

inline void drawLoginPage(ClassicMenuState& st, float screenW, float screenH, float dt, float time) {
    st.uiTime = time;
    st.loginAppear = std::min(1.f, st.loginAppear + dt * 1.8f);
    float appear = uiEaseOutCubic(st.loginAppear);
    float slideY = (1.f - appear) * 36.f;
    int alpha = (int)(appear * 255.f);

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({screenW, screenH});
    ImGui::Begin("##LoginPage", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawBlueHubBackground(dl, screenW, screenH, time);

    float cardW = 420.f;
    float cardH = 470.f;
    if (st.createPasskeyPending || st.unlockPasskeyPending) cardH = 420.f;
    else if (st.googleLoginPending) cardH = 490.f;
    else if (!savedAccounts().empty()) cardH = 520.f;
    float cx = (screenW - cardW) * 0.5f;
    float cy = (screenH - cardH) * 0.5f - 6.f + slideY;

    const char* brand = "BLOCKY";
    float brandSz = 56.f;
    float brandBob = std::sin(time * 1.4f) * 3.f;
    ImVec2 brandSzPx = ImGui::GetFont()->CalcTextSizeA(brandSz, FLT_MAX, 0.f, brand);
    float bx = screenW * 0.5f - brandSzPx.x * 0.5f;
    float by = cy - 88.f + brandBob;
    dl->AddText(ImGui::GetFont(), brandSz + 1.5f, {bx + 2, by + 3},
                IM_COL32(10, 40, 100, (alpha * 90) / 255), brand);
    dl->AddText(ImGui::GetFont(), brandSz, {bx, by},
                IM_COL32(255, 255, 255, alpha), brand);
    const char* sub = L("hub.login_sub");
    ImVec2 subSz = ImGui::CalcTextSize(sub);
    dl->AddText({screenW * 0.5f - subSz.x * 0.5f, cy - 30},
        IM_COL32(210, 232, 255, alpha), sub);

    uiSoftShadow(dl, {cx, cy}, {cx + cardW, cy + cardH}, 20.f, 5);
    ImGui::SetCursorPos({cx, cy});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {1.f, 1.f, 1.f, 0.97f * appear});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 20.f);
    ImGui::BeginChild("LoginCard", {cardW, cardH}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);

    ImDrawList* cdl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetWindowPos();
    cdl->AddRectFilledMultiColor(cp, {cp.x + cardW, cp.y + 6},
        IM_COL32(40, 140, 245, 255), IM_COL32(80, 190, 255, 255),
        IM_COL32(80, 190, 255, 0), IM_COL32(40, 140, 245, 0));

    ImGui::Dummy({0, 18});
    ImGui::SetCursorPosX(32);
    hubPanelHeader("LOGIN");

    if (st.createPasskeyPending) {
        ImGui::SetCursorPosX(32);
        ImGui::TextColored({0.15f, 0.55f, 0.35f, 1.f}, "Crea passkey");
        ImGui::SetCursorPosX(32);
        ImGui::TextWrapped(
            "Account \"%s\" salvato. Scegli un PIN (min 4 cifre) per rientrare senza Google.",
            st.username);
        ImGui::Dummy({0, 8});
        ImGui::SetCursorPosX(32);
        ImGui::Text("Passkey / PIN");
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##pk1", "es. 48291", st.passkeyBuf, sizeof(st.passkeyBuf),
                                 ImGuiInputTextFlags_Password);
        ImGui::SetCursorPosX(32);
        ImGui::Text("Conferma PIN");
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##pk2", "ripeti PIN", st.passkeyConfirmBuf,
                                 sizeof(st.passkeyConfirmBuf), ImGuiInputTextFlags_Password);
        if (!st.loginError.empty()) {
            ImGui::SetCursorPosX(32);
            ImGui::TextColored({0.85f, 0.15f, 0.15f, 1.f}, "%s", st.loginError.c_str());
        }
        ImGui::Dummy({0, 12});
        ImGui::SetCursorPosX(32);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        if (ImGui::Button("Salva passkey", {cardW - 64, 44})) {
            if (std::strlen(st.passkeyBuf) < 4) {
                st.loginError = "PIN troppo corto (minimo 4 caratteri)";
            } else if (std::strcmp(st.passkeyBuf, st.passkeyConfirmBuf) != 0) {
                st.loginError = "I due PIN non coincidono";
            } else if (!setAccountPasskey(st.username, st.passkeyBuf)) {
                st.loginError = "Impossibile salvare la passkey";
            } else {
                st.createPasskeyPending = false;
                // Chi l ha appena scelta la conosce: inutile richiedergliela al
                // prossimo avvio su questo stesso dispositivo.
                trustDevice(st.username);
                st.showLogin = false;
                st.loginError.clear();
                st.passkeyBuf[0] = 0;
                st.passkeyConfirmBuf[0] = 0;
                st.chatLines.push_back("[Sicurezza] Passkey salvata per " + std::string(st.username));
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::SetCursorPosX(32);
        if (ImGui::Button("Salta (senza passkey)", {cardW - 64, 28})) {
            st.createPasskeyPending = false;
            st.showLogin = false;
            st.loginError.clear();
        }
    } else if (st.unlockPasskeyPending) {
        ImGui::SetCursorPosX(32);
        ImGui::TextColored({0.15f, 0.45f, 0.85f, 1.f}, "Sblocca account");
        ImGui::SetCursorPosX(32);
        ImGui::TextWrapped("Account salvato: %s\nInserisci la passkey / PIN.",
                           st.unlockTargetUser);
        ImGui::Dummy({0, 8});
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##unlockpk", "PIN", st.passkeyBuf, sizeof(st.passkeyBuf),
                                 ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
        if (!st.loginError.empty()) {
            ImGui::SetCursorPosX(32);
            ImGui::TextColored({0.85f, 0.15f, 0.15f, 1.f}, "%s", st.loginError.c_str());
        }
        ImGui::Dummy({0, 12});
        ImGui::SetCursorPosX(32);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.26f, 0.52f, 0.96f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        bool unlock = ImGui::Button("Sblocca", {cardW - 64, 44});
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        if (unlock) {
            if (!verifyAccountPasskey(st.unlockTargetUser, st.passkeyBuf)) {
                st.loginError = "Passkey errata";
            } else {
                std::snprintf(st.username, sizeof(st.username), "%s", st.unlockTargetUser);
                st.unlockPasskeyPending = false;
                trustDevice(st.unlockTargetUser);
                st.passkeyBuf[0] = 0;
                st.loginError.clear();
                const SavedAccount* acc = findSavedAccount(st.username);
                completeHubLogin(st, acc && acc->google);
                st.createPasskeyPending = false;
                st.showLogin = false;
            }
        }
        ImGui::SetCursorPosX(32);
        if (ImGui::Button("Altro account", {cardW - 64, 28})) {
            st.unlockPasskeyPending = false;
            st.unlockTargetUser[0] = 0;
            st.passkeyBuf[0] = 0;
            st.loginError.clear();
        }
    } else if (st.googleLoginPending) {
        ImGui::SetCursorPosX(32);
        ImGui::TextColored({0.15f, 0.45f, 0.85f, 1.f}, "Google");
        ImGui::SetCursorPosX(32);
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + cardW - 64);
        ImGui::TextWrapped("%s", L("hub.google_wait"));
        ImGui::PopTextWrapPos();
        ImGui::Dummy({0, 8});
        ImGui::SetCursorPosX(32);
        ImGui::TextUnformatted(L("hub.google_email"));
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##gemail", "es. nome@gmail.com",
                                 st.googleEmail, sizeof(st.googleEmail));
        if (!st.loginError.empty()) {
            ImGui::SetCursorPosX(32);
            ImGui::TextColored({0.85f, 0.15f, 0.15f, 1.f}, "%s", st.loginError.c_str());
        }
        ImGui::Dummy({0, 14});
        ImGui::SetCursorPosX(32);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.26f, 0.52f, 0.96f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.32f, 0.58f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        if (ImGui::Button(L("hub.google_confirm"), {cardW - 64, 44})) {
            if (!looksLikeEmail(st.googleEmail)) {
                st.loginError = "Inserisci un'email Google valida (es. nome@gmail.com)";
            } else if (!googleEmailToUsername(st.googleEmail, st.username, sizeof(st.username))) {
                st.loginError = "Email non valida";
            } else {
                completeHubLogin(st, true);
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::Dummy({0, 8});
        ImGui::SetCursorPosX(32);
        if (ImGui::Button("Riapri Google nel browser", {cardW - 64, 32})) {
            if (!openGoogleAccountSignIn())
                st.loginError = "Impossibile aprire il browser";
        }
        ImGui::SetCursorPosX(32);
        if (ImGui::Button("Annulla", {cardW - 64, 28})) {
            st.googleLoginPending = false;
            st.loginError.clear();
            st.googleEmail[0] = 0;
        }
    } else {
        if (!savedAccounts().empty()) {
            ImGui::SetCursorPosX(32);
            ImGui::TextColored({0.2f, 0.35f, 0.55f, 1.f}, "Account salvati");
            ImGui::BeginChild("##savedAcc", {cardW - 64, 88}, true);
            for (int i = 0; i < (int)savedAccounts().size(); ++i) {
                const auto& a = savedAccounts()[(size_t)i];
                std::string lab = a.username;
                if (a.google) lab += "  (Google)";
                if (a.hasPasskey) lab += "  *";
                lab += "##sa" + std::to_string(i);
                if (ImGui::Selectable(lab.c_str())) {
                    if (a.hasPasskey) {
                        st.unlockPasskeyPending = true;
                        std::snprintf(st.unlockTargetUser, sizeof(st.unlockTargetUser),
                                      "%s", a.username.c_str());
                        st.passkeyBuf[0] = 0;
                        st.loginError.clear();
                    } else {
                        std::snprintf(st.username, sizeof(st.username), "%s", a.username.c_str());
                        if (!a.email.empty())
                            std::snprintf(st.googleEmail, sizeof(st.googleEmail), "%s", a.email.c_str());
                        completeHubLogin(st, a.google);
                    }
                }
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rimuovi account salvato"))
                        removeSavedAccount(a.username.c_str());
                    ImGui::EndPopup();
                }
            }
            ImGui::EndChild();
            ImGui::SetCursorPosX(32);
            ImGui::TextDisabled("* = protetto da passkey · tasto destro = rimuovi");
            ImGui::Dummy({0, 6});
        }

        ImGui::SetCursorPosX(32);
        ImGui::Text("Username");
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##user", "Il tuo nome", st.username, sizeof(st.username));

        ImGui::Dummy({0, 8});
        ImGui::SetCursorPosX(32);
        ImGui::Text("Password");
        ImGui::SetCursorPosX(32);
        ImGui::SetNextItemWidth(cardW - 64);
        ImGui::InputTextWithHint("##pass", "Opzionale", st.password, sizeof(st.password), ImGuiInputTextFlags_Password);

        if (!st.loginError.empty()) {
            ImGui::SetCursorPosX(32);
            bool isBan = std::strstr(st.loginError.c_str(), "BANNATO") != nullptr;
            ImGui::TextColored(isBan ? ImVec4{1.f, 0.25f, 0.2f, 1.f} : ImVec4{0.85f, 0.15f, 0.15f, 1.f},
                               "%s", st.loginError.c_str());
            if (isBan)
                ImGui::TextDisabled("Motivo: manomissione B$ (non acquisti legittimi).");
        }

        ImGui::Dummy({0, 14});
        ImGui::SetCursorPosX(32);
        ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.94f, 0.94f, 0.96f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.88f, 0.88f, 0.90f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.22f, 0.22f, 0.22f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Border, {0.78f, 0.78f, 0.80f, 1.f});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        if (ImGui::Button(L("hub.google"), {cardW - 64, 44})) {
            st.loginError.clear();
            if (!openGoogleAccountSignIn()) {
                st.loginError = "Impossibile aprire Google nel browser";
            } else {
                st.googleLoginPending = true;
                st.googleEmail[0] = 0;
            }
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        ImGui::Dummy({0, 10});
        ImGui::SetCursorPosX(32);
        if (hubPlayButton(L("hub.login"), {cardW - 64, 44}, time))
            completeHubLogin(st, false);

        ImGui::Dummy({0, 10});
        ImGui::SetCursorPosX(32);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.46f, 0.92f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.56f, 1.00f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
        if (ImGui::Button(LB("hub.guest", "guest"), {cardW - 64, 36})) {
            char guest[32];
            std::snprintf(guest, sizeof(guest), "Ospite_%d", (int)(std::rand() % 9000 + 1000));
            auto chk = Moderation::checkUsername(guest);
            if (chk.verdict != Moderation::Verdict::Ok)
                std::snprintf(guest, sizeof(guest), "Guest_%d", (int)(std::rand() % 9000 + 1000));
            std::snprintf(st.username, sizeof(st.username), "%s", guest);
            st.loggedInWithGoogle = false;
            if (completeHubLogin(st, false)) {
                st.chatLines.clear();
                st.chatLines.push_back(std::string("[Sistema] Sei entrato come ") + guest + " su Blocky.");
                st.chatLines.push_back("[Moderazione] Chat: odio bloccato · parolacce **** · anti-spam/mute.");
                st.chatLines.push_back("[Sistema] Hai " + std::to_string(st.shopCoins) + " B$ — vai allo Shop!");
                saveUserProfile(st);
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::Dummy({0, 10});
        ImGui::SetCursorPosX(32);
        if (st.username[0])
            ImGui::TextDisabled("Ciao %s — Accedi una volta, poi non chiede piu'.", st.username);
        else
            ImGui::TextDisabled("Google oppure username — salvato dopo il primo accesso.");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({screenW * 0.5f - 48, screenH - 52});
    ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 0.16f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.f, 1.f, 1.f, 0.30f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    if (ImGui::Button(LB("quit", "quitlogin"), {96, 30})) st.wantQuit = true;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::End();
}

#if 0
    ImGui::Dummy({0, 18});
    ImGui::SetCursorPosX(32);
    hubPanelHeader("LOGIN");

    ImGui::SetCursorPosX(32);
    ImGui::Text("Username");
    ImGui::SetCursorPosX(32);
    ImGui::SetNextItemWidth(cardW - 64);
    ImGui::InputTextWithHint("##user_old", "Il tuo nome", st.username, sizeof(st.username));

    ImGui::Dummy({0, 10});
    ImGui::SetCursorPosX(32);
    ImGui::Text("Password");
    ImGui::SetCursorPosX(32);
    ImGui::SetNextItemWidth(cardW - 64);
    ImGui::InputTextWithHint("##pass", "Opzionale", st.password, sizeof(st.password), ImGuiInputTextFlags_Password);

    if (!st.loginError.empty()) {
        ImGui::SetCursorPosX(32);
        bool isBan = std::strstr(st.loginError.c_str(), "BANNATO") != nullptr;
        ImGui::TextColored(isBan ? ImVec4{1.f, 0.25f, 0.2f, 1.f} : ImVec4{0.85f, 0.15f, 0.15f, 1.f},
                           "%s", st.loginError.c_str());
        if (isBan)
            ImGui::TextDisabled("Motivo: manomissione B$ (non acquisti legittimi).");
    }

    ImGui::Dummy({0, 20});
    ImGui::SetCursorPosX(32);
    if (hubPlayButton(L("hub.login"), {cardW - 64, 48}, time)) {
        if (st.username[0] == '\0') {
            st.loginError = "Inserisci un username";
        } else {
            auto mod = Moderation::checkUsername(st.username);
            if (mod.verdict != Moderation::Verdict::Ok) {
                st.loginError = "Username non consentito (moderazione)";
            } else {
                ecoLoadIntegrity(st.username);
                if (economy().banned) {
                    st.loginError = "Account BANNATO per cheat economia (permanente)";
                    st.loggedIn = false;
                    st.showLogin = true;
                } else {
                    st.loggedIn = true;
                    st.showLogin = false;
                    st.loginError.clear();
                    st.loginAppear = 0.f;
                    if (economy().seal == 0)
                        ecoBoot(st.username, EconomyState::kStartCoins);
                    else
                        ecoReseal(st.username);
                    ecoSyncMenu(st, 0.f);
                    st.chatLines.clear();
                    st.chatLines.push_back("[Sistema] Benvenuto su Blocky, " + std::string(st.username) + "!");
                    st.chatLines.push_back("[Sistema] Hai " + std::to_string(st.shopCoins) + " B$ (Blocky Bucks).");
                    st.chatLines.push_back("[Sistema] Username salvato — al prossimo avvio entri subito.");
                    if (economy().cheatStrikes > 0)
                        st.chatLines.push_back("[Sicurezza] Strike cheat: "
                            + std::to_string(economy().cheatStrikes) + "/2 — al 2° ban permanente.");
                    if (economy().tampered && !economy().lastError.empty())
                        st.chatLines.push_back("[Sicurezza] " + economy().lastError);
                    saveUserProfile(st);
                }
            }
        }
    }

    ImGui::Dummy({0, 12});
    ImGui::SetCursorPosX(32);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.46f, 0.92f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.56f, 1.00f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    if (ImGui::Button(LB("hub.guest", "guest"), {cardW - 64, 40})) {
        char guest[32];
        std::snprintf(guest, sizeof(guest), "Ospite_%d", (int)(std::rand() % 9000 + 1000));
        auto chk = Moderation::checkUsername(guest);
        if (chk.verdict != Moderation::Verdict::Ok)
            std::snprintf(guest, sizeof(guest), "Guest_%d", (int)(std::rand() % 9000 + 1000));
        std::snprintf(st.username, sizeof(st.username), "%s", guest);
        st.loggedIn = true;
        st.showLogin = false;
        st.loginError.clear();
        st.loginAppear = 0.f;
        ecoBoot(st.username, EconomyState::kStartCoins);
        if (economy().banned) {
            st.loginError = "Questo ospite risulta bannato";
            st.loggedIn = false;
            st.showLogin = true;
        } else {
            ecoSyncMenu(st, 0.f);
            st.chatLines.clear();
            st.chatLines.push_back(std::string("[Sistema] Sei entrato come ") + guest + " su Blocky.");
            st.chatLines.push_back("[Moderazione] Chat: odio bloccato · parolacce **** · anti-spam/mute.");
            st.chatLines.push_back("[Sistema] Hai " + std::to_string(st.shopCoins) + " B$ — vai allo Shop!");
            saveUserProfile(st);
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Dummy({0, 12});
    ImGui::SetCursorPosX(32);
    if (st.username[0])
        ImGui::TextDisabled("Ciao %s — Accedi una volta, poi non chiede più.", st.username);
    else
        ImGui::TextDisabled("Username salvato dopo il primo accesso.");

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({screenW * 0.5f - 48, screenH - 52});
    ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 0.16f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.f, 1.f, 1.f, 0.30f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    if (ImGui::Button(LB("quit", "quitlogin"), {96, 30})) st.wantQuit = true;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::End();
}
#endif

// Icona di un gioco pubblicato: caricata una volta sola e tenuta in cache
// per percorso file (path vuoto o file assente -> 0, si ricade sul colore
// accent nella card).
inline GLuint gameIconTexture(const std::string& path) {
    static std::unordered_map<std::string, GLuint> cache;
    static std::unordered_map<std::string, bool> tried;
    if (path.empty()) return 0;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    if (tried[path]) return 0;
    tried[path] = true;
    Texture2D tex;
    if (!tex.load(path, true)) return 0;
    cache[path] = tex.id;
    return tex.id;
}

inline void drawHubPage(ClassicMenuState& st, const std::vector<GameEntry>& games,
                        float dt, float screenW, float screenH, float time) {
    st.charSpin += dt * 30.f;
    st.uiTime = time;
    st.playPulse = time;
    ecoSyncMenu(st, time);
    if (economy().banned) {
        st.loggedIn = false;
        st.showLogin = true;
        st.loginError = "BANNATO PERMANENTE — cheat economia (2 tentativi)";
        return;
    }
    float tabTarget = (st.tab == HubTab::Games) ? 0.f : (st.tab == HubTab::Shop ? 1.f : 2.f);
    st.tabSlide = uiLerp(st.tabSlide, tabTarget, std::min(1.f, dt * 12.f));

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({screenW, screenH});
    ImGui::Begin("##Hub", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawBlueHubBackground(dl, screenW, screenH, time);

    dl->AddText(ImGui::GetFont(), 30.f, {22, 18}, IM_COL32(255, 255, 255, 255), "BLOCKY");
    dl->AddText({128, 28}, IM_COL32(180, 215, 255, 200), "hub");

    ImGui::SetCursorPos({168, 15});
    float tabStartX = ImGui::GetCursorScreenPos().x;
    float tabY = ImGui::GetCursorScreenPos().y;
    if (hubTabButton(L("hub.games"), st.tab == HubTab::Games)) st.tab = HubTab::Games;
    ImGui::SameLine(0, 8);
    if (hubTabButton(L("hub.shop"), st.tab == HubTab::Shop)) st.tab = HubTab::Shop;
    ImGui::SameLine(0, 8);
    if (hubTabButton(L("hub.chat"), st.tab == HubTab::Chat)) st.tab = HubTab::Chat;

    {
        float underlineX = tabStartX + st.tabSlide * (112.f + 8.f);
        dl->AddRectFilled({underlineX + 18, tabY + 38}, {underlineX + 94, tabY + 41},
                          IM_COL32(255, 255, 255, 220), 2.f);
    }

    {
        char userChip[96];
        std::snprintf(userChip, sizeof(userChip), "%s  ·  %d B$",
                      st.username[0] ? st.username : "Player", st.shopCoins);
        ImVec2 chipSz = ImGui::CalcTextSize(userChip);
        float chipX = screenW - chipSz.x - 122.f;
        dl->AddRectFilled({chipX - 16, 14}, {chipX + chipSz.x + 16, 50},
                          IM_COL32(255, 255, 255, 28), 18.f);
        dl->AddRect({chipX - 16, 14}, {chipX + chipSz.x + 16, 50},
                    IM_COL32(255, 255, 255, 40), 18.f);
        dl->AddText({chipX, 24}, IM_COL32(255, 228, 110, 255), userChip);
        ImGui::SetCursorPos({screenW - 168, 16});
        ImGui::PushStyleColor(ImGuiCol_Button, {1.f, 1.f, 1.f, 0.16f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.f, 1.f, 1.f, 0.30f});
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        drawLanguageButton("hublang");
        ImGui::SameLine(0, 6);
        if (ImGui::Button(LB("logout", "logout"), {82, 32})) {
            st.showLogin = true;
            st.loggedIn = false;
            st.password[0] = '\0';
            // Uscire deve voler dire uscire davvero: al prossimo accesso la
            // passkey verra richiesta di nuovo.
            forgetTrustedDevice();
            st.loginAppear = 0.f;
            saveUserProfile(st);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {1.f, 1.f, 1.f, 0.96f});

    ImGui::SetCursorPos({24, 86});
    ImGui::BeginChild("PlayPanel", {268, 220}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    {
        ImVec2 wp = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
            wp, {wp.x + 268, wp.y + 4},
            IM_COL32(30, 200, 110, 255), IM_COL32(60, 230, 140, 255),
            IM_COL32(60, 230, 140, 0), IM_COL32(30, 200, 110, 0));
    }
    hubPanelHeader("PLAY");
    if (!games.empty()) {
        int gi = std::clamp(st.selectedGame, 0, (int)games.size() - 1);
        ImGui::TextWrapped("%s", games[gi].title.c_str());
        ImGui::TextDisabled("%s", games[gi].subtitle.c_str());
        ImGui::TextDisabled("Max %d giocatori · multiplayer auto",
                            games[gi].maxPlayers > 0 ? games[gi].maxPlayers : 8);
    }
    ImGui::Spacing();
    if (hubPlayButton(L("hub.play"), {236, 52}, time)) {
        st.wantPlay = true;
        st.launchGame = st.selectedGame;
    }
    // Col controller in mano il pulsante giusto dev'essere gia' selezionato:
    // altrimenti premere ✕ non fa "gioca", fa "entra nel pannello", e sembra
    // che il tasto non funzioni. Il fuoco iniziale va qui, sul Play.
    ImGui::SetItemDefaultFocus();

    // wantBuild esisteva gia' come campo, dichiarato per questo scopo esatto
    // (entrare in Studio a costruire) — ma nessun pulsante lo impostava mai
    // a true, ed enterStudio() dal lato main.cpp non veniva mai chiamata.
    // Studio (e con lui la Toolbox, i preset di script, ecc.) esisteva nel
    // codice ed era irraggiungibile dall'interfaccia: lo stesso difetto gia'
    // visto con la lista server, qui sul pulsante che porta a Studio stesso.
    if (kIsGameEngine) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.30f, 0.35f, 0.42f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.40f, 0.46f, 0.55f, 1.f});
        if (ImGui::Button("Studio (costruisci)", {236, 34}))
            st.wantBuild = true;
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Apri l'editor: costruisci, scripta, usa la Toolbox.");
    }
    if (st.netStatus[0])
        ImGui::TextWrapped("%s", st.netStatus);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Entra con IP");
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##joinip", st.joinIp, sizeof(st.joinIp));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64);
    ImGui::InputInt("##joinport", &st.joinPort, 0, 0);
    if (st.joinPort < 1024 || st.joinPort > 65535) st.joinPort = 27015;
    if (ImGui::Button("Entra##joinip", {236, 30})) {
        st.wantJoinIp = true;
        st.launchGame = st.selectedGame;
    }
    ImGui::TextDisabled("Per un server fuori dalla tua rete.");

    ImGui::Spacing();
    ImGui::TextDisabled("Server giochi pubblicati");
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##gameserver", st.gameServer, sizeof(st.gameServer));
    ImGui::SameLine();
    const bool fetching = blk::remotegames::isFetching();
    if (fetching) ImGui::BeginDisabled();
    if (ImGui::Button(fetching ? "..." : "Aggiorna", {80, 30}))
        st.wantRefreshGameServer = true;
    if (fetching) ImGui::EndDisabled();
    ImGui::TextDisabled("IP del PC che ha pubblicato (porta 8080).");

    // Il catalogo viaggia sullo stesso server dei giochi: e lo stesso computer.
    const bool catBusy = blk::remotecatalog::isBusy();
    if (catBusy) ImGui::BeginDisabled();
    if (ImGui::Button(catBusy ? "Invio catalogo..." : "Pubblica catalogo", {236, 28}))
        st.wantPublishCatalog = true;
    if (catBusy) ImGui::EndDisabled();
    ImGui::TextDisabled("Manda i tuoi item a tutti i dispositivi.");

    const bool gamesBusy = blk::remotegames::isUploading();
    if (gamesBusy) ImGui::BeginDisabled();
    if (ImGui::Button(gamesBusy ? "Invio giochi..." : "Sincronizza i miei giochi", {236, 28}))
        st.wantSyncGames = true;
    if (gamesBusy) ImGui::EndDisabled();
    ImGui::TextDisabled("Manda al server i giochi gia pubblicati qui.");

    ImGui::Spacing();
    ImGui::TextDisabled("Entra con codice");
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##joincode", st.joinCode, sizeof(st.joinCode),
                      ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine();
    bool poolOk = blk::PoolConfig::isConfigured();
    if (!poolOk) ImGui::BeginDisabled();
    if (ImGui::Button("Entra##joincode", {80, 30})) {
        st.wantJoinCode = true;
        st.launchGame = st.selectedGame;
    }
    if (!poolOk) ImGui::EndDisabled();
    ImGui::TextDisabled(poolOk ? "Per giocare via internet, ovunque (blk://)."
                               : "Pool online non ancora configurato.");
    ImGui::EndChild();

    ImGui::SetCursorPos({24, 320});
    ImGui::BeginChild("AvatarQuick", {268, 180}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    hubPanelHeader("AVATAR");
    ImGui::TextWrapped("Le parti che crei nel Catalog sono gia' sull'avatar (anche senza pubblicarle nello Shop).");
    ImGui::Spacing();
    ImGui::TextDisabled("Sull'avatar: %d item", (int)st.equippedList.size());
    if (!st.equippedList.empty()) {
        for (int i = 0; i < (int)st.equippedList.size() && i < 3; ++i)
            ImGui::BulletText("%s", st.equippedList[i].name.c_str());
        if ((int)st.equippedList.size() > 3)
            ImGui::TextDisabled("…");
        if (ImGui::SmallButton(LB("hub.clear_all", "clrall")))
            st.wantUnequipEquipped = -2;
    }
    ImGui::EndChild();

    ImGui::SetCursorPos({308, 86});
    ImGui::BeginChild("MainPanel", {screenW - 332, screenH - 110}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);

    if (st.tab == HubTab::Games) {
        hubPanelHeader("GAMES");
        ImGui::TextDisabled("Giochi pubblicati con Blocky Game Engine");
        ImGui::Spacing();
        float cardW = 228.f, cardH = 178.f;
        int cols = std::max(1, (int)((screenW - 380) / (cardW + 16)));
        for (int i = 0; i < (int)games.size(); ++i) {
            if (i > 0 && i % cols != 0) ImGui::SameLine(0, 14);
            ImGui::PushID(i);
            const auto& g = games[i];
            bool sel = (st.selectedGame == i);
            float delay = std::min(1.f, (time * 2.2f - i * 0.08f));
            float pop = uiEaseOutCubic(std::max(0.f, delay));

            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                sel ? ImVec4{0.90f, 0.95f, 1.f, pop} : ImVec4{0.99f, 0.995f, 1.f, pop});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.f);
            ImGui::BeginChild("gcard", {cardW, cardH}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float thumbW = cardW - 18.f;
            bool hovered = ImGui::IsWindowHovered();
            if (hovered || sel) {
                cdl->AddRect({p.x - 7, p.y - 7}, {p.x + cardW - 5, p.y + cardH - 5},
                             sel ? IM_COL32(40, 140, 255, 160) : IM_COL32(100, 170, 255, 90),
                             14.f, 0, sel ? 2.2f : 1.4f);
            }
            GLuint iconTex = g.iconPath.empty() ? 0 : gameIconTexture(g.iconPath);
            if (iconTex) {
                cdl->AddImageRounded((ImTextureID)(intptr_t)iconTex, {p.x, p.y}, {p.x + thumbW, p.y + 78},
                    {0, 1}, {1, 0}, IM_COL32_WHITE, 10.f);
            } else {
                cdl->AddRectFilled({p.x, p.y}, {p.x + thumbW, p.y + 78},
                    IM_COL32((int)(g.accent.x * 255), (int)(g.accent.y * 255), (int)(g.accent.z * 255), 255), 10.f);
            }
            cdl->AddRectFilledMultiColor({p.x, p.y}, {p.x + thumbW, p.y + 78},
                IM_COL32(255, 255, 255, 50), IM_COL32(255, 255, 255, 12),
                IM_COL32(0, 0, 0, 40), IM_COL32(0, 0, 0, 40));
            float sh = fmodf(time * 0.35f + i * 0.2f, 1.4f);
            float sx = p.x + sh * thumbW - 40.f;
            cdl->AddRectFilledMultiColor({sx, p.y}, {sx + 50.f, p.y + 78},
                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 35),
                IM_COL32(255, 255, 255, 35), IM_COL32(255, 255, 255, 0));
            if (g.isPublished) {
                cdl->AddRectFilled({p.x + 8, p.y + 8}, {p.x + 92, p.y + 28}, IM_COL32(12, 160, 100, 235), 8.f);
                cdl->AddText({p.x + 16, p.y + 11}, IM_COL32(255, 255, 255, 255), "PUBBLICATO");
            }
            ImGui::Dummy({0, 84});
            ImGui::Text("%s", g.title.c_str());
            ImGui::TextDisabled("%s", g.subtitle.c_str());
            ImGui::TextColored({0.10f, 0.46f, 0.92f, 1.f}, "%d / %d posti",
                               g.playersOnline, g.maxPlayers > 0 ? g.maxPlayers : 8);
            if (hovered && ImGui::IsMouseClicked(0)) st.selectedGame = i;
            // Tasto destro (o tocco prolungato sul tablet): apre l elenco dei
            // server di questo gioco, invece di affidarsi alla ricerca
            // automatica che prende la prima risposta e se non ne arriva
            // nessuna ospita per conto suo, senza dire perche.
            if (hovered && (ImGui::IsMouseClicked(1)
                            || ImGui::IsMouseDoubleClicked(0))) {
                st.selectedGame = i;
                st.wantServerList = true;
            }
            if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                st.wantPlay = true; st.launchGame = i;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    else if (st.tab == HubTab::Shop) {
        seedDefaultShop(st);
        bucksEnsureLoaded();
        hubPanelHeader("SHOP");
        ImGui::SameLine(0, 12);
        ImGui::TextColored({0.92f, 0.68f, 0.08f, 1.f}, "%d B$", st.shopCoins);
        ImGui::TextDisabled("Blocky Bucks — compra con pay-link, poi riscatta il codice");
        ImGui::Spacing();

        {
            auto& bp = bucksPurchase();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, {1.f, 0.97f, 0.88f, 1.f});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
            ImGui::BeginChild("buyBucks", {screenW - 368.f, 200.f}, true);
            ImGui::TextColored({0.75f, 0.45f, 0.05f, 1.f}, "Compra Blocky Bucks");
            ImGui::TextDisabled("Apre il pay-link nel browser.");
            ImGui::Spacing();
            for (int i = 0; i < (int)bp.packs.size(); ++i) {
                const auto& pk = bp.packs[i];
                ImGui::PushID(9000 + i);
                char lab[96];
                std::snprintf(lab, sizeof(lab), "%s · %s", pk.title.c_str(), pk.priceLabel.c_str());
                if (ImGui::Button(lab, ImVec2(-1, 28))) {
                    if (bucksOpenPayLink(pk))
                        st.chatLines.push_back("[Shop] Pay-link aperto: " + pk.title);
                    else
                        st.chatLines.push_back("[Shop] Impossibile aprire il link");
                }
                ImGui::PopID();
            }
            ImGui::Spacing();
            ImGui::Text("Codice riscatto");
            ImGui::SetNextItemWidth(220.f);
            ImGui::InputText("##redeem", bp.redeemBuf, sizeof(bp.redeemBuf));
            ImGui::SameLine();
            if (ImGui::Button("Riscatta")) {
                std::string err;
                if (bucksRedeem(st.username, bp.redeemBuf, err)) {
                    ecoSyncMenu(st);
                    saveUserProfile(st);
                    st.chatLines.push_back(std::string("[Shop] ") + bp.lastMsg);
                } else {
                    st.chatLines.push_back(std::string("[Shop] ") + err);
                }
            }
            if (!bp.lastMsg.empty())
                ImGui::TextColored({0.15f, 0.55f, 0.25f, 1.f}, "%s", bp.lastMsg.c_str());
            ImGui::TextDisabled("Demo: BB-100-DEMO01 · BB-500-DEMO01 · BB-1200-DEMO1");
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (st.shopItems.empty())
            ImGui::TextWrapped("Nessun item. Gli avatar pubblicati da Blocky Game Engine compaiono qui.");

        float shopRowW = screenW - 368.f;
        for (int i = 0; i < (int)st.shopItems.size(); ++i) {
            auto& it = st.shopItems[i];
            ImGui::PushID(i);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.97f, 0.985f, 1.f, 1.f});
            ImGui::BeginChild("shopitem", {shopRowW, it.fromPublisher ? 86.f : 64.f}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
            if (it.kind == 1) {
                ImGui::TextColored({0.55f, 0.28f, 0.85f, 1.f}, "[Walk] %s", it.name.c_str());
                ImGui::TextDisabled("Walking Animation · da %s · %d B$", it.seller.c_str(), it.price);
            } else if (it.fromPublisher) {
                ImGui::TextColored({0.12f, 0.48f, 0.90f, 1.f}, "[Avatar] %s", it.name.c_str());
                if (it.price <= 0)
                    ImGui::TextDisabled("tuo · creato nel Catalog · gia' equipaggiabile");
                else
                    ImGui::TextDisabled("da %s · slot %d · %d B$", it.seller.c_str(), it.slot, it.price);
            } else {
                ImGui::Text("%s — %d B$", it.name.c_str(), it.price);
                ImGui::TextDisabled("da %s", it.seller.c_str());
            }
            ImGui::SameLine(shopRowW - 210);
            if (it.owned) {
                if (it.equipped) {
                    ImGui::TextColored({0.10f, 0.46f, 0.92f, 1.f}, "%s", L("hub.equipped"));
                    ImGui::SameLine();
                    if (ImGui::Button(LB("hub.unequip", "uneq")))
                        st.wantUnequipShop = i;
                } else {
                    ImGui::TextColored({0.10f, 0.68f, 0.36f, 1.f}, "%s", L("hub.owned"));
                    ImGui::SameLine();
                    if ((it.fromPublisher || it.kind == 1) && !it.sourcePath.empty()) {
                        if (ImGui::Button(LB("hub.equip", "eq")))
                            st.wantEquipShop = i;
                    }
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, {0.10f, 0.46f, 0.94f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.56f, 1.f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
                if (ImGui::Button(LB("hub.buy", "buy"))) {
                    if (ecoSpend(st.username, it.price, it.name.c_str())) {
                        ecoSyncMenu(st);
                        it.owned = true;
                        st.chatLines.push_back(std::string("[Shop] Comprato ") + it.name
                            + " (−" + std::to_string(it.price) + " B$)");
                        saveUserProfile(st);
                        saveShopItems(st);
                        std::string me = st.username[0] ? st.username : "Player";
                        if (it.fromPublisher && it.seller != me) {
                            st.chatLines.push_back("[Shop] " + it.seller + " ha guadagnato dalla vendita!");
                        }
                    } else {
                        st.chatLines.push_back(std::string("[Shop] ") +
                            (economy().lastError.empty() ? "B$ insufficienti" : economy().lastError));
                    }
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
    }
    else if (st.tab == HubTab::Chat) {
        hubPanelHeader("CHAT");
        ImGui::SameLine(0, 8);
        ImGui::TextDisabled("parolacce censurate · puoi segnalare");

        float logH = (float)screenH - 380.f;
        if (logH < 120.f) logH = 120.f;
        ImGui::BeginChild("ChatLog", {0, logH}, true);
        for (const auto& line : st.chatLines) ImGui::TextUnformatted(line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();

        ImGui::SetNextItemWidth(screenW - 450.f);
        bool send = ImGui::InputText("##chat", st.chatInput, sizeof(st.chatInput), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button(LB("hub.send", "send")) || send) && st.chatInput[0]) {
            std::string user = st.username[0] ? st.username : "Player";
            Moderation::tryAppendChat(st.chatLines, st.mod, user, st.chatInput);
            st.chatInput[0] = '\0';
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.85f, 0.35f, 0.20f, 1.f}, "%s", L("hub.report"));
        ImGui::TextWrapped(
            "Scegli un messaggio reale dalla lista. Il sistema verifica se esiste "
            "e se contiene una violazione — altrimenti il report viene respinto.");

        ImGui::BeginChild("##reportMsgs", {0, 90}, true);
        bool any = false;
        for (int i = (int)st.mod.history.size() - 1; i >= 0; --i) {
            const auto& e = st.mod.history[(size_t)i];
            if (e.blocked) continue;
            any = true;
            bool sel = (st.mod.selectedMsgId == e.id);
            std::string lab = "#" + std::to_string(e.id) + " [" + e.user + "] " + e.display;
            if (lab.size() > 90) lab = lab.substr(0, 87) + "...";
            lab += "##rm" + std::to_string(e.id);
            if (ImGui::Selectable(lab.c_str(), sel))
                st.mod.selectedMsgId = e.id;
        }
        if (!any)
            ImGui::TextDisabled("Nessun messaggio segnalabile ancora.");
        ImGui::EndChild();

        ImGui::SetNextItemWidth(screenW - 450.f);
        ImGui::InputTextWithHint("##rreason", "Motivo (opzionale)", st.mod.reasonBuf, sizeof(st.mod.reasonBuf));
        ImGui::SameLine();
        if (ImGui::Button(LB("hub.report_send", "repsend"))) {
            std::string me = st.username[0] ? st.username : "Player";
            auto rep = Moderation::submitReport(st.mod, me, st.mod.selectedMsgId, st.mod.reasonBuf);
            st.mod.lastReportMsg = std::string("[Report #") + std::to_string(rep.id) + "] "
                + Moderation::reportStatusLabel(rep.status) + " — " + rep.note;
            st.chatLines.push_back(st.mod.lastReportMsg);
            st.mod.reasonBuf[0] = '\0';
        }

        if (!st.mod.lastReportMsg.empty())
            ImGui::TextWrapped("%s", st.mod.lastReportMsg.c_str());

        if (!st.mod.reports.empty()) {
            ImGui::TextDisabled("Ultimi esiti:");
            int shown = 0;
            for (int i = (int)st.mod.reports.size() - 1; i >= 0 && shown < 3; --i, ++shown) {
                const auto& r = st.mod.reports[(size_t)i];
                ImGui::BulletText("#%d %s → %s: %s", r.id, r.reporter.c_str(),
                    r.accused.empty() ? "?" : r.accused.c_str(),
                    Moderation::reportStatusLabel(r.status));
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::End();
}

inline void drawClassicMainMenu(ClassicMenuState& st, const std::vector<GameEntry>& games,
                                float dt, float screenW, float screenH, float time) {
    if (st.showLogin || !st.loggedIn)
        drawLoginPage(st, screenW, screenH, dt, time);
    else
        drawHubPage(st, games, dt, screenW, screenH, time);
        drawServerListWindow(st);
}

inline void drawLoadingScreen(float progress, const std::string& gameTitle,
                              float screenW, float screenH, float time = 0.f) {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({screenW, screenH});
    ImGui::Begin("##Loading", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawBlueHubBackground(dl, screenW, screenH, time);

    ImVec2 c{screenW * 0.5f, screenH * 0.48f};
    const char* brand = "BLOCKY";
    float bob = std::sin(time * 2.2f) * 4.f;
    ImVec2 brandSz = ImGui::GetFont()->CalcTextSizeA(46.f, FLT_MAX, 0.f, brand);
    dl->AddText(ImGui::GetFont(), 46.f, {c.x - brandSz.x * 0.5f + 2, c.y - 118 + bob + 3},
                IM_COL32(10, 40, 100, 90), brand);
    dl->AddText(ImGui::GetFont(), 46.f, {c.x - brandSz.x * 0.5f, c.y - 118 + bob},
                IM_COL32(255, 255, 255, 255), brand);

    const char* loading = "Caricamento...";
    ImVec2 loadSz = ImGui::CalcTextSize(loading);
    int blink = 180 + (int)((0.5f + 0.5f * std::sin(time * 4.f)) * 75.f);
    dl->AddText({c.x - loadSz.x * 0.5f, c.y - 52}, IM_COL32(220, 235, 255, blink), loading);

    ImVec2 titleSz = ImGui::CalcTextSize(gameTitle.c_str());
    dl->AddText({c.x - titleSz.x * 0.5f, c.y - 28}, IM_COL32(255, 255, 255, 255), gameTitle.c_str());

    float barW = 400.f;
    ImVec2 bar0{c.x - barW * 0.5f, c.y + 20};
    ImVec2 bar1{c.x + barW * 0.5f, c.y + 46};
    uiSoftShadow(dl, bar0, bar1, 12.f, 3);
    dl->AddRectFilled(bar0, bar1, IM_COL32(14, 48, 110, 230), 12.f);
    float fill = barW * (progress < 0.f ? 0.f : (progress > 1.f ? 1.f : progress));
    if (fill > 2.f) {
        dl->AddRectFilled(bar0, {bar0.x + fill, bar1.y}, IM_COL32(36, 215, 115, 255), 12.f);
        float shimmer = fmodf(time * 1.8f, 1.f);
        float sx = bar0.x + shimmer * fill - 30.f;
        if (sx < bar0.x + fill)
            dl->AddRectFilledMultiColor({sx, bar0.y}, {sx + 40.f, bar1.y},
                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 70),
                IM_COL32(255, 255, 255, 70), IM_COL32(255, 255, 255, 0));
    }
    dl->AddRect(bar0, bar1, IM_COL32(160, 210, 255, 130), 12.f, 0, 1.6f);

    char pct[16];
    std::snprintf(pct, sizeof(pct), "%d%%", (int)(progress * 100.f + 0.5f));
    ImVec2 pctSz = ImGui::CalcTextSize(pct);
    dl->AddText({c.x - pctSz.x * 0.5f, c.y + 58}, IM_COL32(210, 230, 255, 255), pct);
    ImGui::End();
}

/**
 * Finestra "Server di questo gioco".
 *
 * Mostra quello che risponde davvero in rete. Se l'elenco e' vuoto non e' un
 * guasto muto: significa che nessun server ha risposto, ed e' un'informazione
 * utile quanto un elenco pieno.
 */
inline void drawServerListWindow(ClassicMenuState& st) {
    if (!st.showServerList) return;

    ImGui::SetNextWindowSize({520, 320}, ImGuiCond_Appearing);
    if (!ImGui::Begin("Server di questo gioco##srvlist", &st.showServerList)) {
        ImGui::End();
        return;
    }

    const bool scanning = blk::serverbrowser::isScanning();
    if (scanning) ImGui::BeginDisabled();
    if (ImGui::Button(scanning ? "Cerco..." : "Cerca di nuovo", {150, 32}))
        blk::serverbrowser::startScan(st.joinPort, st.selectedGame);
    if (scanning) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("porta %d", st.joinPort);

    ImGui::Separator();

    const auto servers = blk::serverbrowser::result();
    if (scanning && servers.empty()) {
        ImGui::TextDisabled("Sto chiedendo in rete...");
    } else if (servers.empty()) {
        ImGui::TextColored({1.f, 0.72f, 0.3f, 1.f}, "Nessun server ha risposto.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Controlla che sul computer che ospita sia avviato BlockyServer, "
            "che sia sulla stessa rete e che il firewall di Windows lo lasci "
            "passare. Puoi anche entrare a mano con \"Entra con IP\".");
    } else {
        for (size_t i = 0; i < servers.size(); ++i) {
            ImGui::PushID((int)i);
            const auto& s = servers[i];
            const bool full = (s.players >= s.maxPlayers);
            if (full) ImGui::BeginDisabled();
            if (ImGui::Button(full ? "Pieno" : "Entra", {90, 28})) {
                std::snprintf(st.joinIp, sizeof(st.joinIp), "%s", s.ip.c_str());
                st.joinPort = s.port;
                st.wantJoinIp = true;
                st.launchGame = st.selectedGame;
                st.showServerList = false;
            }
            if (full) ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextUnformatted(s.label().c_str());
            ImGui::PopID();
        }
    }

    ImGui::End();
}
