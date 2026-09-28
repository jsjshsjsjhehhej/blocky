#pragma once

/**
 * Tema di Studio caricato da assets/ui/theme.json invece che scritto a
 * mano nel C++. La tabella qui sotto elenca ogni colore/valore CON il suo
 * default attuale: se il file esterno manca, e' vuoto, o non contiene una
 * certa chiave, quella voce resta esattamente come oggi — cambiare l'aspetto
 * senza toccare il JSON e' impossibile per costruzione.
 *
 * Non e' un header riusabile in senso stretto (usa ImGui direttamente), ma
 * non dipende da nessuna globale di main.cpp: puo' essere incluso da
 * qualunque punto che gia' include imgui.h + GameHub.h (per resolveScriptPath).
 */

#include "../engine/GameHub.h"

#include <cmath>
#include <fstream>
#include <imgui.h>

struct UiThemeColorDef {
    ImGuiCol_ id;
    const char* jsonName;
    float def[4];
};

// Stessi 26 colori e stessi valori che applyStudioTheme() scriveva a mano
// prima — il default di ogni riga E' l'aspetto attuale di Studio.
inline const UiThemeColorDef kUiThemeColors[] = {
    { ImGuiCol_WindowBg,         "WindowBg",         {0.08f, 0.11f, 0.18f, 1.f} },
    { ImGuiCol_ChildBg,          "ChildBg",          {0.10f, 0.14f, 0.22f, 1.f} },
    { ImGuiCol_PopupBg,          "PopupBg",          {0.09f, 0.13f, 0.21f, 1.f} },
    { ImGuiCol_Border,           "Border",           {0.20f, 0.32f, 0.52f, 1.f} },
    { ImGuiCol_FrameBg,          "FrameBg",          {0.12f, 0.18f, 0.30f, 1.f} },
    { ImGuiCol_FrameBgHovered,   "FrameBgHovered",   {0.16f, 0.26f, 0.42f, 1.f} },
    { ImGuiCol_FrameBgActive,    "FrameBgActive",    {0.20f, 0.34f, 0.54f, 1.f} },
    { ImGuiCol_TitleBg,          "TitleBg",          {0.05f, 0.08f, 0.14f, 1.f} },
    { ImGuiCol_TitleBgActive,    "TitleBgActive",    {0.10f, 0.28f, 0.58f, 1.f} },
    { ImGuiCol_MenuBarBg,        "MenuBarBg",        {0.06f, 0.10f, 0.18f, 1.f} },
    { ImGuiCol_Header,           "Header",           {0.12f, 0.36f, 0.70f, 0.65f} },
    { ImGuiCol_HeaderHovered,    "HeaderHovered",    {0.16f, 0.46f, 0.86f, 0.88f} },
    { ImGuiCol_HeaderActive,     "HeaderActive",     {0.20f, 0.54f, 0.94f, 1.f} },
    { ImGuiCol_Button,           "Button",           {0.14f, 0.26f, 0.46f, 1.f} },
    { ImGuiCol_ButtonHovered,    "ButtonHovered",    {0.20f, 0.40f, 0.70f, 1.f} },
    { ImGuiCol_ButtonActive,     "ButtonActive",     {0.10f, 0.46f, 0.86f, 1.f} },
    { ImGuiCol_Text,             "Text",             {0.93f, 0.96f, 1.00f, 1.f} },
    { ImGuiCol_TextDisabled,     "TextDisabled",     {0.48f, 0.58f, 0.72f, 1.f} },
    { ImGuiCol_Separator,        "Separator",        {0.20f, 0.30f, 0.46f, 1.f} },
    { ImGuiCol_CheckMark,        "CheckMark",        {0.42f, 0.80f, 1.00f, 1.f} },
    { ImGuiCol_SliderGrab,       "SliderGrab",       {0.28f, 0.60f, 0.98f, 1.f} },
    { ImGuiCol_SliderGrabActive, "SliderGrabActive", {0.48f, 0.76f, 1.00f, 1.f} },
    { ImGuiCol_Tab,              "Tab",              {0.10f, 0.16f, 0.28f, 1.f} },
    { ImGuiCol_TabHovered,       "TabHovered",       {0.18f, 0.44f, 0.80f, 1.f} },
    { ImGuiCol_TabActive,        "TabActive",        {0.12f, 0.38f, 0.72f, 1.f} },
    { ImGuiCol_NavHighlight,             "NavHighlight",             {0.f, 0.f, 0.f, 0.f} },
    { ImGuiCol_NavWindowingHighlight,    "NavWindowingHighlight",    {0.f, 0.f, 0.f, 0.f} },
};

