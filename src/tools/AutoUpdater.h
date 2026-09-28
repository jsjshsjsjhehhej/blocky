#pragma once

/**
 * Auto-update silenzioso di libblocky.dll — solo build di distribuzione
 * (mai in BLOCKY_DEV, altrimenti una build locale finirebbe sovrascritta
 * dall'ultima release pubblicata). Vive nei lanciatori minimi (Blocky.exe,
 * BlockyGameEngine.exe, BlockyStudio.exe), PRIMA di caricare la dll — la dll
 * non e' ancora in memoria a quel punto, quindi sostituirla e' solo un
 * overwrite di file, niente trucchi di auto-replace di un eseguibile in
 * esecuzione (che qui non serve: i lanciatori stessi non si aggiornano,
 * cambiano troppo raramente per giustificarlo).
 *
 * Fallimento in QUALSIASI punto (rete assente, GitHub irraggiungibile, hash
 * che non combacia) = si prosegue silenziosamente con la dll gia' installata.
 * Non e' mai accettabile bloccare l'avvio del gioco per un problema di rete.
 *
 * I/O con API Win32 dirette (CreateFile/ReadFile/WriteFile), non
 * <fstream>/<sstream>: i lanciatori sono deliberatamente minimi (~80 KB,
 * solo kernel32 — vedi blocky_launcher.cpp) e iostream linkato staticamente
 * porta dentro tutta la libreria dei locale/facet, gonfiando l'eseguibile
 * di centinaia di KB per un file di poche righe.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <commctrl.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#endif

namespace blk::AutoUpdater {

// Riempire con "owner/repo" del repository GitHub reale prima di usarlo
// davvero — finche' resta un placeholder, tryAutoUpdate() non chiama
// nemmeno la rete, che e' esattamente il comportamento silenzioso voluto.
inline const char* kRepoOwner = "REPLACE_ME";
inline const char* kRepoName  = "REPLACE_ME";

#ifdef _WIN32

/**
 * Copia locale di "dov'e' l'eseguibile", e l'unica giustificata del motore.
 *
 * Il posto giusto sarebbe plat::selfDir(), ma questo file vive nei lanciatori
 * minimi, che non hanno nemmeno src/ nell'include path (provato: il target
 * non compila se si include platform/Platform.h) — e Platform.h porta dentro
 * <filesystem>, cioe' esattamente il peso che il commento in cima a questo
 * file esiste per evitare.
 *
 * Il ripiego `return "."` invece e' stato tolto: quel ripiego, in
 * Integrity.h, ha scollegato il telefono da ogni partita per una serata
 * intera facendo sembrare il problema la rete. Se non sappiamo dove siamo lo
 * diciamo, e chi chiama si ferma — non aggiorniamo una dll a caso nella
 * cartella di lavoro di turno.
 */
inline std::string selfDir() {
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0) return {};              // vuoto = non lo sappiamo
    std::string p(buf, n);
    size_t s = p.find_last_of("\\/");
    return s == std::string::npos ? std::string() : p.substr(0, s);
}

/** Legge un file intero in memoria. Stringa vuota se non esiste/e' vuoto/fallisce. */
inline std::string readWholeFile(const std::string& path) {
    std::string out;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    DWORD size = GetFileSize(h, nullptr);
    if (size != INVALID_FILE_SIZE && size > 0) {
        out.resize(size);
        DWORD read = 0;
        if (!ReadFile(h, out.data(), size, &read, nullptr) || read != size)
            out.clear();
    }
    CloseHandle(h);
    return out;
}

/** Scrive un file intero, sovrascrivendo. Ritorna false se fallisce. */
inline bool writeWholeFile(const std::string& path, const char* data, DWORD len) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, data, len, &written, nullptr) && written == len;
    CloseHandle(h);
    return ok;
}

inline std::string versionFilePath() { return selfDir() + "\\version.txt"; }

inline std::string readLocalVersion() {
    std::string v = readWholeFile(versionFilePath());
    while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' '))
        v.pop_back();
    return v.empty() ? "0.0.0" : v;
}

