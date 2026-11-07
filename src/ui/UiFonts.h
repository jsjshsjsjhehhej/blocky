#pragma once

/**
 * Font dell'interfaccia.
 *
 * Senza questo ImGui usa ProggyClean: un font bitmap da 13px, monospaziato,
 * che ingrandito con FontGlobalScale diventa sgranato — e' la cosa che piu'
 * di tutte fa sembrare l'interfaccia "un tool di debug".
 *
 * Font di Blocky, in assets/fonts/ (licenza OFL, vedi LICENSE-OFL.txt):
 *   Source Sans Pro          testo         (il font storico di Roblox)
 *   Source Sans Pro Bold     pulsanti, nomi, titoli piccoli
 *   Montserrat Bold          intestazioni dei pannelli
 *   Montserrat Black         scritte grandi ("BLOCKY")
 * Se mancano si ripiega sul font del sistema (Segoe UI, Roboto).
 *
 * Il font si rasterizza alla grandezza VERA a cui verra' mostrato (base x
 * scala dell'interfaccia) e font->Scale compensa FontGlobalScale: il testo
 * resta nitido invece di essere un'immagine piccola stirata.
 */

#include <cstdio>
#include <string>

#include "imgui.h"
#include "ui/Loc.h"   // la lingua scelta decide se serve il font CJK

