#pragma once

/**
 * Moderazione Blocky v2 — chat, username, avatar/giochi.
 * - Odio/razzismo → blocco + strike
 * - Parolacce → censura (****) + soft strike
 * - Anti-spam / rate-limit / mute
 * - Bypass leet / punti / ripetizioni (c.a.z.z.o, fuuuck)
 * - Report giocatori → verifica automatica
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

namespace Moderation {

enum class Verdict {
    Ok,
    Blocked
};

struct CheckResult {
    Verdict verdict = Verdict::Ok;
    std::string reason;
};

inline std::string toLowerAscii(std::string s) {
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

/** Normalizza aggressiva: leet, strip separatori, collassa ripetizioni (fuuuck→fuck). */
inline std::string normalizeForMatch(const std::string& raw) {
    std::string tmp;
    tmp.reserve(raw.size());
    for (unsigned char uc : raw) {
        char c = (char)std::tolower(uc);
        switch (c) {
        case '0': c = 'o'; break;
        case '1': case '|': c = 'i'; break;
        case '3': c = 'e'; break;
        case '4': case '@': c = 'a'; break;
        case '5': case '$': c = 's'; break;
        case '7': case '+': c = 't'; break;
        case '8': c = 'b'; break;
        case '9': c = 'g'; break;
        case '!': c = 'i'; break;
        case '*': continue; // già censurato
        default: break;
        }
        // Separatori usati per bypass: c.a.z.z.o / c-a-z-z-o / c a z z o
        if (c == '.' || c == '-' || c == '_' || c == ' ' || c == '\t'
            || c == '/' || c == '\\' || c == ',' || c == ';' || c == ':'
            || c == '\'' || c == '"' || c == '`')
            continue;
        if (c >= 'a' && c <= 'z')
            tmp.push_back(c);
        else if (c >= '0' && c <= '9')
            tmp.push_back(c);
    }
    // Collassa 3+ lettere uguali → 2 (cazzooo → cazzoo → ancora utile)
    std::string out;
    out.reserve(tmp.size());
    for (size_t i = 0; i < tmp.size(); ++i) {
        if (out.size() >= 2 && out[out.size() - 1] == tmp[i] && out[out.size() - 2] == tmp[i])
            continue;
        out.push_back(tmp[i]);
    }
    return out;
}

/** Match termine con confine “soft” (evita assassin→ass su termini corti). */
inline bool matchTermInNorm(const std::string& norm, const std::string& term) {
    if (term.size() < 3 || norm.size() < term.size()) return false;
    if (term.size() >= 5) {
        // termini lunghi: substring ok
        return norm.find(term) != std::string::npos;
    }
    // termini corti (3–4): richiedi bordo non-alfabetico
    size_t pos = 0;
    while ((pos = norm.find(term, pos)) != std::string::npos) {
        bool leftOk = (pos == 0) || (norm[pos - 1] < 'a' || norm[pos - 1] > 'z');
        size_t end = pos + term.size();
        bool rightOk = (end >= norm.size()) || (norm[end] < 'a' || norm[end] > 'z');
        if (leftOk && rightOk) return true;
        // anche se attaccato a cifra
        if ((pos == 0 || !std::isalpha((unsigned char)norm[pos - 1]))
            && (end >= norm.size() || !std::isalpha((unsigned char)norm[end])))
            return true;
        pos += 1;
    }
    return false;
}

inline bool isWordChar(unsigned char c) {
    return std::isalnum(c) || c >= 128; // tieni anche UTF-8 bytes dentro il token
}

inline const std::vector<std::string>& blockedTerms() {
    static const std::vector<std::string> k = {
        "nigger", "nigga", "nigg", "coon", "spic", "chink", "gook",
        "kike", "wetback", "beaner", "paki", "raghead", "towelhead",
        "sandnigger", "porchmonkey", "jigaboo", "darkie", "redskin",
        "slanteye", "zipperhead", "niglet", "monkeynigger",
        "heilhitler", "siegheil", "whitepower", "whitepride", "kkk",
        "gasjews", "holocaustdenial", "hitlerwasright", "nazisupreme",
        "negraccio", "negrona", "terrone", "polentone",
        "zingaro", "zingara", "vucumpra", "vucumprà",
        "scimmianera", "faccianera", "sporconero", "sporconera",
        "mussulmanodimerda", "cinesaccio", "albanesedimerda",
        "rumenodimerda", "marocchinodimerda", "negrodimerda",
        "killnig", "hangnig", "lynchnig",
        "faggotsdie", "tranny", "faggot", "dyke",
        "1488", "88heil", "14words", "wprace",
        "killalljews", "deathtoeurope", "rape",
    };
    return k;
}

