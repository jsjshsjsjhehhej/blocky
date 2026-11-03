#pragma once
#include "systems/Mobs.h"
#include <imgui.h>
inline void drawMobsHud(const Mobs& s) {
    if (s.count() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 740.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##MobHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Mobs: %d", s.count());
    ImGui::End();
}
