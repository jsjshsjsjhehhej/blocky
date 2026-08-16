#pragma once

/**
 * Acquisto Blocky Bucks (B$) via pay-link (Stripe Payment Link o simile).
 *
 * Flusso:
 *  1. Shop → Compra B$ → apre payLink nel browser
 *  2. Dopo il pagamento l'utente riceve un codice
 *  3. Incolla il codice in Shop → credito B$
 *
 * Config: %APPDATA%/Blocky/bucks_store.json
 * Codici:  %APPDATA%/Blocky/bucks_codes.json
 */

#include "Economy.h"
#include "GameHub.h"
#include "GoogleAuth.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct BucksPack {
    std::string id;
    std::string title;
    int bucks = 0;
    std::string priceLabel;
    std::string payLink;
};

struct BucksRedeemCode {
    std::string code;
    int bucks = 0;
    bool used = false;
};

struct BucksPurchaseState {
    std::vector<BucksPack> packs;
    std::vector<BucksRedeemCode> codes;
    char redeemBuf[64] = {};
    std::string lastMsg;
    std::string pendingPackId;
    bool loaded = false;
};

inline BucksPurchaseState& bucksPurchase() {
    static BucksPurchaseState s;
    return s;
}

inline std::string bucksStorePath() { return blockyDataDir() + "/bucks_store.json"; }
inline std::string bucksCodesPath() { return blockyDataDir() + "/bucks_codes.json"; }
inline std::string bucksUsedPath() { return blockyDataDir() + "/bucks_used.txt"; }

inline std::string bucksNormCode(std::string s) {
    std::string o;
    for (unsigned char c : s) {
        if (c >= 'a' && c <= 'z') o.push_back((char)(c - 'a' + 'A'));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')
            o.push_back((char)c);
    }
    return o;
}

inline std::string bucksEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c >= 32) o.push_back((char)c);
    }
    return o;
}

/** Grant da acquisto (codice) — tetto più alto dei reward di gioco. */
inline bool ecoGrantPurchase(const char* user, int amount, const char* reason = nullptr) {
    auto& e = economy();
    if (e.banned) {
        e.lastError = "Account bannato";
        return false;
    }
    if (!ecoVerify(user, 0.f)) return false;
    if (amount <= 0 || amount > 100000) {
        e.lastError = "importo acquisto non valido";
        return false;
    }
    long long next = (long long)e.coins + amount;
    if (next > EconomyState::kMaxCoins) next = EconomyState::kMaxCoins;
    e.coins = (int)next;
    ecoReseal(user);
    if (reason)
        std::fprintf(stdout, "[Purchase] +%d B$ (%s) -> %d\n", amount, reason, e.coins);
    return true;
}

inline void bucksSeedDefaultPacks(BucksPurchaseState& st) {
    st.packs.clear();
    auto add = [&](const char* id, const char* title, int b, const char* price, const char* link) {
        BucksPack p;
        p.id = id;
        p.title = title;
        p.bucks = b;
        p.priceLabel = price;
        p.payLink = link;
        st.packs.push_back(std::move(p));
    };
    // Sostituisci payLink in bucks_store.json con i tuoi Stripe Payment Links
    add("b100",  "100 Blocky Bucks",   100,  "€0.99",  "https://buy.stripe.com/test_blocky_100");
    add("b500",  "500 Blocky Bucks",   500,  "€3.99",  "https://buy.stripe.com/test_blocky_500");
    add("b1200", "1.200 Blocky Bucks", 1200, "€7.99",  "https://buy.stripe.com/test_blocky_1200");
    add("b3000", "3.000 Blocky Bucks", 3000, "€14.99", "https://buy.stripe.com/test_blocky_3000");
}

inline void bucksSeedDemoCodes(BucksPurchaseState& st) {
    if (!st.codes.empty()) return;
    auto add = [&](const char* code, int b) {
        BucksRedeemCode c;
        c.code = bucksNormCode(code);
        c.bucks = b;
        c.used = false;
        st.codes.push_back(std::move(c));
    };
    add("BB-100-DEMO01", 100);
    add("BB-500-DEMO01", 500);
    add("BB-1200-DEMO1", 1200);
}