struct UiThemeRounding {
    float window = 12.f, child = 10.f, frame = 8.f, grab = 6.f, popup = 10.f, scrollbar = 8.f;
    float itemSpacingX = 10.f, itemSpacingY = 8.f;
    float windowPadX = 14.f, windowPadY = 12.f;
    float framePadX = 11.f, framePadY = 7.f;
};

// Stessi 24 colori e stessi valori che applyClassicRobloxTheme() (ClassicMenu.h)
// scriveva a mano — la palette chiara "Roblox classico" usata dall'hub/menu di
// Blocky.exe (giochi, shop, chat...), separata dalla palette scura di Studio
// qui sopra. Chiavi JSON con prefisso "Hub" apposta: diversi ID ImGuiCol_
// (es. WindowBg) hanno un colore diverso nelle due palette, quindi non possono
// condividere la stessa chiave nello stesso file.
inline const UiThemeColorDef kHubThemeColors[] = {
    { ImGuiCol_WindowBg,         "HubWindowBg",         {0.06f, 0.22f, 0.48f, 0.0f} },
    { ImGuiCol_ChildBg,          "HubChildBg",          {0.99f, 0.995f, 1.00f, 0.96f} },
    { ImGuiCol_PopupBg,          "HubPopupBg",          {0.99f, 0.995f, 1.00f, 0.98f} },
    { ImGuiCol_Button,           "HubButton",           {0.10f, 0.46f, 0.94f, 1.f} },
    { ImGuiCol_ButtonHovered,    "HubButtonHovered",    {0.18f, 0.56f, 1.00f, 1.f} },
    { ImGuiCol_ButtonActive,     "HubButtonActive",     {0.07f, 0.36f, 0.80f, 1.f} },
    { ImGuiCol_Header,           "HubHeader",           {0.16f, 0.50f, 0.96f, 0.35f} },
    { ImGuiCol_HeaderHovered,    "HubHeaderHovered",    {0.20f, 0.56f, 1.00f, 0.55f} },
    { ImGuiCol_HeaderActive,     "HubHeaderActive",     {0.14f, 0.44f, 0.90f, 0.75f} },
    { ImGuiCol_FrameBg,          "HubFrameBg",          {0.93f, 0.96f, 1.00f, 1.f} },
    { ImGuiCol_FrameBgHovered,   "HubFrameBgHovered",   {0.88f, 0.93f, 1.00f, 1.f} },
    { ImGuiCol_FrameBgActive,    "HubFrameBgActive",    {0.82f, 0.90f, 1.00f, 1.f} },
    { ImGuiCol_Text,             "HubText",             {0.08f, 0.14f, 0.26f, 1.f} },
    { ImGuiCol_TextDisabled,     "HubTextDisabled",     {0.40f, 0.50f, 0.66f, 1.f} },
    { ImGuiCol_Border,           "HubBorder",           {0.78f, 0.86f, 0.96f, 0.8f} },
    { ImGuiCol_Separator,        "HubSeparator",        {0.82f, 0.88f, 0.96f, 1.f} },
    { ImGuiCol_CheckMark,        "HubCheckMark",        {0.10f, 0.46f, 0.94f, 1.f} },
    { ImGuiCol_SliderGrab,       "HubSliderGrab",       {0.18f, 0.52f, 0.96f, 1.f} },
    { ImGuiCol_SliderGrabActive, "HubSliderGrabActive", {0.32f, 0.66f, 1.00f, 1.f} },
    { ImGuiCol_ScrollbarBg,      "HubScrollbarBg",      {0.92f, 0.95f, 1.00f, 0.45f} },
    { ImGuiCol_ScrollbarGrab,    "HubScrollbarGrab",    {0.52f, 0.68f, 0.92f, 1.f} },
    { ImGuiCol_NavHighlight,             "HubNavHighlight",             {0.f, 0.f, 0.f, 0.f} },
    { ImGuiCol_NavWindowingHighlight,    "HubNavWindowingHighlight",    {0.f, 0.f, 0.f, 0.f} },
    { ImGuiCol_NavWindowingDimBg,        "HubNavWindowingDimBg",        {0.f, 0.f, 0.f, 0.f} },
};

struct UiHubRounding {
    float window = 16.f, child = 14.f, frame = 10.f, grab = 8.f, popup = 12.f, scrollbar = 10.f;
    float itemSpacingX = 14.f, itemSpacingY = 10.f;
    float windowPadX = 16.f, windowPadY = 14.f;
    float framePadX = 14.f, framePadY = 9.f;
};

