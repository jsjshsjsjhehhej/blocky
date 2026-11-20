#pragma once

/**
 * Tema dello Studio: Scuro (come prima) o Chiaro, come l'impostazione Tema di
 * Roblox Studio. Tutti i colori che lo Studio disegnava a mano (ribbon,
 * pannelli, intestazioni delle Proprieta', schede degli script) vengono da qui.
 * La scelta si salva in <cartella dati>/studio_theme.txt.
 */

#include <imgui.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace studio_theme {

struct Palette {
    // ribbon
    ImU32 tabStrip, tabActive, tabHover, tabText, tabTextActive, ribbonBody;
    ImU32 btnActive, btnHover, btnBorder, icon, iconActive, iconDisabled, groupLabel, separator, menuArrow;
    ImU32 bannerBg;
    // pannelli
    ImVec4 windowBg, panelBg, panelDeep, text, textDim, border, frameBg, sectionHeader, sectionHeaderHover;
    ImVec4 docTabs, scriptBg;
};

inline bool& lightFlag() {
    static bool v = false;
    return v;
}
inline bool isLight() { return lightFlag(); }

inline const Palette& pal() {
    static const Palette dark = {
        IM_COL32(36, 36, 38, 255), IM_COL32(46, 46, 48, 255), IM_COL32(58, 58, 62, 255), IM_COL32(200, 200, 204, 255),
        IM_COL32(255, 255, 255, 255), IM_COL32(36, 36, 38, 255),
        IM_COL32(0, 120, 212, 255), IM_COL32(70, 70, 74, 255), IM_COL32(80, 80, 84, 255), IM_COL32(220, 220, 224, 255),
        IM_COL32(255, 255, 255, 255), IM_COL32(140, 140, 145, 255), IM_COL32(160, 160, 165, 255), IM_COL32(70, 70, 74, 255),
        IM_COL32(180, 180, 185, 255),
        IM_COL32(36, 36, 38, 255),
        {0.18f, 0.18f, 0.19f, 1.f}, {0.20f, 0.20f, 0.21f, 1.f}, {0.14f, 0.14f, 0.15f, 1.f}, {0.95f, 0.95f, 0.96f, 1.f},
        {0.62f, 0.62f, 0.65f, 1.f}, {0.10f, 0.10f, 0.11f, 1.f}, {0.24f, 0.24f, 0.26f, 1.f}, {0.16f, 0.16f, 0.17f, 1.f},
        {0.22f, 0.22f, 0.24f, 1.f},
        {0.18f, 0.18f, 0.19f, 1.f}, {0.12f, 0.12f, 0.13f, 1.f},
    };
    static const Palette light = {
        IM_COL32(236, 236, 238, 255), IM_COL32(255, 255, 255, 255), IM_COL32(226, 228, 232, 255), IM_COL32(90, 92, 100, 255),
        IM_COL32(20, 22, 28, 255), IM_COL32(250, 250, 251, 255),
        IM_COL32(204, 226, 250, 255), IM_COL32(229, 236, 245, 255), IM_COL32(110, 160, 225, 200), IM_COL32(60, 66, 78, 255),
        IM_COL32(20, 60, 120, 255), IM_COL32(170, 174, 182, 255), IM_COL32(110, 114, 124, 255), IM_COL32(214, 216, 222, 255),
        IM_COL32(110, 116, 128, 255),
        IM_COL32(242, 243, 246, 255),
        {0.97f, 0.97f, 0.98f, 1.f}, {0.98f, 0.98f, 0.99f, 1.f}, {1.00f, 1.00f, 1.00f, 1.f}, {0.10f, 0.11f, 0.13f, 1.f},
        {0.45f, 0.46f, 0.50f, 1.f}, {0.80f, 0.81f, 0.84f, 1.f}, {0.93f, 0.93f, 0.95f, 1.f}, {0.90f, 0.91f, 0.93f, 1.f},
        {0.85f, 0.87f, 0.91f, 1.f},
        {0.93f, 0.93f, 0.95f, 1.f}, {1.00f, 1.00f, 1.00f, 1.f},
    };
    return isLight() ? light : dark;
}

/** Sopra il tema caricato da theme.json: con il tema chiaro, i colori chiari di ImGui ritoccati. */
inline void applyToImGui(ImGuiStyle& st) {
    if (!isLight()) return;
    ImGui::StyleColorsLight(&st);
    ImVec4* c = st.Colors;
    const Palette& p = pal();
    c[ImGuiCol_WindowBg] = p.windowBg;
    c[ImGuiCol_ChildBg] = {0.f, 0.f, 0.f, 0.f};
    c[ImGuiCol_PopupBg] = {1.f, 1.f, 1.f, 0.98f};
    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.textDim;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_FrameBg] = p.frameBg;
    c[ImGuiCol_FrameBgHovered] = {0.88f, 0.92f, 0.98f, 1.f};
    c[ImGuiCol_FrameBgActive] = {0.80f, 0.88f, 0.98f, 1.f};
    c[ImGuiCol_TitleBg] = {0.92f, 0.92f, 0.94f, 1.f};
    c[ImGuiCol_TitleBgActive] = {0.88f, 0.89f, 0.92f, 1.f};
    c[ImGuiCol_Button] = {0.90f, 0.91f, 0.93f, 1.f};
    c[ImGuiCol_ButtonHovered] = {0.82f, 0.88f, 0.97f, 1.f};
    c[ImGuiCol_ButtonActive] = {0.70f, 0.82f, 0.97f, 1.f};
    c[ImGuiCol_Header] = {0.00f, 0.47f, 0.83f, 0.25f};
    c[ImGuiCol_HeaderHovered] = {0.00f, 0.47f, 0.83f, 0.15f};
    c[ImGuiCol_HeaderActive] = {0.00f, 0.47f, 0.83f, 0.35f};
    c[ImGuiCol_Tab] = {0.90f, 0.91f, 0.93f, 1.f};
    c[ImGuiCol_TabHovered] = {1.f, 1.f, 1.f, 1.f};
    c[ImGuiCol_TabSelected] = {1.f, 1.f, 1.f, 1.f};
    c[ImGuiCol_MenuBarBg] = {0.95f, 0.95f, 0.96f, 1.f};
}

inline std::string& filePath() {
    static std::string p;
    return p;
}

/** Legge la scelta salvata (una volta sola). */
inline void load(const std::string& path) {
    static bool done = false;
    if (done) return;
    done = true;
    filePath() = path;
    std::ifstream f(path);
    std::string v;
    if (f >> v) lightFlag() = v == "light";
}

inline void setLight(bool light) {
    lightFlag() = light;
    if (filePath().empty()) return;
    std::ofstream f(filePath());
    f << (light ? "light" : "dark") << '\n';
}

} // namespace studio_theme
