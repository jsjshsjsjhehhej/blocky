#pragma once
#include "systems/Combo.h"
#include <imgui.h>
inline void drawComboHud(const Combo& s) {
    if (s.count() < 2) return;
    ImGui::SetNextWindowPos({12.f, 620.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##ComboHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("COMBO x%d", s.count());
    ImGui::End();
}