/** Parolacce: non bloccano, vengono censurate. Lista ampliata IT/EN + varianti. */
inline const std::vector<std::string>& swearTerms() {
    static const std::vector<std::string> k = {
        // IT core
        "cazzo", "cazzi", "cazzone", "cazzata", "cazzate", "incazzo", "incazzato", "incazzata",
        "merda", "merde", "merdoso", "merdosa", "stronzo", "stronza", "stronzi", "stronze",
        "coglione", "coglioni", "cogliona", "coglionata",
        "puttana", "puttane", "puttaniere", "troia", "troie", "troione",
        "vaffanculo", "fanculo", "affanculo", "vaiaffanculo",
        "minchia", "minchie", "minchione", "figa", "fighe", "figone",
        "diocane", "porcodio", "porcoddio", "dioiboia", "dioporco", "madonna",
        "bastardo", "bastarda", "bastardi", "deficiente", "deficienti",
        "idiota", "idioti", "scemo", "scema", "scemi", "imbecille", "imbecilli",
        "pezzodimerda", "testadiminchia", "facciadimerda", "lurido", "lurida",
        "culattone", "ricchione", "finocchio",
        "zoccola", "baldracca", "mignotta", "bagascia",
        "cazzaro", "cazzari", "pirla", "pirlata",
        // EN core + variants
        "fuck", "fucking", "fucker", "motherfucker", "mfucker", "fuk", "fck", "fukc",
        "shit", "shitty", "bullshit", "horseshit", "crap",
        "bitch", "bitches", "asshole", "arsehole", "bastard", "damn", "dammit",
        "cunt", "dick", "dickhead", "cock", "cocksucker",
        "piss", "pissed", "bollocks", "wanker", "tosser", "slut", "whore",
        "retard", "retarded", "stfu", "gtfo", "kys",
    };
    return k;
}

inline bool hasRacistContextNear(const std::string& norm, const std::string& word) {
    size_t pos = norm.find(word);
    if (pos == std::string::npos) return false;
    static const char* badNearNorm[] = {
        "dimerda", "merda", "sporco", "vattene", "tornatene", "scimmia",
        "kill", "muori", "odio", "hate", "fuck", "shit"
    };
    for (const char* b : badNearNorm) {
        if (norm.find(b) != std::string::npos)
            return true;
    }
    return false;
}

inline bool containsAnyNormalized(const std::string& norm, const std::vector<std::string>& terms) {
    for (const auto& term : terms) {
        std::string t = normalizeForMatch(term);
        if (t.size() < 3) continue;
        if (matchTermInNorm(norm, t))
            return true;
    }
    return false;
}

inline CheckResult checkText(const std::string& text, const char* contextLabel) {
    CheckResult r;
    if (text.empty()) return r;

    const std::string norm = normalizeForMatch(text);
    if (norm.empty()) return r;

    for (const auto& term : blockedTerms()) {
        std::string t = normalizeForMatch(term);
        if (t.size() < 3) continue;
        if (matchTermInNorm(norm, t) || (t.size() >= 5 && norm.find(t) != std::string::npos)) {
            r.verdict = Verdict::Blocked;
            r.reason = std::string("[Moderazione] Contenuto bloccato (")
                + contextLabel + "): linguaggio d'odio / razzista non consentito.";
            return r;
        }
    }

    if (hasRacistContextNear(norm, "marocchino")
        || hasRacistContextNear(norm, "cinese")
        || hasRacistContextNear(norm, "rumeno")
        || hasRacistContextNear(norm, "africano")
        || hasRacistContextNear(norm, "ebreo")
        || hasRacistContextNear(norm, "musulmano")
        || hasRacistContextNear(norm, "mussulmano")) {
        r.verdict = Verdict::Blocked;
        r.reason = std::string("[Moderazione] Contenuto bloccato (")
            + contextLabel + "): linguaggio discriminatorio non consentito.";
        return r;
    }

    return r;
}

