#pragma once
#include "systems/Stamina.h"
#include <imgui.h>
inline void drawStaminaHud(const Stamina& s) {
    if (s.all().empty()) return;
    ImGui::SetNextWindowPos({12.f, 660.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##StamHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    for (const auto& a : s.all()) {
        ImGui::Text("STA %s  %.0f/%.0f", a.name.c_str(), a.value, a.max);
        break;
    }
    ImGui::End();
}
