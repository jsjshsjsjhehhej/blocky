#pragma once

// Lato Studio del generatore (vedi PartProps.def): un controllo per ogni
// proprieta', con l'aiuto della tabella come tooltip.

#include "PartProps.h"

#include <imgui.h>

namespace partprops {

inline bool editUi(const char* id, bool& v, float, float) { return ImGui::Checkbox(id, &v); }
inline bool editUi(const char* id, float& v, float mn, float mx) {
    const float speed = (mx - mn) / 200.f;
    return ImGui::DragFloat(id, &v, speed > 0.f ? speed : 0.01f, mn, mx, "%.2f",
                            ImGuiSliderFlags_AlwaysClamp);
}

inline void helpTooltip(const char* help) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
}

} // namespace partprops
