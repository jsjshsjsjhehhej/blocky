#pragma once

#include "systems/Meteo.h"
#include <imgui.h>
#include <cstdio>

inline void drawMeteoHud(const Meteo& s) {
    if (s.speed() <= 0.f && s.weather() == Meteo::Weather::Sole && s.hours() == 12.f)
        return;
    char buf[64];
    const int h = (int)s.hours();
    const int m = (int)((s.hours() - (float)h) * 60.f);
    std::snprintf(buf, sizeof(buf), "%02d:%02d  %s", h, m, Meteo::weatherName(s.weather()));
    ImGui::SetNextWindowPos({12.f, 16.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##MeteoHud", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(buf);
    ImGui::End();
}