namespace blk {
namespace ui {

inline ImFont*& fontRegular() { static ImFont* f = nullptr; return f; }
inline ImFont*& fontBold() { static ImFont* f = nullptr; return f; }
inline ImFont*& fontTitle() { static ImFont* f = nullptr; return f; }
inline ImFont*& fontHeading() { static ImFont* f = nullptr; return f; }

/** Intestazioni dei pannelli; senza Montserrat, il grassetto normale. */
inline ImFont* headingFont() { return fontHeading() ? fontHeading() : fontBold(); }

/** Font per testo grande disegnato con AddText(font, size, ...). */
inline ImFont* displayFont() { return fontTitle() ? fontTitle() : ImGui::GetFont(); }

/** Usa un font per il blocco corrente; se manca (font di default) non fa nulla. */
struct FontScope {
    explicit FontScope(ImFont* f) : pushed_(f != nullptr) { if (f) ImGui::PushFont(f); }
    ~FontScope() { if (pushed_) ImGui::PopFont(); }
    FontScope(const FontScope&) = delete;
    FontScope& operator=(const FontScope&) = delete;
private:
    bool pushed_;
};

inline bool fileReadable(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

inline const char* firstReadable(const char* const* paths) {
    for (int i = 0; paths[i]; ++i)
        if (fileReadable(paths[i])) return paths[i];
    return nullptr;
}

/**
 * Lettere usate nei testi di Blocky: latino con accenti, piu' la
 * punteggiatura "tipografica" (— … · • ’ “ ”) e frecce e simboli comuni,
 * che con il font di default uscivano come "?".
 */
inline const ImWchar* glyphRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // latino base + accenti
        0x0100, 0x017F, // latino esteso A
        0x0180, 0x024F, // latino esteso B (romeno, croato...)
        0x0370, 0x03FF, // greco
        0x0400, 0x052F, // cirillico (russo, ucraino, ...)
        0x1E00, 0x1EFF, // latino esteso aggiuntivo (vietnamita)
        0x2010, 0x205E, // trattini, virgolette, puntini, punto elenco
        0x20AC, 0x20AC, // euro
        0x2190, 0x21FF, // frecce
        0x2212, 0x2212, // meno
        0x25A0, 0x25FF, // forme (quadrati, triangoli, cerchi)
        0x2600, 0x26FF, // simboli vari
        0x2713, 0x2717, // spunte e croci
        0,
    };
    return ranges;
}


/** Cinese, giapponese, coreano: caratteri del font di sistema uniti a `into`. */
inline void mergeCjk(float px, float uiScale) {
    const std::string code = loc().code;
    if (!locNeedsCjkFont(code)) return;
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* ranges = code == "zh" ? io.Fonts->GetGlyphRangesChineseSimplifiedCommon()
                          : code == "ja" ? io.Fonts->GetGlyphRangesJapanese()
                                         : io.Fonts->GetGlyphRangesKorean();
#ifdef __ANDROID__
    static const char* const zh[] = {"/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/NotoSansSC-Regular.otf", "/system/fonts/DroidSansFallback.ttf", nullptr};
    static const char* const ja[] = {"/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/NotoSansJP-Regular.otf", "/system/fonts/DroidSansFallback.ttf", nullptr};
    static const char* const ko[] = {"/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/NotoSansKR-Regular.otf", "/system/fonts/DroidSansFallback.ttf", nullptr};
#elif defined(_WIN32)
    static const char* const zh[] = {"C:\\Windows\\Fonts\\msyh.ttc", "C:\\Windows\\Fonts\\simsun.ttc", nullptr};
    static const char* const ja[] = {"C:\\Windows\\Fonts\\YuGothM.ttc", "C:\\Windows\\Fonts\\meiryo.ttc", "C:\\Windows\\Fonts\\msgothic.ttc", nullptr};
    static const char* const ko[] = {"C:\\Windows\\Fonts\\malgun.ttf", "C:\\Windows\\Fonts\\gulim.ttc", nullptr};
#else
    static const char* const zh[] = {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", nullptr};
    static const char* const ja[] = {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", nullptr};
    static const char* const ko[] = {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", nullptr};
#endif
    const char* path = firstReadable(code == "zh" ? zh : code == "ja" ? ja : ko);
    if (!path) {
        std::printf("[UI] nessun font per %s: i caratteri usciranno come '?'\n", code.c_str());
        return;
    }
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF(path, px * uiScale, &cfg, ranges);
}

inline ImFont* addFont(const std::string& path, float px, float uiScale) {
    if (path.empty() || !fileReadable(path.c_str())) return nullptr;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    ImFont* f = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), px * uiScale, &cfg, glyphRanges());
    if (f) {
        if (px <= 20.f) mergeCjk(px, uiScale);   // subito dopo: si unisce a questo font (non al titolo grande)
        f->Scale = 1.f / uiScale; // FontGlobalScale = uiScale lo riporta a px
    }
    return f;
}

/**
 * Da chiamare una volta, dopo CreateContext e prima del primo frame.
 * fontDir = cartella assets/fonts (vuota = solo font di sistema).
 * Falso = nessun font trovato: resta quello di default di ImGui.
 */
inline bool loadUiFonts(float uiScale, const std::string& fontDir) {
    if (uiScale < 0.5f) uiScale = 1.f;
    const std::string dir = fontDir.empty() ? std::string() : fontDir + "/";

    // Source Sans a 16px occupa quanto Segoe UI a 15: e' piu' stretto.
    ImFont* reg = dir.empty() ? nullptr : addFont(dir + "SourceSansPro-Regular.ttf", 16.f, uiScale);
    ImFont* bold = nullptr;
    const char* used = "Source Sans Pro";
    if (reg) {
        bold = addFont(dir + "SourceSansPro-Bold.ttf", 16.f, uiScale);
        fontHeading() = addFont(dir + "Montserrat-Bold.ttf", 15.f, uiScale);
        fontTitle() = addFont(dir + "Montserrat-Black.ttf", 48.f, uiScale);
    } else {
#ifdef __ANDROID__
        static const char* const regular[] = {"/system/fonts/Roboto-Regular.ttf", "/system/fonts/DroidSans.ttf", nullptr};
        static const char* const boldList[] = {"/system/fonts/Roboto-Bold.ttf", "/system/fonts/DroidSans-Bold.ttf", nullptr};
#elif defined(_WIN32)
        static const char* const regular[] = {"C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf", nullptr};
        static const char* const boldList[] = {"C:\\Windows\\Fonts\\seguisb.ttf", "C:\\Windows\\Fonts\\segoeuib.ttf",
                                               "C:\\Windows\\Fonts\\arialbd.ttf", nullptr};
#else
        static const char* const regular[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr};
        static const char* const boldList[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", nullptr};
#endif
        const char* regPath = firstReadable(regular);
        const char* boldPath = firstReadable(boldList);
        reg = regPath ? addFont(regPath, 15.f, uiScale) : nullptr;
        if (!reg) {
            std::printf("[UI] nessun font trovato: resta il font di default\n");
            return false;
        }
        used = regPath;
        if (boldPath) {
            bold = addFont(boldPath, 15.f, uiScale);
            fontTitle() = addFont(boldPath, 48.f, uiScale);
        }
    }
    fontRegular() = reg;
    fontBold() = bold;
    ImGui::GetIO().FontDefault = reg;
    std::printf("[UI] font: %s%s%s\n", used, fontHeading() ? " + Montserrat" : "", bold ? " (con grassetto)" : "");
    return true;
}

} // namespace ui
} // namespace blk
