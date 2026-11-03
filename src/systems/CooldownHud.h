#pragma once
#include "systems/Cooldown.h"
#include <imgui.h>
inline void drawCooldownHud(const Cooldown& s) {
    bool any = false;
    for (const auto& sl : s.all())
        if (sl.left > 0.f) { any = true; break; }
    if (!any) return;
    ImGui::SetNextWindowPos({12.f, 420.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##CdHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    for (const auto& sl : s.all())
        if (sl.left > 0.f)
            ImGui::Text("%s  %.1fs", sl.name.c_str(), sl.left);
    ImGui::End();
}