inline bool isSwearToken(const std::string& token) {
    std::string n = normalizeForMatch(token);
    if (n.size() < 3) return false;
    for (const auto& s : swearTerms()) {
        std::string t = normalizeForMatch(s);
        if (t.empty()) continue;
        if (n == t || (t.size() >= 4 && n.find(t) != std::string::npos))
            return true;
    }
    // multi-word swears già coperte da normalize sull'intera frase in containsSwear
    return false;
}

inline bool containsSwear(const std::string& text) {
    return containsAnyNormalized(normalizeForMatch(text), swearTerms());
}

/** Sostituisce le parolacce con * della stessa lunghezza (token per token). */
inline std::string censorSwears(const std::string& msg, bool* outDidCensor = nullptr) {
    if (outDidCensor) *outDidCensor = false;
    if (msg.empty()) return msg;

    std::string out;
    out.reserve(msg.size());
    size_t i = 0;
    while (i < msg.size()) {
        if (!isWordChar((unsigned char)msg[i])) {
            out.push_back(msg[i++]);
            continue;
        }
        size_t j = i;
        while (j < msg.size() && isWordChar((unsigned char)msg[j])) ++j;
        std::string token = msg.substr(i, j - i);
        if (isSwearToken(token)) {
            out.append(token.size(), '*');
            if (outDidCensor) *outDidCensor = true;
        } else {
            out += token;
        }
        i = j;
    }

    // Frasi unite tipo "vaffanculo" già gestite; "dio cane" come due token:
    // se entrambi presenti in sequenza, ricensura a livello frase se ancora presenti
    if (containsSwear(out)) {
        // fallback: se dopo token-censor resta match normalizzato, maschera parole residue note
        std::string second;
        second.reserve(out.size());
        i = 0;
        bool any = false;
        while (i < out.size()) {
            if (!isWordChar((unsigned char)out[i])) {
                second.push_back(out[i++]);
                continue;
            }
            size_t j = i;
            while (j < out.size() && isWordChar((unsigned char)out[j])) ++j;
            std::string token = out.substr(i, j - i);
            std::string n = normalizeForMatch(token);
            bool bad = false;
            for (const auto& s : swearTerms()) {
                std::string t = normalizeForMatch(s);
                if (t.size() >= 4 && n.find(t) != std::string::npos) { bad = true; break; }
                if (n == t) { bad = true; break; }
            }
            if (bad) {
                second.append(token.size(), '*');
                any = true;
            } else {
                second += token;
            }
            i = j;
        }
        if (any) {
            if (outDidCensor) *outDidCensor = true;
            return second;
        }
    }
    return out;
}

inline CheckResult checkChat(const std::string& msg) {
    return checkText(msg, "chat");
}

inline CheckResult checkUsername(const std::string& name) {
    return checkText(name, "username");
}

inline CheckResult checkAvatarOrModelName(const std::string& nameOrPath) {
    std::string name = nameOrPath;
    auto slash = name.find_last_of("\\/");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name = name.substr(0, dot);

    auto r = checkText(name, "avatar/modello");
    if (r.verdict == Verdict::Blocked) {
        r.reason = "[Moderazione] Nome modello/avatar non consentito "
                   "(contenuto razzista o d'odio). Rinomina il file o l'item.";
    }
    return r;
}

inline CheckResult checkGameTitle(const std::string& title) {
    auto r = checkText(title, "titolo gioco");
    if (r.verdict == Verdict::Blocked) {
        r.reason = "[Moderazione] Titolo gioco non consentito "
                   "(contenuto razzista o d'odio).";
    }
    return r;
}

// ─── Log chat + Report con verifica ─────────────────────────────────────────

struct ChatEntry {
    int id = 0;
    std::string user;
    std::string original; // per verifica report (non mostrato in UI)
    std::string display;
    bool censored = false;
    bool blocked = false; // tentativo odio (non pubblicato)
};