inline void writeLocalVersion(const std::string& v) {
    writeWholeFile(versionFilePath(), v.data(), (DWORD)v.size());
}

/** Confronto versioni "vN.N.N" semplice, numero per numero. */
inline bool isNewerVersion(const std::string& remote, const std::string& local) {
    auto parse = [](const std::string& s) {
        std::vector<int> parts;
        std::string cur;
        for (char c : s) {
            if (c == 'v' || c == 'V') continue;
            if (c == '.') { parts.push_back(cur.empty() ? 0 : std::atoi(cur.c_str())); cur.clear(); }
            else cur.push_back(c);
        }
        parts.push_back(cur.empty() ? 0 : std::atoi(cur.c_str()));
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    };
    auto r = parse(remote), l = parse(local);
    for (size_t i = 0; i < 3; ++i) {
        if (r[i] != l[i]) return r[i] > l[i];
    }
    return false;
}

/**
 * Finestrella di aggiornamento in stile Roblox moderno: appare SOLO quando
 * c'e' davvero un download in corso (mai per un avvio normale senza
 * aggiornamenti — quella e' la parte "silenziosa" di questo file), un
 * riquadro scuro coi colori del tema di Studio (assets/ui/theme.json:
 * WindowBg/Text) con titolo, riga di stato e barra di progresso. Nessun
 * pulsante: non e' chiudibile dall'utente, si chiude da sola a fine
 * operazione (successo o fallimento silenzioso).
 */
struct UpdateUi {
    HWND hwnd = nullptr;
    HWND label = nullptr;
    HWND progress = nullptr;
    bool created = false;
    COLORREF bgColor = RGB(20, 28, 46);      // default = WindowBg di oggi, se theme.json manca
    COLORREF textColor = RGB(230, 236, 247); // default = Text di oggi, se theme.json manca
    HBRUSH bgBrush = nullptr;
};
inline UpdateUi g_updateUi;

/** Stesso "cerca la chiave nel testo intero, prendi i primi 3-4 numeri dopo [" di UiTheme.h,
 *  copiato qui invece che incluso: UiTheme.h porta dentro ImGui + <fstream>, troppo pesanti
 *  per un lanciatore che deve restare minimo (vedi commento in cima al file). */
inline void updateFindColor4(const std::string& json, const std::string& key, float out[4]) {
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return;
    auto b = json.find('[', k);
    if (b == std::string::npos) return;
    float r = 0, g = 0, bl = 0, a = 1.f;
    if (std::sscanf(json.c_str() + b + 1, "%f,%f,%f,%f", &r, &g, &bl, &a) < 3) return;
    out[0] = r; out[1] = g; out[2] = bl; out[3] = a;
}

/** Legge WindowBg/Text da assets/ui/theme.json accanto al lanciatore — stesso file che
 *  Studio e la HUD di gioco usano. Se manca o e' vuoto restano i default sopra: nessun
 *  cambio visibile finche' l'utente non ci mette mano, come tutto il resto del tema. */
inline void loadUpdateWindowColors() {
    float bg[4] = {20.f / 255.f, 28.f / 255.f, 46.f / 255.f, 1.f};
    float text[4] = {230.f / 255.f, 236.f / 255.f, 247.f / 255.f, 1.f};
    std::string json = readWholeFile(selfDir() + "\\assets\\ui\\theme.json");
    if (!json.empty()) {
        updateFindColor4(json, "WindowBg", bg);
        updateFindColor4(json, "Text", text);
    }
    auto toByte = [](float v) { int i = (int)(v * 255.f + 0.5f); return (BYTE)(i < 0 ? 0 : (i > 255 ? 255 : i)); };
    g_updateUi.bgColor = RGB(toByte(bg[0]), toByte(bg[1]), toByte(bg[2]));
    g_updateUi.textColor = RGB(toByte(text[0]), toByte(text[1]), toByte(text[2]));
}

inline LRESULT CALLBACK updateWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_CLOSE) return 0; // niente [x]: si chiude solo da tryAutoUpdate
    if (msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, g_updateUi.textColor);
        SetBkColor(hdc, g_updateUi.bgColor);
        return (LRESULT)g_updateUi.bgBrush;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

