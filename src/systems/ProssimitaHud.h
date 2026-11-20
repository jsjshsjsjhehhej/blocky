#pragma once
#include "systems/Prossimita.h"
#include <imgui.h>

// Il prompt vero (tasto + azione sull'oggetto) lo disegna drawProximityBillboards.
// Qui resta solo la scritta del ClickDetector.
inline void drawProssimitaHud(const Prossimita& s) {
    if (s.clickHint().empty()) return;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({ds.x * 0.5f, ds.y * 0.62f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##ClickHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("%s", s.clickHint().c_str());
    ImGui::End();
}
