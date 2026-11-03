#pragma once

// Riquadro della Classifica in alto a destra, sotto la barra di gioco.
// Compare solo quando uno script ha creato almeno una colonna.

#include "systems/Classifica.h"

#include <imgui.h>

#include <cmath>
#include <cstdio>

inline void drawClassificaHud(const Classifica& s) {
    if (s.stats().empty()) return;
    const auto rows = s.sortedRows();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float nameW = 110.f, colW = 64.f;
    const float w = nameW + colW * (float)s.stats().size() + 16.f;

    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x - w - 8.f, vp->WorkPos.y + 56.f},
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##Classifica", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTable("##cls", 1 + (int)s.stats().size(), ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Giocatore", ImGuiTableColumnFlags_WidthFixed, nameW);
        for (const auto& st : s.stats())
            ImGui::TableSetupColumn(st.c_str(), ImGuiTableColumnFlags_WidthFixed, colW);
        ImGui::TableHeadersRow();

        int shown = 0;
        for (const auto& r : rows) {
            if (++shown > 10) break;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const ImVec4 col = r.local ? ImVec4{1.f, 0.9f, 0.4f, 1.f} : ImVec4{1.f, 1.f, 1.f, 1.f};
            ImGui::TextColored(col, "%s", r.player.c_str());
            for (double v : r.values) {
                ImGui::TableNextColumn();
                if (std::floor(v) == v && std::fabs(v) < 1e15)
                    ImGui::TextColored(col, "%.0f", v);
                else
                    ImGui::TextColored(col, "%.2f", v);
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
