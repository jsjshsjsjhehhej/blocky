#pragma once


#include <imgui.h>
#include <cmath>
#include <algorithm>

inline float studioEaseOut(float t) {
    t = std::clamp(t, 0.f, 1.f);
    float u = 1.f - t;
    return 1.f - u * u * u;
}

inline void studioPanelHeader(ImDrawList* dl, ImVec2 wp, float w, const char* title, float time) {
    float pulse = 0.5f + 0.5f * std::sin(time * 2.2f);
    dl->AddRectFilledMultiColor(wp, {wp.x + w, wp.y + 3.f},
        IM_COL32(40, 140, 255, 220), IM_COL32(80, 200, 255, 180 + (int)(pulse * 40)),
        IM_COL32(80, 200, 255, 0), IM_COL32(40, 140, 255, 0));
    ImGui::Dummy({0, 4});
    ImGui::TextColored({0.45f, 0.82f, 1.f, 1.f}, "%s", title);
    ImGui::Separator();
}

inline bool studioAccentButton(const char* label, ImVec2 size, ImVec4 col) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        {std::min(1.f, col.x + 0.1f), std::min(1.f, col.y + 0.1f), std::min(1.f, col.z + 0.1f), 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {col.x * 0.85f, col.y * 0.85f, col.z * 0.85f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1, 1, 1, 1});
    bool p = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return p;
}

inline void studioSectionLabel(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored({0.55f, 0.72f, 0.95f, 1.f}, "%s", text);
    ImGui::Separator();
}
