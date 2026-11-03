#pragma once
#include "systems/Hitbox.h"
#include <imgui.h>
inline void drawHitboxHud(const Hitbox& s) {
    if (s.count() <= 0) return;
    ImGui::SetNextWindowPos({12.f, 500.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##HbHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("Hitbox: %d  overlap: %d", s.count(), (int)s.frameHits().size());
    ImGui::End();
}