enum class ReportStatus {
    Confirmed,          // messaggio trovato + violazione vera
    RejectedNoEvidence, // messaggio inesistente / non verificabile
    RejectedWrongUser,  // messaggio non di quell'utente
    RejectedClean,      // messaggio pulito → report falso/non fondato
    RejectedSelf,       // auto-report
    RejectedDuplicate
};

inline const char* reportStatusLabel(ReportStatus s) {
    switch (s) {
    case ReportStatus::Confirmed: return "CONFERMATO (violazione verificata)";
    case ReportStatus::RejectedNoEvidence: return "RESPINTO (messaggio non trovato)";
    case ReportStatus::RejectedWrongUser: return "RESPINTO (utente non corrisponde)";
    case ReportStatus::RejectedClean: return "RESPINTO (nessuna violazione — report non fondato)";
    case ReportStatus::RejectedSelf: return "RESPINTO (non puoi segnalare te stesso)";
    case ReportStatus::RejectedDuplicate: return "RESPINTO (già segnalato)";
    }
    return "?";
}

struct Report {
    int id = 0;
    std::string reporter;
    std::string accused;
    int messageId = -1;
    std::string reason;
    ReportStatus status = ReportStatus::RejectedNoEvidence;
    std::string note;
};

struct Session {
    std::vector<ChatEntry> history;
    std::vector<Report> reports;
    int nextMsgId = 1;
    int nextReportId = 1;

    // Anti-spam / mute
    int strikes = 0;
    double mutedUntil = 0.0;       // chrono seconds
    double lastChatAt = 0.0;
    std::string lastChatNorm;
    int repeatCount = 0;
    float minChatIntervalSec = 0.65f;

    // UI report
    int selectedMsgId = -1;
    char reasonBuf[160] = "";
    std::string lastReportMsg;

    bool isMuted() const {
        using clock = std::chrono::steady_clock;
        double now = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
        return now < mutedUntil;
    }
    double muteRemaining() const {
        using clock = std::chrono::steady_clock;
        double now = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
        return std::max(0.0, mutedUntil - now);
    }
    void addStrike(int n, double muteSec) {
        strikes += n;
        using clock = std::chrono::steady_clock;
        double now = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
        mutedUntil = std::max(mutedUntil, now + muteSec);
    }
};

inline std::string reportsPath() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0]) {
        std::string dir = std::string(appdata) + "\\Blocky";
        _mkdir(dir.c_str());
        return dir + "\\reports.log";
    }
#endif
    return "blocky_reports.log";
}

inline void appendReportLog(const Report& r) {
    FILE* f = std::fopen(reportsPath().c_str(), "ab");
    if (!f) return;
    std::fprintf(f, "#%d | %s → %s | msg=%d | %s | %s | %s\n",
        r.id, r.reporter.c_str(), r.accused.c_str(), r.messageId,
        reportStatusLabel(r.status), r.reason.c_str(), r.note.c_str());
    std::fclose(f);
}

inline const ChatEntry* findMessage(const Session& s, int id) {
    for (const auto& e : s.history)
        if (e.id == id) return &e;
    return nullptr;
}

/**
 * Verifica il report sul messaggio citato.
 * Solo se il messaggio esiste e contiene una violazione reale → CONFERMATO.
 */
