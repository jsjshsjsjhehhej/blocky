#pragma once


#include <imgui.h>

#include <cctype>
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

/**
 * L'unico dei quattro registri di PartShape che NON si accorpa qui: Icon
 * vive in StudioIcons.h, incluso dopo PartShape.h, e farlo dipendere da li'
 * creerebbe un ordine di inclusione fragile per un guadagno minimo.
 *
 * Percio' resta una switch — ma CHIUSA: niente "default", ogni valore
 * dell'enum e' elencato a mano (Count compreso, che non e' una forma vera ma
 * va comunque coperto). Con -Wswitch attivo per questa sola funzione, il
 * COMPILATORE rifiuta di costruire se PartShape guadagna un valore e questa
 * funzione non viene aggiornata — invece di restituire silenziosamente
 * Icon::Cube per una forma che nessuno ha ancora deciso come disegnare.
 * E' lo stesso principio dello static_assert sulla dimensione di NetState:
 * un dimenticanza diventa un errore di build, non un bug scoperto a occhio.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch"
inline Icon iconForShape(PartShape shape) {
    switch (shape) {
    case PartShape::Block:    return Icon::Cube;
    case PartShape::Ball:     return Icon::Sphere;
    case PartShape::Cylinder: return Icon::Cylinder;
    case PartShape::Wedge:    return Icon::Wedge;
    case PartShape::Count:    return Icon::Cube;   // sentinella, mai una forma vera
    }
    return Icon::Cube;
}
#pragma GCC diagnostic pop

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

    // Rinomina in-place con doppio click, come in Roblox Studio — prima
    // l'unico modo per rinominare era il campo Name nel Properties.
    static int renaming = -1;
    static char renameBuf[128] = {};
    static bool renameFocusPending = false;
    auto startRename = [&](int i, const std::string& name) {
        renaming = i;
        renameFocusPending = true;
        std::snprintf(renameBuf, sizeof(renameBuf), "%s", name.c_str());
    };

    if (ImGui::TreeNodeEx("Workspace", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {

        for (int i = 0; i < (int)s.world.parts.size(); ++i) {
            WorkspacePart& p = s.world.parts[(size_t)i];

            if (renaming == i) {
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (renameFocusPending) { ImGui::SetKeyboardFocusHere(); renameFocusPending = false; }
                const bool confirmed = ImGui::InputText("##rename", renameBuf, sizeof(renameBuf),
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
                if (confirmed || (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())) {
                    if (renameBuf[0] != '\0') {
                        p.name = renameBuf;
                        s.dirty = true;
                    }
                    renaming = -1;
                }
                ImGui::PopID();
            } else {
                if (explorerItem(p.name.c_str(), iconForShape(p.shape), i == s.selected, i))
                    s.selectPartIndex(i);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    startRename(i, p.name);
            }

            if (ImGui::BeginPopupContextItem("part_ctx")) {
                s.selectPartIndex(i);
                if (ImGui::MenuItem("Rinomina")) startRename(i, p.name);
                if (ImGui::MenuItem("Duplica")) s.duplicateSelected();
                if (ImGui::MenuItem("Elimina")) s.deleteSelected();
                ImGui::EndPopup();
            }

            if (!p.script.empty()) {
                ImGui::Indent(16.f);
                const char* scriptLabel = (p.scriptType == ScriptType::Client) ? "LocalScript"
                                        : (p.scriptType == ScriptType::Server) ? "Script"
                                        : "Script (Legacy)";
                explorerItem(scriptLabel, Icon::Script, false, 10000 + i);
                ImGui::Unindent(16.f);
            }
        }

        for (int i = 0; i < (int)s.world.models.size(); ++i) {
            if (explorerItem(s.world.models[(size_t)i].name.c_str(), Icon::Cube,
                             i == s.selectedModel, 20000 + i))
                s.selectModelIndex(i);
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("StarterGui", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        for (int i = 0; i < (int)s.world.labels.size(); ++i) {
            if (explorerItem(s.world.labels[(size_t)i].name.c_str(), Icon::Script,
                             i == s.selectedLabel, 30000 + i))
                s.selectLabelIndex(i);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        explorerItem("SunSky", Icon::Light, false, 40000);
        // Stessi effetti figli che si vedono sotto Lighting in Roblox
        // Studio — anche qui sono voci decorative (un solo LightingService
        // condiviso li' dietro, non oggetti separati), ma danno lo stesso
        // colpo d'occhio: "dentro Lighting ci sono altre cose", non un
        // contenitore vuoto con un solo elemento.
        static const char* const kLightingChildren[] = {
            "Atmosphere", "Sky", "Bloom", "DepthOfField", "SunRays",
        };
        for (int i = 0; i < (int)(sizeof(kLightingChildren) / sizeof(kLightingChildren[0])); ++i)
            explorerItem(kLightingChildren[i], Icon::Light, false, 40001 + i);
        ImGui::TreePop();
    }

    // Servizi Roblox che oggi in BlockyStudio sono contenitori vuoti (nessun
    // dato del motore vive ancora li' dentro) ma restano nell'Explorer per
    // coerenza con Roblox Studio — esattamente come in un place Roblox
    // vuoto, dove la maggior parte di questi non contiene nulla finche' non
    // ci si mette qualcosa. Non e' finto: sono posti veri, solo vuoti.
    static const char* const kEmptyServices[] = {
        "Players", "MaterialService", "ReplicatedFirst", "ReplicatedStorage",
        "ServerScriptService", "ServerStorage", "StarterPack", "StarterPlayer",
        "Teams", "SoundService", "TextChatService",
    };
    for (int i = 0; i < (int)(sizeof(kEmptyServices) / sizeof(kEmptyServices[0])); ++i) {
        ImGui::PushID(50000 + i);
        ImGui::TreeNodeEx(kEmptyServices[i], ImGuiTreeNodeFlags_SpanAvailWidth
                                            | ImGuiTreeNodeFlags_Leaf
                                            | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        ImGui::PopID();
    }

    ImGui::EndChild();
}


// Confronto case-insensitive senza allocare: per il filtro proprieta'.
inline bool containsCI(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack) return false;
    const size_t hn = std::strlen(haystack), nn = std::strlen(needle);
    if (nn > hn) return false;
    for (size_t i = 0; i + nn <= hn; ++i) {
        size_t j = 0;
        for (; j < nn; ++j) {
            if (std::tolower((unsigned char)haystack[i + j]) != std::tolower((unsigned char)needle[j]))
                break;
        }
        if (j == nn) return true;
    }
    return false;
}

inline bool propCategory(const char* name) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    return ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanAllColumns
                                 | ImGuiTreeNodeFlags_DefaultOpen
                                 | ImGuiTreeNodeFlags_Framed);
}

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

    // Filtro proprieta' (Ctrl+Maiusc+P in Roblox Studio) — sempre visibile
    // anche senza selezione, come nell'originale.
    static char filterBuf[64] = {};
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##propfilter", "Filtra proprietà (Ctrl+Maiusc+P)",
                             filterBuf, sizeof(filterBuf));
    const bool filtering = filterBuf[0] != '\0';
    auto show = [&](const char* label) { return !filtering || containsCI(label, filterBuf); };

    // Modelli ed etichette hanno le loro proprieta' — prima solo le Part
    // erano davvero editabili da qui, Model/Label comparivano nell'Explorer
    // ma senza nessun modo di modificarle.
    if (WorkspaceModel* m = s.selectedModelPtr()) {
        ImGui::BeginChild("props_scroll_model", {0, 0}, false);
        const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("props_model", 2, flags)) {
            ImGui::TableSetupColumn("Proprietà", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableSetupColumn("Valore", ImGuiTableColumnFlags_WidthStretch, 0.58f);
            if (show("Name") && propCategory("Data")) {
                propRow("Name");
                if (inputText("##mname", m->name)) s.dirty = true;
                propRow("Position");
                if (ImGui::DragFloat3("##mpos", &m->position.x, 0.1f)) s.dirty = true;
                propRow("Rotation");
                if (ImGui::DragFloat3("##mrot", &m->rotation.x, 1.f)) s.dirty = true;
                propRow("Scale");
                if (ImGui::DragFloat3("##mscale", &m->scale.x, 0.02f, 0.02f, 50.f)) s.dirty = true;
                ImGui::TreePop();
            }
            if (show("Tint") && propCategory("Appearance")) {
                propRow("Tint");
                if (ImGui::ColorEdit3("##mtint", &m->tint.x, ImGuiColorEditFlags_NoInputs)) s.dirty = true;
                ImGui::TreePop();
            }
            if (show("CanCollide") && propCategory("Behavior")) {
                propRow("CanCollide");
                if (ImGui::Checkbox("##mcoll", &m->canCollide)) s.dirty = true;
                ImGui::TreePop();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        return;
    }

    if (WorkspaceLabel* l = s.selectedLabelPtr()) {
        ImGui::BeginChild("props_scroll_label", {0, 0}, false);
        const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("props_label", 2, flags)) {
            ImGui::TableSetupColumn("Proprietà", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableSetupColumn("Valore", ImGuiTableColumnFlags_WidthStretch, 0.58f);
            if (show("Name") && propCategory("Data")) {
                propRow("Name");
                if (inputText("##lname", l->name)) s.dirty = true;
                propRow("ClassName");
                ImGui::TextUnformatted(l->className.c_str());
                ImGui::TreePop();
            }
            if ((show("Text") || show("Color") || show("FontScale") || show("Visible"))
                && propCategory("Appearance")) {
                if (show("Text")) {
                    propRow("Text");
                    if (inputText("##ltext", l->text)) s.dirty = true;
                }
                if (show("Color")) {
                    propRow("Color");
                    if (ImGui::ColorEdit3("##lcol", &l->color.x, ImGuiColorEditFlags_NoInputs)) s.dirty = true;
                }
                if (show("FontScale")) {
                    propRow("FontScale");
                    if (ImGui::DragFloat("##lfont", &l->fontScale, 0.02f, 0.2f, 4.f)) s.dirty = true;
                }
                if (show("BgColor")) {
                    propRow("BgColor");
                    if (ImGui::ColorEdit3("##lbg", &l->bgColor.x, ImGuiColorEditFlags_NoInputs)) s.dirty = true;
                }
                if (show("BgTransparency")) {
                    propRow("BgTransparency");
                    if (ImGui::SliderFloat("##lbgt", &l->bgTransparency, 0.f, 1.f)) s.dirty = true;
                }
                if (show("Visible")) {
                    propRow("Visible");
                    if (ImGui::Checkbox("##lvis", &l->visible)) s.dirty = true;
                }
                ImGui::TreePop();
            }
            if ((show("AnchorX") || show("OffsetX")) && propCategory("Layout")) {
                propRow("Anchor");
                float anch[2] = {l->anchorX, l->anchorY};
                if (ImGui::DragFloat2("##lanch", anch, 0.01f, 0.f, 1.f)) {
                    l->anchorX = anch[0]; l->anchorY = anch[1]; s.dirty = true;
                }
                propRow("Offset");
                float off[2] = {l->offsetX, l->offsetY};
                if (ImGui::DragFloat2("##loff", off, 1.f)) {
                    l->offsetX = off[0]; l->offsetY = off[1]; s.dirty = true;
                }
                ImGui::TreePop();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        return;
    }

    WorkspacePart* p = s.selectedPart();
    if (!p) {
        ImGui::Spacing();
        ImGui::TextDisabled("Nessuna selezione.");
        ImGui::Spacing();
        ImGui::TextWrapped("Clicca una Part, un Model o una Label nella viewport o nell'Explorer.");
        return;
    }

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

        const bool anyData = show("Name") || show("Position") || show("Size") || show("Orientation");
        if (anyData && propCategory("Data")) {
            if (show("Name")) {
                propRow("Name");
                if (inputText("##name", p->name)) s.dirty = true;
            }
            if (show("Position")) {
                propRow("Position");
                if (ImGui::DragFloat3("##pos", &p->position.x, 0.1f)) touch();
            }
            if (show("Size")) {
                propRow("Size");
                if (ImGui::DragFloat3("##size", &p->size.x, 0.1f, 0.05f, 2048.f)) touch();
            }
            if (show("Orientation")) {
                propRow("Orientation");
                if (ImGui::DragFloat3("##rot", &p->rotation.x, 1.f)) touch();
            }
            ImGui::TreePop();
        }

        const bool anyAppearance = show("Color") || show("Shape") || show("Material")
                                 || show("Emissive") || show("Studs");
        if (anyAppearance && propCategory("Appearance")) {
            if (show("Color")) {
                propRow("Color");
                if (ImGui::ColorEdit3("##col", &p->color.x, ImGuiColorEditFlags_NoInputs)) s.dirty = true;
            }
            if (show("Shape")) {
                propRow("Shape");
                int shape = (int)p->shape;
                const char* shapes[] = {"Block", "Ball", "Cylinder", "Wedge"};
                if (ImGui::Combo("##shape", &shape, shapes, IM_ARRAYSIZE(shapes))) {
                    p->shape = (PartShape)shape;
                    s.dirty = true;
                }
            }
            if (show("Material")) {
                propRow("Material");
                int material = (int)p->material;
                const char* materials[] = {"Plastic", "SmoothPlastic", "Neon", "Metal", "Wood",
                                           "Concrete", "Grass", "Glass", "Ice", "Brick", "ForceField"};
                if (ImGui::Combo("##mat", &material, materials, IM_ARRAYSIZE(materials))) {
                    p->setMaterial((PartMaterial)material);
                    s.dirty = true;
                }
            }
            if (show("Emissive")) {
                propRow("Emissive");
                if (ImGui::SliderFloat("##emis", &p->emissive, 0.f, 2.f)) s.dirty = true;
            }
            if (show("Studs")) {
                propRow("Studs");
                if (ImGui::Checkbox("##studs", &p->useStud)) s.dirty = true;
            }
            ImGui::TreePop();
        }

        const bool anyBehavior = show("Anchored") || show("CanCollide") || show("CanDestroy") || show("Health");
        if (anyBehavior && propCategory("Behavior")) {
            if (show("Anchored")) {
                propRow("Anchored");
                if (ImGui::Checkbox("##anch", &p->anchored)) s.dirty = true;
            }
            if (show("CanCollide")) {
                propRow("CanCollide");
                if (ImGui::Checkbox("##coll", &p->canCollide)) s.dirty = true;
            }
            if (show("CanDestroy")) {
                propRow("CanDestroy");
                if (ImGui::Checkbox("##dest", &p->canDestroy)) s.dirty = true;
            }
            if (show("Health")) {
                propRow("Health");
                if (ImGui::DragFloat("##hp", &p->health, 1.f, 0.f, p->maxHealth)) s.dirty = true;
            }
            ImGui::TreePop();
        }

        const bool anyLight = show("PointLight") || (p->pointLight && (show("Range") || show("Brightness")));
        if (anyLight && propCategory("Light")) {
            if (show("PointLight")) {
                propRow("PointLight");
                if (ImGui::Checkbox("##pl", &p->pointLight)) s.dirty = true;
            }

            if (p->pointLight) {
                if (show("Range")) {
                    propRow("Range");
                    if (ImGui::DragFloat("##range", &p->lightRange, 0.5f, 0.f, 200.f)) s.dirty = true;
                }
                if (show("Brightness")) {
                    propRow("Brightness");
                    if (ImGui::DragFloat("##bright", &p->lightBrightness, 0.05f, 0.f, 10.f)) s.dirty = true;
                }
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

}
