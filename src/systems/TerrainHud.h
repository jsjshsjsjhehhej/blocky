#pragma once
#include "systems/Terrain.h"
#include <imgui.h>
inline void drawTerrainHud(const Terrain& s) {
    if (s.empty()) return;
    ImGui::SetNextWindowPos({12.f, 700.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##TerHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Terrain  blocchi:%d  chunk:%d", s.blockCount(), s.chunkCount());
    ImGui::End();
}