// Stesso stile "cerca la chiave nel testo intero" di jsonFindFloat/Bool in
// GameHub.h: non capisce l'annidamento degli oggetti, ma non serve — ogni
// nome qui e' unico in tutto il file.
inline bool uiThemeFindColor4(const std::string& json, const std::string& key, float out[4]) {
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return false;
    auto b = json.find('[', k);
    if (b == std::string::npos) return false;
    float r = 0, g = 0, bl = 0, a = 1.f;
    int n = std::sscanf(json.c_str() + b + 1, "%f,%f,%f,%f", &r, &g, &bl, &a);
    if (n < 3) return false;
    out[0] = r; out[1] = g; out[2] = bl; out[3] = (n >= 4) ? a : 1.f;
    return true;
}

// Colori dell'HUD di gioco (barra vita, zaino) che non passano dallo stile
// ImGui: sono disegnati a mano con ImDrawList, quindi vivono in una
// struttura a parte invece che in s.Colors[]. Stessa idea di kUiThemeColors:
// il default qui sotto E' l'aspetto di oggi.
struct UiExtraColors {
    float healthFull[4] = {0.25f, 0.85f, 0.20f, 1.f};  // pieno = verde
    float healthMid[4]  = {1.00f, 0.85f, 0.20f, 1.f};  // meta' = giallo
    float healthLow[4]  = {0.95f, 0.20f, 0.15f, 1.f};  // vuoto = rosso
    float healthDead[4] = {110.f/255.f, 110.f/255.f, 115.f/255.f, 1.f};  // morto = grigio
    // Valori esatti (n/255) delle sei IM_COL32 originali, non arrotondati.
    float backpackHue[6][4] = {
        {70.f/255.f, 140.f/255.f, 255.f/255.f, 230.f/255.f},
        {80.f/255.f, 200.f/255.f,  90.f/255.f, 230.f/255.f},
        {240.f/255.f, 180.f/255.f, 50.f/255.f, 230.f/255.f},
        {230.f/255.f,  80.f/255.f, 70.f/255.f, 230.f/255.f},
        {180.f/255.f,  90.f/255.f, 220.f/255.f, 230.f/255.f},
        {90.f/255.f, 210.f/255.f, 210.f/255.f, 230.f/255.f},
    };
};

inline UiExtraColors& uiExtraColors() {
    static UiExtraColors e;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string path = resolveScriptPath("ui/theme.json");
        std::ifstream f(path, std::ios::binary);
        std::string json;
        if (f) json.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!json.empty()) {
            uiThemeFindColor4(json, "HealthFull", e.healthFull);
            uiThemeFindColor4(json, "HealthMid", e.healthMid);
            uiThemeFindColor4(json, "HealthLow", e.healthLow);
            uiThemeFindColor4(json, "HealthDead", e.healthDead);
            uiThemeFindColor4(json, "BackpackHue0", e.backpackHue[0]);
            uiThemeFindColor4(json, "BackpackHue1", e.backpackHue[1]);
            uiThemeFindColor4(json, "BackpackHue2", e.backpackHue[2]);
            uiThemeFindColor4(json, "BackpackHue3", e.backpackHue[3]);
            uiThemeFindColor4(json, "BackpackHue4", e.backpackHue[4]);
            uiThemeFindColor4(json, "BackpackHue5", e.backpackHue[5]);
        }
    }
    return e;
}

// Interpolazione rosso->giallo->verde in base alla percentuale vita, usata
// sia dalla barra vita del player (in basso a destra) sia da quella sopra
// la testa degli altri giocatori — prima erano due copie scritte a mano con
// leggere differenze di arrotondamento, stesso identico concetto.
inline ImU32 healthBarColor(float pct, bool dead) {
    const auto& e = uiExtraColors();
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto toCol = [](const float c[4]) {
        return IM_COL32((int)std::lround(c[0] * 255), (int)std::lround(c[1] * 255),
                        (int)std::lround(c[2] * 255), (int)std::lround(c[3] * 255));
    };
    if (dead) return toCol(e.healthDead);
    const float* lo;
    const float* hi;
    float t;
    if (pct > 0.5f) { lo = e.healthMid; hi = e.healthFull; t = (pct - 0.5f) * 2.f; }
    else            { lo = e.healthLow; hi = e.healthMid;  t = pct * 2.f; }
    float c[4] = { lerp(lo[0], hi[0], t), lerp(lo[1], hi[1], t), lerp(lo[2], hi[2], t), lerp(lo[3], hi[3], t) };
    return toCol(c);
}

