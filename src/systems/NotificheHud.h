#pragma once

#include "systems/Notifiche.h"
#include <imgui.h>

inline void drawNotificheHud(const Notifiche& s) {
    if (s.items().empty()) return;
    float y = 16.f;
    int i = 0;
    for (const auto& t : s.items()) {
        ImGui::SetNextWindowPos({280.f, y}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::PushID(i++);
        ImGui::Begin("##toast", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        ImGui::TextUnformatted(t.text.c_str());
        ImGui::End();
        ImGui::PopID();
        y += 36.f;
    }
}
