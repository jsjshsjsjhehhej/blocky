#pragma once

/**
 * Chrome UI di Blocky Studio — look vicino a Roblox Studio (grigi neutri,
 * selezione blu, pannelli dock, ClassName in Properties). Niente accenti
 * “neon” sparsi: un solo linguaggio visivo.
 */

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace studio_chrome {

// Palette Studio (dark) — allineata a theme.json
inline constexpr ImVec4 kBg        = {0.18f, 0.18f, 0.19f, 1.f};   // #2E2E30
inline constexpr ImVec4 kBgDeep    = {0.14f, 0.14f, 0.15f, 1.f};   // #242426
inline constexpr ImVec4 kBgPanel   = {0.20f, 0.20f, 0.21f, 1.f};
inline constexpr ImVec4 kBorder    = {0.10f, 0.10f, 0.11f, 1.f};
inline constexpr ImVec4 kText      = {0.95f, 0.95f, 0.96f, 1.f};
inline constexpr ImVec4 kTextDim   = {0.62f, 0.62f, 0.65f, 1.f};
inline constexpr ImVec4 kSelect    = {0.00f, 0.47f, 0.83f, 1.f};   // Studio blue
inline constexpr ImVec4 kSelectSoft= {0.00f, 0.47f, 0.83f, 0.45f};
inline constexpr ImVec4 kPlayGreen = {0.18f, 0.62f, 0.32f, 1.f};
inline constexpr ImVec4 kStopRed   = {0.78f, 0.22f, 0.18f, 1.f};

inline void pushDockPanelColors() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBgPanel);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgDeep);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, kTextDim);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
    ImGui::PushStyleColor(ImGuiCol_Header, kSelectSoft);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.00f, 0.47f, 0.83f, 0.65f});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kSelect);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.24f, 0.24f, 0.26f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_TitleBg, kBgDeep);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, kBgDeep);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.f, 4.f});
}

inline void popDockPanelColors() {
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(11);
}

inline void pushRibbonColors() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBgDeep);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.28f, 0.28f, 0.30f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.36f, 0.36f, 0.38f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSelect);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 6.f});
}

inline void popRibbonColors() {
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(5);
}

/** Intestazione Properties stile Studio: ClassName grande + Name sotto. */
inline void drawClassNameBanner(const char* className, const char* instanceName) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 52.f;
    dl->AddRectFilled(p, {p.x + w, p.y + h}, IM_COL32(36, 36, 38, 255));
    dl->AddRectFilled(p, {p.x + 3.f, p.y + h}, IM_COL32(0, 120, 212, 255));
    ImGui::Dummy({0, 6});
    ImGui::Indent(10.f);
    ImGui::TextColored(kSelect, "%s", className ? className : "Instance");
    if (instanceName && instanceName[0])
        ImGui::TextUnformatted(instanceName);
    else
        ImGui::TextDisabled("(senza nome)");
    ImGui::Unindent(10.f);
    ImGui::Dummy({0, 6});
    ImGui::Separator();
}

inline void section(const char* title) {
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

inline bool toolToggle(const char* label, bool on) {
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button, kSelect);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.10f, 0.55f, 0.95f, 1.f});
    }
    bool pressed = ImGui::Button(label, {0, 28});
    if (on) ImGui::PopStyleColor(2);
    return pressed;
}

} // namespace studio_chrome

// Compat: API usata dal resto del codice
inline bool studioAccentButton(const char* label, ImVec2 size, ImVec4 col) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        {std::min(1.f, col.x + 0.08f), std::min(1.f, col.y + 0.08f), std::min(1.f, col.z + 0.08f), 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {col.x * 0.85f, col.y * 0.85f, col.z * 0.85f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
    bool p = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return p;
}

inline void studioSectionLabel(const char* text) {
    studio_chrome::section(text);
}

inline void studioPanelHeader(ImDrawList* dl, ImVec2 wp, float w, const char* title, float) {
    dl->AddRectFilled(wp, {wp.x + w, wp.y + 2.f}, IM_COL32(0, 120, 212, 255));
    ImGui::Dummy({0, 4});
    ImGui::TextUnformatted(title);
    ImGui::Separator();
}

inline float studioEaseOut(float t) {
    t = std::clamp(t, 0.f, 1.f);
    float u = 1.f - t;
    return 1.f - u * u * u;
}