inline void updatePumpMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

inline void updateSleepPumping(DWORD ms) {
    DWORD start = GetTickCount();
    do {
        updatePumpMessages();
        Sleep(15);
    } while (GetTickCount() - start < ms);
}

inline void createUpdateWindow(const std::string& version, const std::string& appName) {
    loadUpdateWindowColors();

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    HINSTANCE hInst = GetModuleHandleA(nullptr);
    g_updateUi.bgBrush = CreateSolidBrush(g_updateUi.bgColor);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = updateWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "BlockyUpdaterWnd";
    wc.hbrBackground = g_updateUi.bgBrush;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    const int w = 420, h = 130;
    const int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
    g_updateUi.hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, "BlockyUpdaterWnd", appName.c_str(),
        WS_POPUP | WS_BORDER | WS_VISIBLE, (sx - w) / 2, (sy - h) / 2, w, h,
        nullptr, nullptr, hInst, nullptr);
    if (!g_updateUi.hwnd) return;

    HFONT titleFont = CreateFontA(-20, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
    HFONT bodyFont = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");

    std::string titleText = appName + " si sta aggiornando";
    HWND title = CreateWindowA("STATIC", titleText.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        24, 18, w - 48, 26, g_updateUi.hwnd, nullptr, hInst, nullptr);
    SendMessageA(title, WM_SETFONT, (WPARAM)titleFont, TRUE);

    std::string statusText = "Download versione " + version + " in corso...";
    g_updateUi.label = CreateWindowA("STATIC", statusText.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        24, 50, w - 48, 20, g_updateUi.hwnd, nullptr, hInst, nullptr);
    SendMessageA(g_updateUi.label, WM_SETFONT, (WPARAM)bodyFont, TRUE);

    g_updateUi.progress = CreateWindowExA(0, PROGRESS_CLASSA, nullptr, WS_CHILD | WS_VISIBLE,
        24, 80, w - 48, 16, g_updateUi.hwnd, nullptr, hInst, nullptr);
    SendMessageA(g_updateUi.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(g_updateUi.progress, PBM_SETPOS, 0, 0);

    ShowWindow(g_updateUi.hwnd, SW_SHOW);
    UpdateWindow(g_updateUi.hwnd);
    g_updateUi.created = true;
    updatePumpMessages();
}

inline void updateSetStatus(const std::string& text) {
    if (g_updateUi.label) SetWindowTextA(g_updateUi.label, text.c_str());
    updatePumpMessages();
}

inline void updateSetProgress(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (g_updateUi.progress) SendMessageA(g_updateUi.progress, PBM_SETPOS, (WPARAM)pct, 0);
    updatePumpMessages();
}

/** Mostra un ultimo messaggio (es. errore) per un istante, poi chiude la finestra. */
inline void updateCloseWithMessage(const std::string& text) {
    if (!g_updateUi.created) return;
    updateSetStatus(text);
    updateSleepPumping(1100);
    DestroyWindow(g_updateUi.hwnd);
    updatePumpMessages();
    g_updateUi = UpdateUi{};
}

inline void updateClose() {
    if (!g_updateUi.created) return;
    DestroyWindow(g_updateUi.hwnd);
    updatePumpMessages();
    g_updateUi = UpdateUi{};
}

/** GET su un URL https/http via WinINet. Ritorna vuoto se qualunque passo fallisce. */
inline std::string httpGet(const std::string& url) {
    std::string result;
    HINTERNET hInternet = InternetOpenA("BlockyUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return result;

    const char* headers = "User-Agent: BlockyUpdater/1.0\r\nAccept: application/vnd.github+json\r\n";
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers, (DWORD)std::strlen(headers),
                                      INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                      0);
    if (hUrl) {
        char buf[8192];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
            result.append(buf, bytesRead);
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInternet);
    return result;
}

/**
 * Scarica un URL su file. Ritorna false se qualunque passo fallisce (niente
 * file parziali lasciati in giro). Se onProgress non e' nullptr ed il
 * server manda Content-Length, viene chiamato con la percentuale 0-100 man
 * mano che i dati arrivano (usato per la barra di progresso della finestra
 * di aggiornamento — vedi updateSetProgress).
 */
inline bool httpDownloadToFile(const std::string& url, const std::string& outPath,
                                void (*onProgress)(int) = nullptr) {
    HINTERNET hInternet = InternetOpenA("BlockyUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return false;

    const char* headers = "User-Agent: BlockyUpdater/1.0\r\n";
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers, (DWORD)std::strlen(headers),
                                      INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                      0);
    if (!hUrl) { InternetCloseHandle(hInternet); return false; }

    DWORD total = 0, totalSize = sizeof(total);
    HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &total, &totalSize, nullptr);

    HANDLE hFile = CreateFileA(outPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = (hFile != INVALID_HANDLE_VALUE);
    if (ok) {
        char buf[8192];
        DWORD bytesRead = 0;
        DWORD downloaded = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
            DWORD written = 0;
            if (!WriteFile(hFile, buf, bytesRead, &written, nullptr) || written != bytesRead) {
                ok = false;
                break;
            }
            downloaded += bytesRead;
            if (onProgress && total > 0)
                onProgress((int)(((unsigned long long)downloaded * 100ull) / total));
        }
        CloseHandle(hFile);
    }
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    if (!ok) std::remove(outPath.c_str());
    return ok;
}

