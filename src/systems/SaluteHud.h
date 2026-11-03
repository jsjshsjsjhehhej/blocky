#pragma once
#include "systems/Salute.h"
#include <imgui.h>
inline void drawSaluteHud(const Salute& s) {
    if (s.all().empty()) return;
    ImGui::SetNextWindowPos({12.f, 540.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##HpHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    int n = 0;
    for (const auto& a : s.all()) {
        if (n++ >= 4) break;
        ImGui::Text("%s  %.0f/%.0f%s", a.name.c_str(), a.hp, a.max, a.dead ? "  X" : "");
    }
    ImGui::End();
}
