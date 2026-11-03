#pragma once
#include "systems/Prossimita.h"
#include <imgui.h>
inline void drawProssimitaHud(const Prossimita& s) {
    if (s.nearest().empty()) return;
    ImGui::SetNextWindowPos({400.f, 520.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##ProxHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("[E]  %s", s.nearestText());
    ImGui::End();
}
