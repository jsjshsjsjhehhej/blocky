#pragma once

/**
 * Pannelli di Blocky Studio: Explorer, Properties, Output.
 *
 * Le proprietà stanno in una tabella a due colonne con categorie richiudibili
 * invece che in una fila di widget: con una trentina di campi per Part la
 * colonna dei nomi allineata è l'unica cosa che li rende scorribili a occhio.
 */

#include <imgui.h>

#include <cfloat>
#include <cstring>
#include <string>

#include "studio/StudioIcons.h"
#include "studio/StudioState.h"

namespace studio {

inline ImVec4 logColor(LogLevel level) {
    switch (level) {
    case LogLevel::Warn:   return {1.f, 0.78f, 0.32f, 1.f};
    case LogLevel::Error:  return {1.f, 0.44f, 0.40f, 1.f};
    case LogLevel::Script: return {0.60f, 0.86f, 1.f, 1.f};
    default:               return {0.80f, 0.84f, 0.90f, 1.f};
    }
}

inline Icon iconForShape(PartShape shape) {
    switch (shape) {
    case PartShape::Ball:     return Icon::Sphere;
    case PartShape::Cylinder: return Icon::Cylinder;
    case PartShape::Wedge:    return Icon::Wedge;
    default:                  return Icon::Cube;
    }
}

/** Linguetta in cima al pannello, come i pannelli agganciati di un editor. */
inline void panelTitle(const char* name, Icon icon) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float h = 26.f;

    dl->AddRectFilled({p.x - 8.f, p.y - 8.f}, {p.x + width + 8.f, p.y + h}, IM_COL32(32, 37, 45, 255));

    const float tabW = ImGui::CalcTextSize(name).x + 42.f;
    dl->AddRectFilled({p.x - 8.f, p.y - 8.f}, {p.x - 8.f + tabW, p.y + h}, IM_COL32(46, 53, 64, 255));
    dl->AddRectFilled({p.x - 8.f, p.y + h - 2.5f}, {p.x - 8.f + tabW, p.y + h},
                      IM_COL32(64, 156, 255, 255));

    drawIcon(dl, {p.x + 8.f, p.y + 5.f}, 15.f, icon, IM_COL32(150, 200, 255, 255));
    dl->AddText({p.x + 20.f, p.y + 0.f}, IM_COL32(228, 234, 244, 255), name);

    ImGui::Dummy({0.f, h - 4.f});
}

