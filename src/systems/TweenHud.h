#pragma once

#include "systems/Tween.h"
#include <imgui.h>
#include <cstdio>

inline void drawTweenHud(const Tween& s) {
    if (s.activeCount() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 88.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##TweenHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Tween attivi: %d", s.activeCount());
    ImGui::End();
}
