#pragma once
#include "systems/Proiettili.h"
#include <imgui.h>
inline void drawProiettiliHud(const Proiettili& s) {
    if (s.count() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 580.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##PrjHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Proiettili: %d", s.count());
    ImGui::End();
}
