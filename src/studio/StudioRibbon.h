#pragma once


#include <imgui.h>
#include <cstring>

#include "studio/StudioIcons.h"

namespace studio {

constexpr float kRibbonTabHeight = 28.f;
constexpr float kRibbonBodyHeight = 96.f;
constexpr float kRibbonHeight = kRibbonTabHeight + kRibbonBodyHeight;

constexpr float kBigButtonW = 62.f;
constexpr float kBigButtonH = 62.f;
constexpr float kSmallButtonW = 108.f;
constexpr float kSmallButtonH = 18.f;

inline bool ribbonTabs(const char* const* names, int count, int& current) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    bool changed = false;

    dl->AddRectFilled(start, {start.x + width, start.y + kRibbonTabHeight},
                      IM_COL32(28, 32, 39, 255));

    float x = start.x + 6.f;
    for (int i = 0; i < count; ++i) {
        const float w = ImGui::CalcTextSize(names[i]).x + 26.f;
        const ImVec2 min(x, start.y);
        const ImVec2 max(x + w, start.y + kRibbonTabHeight);

        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton("##tab", {w, kRibbonTabHeight});
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        const bool active = (i == current);
        if (active) {
            dl->AddRectFilled(min, max, IM_COL32(42, 48, 58, 255));
            dl->AddRectFilled({min.x, max.y - 2.5f}, max, IM_COL32(64, 156, 255, 255));
        } else if (hovered) {
            dl->AddRectFilled(min, max, IM_COL32(36, 41, 50, 255));
        }

        dl->AddText({min.x + 13.f, min.y + 6.f},
                    active ? IM_COL32(235, 240, 248, 255) : IM_COL32(160, 170, 185, 255),
                    names[i]);

        if (pressed && !active) { current = i; changed = true; }
        x += w;
    }

    ImGui::SetCursorScreenPos({start.x, start.y + kRibbonTabHeight});
    dl->AddRectFilled({start.x, start.y + kRibbonTabHeight},
                      {start.x + width, start.y + kRibbonHeight},
                      IM_COL32(42, 48, 58, 255));
    return changed;
}

inline void ribbonBeginGroup() {
    ImGui::BeginGroup();
    ImGui::Dummy({0.f, 4.f});
}

inline void ribbonEndGroup(const char* label) {
    ImGui::EndGroup();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float labelY = ImGui::GetWindowPos().y + kRibbonHeight - 18.f;
    const float textW = ImGui::CalcTextSize(label).x;
    dl->AddText({min.x + (max.x - min.x - textW) * 0.5f, labelY},
                IM_COL32(140, 150, 165, 255), label);

    ImGui::SameLine(0.f, 10.f);
    const ImVec2 sep = ImGui::GetCursorScreenPos();
    dl->AddLine({sep.x, min.y - 2.f}, {sep.x, labelY - 4.f}, IM_COL32(62, 70, 84, 255), 1.f);
    ImGui::Dummy({10.f, 0.f});
    ImGui::SameLine(0.f, 0.f);
}

inline bool ribbonBigButton(const char* id, Icon icon, const char* label,
                            bool active = false, bool enabled = true) {
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##b", {kBigButtonW, kBigButtonH}) && enabled;
    const bool hovered = ImGui::IsItemHovered() && enabled;
    ImGui::PopID();

    const ImVec2 max(min.x + kBigButtonW, min.y + kBigButtonH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active)       dl->AddRectFilled(min, max, IM_COL32(38, 96, 168, 255), 4.f);
    else if (hovered) dl->AddRectFilled(min, max, IM_COL32(58, 66, 80, 255), 4.f);
    if (active || hovered) dl->AddRect(min, max, IM_COL32(88, 150, 220, 160), 4.f);

    const ImU32 tint = enabled ? (active ? IM_COL32(235, 244, 255, 255)
                                         : IM_COL32(205, 214, 228, 255))
                               : IM_COL32(110, 116, 128, 255);

    drawIcon(dl, {min.x + kBigButtonW * 0.5f, min.y + 24.f}, 27.f, icon, tint);

    const float textW = ImGui::CalcTextSize(label).x;
    dl->AddText({min.x + (kBigButtonW - textW) * 0.5f, min.y + 43.f}, tint, label);

    ImGui::SameLine(0.f, 3.f);
    return pressed && enabled;
}

inline bool ribbonSmallButton(const char* id, Icon icon, const char* label,
                              bool active = false, bool enabled = true) {
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##s", {kSmallButtonW, kSmallButtonH}) && enabled;
    const bool hovered = ImGui::IsItemHovered() && enabled;
    ImGui::PopID();

    const ImVec2 max(min.x + kSmallButtonW, min.y + kSmallButtonH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active)       dl->AddRectFilled(min, max, IM_COL32(38, 96, 168, 255), 3.f);
    else if (hovered) dl->AddRectFilled(min, max, IM_COL32(58, 66, 80, 255), 3.f);

    const ImU32 tint = enabled ? IM_COL32(205, 214, 228, 255) : IM_COL32(110, 116, 128, 255);
    drawIcon(dl, {min.x + 12.f, min.y + kSmallButtonH * 0.5f}, 15.f, icon, tint);
    dl->AddText({min.x + 25.f, min.y + 3.f}, tint, label);

    return pressed && enabled;
}

inline void ribbonBeginStack() {
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));
}

inline void ribbonEndStack() {
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::SameLine(0.f, 3.f);
}

}