inline void loadUiTheme(ImGuiStyle& s) {
    UiThemeRounding r; // solo default: gia' pronti se il file manca

    std::string path = resolveScriptPath("ui/theme.json");
    std::ifstream f(path, std::ios::binary);
    std::string json;
    if (f) json.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (!json.empty()) {
        jsonFindFloat(json, "roundingWindow", 0, r.window);
        jsonFindFloat(json, "roundingChild", 0, r.child);
        jsonFindFloat(json, "roundingFrame", 0, r.frame);
        jsonFindFloat(json, "roundingGrab", 0, r.grab);
        jsonFindFloat(json, "roundingPopup", 0, r.popup);
        jsonFindFloat(json, "roundingScrollbar", 0, r.scrollbar);
        jsonFindFloat(json, "itemSpacingX", 0, r.itemSpacingX);
        jsonFindFloat(json, "itemSpacingY", 0, r.itemSpacingY);
        jsonFindFloat(json, "windowPaddingX", 0, r.windowPadX);
        jsonFindFloat(json, "windowPaddingY", 0, r.windowPadY);
        jsonFindFloat(json, "framePaddingX", 0, r.framePadX);
        jsonFindFloat(json, "framePaddingY", 0, r.framePadY);
        std::fprintf(stdout, "[UiTheme] %s caricato\n", path.c_str());
    }

    s.WindowRounding = r.window;
    s.ChildRounding = r.child;
    s.FrameRounding = r.frame;
    s.GrabRounding = r.grab;
    s.PopupRounding = r.popup;
    s.ScrollbarRounding = r.scrollbar;
    s.ItemSpacing = {r.itemSpacingX, r.itemSpacingY};
    s.WindowPadding = {r.windowPadX, r.windowPadY};
    s.FramePadding = {r.framePadX, r.framePadY};
    s.WindowBorderSize = 1.f;
    s.ChildBorderSize = 1.f;

    for (const auto& def : kUiThemeColors) {
        float v[4] = {def.def[0], def.def[1], def.def[2], def.def[3]};
        if (!json.empty()) uiThemeFindColor4(json, def.jsonName, v);
        s.Colors[def.id] = {v[0], v[1], v[2], v[3]};
    }
}

// Stesso identico meccanismo di loadUiTheme() qui sopra, ma per la palette
// chiara dell'hub (applyClassicRobloxTheme in ClassicMenu.h) — cosi' anche
// il menu di Blocky.exe (giochi, shop, chat) si puo' ricolorare da
// theme.json senza ricompilare, non solo Studio.
inline void loadHubTheme(ImGuiStyle& s) {
    UiHubRounding r; // solo default: gia' pronti se il file manca

    std::string path = resolveScriptPath("ui/theme.json");
    std::ifstream f(path, std::ios::binary);
    std::string json;
    if (f) json.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (!json.empty()) {
        jsonFindFloat(json, "hubRoundingWindow", 0, r.window);
        jsonFindFloat(json, "hubRoundingChild", 0, r.child);
        jsonFindFloat(json, "hubRoundingFrame", 0, r.frame);
        jsonFindFloat(json, "hubRoundingGrab", 0, r.grab);
        jsonFindFloat(json, "hubRoundingPopup", 0, r.popup);
        jsonFindFloat(json, "hubRoundingScrollbar", 0, r.scrollbar);
        jsonFindFloat(json, "hubItemSpacingX", 0, r.itemSpacingX);
        jsonFindFloat(json, "hubItemSpacingY", 0, r.itemSpacingY);
        jsonFindFloat(json, "hubWindowPaddingX", 0, r.windowPadX);
        jsonFindFloat(json, "hubWindowPaddingY", 0, r.windowPadY);
        jsonFindFloat(json, "hubFramePaddingX", 0, r.framePadX);
        jsonFindFloat(json, "hubFramePaddingY", 0, r.framePadY);
        std::fprintf(stdout, "[UiTheme] %s caricato (hub)\n", path.c_str());
    }

    s.WindowRounding = r.window;
    s.ChildRounding = r.child;
    s.FrameRounding = r.frame;
    s.GrabRounding = r.grab;
    s.PopupRounding = r.popup;
    s.ScrollbarRounding = r.scrollbar;
    s.ItemSpacing = {r.itemSpacingX, r.itemSpacingY};
    s.WindowPadding = {r.windowPadX, r.windowPadY};
    s.FramePadding = {r.framePadX, r.framePadY};
    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.FrameBorderSize = 0.f;

    for (const auto& def : kHubThemeColors) {
        float v[4] = {def.def[0], def.def[1], def.def[2], def.def[3]};
        if (!json.empty()) uiThemeFindColor4(json, def.jsonName, v);
        s.Colors[def.id] = {v[0], v[1], v[2], v[3]};
    }
}