/** SHA256 di un file, esadecimale minuscolo. Vuoto se qualunque passo fallisce. */
inline std::string sha256File(const std::string& path) {
    std::string result;
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return result;

    HCRYPTHASH hHash = 0;
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return result;
    }

    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = (h != INVALID_HANDLE_VALUE);
    if (ok) {
        char buf[8192];
        DWORD bytesRead = 0;
        while (ReadFile(h, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
            if (!CryptHashData(hHash, (const BYTE*)buf, bytesRead, 0)) { ok = false; break; }
        }
        CloseHandle(h);
    }

    if (ok) {
        BYTE hash[32];
        DWORD hashLen = sizeof(hash);
        if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
            char hex[65];
            for (DWORD i = 0; i < hashLen; ++i)
                std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
            result.assign(hex, hashLen * 2);
        }
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return result;
}

/** Cerca "browser_download_url" dentro il primo oggetto {...} che ha "name":"nameVal". */
inline std::string findAssetUrl(const std::string& json, const std::string& nameVal) {
    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
        size_t q1 = json.find('"', json.find(':', pos) + 1);
        size_t q2 = (q1 == std::string::npos) ? std::string::npos : json.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) break;
        std::string name = json.substr(q1 + 1, q2 - q1 - 1);
        if (name == nameVal) {
            size_t urlKey = json.find("\"browser_download_url\"", q2);
            size_t objEnd = json.find('}', q2);
            if (urlKey != std::string::npos && (objEnd == std::string::npos || urlKey < objEnd)) {
                size_t u1 = json.find('"', json.find(':', urlKey) + 1);
                size_t u2 = (u1 == std::string::npos) ? std::string::npos : json.find('"', u1 + 1);
                if (u1 != std::string::npos && u2 != std::string::npos)
                    return json.substr(u1 + 1, u2 - u1 - 1);
            }
        }
        pos = q2 + 1;
    }
    return {};
}

