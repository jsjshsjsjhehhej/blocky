#pragma once


#include <imgui.h>
#include <cstdio>
#include <cstring>

#include "studio/StudioIcons.h"
#include "studio/StudioTheme.h"

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

    // Tab strip grigio Studio (#242426 / #2E2E30) + underline blu #0078D4
    dl->AddRectFilled(start, {start.x + width, start.y + kRibbonTabHeight},
                      studio_theme::pal().tabStrip);

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
            dl->AddRectFilled(min, max, studio_theme::pal().tabActive);
            dl->AddRectFilled({min.x, max.y - 2.5f}, max, IM_COL32(0, 120, 212, 255));
        } else if (hovered) {
            dl->AddRectFilled(min, max, studio_theme::pal().tabHover);
        }

        dl->AddText({min.x + 13.f, min.y + 6.f},
                    active ? studio_theme::pal().tabTextActive : studio_theme::pal().tabText,
                    names[i]);

        if (pressed && !active) { current = i; changed = true; }
        x += w;
    }

    dl->AddRectFilled({start.x, start.y + kRibbonTabHeight},
                      {start.x + width, start.y + kRibbonHeight},
                      studio_theme::pal().ribbonBody);
    // Sotto la tab strip: Dummy subito dopo SetCursor* spegne IsSetPos in sicurezza.
    ImGui::SetCursorScreenPos({start.x, start.y + kRibbonTabHeight});
    ImGui::Dummy({1.f, 1.f});
    return changed;
}

/**
 * Pulsantino solo icona sulla striscia delle schede (in alto a destra), come
 * la barra di accesso rapido di Roblox Studio: Salva, Annulla, Ripeti.
 * `x` = dove disegnarlo (coordinate schermo); restituisce il clic.
 */
inline bool ribbonQuickButton(const char* id, Icon icon, const char* tooltip, ImVec2 at, bool enabled = true) {
    const float s = kRibbonTabHeight - 6.f;
    ImGui::SetCursorScreenPos(at);
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##q", {s, s}) && enabled;
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered && enabled) dl->AddRectFilled(at, {at.x + s, at.y + s}, studio_theme::pal().btnHover, 3.f);
    drawIcon(dl, {at.x + s * 0.5f, at.y + s * 0.5f}, s * 0.62f, icon,
             enabled ? studio_theme::pal().icon : studio_theme::pal().iconDisabled);
    if (hovered && tooltip && tooltip[0]) ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

inline void ribbonBeginGroup() {
    ImGui::BeginGroup();
    ImGui::Dummy({0.f, 4.f});
}

inline void ribbonEndGroup(const char* label, bool last = false) {
    ImGui::EndGroup();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float labelY = ImGui::GetWindowPos().y + kRibbonHeight - 18.f;
    const float textW = ImGui::CalcTextSize(label).x;
    dl->AddText({min.x + (max.x - min.x - textW) * 0.5f, labelY},
                studio_theme::pal().groupLabel, label);

    if (last) return;

    // Separatore + spazio verso il gruppo successivo. Dummy con altezza > 0
    // evita assert ImGui su item a dimensione zero in alcuni path di layout.
    ImGui::SameLine(0.f, 10.f);
    const ImVec2 sep = ImGui::GetCursorScreenPos();
    dl->AddLine({sep.x, min.y - 2.f}, {sep.x, labelY - 4.f}, studio_theme::pal().separator, 1.f);
    ImGui::Dummy({10.f, 1.f});
    ImGui::SameLine(0.f, 0.f);
}

inline bool ribbonBigButton(const char* id, Icon icon, const char* label,
                            bool active = false, bool enabled = true) {
    // Largo quanto serve al testo: in tedesco o in russo le parole sono lunghe.
    const float textW = ImGui::CalcTextSize(label).x;
    const float w = textW + 12.f > kBigButtonW ? textW + 12.f : kBigButtonW;
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##b", {w, kBigButtonH}) && enabled;
    const bool hovered = ImGui::IsItemHovered() && enabled;
    ImGui::PopID();

    const ImVec2 max(min.x + w, min.y + kBigButtonH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active)       dl->AddRectFilled(min, max, studio_theme::pal().btnActive, 4.f);
    else if (hovered) dl->AddRectFilled(min, max, studio_theme::pal().btnHover, 4.f);
    if (active || hovered) dl->AddRect(min, max, studio_theme::pal().btnBorder, 4.f);

    const ImU32 tint = enabled ? (active ? studio_theme::pal().iconActive : studio_theme::pal().icon) : studio_theme::pal().iconDisabled;

    drawIcon(dl, {min.x + w * 0.5f, min.y + 24.f}, 27.f, icon, tint);
    dl->AddText({min.x + (w - textW) * 0.5f, min.y + 43.f}, tint, label);

    ImGui::SameLine(0.f, 3.f);
    return pressed && enabled;
}

/**
 * Pulsante grande con menu a tendina (freccina in basso a destra): i comandi
 * rari stanno nel menu invece che in dieci pulsanti. Vero = menu aperto, e
 * allora chi chiama disegna le voci e poi chiama ImGui::EndPopup().
 */
inline bool ribbonMenuButton(const char* id, Icon icon, const char* label, bool active = false) {
    char popup[96];
    std::snprintf(popup, sizeof(popup), "%s##menu", id);
    if (ribbonBigButton(id, icon, label, active)) ImGui::OpenPopup(popup);
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddTriangleFilled({max.x - 11.f, max.y - 9.f}, {max.x - 4.f, max.y - 9.f}, {max.x - 7.5f, max.y - 5.f},
                          studio_theme::pal().menuArrow);
    ImGui::SetNextWindowPos({ImGui::GetItemRectMin().x, max.y + 2.f});
    return ImGui::BeginPopup(popup);
}

inline bool ribbonSmallButton(const char* id, Icon icon, const char* label,
                              bool active = false, bool enabled = true) {
    const float textW = ImGui::CalcTextSize(label).x;
    const float w = textW + 32.f > kSmallButtonW ? textW + 32.f : kSmallButtonW;
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##s", {w, kSmallButtonH}) && enabled;
    const bool hovered = ImGui::IsItemHovered() && enabled;
    ImGui::PopID();

    const ImVec2 max(min.x + w, min.y + kSmallButtonH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active)       dl->AddRectFilled(min, max, studio_theme::pal().btnActive, 3.f);
    else if (hovered) dl->AddRectFilled(min, max, studio_theme::pal().btnHover, 3.f);

    const ImU32 tint = enabled ? studio_theme::pal().icon : studio_theme::pal().iconDisabled;
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