inline bool inputText(const char* label, std::string& value, size_t capacity = 128) {
    char buf[256];
    const size_t limit = capacity < sizeof(buf) ? capacity : sizeof(buf);
    std::snprintf(buf, limit, "%s", value.c_str());
    if (ImGui::InputText(label, buf, limit)) {
        value = buf;
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ Explorer

/** Voce con icona; ritorna true se cliccata. */
inline bool explorerItem(const char* label, Icon icon, bool selected, int id) {
    ImGui::PushID(id);

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetTextLineHeight() + 5.f;

    const bool clicked = ImGui::InvisibleButton("##row", {width, h});
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected)      dl->AddRectFilled(min, {min.x + width, min.y + h}, IM_COL32(38, 96, 168, 255), 3.f);
    else if (hovered)  dl->AddRectFilled(min, {min.x + width, min.y + h}, IM_COL32(52, 59, 71, 255), 3.f);

    drawIcon(dl, {min.x + 13.f, min.y + h * 0.5f}, 14.f, icon,
             selected ? IM_COL32(235, 244, 255, 255) : IM_COL32(160, 195, 235, 255));
    dl->AddText({min.x + 26.f, min.y + 2.f},
                selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(206, 213, 224, 255), label);

    ImGui::PopID();
    return clicked;
}

inline void drawExplorer(StudioState& s) {
    panelTitle("Explorer", Icon::Folder);

    ImGui::BeginChild("explorer_scroll", {0, 0}, false);

    if (ImGui::TreeNodeEx("Workspace", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {

        if (s.world.hasBaseplate) explorerItem("Baseplate", Icon::Grid, false, -2);

        for (int i = 0; i < (int)s.world.parts.size(); ++i) {
            const WorkspacePart& p = s.world.parts[(size_t)i];
            if (explorerItem(p.name.c_str(), iconForShape(p.shape), i == s.selected, i))
                s.selected = i;

            if (ImGui::BeginPopupContextItem("part_ctx")) {
                s.selected = i;
                if (ImGui::MenuItem("Duplica")) s.duplicateSelected();
                if (ImGui::MenuItem("Elimina")) s.deleteSelected();
                ImGui::EndPopup();
            }

            // Uno Script figlio è la cosa che si cerca più spesso nell'albero.
            if (!p.script.empty()) {
                ImGui::Indent(16.f);
                explorerItem("Script", Icon::Script, false, 10000 + i);
                ImGui::Unindent(16.f);
            }
        }

        for (int i = 0; i < (int)s.world.models.size(); ++i)
            explorerItem(s.world.models[(size_t)i].name.c_str(), Icon::Cube, false, 20000 + i);

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("StarterGui", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        for (int i = 0; i < (int)s.world.labels.size(); ++i)
            explorerItem(s.world.labels[(size_t)i].name.c_str(), Icon::Script, false, 30000 + i);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        explorerItem("SunSky", Icon::Light, false, 40000);
        ImGui::TreePop();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------- Properties

inline bool propCategory(const char* name) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    return ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanAllColumns
                                 | ImGuiTreeNodeFlags_DefaultOpen
                                 | ImGuiTreeNodeFlags_Framed);
}

/** Prepara la riga e lascia il cursore nella colonna del valore. */
inline void propRow(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

inline void drawProperties(StudioState& s) {
    panelTitle("Properties", Icon::Cube);

    WorkspacePart* p = s.selectedPart();
    if (!p) {
        ImGui::Spacing();
        ImGui::TextDisabled("Nessuna selezione.");
        ImGui::Spacing();
        ImGui::TextWrapped("Clicca una Part nella viewport o nell'Explorer.");
        return;
    }

    // L'undo si registra al rilascio, non a ogni pixel di drag: altrimenti un
    // trascinamento riempirebbe lo stack di centinaia di step da un decimo.
    static WorkspacePart before;
    static bool editing = false;
    auto touch = [&]() {
        if (!editing) { before = *p; editing = true; }
        s.dirty = true;
    };

    ImGui::BeginChild("props_scroll", {0, 0}, false);

    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
                                | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("props", 2, flags)) {
        ImGui::TableSetupColumn("Proprietà", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn("Valore", ImGuiTableColumnFlags_WidthStretch, 0.58f);

        if (propCategory("Data")) {
            propRow("Name");
            if (inputText("##name", p->name)) s.dirty = true;

            propRow("Position");
            if (ImGui::DragFloat3("##pos", &p->position.x, 0.1f)) touch();

            propRow("Size");
            if (ImGui::DragFloat3("##size", &p->size.x, 0.1f, 0.05f, 2048.f)) touch();

            propRow("Orientation");
            if (ImGui::DragFloat3("##rot", &p->rotation.x, 1.f)) touch();
            ImGui::TreePop();
        }

        if (propCategory("Appearance")) {
            propRow("Color");
            if (ImGui::ColorEdit3("##col", &p->color.x, ImGuiColorEditFlags_NoInputs)) s.dirty = true;

            propRow("Shape");
            int shape = (int)p->shape;
            const char* shapes[] = {"Block", "Ball", "Cylinder", "Wedge"};
            if (ImGui::Combo("##shape", &shape, shapes, IM_ARRAYSIZE(shapes))) {
                p->shape = (PartShape)shape;
                s.dirty = true;
            }

            propRow("Material");
            int material = (int)p->material;
            const char* materials[] = {"Plastic", "SmoothPlastic", "Neon", "Metal", "Wood",
                                       "Concrete", "Grass", "Glass", "Ice", "Brick", "ForceField"};
            if (ImGui::Combo("##mat", &material, materials, IM_ARRAYSIZE(materials))) {
                p->setMaterial((PartMaterial)material);
                s.dirty = true;
            }

            propRow("Emissive");
            if (ImGui::SliderFloat("##emis", &p->emissive, 0.f, 2.f)) s.dirty = true;

            propRow("Studs");
            if (ImGui::Checkbox("##studs", &p->useStud)) s.dirty = true;
            ImGui::TreePop();
        }

        if (propCategory("Behavior")) {
            propRow("Anchored");
            if (ImGui::Checkbox("##anch", &p->anchored)) s.dirty = true;

            propRow("CanCollide");
            if (ImGui::Checkbox("##coll", &p->canCollide)) s.dirty = true;

            propRow("CanDestroy");
            if (ImGui::Checkbox("##dest", &p->canDestroy)) s.dirty = true;

            propRow("Health");
            if (ImGui::DragFloat("##hp", &p->health, 1.f, 0.f, p->maxHealth)) s.dirty = true;
            ImGui::TreePop();
        }

        if (propCategory("Light")) {
            propRow("PointLight");
            if (ImGui::Checkbox("##pl", &p->pointLight)) s.dirty = true;

            if (p->pointLight) {
                propRow("Range");
                if (ImGui::DragFloat("##range", &p->lightRange, 0.5f, 0.f, 200.f)) s.dirty = true;

                propRow("Brightness");
                if (ImGui::DragFloat("##bright", &p->lightBrightness, 0.05f, 0.f, 10.f)) s.dirty = true;
            }
            ImGui::TreePop();
        }

        ImGui::EndTable();
    }

    if (editing && !ImGui::IsAnyItemActive()) {
        s.undo.recordTransform(s.selected, before, *p);
        editing = false;
    }

    ImGui::EndChild();
}

// -------------------------------------------------------------------- Output

/** Senza linguetta: sta già dentro una tab bar, ne basta una. */
inline void drawOutput(StudioState& s) {
    if (ImGui::SmallButton("Pulisci")) s.output.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%d righe", (int)s.output.lines.size());
    ImGui::Separator();

    ImGui::BeginChild("output_scroll", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const LogLine& line : s.output.lines)
        ImGui::TextColored(logColor(line.level), "%s", line.text.c_str());

    if (s.output.scrollToBottom) {
        ImGui::SetScrollHereY(1.f);
        s.output.scrollToBottom = false;
    }
    ImGui::EndChild();
}

} // namespace studio