inline std::string findJsonStr(const std::string& json, const std::string& key) {
    size_t k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return {};
    size_t q1 = json.find('"', json.find(':', k) + 1);
    if (q1 == std::string::npos) return {};
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

/**
 * Punto d'ingresso unico, da chiamare all'avvio dei lanciatori. appName e'
 * il nome mostrato nella finestra di aggiornamento (titolo + testo) cosi'
 * si capisce SEMPRE quale dei tre eseguibili si sta aggiornando: "Blocky"
 * (Blocky.exe), "Blocky Game Engine" (BlockyGameEngine.exe) o "Blocky
 * Studio" (BlockyStudio.exe) — la dll condivisa e' la stessa per tutti e
 * tre, ma chi la sta scaricando in quel momento e' quello che l'utente ha
 * effettivamente lanciato.
 */
inline void tryAutoUpdate(const char* appName = "Blocky") {
    if (std::string(kRepoOwner) == "REPLACE_ME" || std::string(kRepoName) == "REPLACE_ME")
        return; // repo non ancora configurato: no-op silenzioso

    // Se non sappiamo dove siamo non si aggiorna niente. Prima selfDir()
    // rispondeva "." e l'aggiornamento sarebbe finito nella cartella di
    // lavoro di turno, sovrascrivendo una dll che non e' la nostra.
    if (selfDir().empty()) return;

    std::string apiUrl = "https://api.github.com/repos/" + std::string(kRepoOwner)
                        + "/" + std::string(kRepoName) + "/releases/latest";
    std::string json = httpGet(apiUrl);
    if (json.empty()) return;

    std::string tag = findJsonStr(json, "tag_name");
    if (tag.empty()) return;

    std::string localVersion = readLocalVersion();
    if (!isNewerVersion(tag, localVersion)) return;

    std::string dllUrl = findAssetUrl(json, "libblocky.dll");
    std::string hashUrl = findAssetUrl(json, "libblocky.dll.sha256");
    if (dllUrl.empty() || hashUrl.empty()) return;

    // Da qui in poi c'e' davvero un aggiornamento da installare: e' l'unico
    // momento in cui l'utente vede una finestra, esattamente come il
    // bootstrapper di Roblox non si mostra mai per un avvio senza update.
    createUpdateWindow(tag, appName);

    std::string dir = selfDir();
    std::string newDll = dir + "\\libblocky.dll.new";
    std::string newHash = dir + "\\libblocky.dll.sha256.new";

    updateSetStatus("Download versione " + tag + " in corso...");
    if (!httpDownloadToFile(dllUrl, newDll, &updateSetProgress)) {
        updateCloseWithMessage("Aggiornamento non riuscito: si continua con la versione attuale.");
        return;
    }
    updateSetStatus("Verifica del file scaricato...");
    if (!httpDownloadToFile(hashUrl, newHash)) {
        std::remove(newDll.c_str());
        updateCloseWithMessage("Aggiornamento non riuscito: si continua con la versione attuale.");
        return;
    }

    std::string expectedHash = readWholeFile(newHash);
    // Un file .sha256 tipico e' "HASH *filename\n" o solo "HASH": prendi solo la prima parola.
    size_t sp = expectedHash.find_first_of(" \t\r\n");
    if (sp != std::string::npos) expectedHash = expectedHash.substr(0, sp);
    for (char& c : expectedHash) c = (char)std::tolower((unsigned char)c);
    std::remove(newHash.c_str());

    std::string actualHash = sha256File(newDll);
    if (expectedHash.empty() || actualHash.empty() || actualHash != expectedHash) {
        std::remove(newDll.c_str());
        updateCloseWithMessage("Aggiornamento non riuscito: si continua con la versione attuale.");
        return; // hash non combacia: scarta, mai sostituire senza verifica
    }

    updateSetStatus("Installazione dell'aggiornamento...");
    updateSetProgress(100);
    std::string dllPath = dir + "\\libblocky.dll";
    std::string oldBackup = dir + "\\libblocky.dll.old";
    std::remove(oldBackup.c_str());
    std::rename(dllPath.c_str(), oldBackup.c_str()); // ok anche se fallisce (prima installazione)
    if (std::rename(newDll.c_str(), dllPath.c_str()) != 0) {
        // sostituzione fallita: ripristina la vecchia dll, non lasciare l'app senza dll
        std::rename(oldBackup.c_str(), dllPath.c_str());
        std::remove(newDll.c_str());
        updateCloseWithMessage("Aggiornamento non riuscito: si continua con la versione attuale.");
        return;
    }
    std::remove(oldBackup.c_str());
    writeLocalVersion(tag);

    updateSetStatus(std::string("Fatto! Avvio di ") + appName + "...");
    updateSleepPumping(500);
    updateClose();
}

#else // non _WIN32: no-op, l'updater e' solo per i lanciatori Windows di oggi

inline void tryAutoUpdate(const char* appName = "Blocky") { (void)appName; }

#endif

} // namespace AutoUpdater