inline Report submitReport(Session& s,
                           const std::string& reporter,
                           int messageId,
                           const std::string& reason) {
    Report r;
    r.id = s.nextReportId++;
    r.reporter = reporter;
    r.messageId = messageId;
    r.reason = reason.empty() ? "(nessun motivo)" : reason;

    if (messageId < 0) {
        r.status = ReportStatus::RejectedNoEvidence;
        r.note = "Devi selezionare un messaggio dalla chat da verificare.";
        s.reports.push_back(r);
        appendReportLog(r);
        return r;
    }

    for (const auto& prev : s.reports) {
        if (prev.reporter == reporter && prev.messageId == messageId) {
            r.status = ReportStatus::RejectedDuplicate;
            r.note = "Hai già segnalato questo messaggio.";
            s.reports.push_back(r);
            appendReportLog(r);
            return r;
        }
    }

    const ChatEntry* msg = findMessage(s, messageId);
    if (!msg) {
        r.status = ReportStatus::RejectedNoEvidence;
        r.note = "Il messaggio non è più nel log — impossibile verificare.";
        s.reports.push_back(r);
        appendReportLog(r);
        return r;
    }

    r.accused = msg->user;
    if (toLowerAscii(r.accused) == toLowerAscii(reporter)) {
        r.status = ReportStatus::RejectedSelf;
        r.note = "Non puoi segnalare i tuoi messaggi.";
        s.reports.push_back(r);
        appendReportLog(r);
        return r;
    }

    // Verifica contenuto reale (testo originale, non solo display censurato)
    const std::string& raw = msg->original;
    auto hate = checkText(raw, "report");
    bool swear = containsSwear(raw) || msg->censored;
    bool blockedHate = msg->blocked || hate.verdict == Verdict::Blocked;

    if (blockedHate) {
        r.status = ReportStatus::Confirmed;
        r.note = "Verificato: linguaggio d'odio / razzista nel messaggio.";
    } else if (swear) {
        r.status = ReportStatus::Confirmed;
        r.note = "Verificato: parolaccia nel messaggio (in chat era già censurata).";
    } else {
        r.status = ReportStatus::RejectedClean;
        r.note = "Messaggio controllato: nessuna violazione. Report non fondato.";
    }

    s.reports.push_back(r);
    appendReportLog(r);
    return r;
}

/** Invia in chat: mute/spam, blocca odio, censura parolacce, strike. */
inline bool tryAppendChat(std::vector<std::string>& lines,
                          Session& session,
                          const std::string& username,
                          const std::string& msg) {
    using clock = std::chrono::steady_clock;
    double now = std::chrono::duration<double>(clock::now().time_since_epoch()).count();

    if (session.isMuted()) {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
            "[Moderazione] Sei mutato per ancora %.0fs (strikes=%d).",
            session.muteRemaining(), session.strikes);
        lines.push_back(buf);
        return false;
    }

    if (msg.size() > 280) {
        lines.push_back("[Moderazione] Messaggio troppo lungo (max 280).");
        return false;
    }

    // Rate limit
    if (session.lastChatAt > 0.0 && (now - session.lastChatAt) < session.minChatIntervalSec) {
        lines.push_back("[Moderazione] Troppo veloce — aspetta un attimo.");
        session.addStrike(1, 2.0);
        return false;
    }

    // Spam ripetuto
    std::string normMsg = normalizeForMatch(msg);
    if (!normMsg.empty() && normMsg == session.lastChatNorm) {
        session.repeatCount++;
        if (session.repeatCount >= 3) {
            session.addStrike(2, 15.0);
            lines.push_back("[Moderazione] Spam rilevato — mute 15s.");
            return false;
        }
    } else {
        session.repeatCount = 0;
        session.lastChatNorm = normMsg;
    }

    auto r = checkChat(msg);
    if (r.verdict != Verdict::Ok) {
        ChatEntry e;
        e.id = session.nextMsgId++;
        e.user = username;
        e.original = msg;
        e.display = r.reason;
        e.blocked = true;
        session.history.push_back(std::move(e));
        lines.push_back(r.reason);
        session.addStrike(3, 60.0);
        lines.push_back("[Moderazione] Strike odio — mute 60s.");
        session.lastChatAt = now;
        return false;
    }

    bool censored = false;
    std::string display = censorSwears(msg, &censored);

    ChatEntry e;
    e.id = session.nextMsgId++;
    e.user = username;
    e.original = msg;
    e.display = display;
    e.censored = censored;
    session.history.push_back(e);

    lines.push_back("[" + username + "] " + display);
    if (censored) {
        lines.push_back("[Moderazione] Parolaccia censurata.");
        session.addStrike(1, 0.0); // strike soft, no mute obbligatorio
        if (session.strikes >= 6) {
            session.addStrike(0, 20.0);
            lines.push_back("[Moderazione] Troppe parolacce — mute 20s.");
        }
    }
    session.lastChatAt = now;
    return true;
}

} // namespace Moderation
