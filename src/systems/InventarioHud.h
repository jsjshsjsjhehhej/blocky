#pragma once

#include "systems/Inventario.h"
#include <imgui.h>

inline void drawInventarioHud(const Inventario& s) {
    if (s.slots().empty()) return;
    ImGui::SetNextWindowPos({12.f, 120.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("##InvHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted("Inventario");
    int shown = 0;
    for (const auto& sl : s.slots()) {
        if (++shown > 8) break;
        const char* mark = (shown - 1 == s.selected()) ? ">" : " ";
        ImGui::Text("%s %s  x%d", mark, sl.name.c_str(), sl.count);
    }
    ImGui::End();
}
