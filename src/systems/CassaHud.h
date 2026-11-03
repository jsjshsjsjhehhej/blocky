#pragma once
#include "systems/Cassa.h"
#include <imgui.h>
inline void drawCassaHud(const Cassa& s) {
    if (s.chestCount() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 260.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##CassaHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Casse: %d", s.chestCount());
    ImGui::End();
}
