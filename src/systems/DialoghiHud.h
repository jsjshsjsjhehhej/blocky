#pragma once
#include "systems/Dialoghi.h"
#include <imgui.h>
inline void drawDialoghiHud(const Dialoghi& s) {
    if (!s.open() || !s.current()) return;
    const auto* n = s.current();
    ImGui::SetNextWindowPos({220.f, 360.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({420.f, 0.f});
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("##DlgHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
    ImGui::Text("%s — %s", s.title().c_str(), n->speaker.c_str());
    ImGui::TextWrapped("%s", n->text.c_str());
    int i = 1;
    for (const auto& c : n->choices)
        ImGui::BulletText("%d) %s", i++, c.text.c_str());
    ImGui::End();
}