inline bool bucksSaveStore(const BucksPurchaseState& st) {
    std::ostringstream j;
    j << "{\n  \"packs\": [\n";
    for (size_t i = 0; i < st.packs.size(); ++i) {
        const auto& p = st.packs[i];
        if (i) j << ",\n";
        j << "    {\"id\":\"" << bucksEscape(p.id)
          << "\",\"title\":\"" << bucksEscape(p.title)
          << "\",\"bucks\":" << p.bucks
          << ",\"price\":\"" << bucksEscape(p.priceLabel)
          << "\",\"payLink\":\"" << bucksEscape(p.payLink) << "\"}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(bucksStorePath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool bucksSaveCodes(const BucksPurchaseState& st) {
    std::ostringstream j;
    j << "{\n  \"codes\": [\n";
    for (size_t i = 0; i < st.codes.size(); ++i) {
        const auto& c = st.codes[i];
        if (i) j << ",\n";
        j << "    {\"code\":\"" << bucksEscape(c.code)
          << "\",\"bucks\":" << c.bucks
          << ",\"used\":" << (c.used ? "true" : "false") << "}";
    }
    j << "\n  ]\n}\n";
    std::ofstream f(bucksCodesPath());
    if (!f) return false;
    f << j.str();
    return true;
}

inline bool bucksLoadUsedInto(BucksPurchaseState& st) {
    std::string txt = readTextFile(bucksUsedPath());
    if (txt.empty()) return false;
    for (auto& c : st.codes) {
        if (txt.find(c.code) != std::string::npos)
            c.used = true;
    }
    return true;
}

inline void bucksMarkUsed(const std::string& code) {
    std::ofstream f(bucksUsedPath(), std::ios::app);
    if (f) f << code << "\n";
}

inline void bucksLoadStore(BucksPurchaseState& st) {
    st.packs.clear();
    std::string json = readTextFile(bucksStorePath());
    if (json.empty()) {
        bucksSeedDefaultPacks(st);
        bucksSaveStore(st);
    } else {
        size_t pos = 0;
        while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
            BucksPack p;
            p.id = jsonFindStr(json, "id", pos);
            p.title = jsonFindStr(json, "title", pos);
            float b = 0.f;
            jsonFindFloat(json, "bucks", pos, b);
            p.bucks = (int)b;
            p.priceLabel = jsonFindStr(json, "price", pos);
            if (p.priceLabel.empty())
                p.priceLabel = jsonFindStr(json, "priceLabel", pos);
            p.payLink = jsonFindStr(json, "payLink", pos);
            if (!p.id.empty() && p.bucks > 0)
                st.packs.push_back(std::move(p));
            pos += 4;
        }
        if (st.packs.empty()) {
            bucksSeedDefaultPacks(st);
            bucksSaveStore(st);
        }
    }

    st.codes.clear();
    std::string cj = readTextFile(bucksCodesPath());
    if (!cj.empty()) {
        size_t pos = 0;
        while ((pos = cj.find("\"code\"", pos)) != std::string::npos) {
            BucksRedeemCode c;
            c.code = bucksNormCode(jsonFindStr(cj, "code", pos));
            float b = 0.f;
            jsonFindFloat(cj, "bucks", pos, b);
            c.bucks = (int)b;
            c.used = jsonFindBool(cj, "used", pos, false);
            if (!c.code.empty() && c.bucks > 0)
                st.codes.push_back(std::move(c));
            pos += 6;
        }
    }
    bucksSeedDemoCodes(st);
    bucksLoadUsedInto(st);
    bucksSaveCodes(st);
    st.loaded = true;
}

inline void bucksEnsureLoaded() {
    if (!bucksPurchase().loaded)
        bucksLoadStore(bucksPurchase());
}

/** Se i link sono ancora placeholder Stripe test → pagina demo locale. */
inline void bucksSetFallbackPayPage(const std::string& fileUrlOrHttp) {
    bucksEnsureLoaded();
    auto& st = bucksPurchase();
    for (auto& p : st.packs) {
        if (p.payLink.find("stripe.com/test_blocky") != std::string::npos
            || p.payLink.empty()) {
            p.payLink = fileUrlOrHttp + "?pack=" + p.id + "&bucks=" + std::to_string(p.bucks);
        }
    }
}

inline bool bucksOpenPayLink(const BucksPack& pack) {
    if (pack.payLink.empty()) return false;
    bucksPurchase().pendingPackId = pack.id;
    bucksPurchase().lastMsg = std::string("Aperto pagamento: ") + pack.title
        + " — dopo il pay inserisci il codice qui sotto.";
    return openUrlInBrowser(pack.payLink.c_str());
}

inline bool bucksRedeem(const char* user, const char* rawCode, std::string& errOut) {
    bucksEnsureLoaded();
    auto& st = bucksPurchase();
    std::string code = bucksNormCode(rawCode ? rawCode : "");
    if (code.size() < 6) {
        errOut = "Codice troppo corto";
        return false;
    }
    for (auto& c : st.codes) {
        if (c.code != code) continue;
        if (c.used) {
            errOut = "Codice gia' usato";
            return false;
        }
        if (!ecoGrantPurchase(user, c.bucks, ("redeem " + code).c_str())) {
            errOut = economy().lastError.empty() ? "Grant fallito" : economy().lastError;
            return false;
        }
        c.used = true;
        bucksMarkUsed(code);
        bucksSaveCodes(st);
        errOut.clear();
        st.lastMsg = std::string("+") + std::to_string(c.bucks) + " B$ accreditati!";
        st.redeemBuf[0] = 0;
        return true;
    }
    errOut = "Codice non valido";
    return false;
}
