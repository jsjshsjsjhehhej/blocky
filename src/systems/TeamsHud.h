#pragma once

#include "systems/Teams.h"
#include <imgui.h>

inline void drawTeamsHud(const Teams& s) {
    if (s.mine().empty()) return;
    ImGui::SetNextWindowPos({12.f, 52.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##TeamHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Team: %s", s.mine().c_str());
    ImGui::End();
}
