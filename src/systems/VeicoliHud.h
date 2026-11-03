#pragma once

// Bottone "Guida"/"Scendi" in basso al centro e tachimetro mentre si guida.
// E' un bottone vero (non solo il tasto F) perche' sul telefono non c'e'
// tastiera.

#include "systems/Veicoli.h"

#include <imgui.h>

#include <cmath>

inline void drawVeicoliHud(Veicoli& s) {
    if (!s.seated() && s.nearVehicle() < 0) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 size = s.seated() ? ImVec2{230.f, 0.f} : ImVec2{170.f, 0.f};
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 120.f},
                            ImGuiCond_Always, {0.5f, 1.f});
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##Veicoli", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize);
    if (s.seated()) {
        ImGui::Text("%3.0f stud/s", std::fabs(s.speed()));
        ImGui::SameLine();
        if (ImGui::Button("Scendi (F)", {110.f, 36.f})) s.requestToggle();
    } else if (ImGui::Button("Guida (F)", {150.f, 36.f})) {
        s.requestToggle();
    }
    ImGui::End();
}
