#pragma once

#include "systems/Missioni.h"
#include <imgui.h>

inline void drawMissioniHud(const Missioni& s) {
    if (s.activeCount() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 280.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::Begin("##MissHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted("Missioni");
    for (const auto& q : s.all()) {
        if (!q.active) continue;
        ImGui::Text("%s", q.title.c_str());
        for (const auto& st : q.steps)
            ImGui::BulletText("%s %s", st.done ? "[x]" : "[ ]", st.text.c_str());
    }
    ImGui::End();
}
