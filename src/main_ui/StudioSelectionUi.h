// Lavorare con piu' part insieme in Studio, come in Roblox Studio:
//   - contorno di tutte le part selezionate e riquadro del Model selezionato
//   - selezione a riquadro (trascina sul vuoto; Ctrl aggiunge, Alt = part singole)
//   - Ctrl+C / Ctrl+X / Ctrl+V di part e Model (con i loro vincoli)
//   - Proprieta' su tutta la selezione (colore, materiale, ogni proprieta' di
//     PartProps.def) con un solo Ctrl+Z
//   - scala di un Model o di una selezione attorno al suo centro
//   - linee dei vincoli (Weld/Hinge/Rope/Spring/Rod) nel viewport
//
// Disegna tutto in 2D sopra la scena (linee ImGui proiettate con la camera
// di Studio), come i riquadri di selezione di Roblox: niente stato OpenGL.
//
// Non e' un header riusabile: usa le globali di main.cpp. Va incluso dopo
// StudioGroupsUi.h.

#include "engine/PartClipboard.h"
#include "engine/PartConstraints.h"
#include "engine/PartGroups.h"

// ---- Proiezione -----------------------------------------------------------

static bool studioWorldToScreen(const glm::vec3& w, ImVec2& out) {
    const glm::vec4 clip = g_lastProj * g_lastView * glm::vec4(w, 1.f);
    if (clip.w <= 0.05f) return false;   // dietro la camera
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2((ndc.x * 0.5f + 0.5f) * (float)g_w, (1.f - (ndc.y * 0.5f + 0.5f)) * (float)g_h);
    return true;
}

static void studioDrawLine3D(ImDrawList* dl, const glm::vec3& a, const glm::vec3& b, ImU32 col, float t) {
    ImVec2 sa, sb;
    if (studioWorldToScreen(a, sa) && studioWorldToScreen(b, sb)) dl->AddLine(sa, sb, col, t);
}

// 12 spigoli di un parallelepipedo dati gli 8 vertici (bit 0 = x, 1 = y, 2 = z).
static void studioDrawWireBox(ImDrawList* dl, const glm::vec3 (&c)[8], ImU32 col, float t) {
    static const int kEdges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                      {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : kEdges) studioDrawLine3D(dl, c[e[0]], c[e[1]], col, t);
}

static void studioPartCorners(const WorkspacePart& p, glm::vec3 (&c)[8], float pad = 0.f) {
    const glm::mat4 m = glm::translate(glm::mat4(1.f), p.position) * partRotationMatrix(p.rotation)
                      * glm::scale(glm::mat4(1.f), p.size + glm::vec3(pad));
    for (int k = 0; k < 8; ++k)
        c[k] = glm::vec3(m * glm::vec4((k & 1) ? 0.5f : -0.5f, (k & 2) ? 0.5f : -0.5f,
                                       (k & 4) ? 0.5f : -0.5f, 1.f));
}

// ---- Selezione a riquadro ---------------------------------------------------

struct StudioMarquee {
    bool active = false;
    ImVec2 from;
};
static StudioMarquee g_marquee;

// Clic sul vuoto nel viewport (onMouseBtn): parte il riquadro.
static void studioBeginMarquee(float mx, float my) {
    g_marquee.active = true;
    g_marquee.from = ImVec2(mx, my);
}

// Rilascio: se il mouse si e' mosso abbastanza, seleziona cio' che sta dentro
// (il centro della part nel riquadro). Un Model si prende intero, come col clic.
static void studioEndMarquee(float mx, float my) {
    if (!g_marquee.active) return;
    g_marquee.active = false;
    const ImVec2 a = g_marquee.from;
    if (std::fabs(mx - a.x) < 5.f && std::fabs(my - a.y) < 5.f) return;   // era un clic
    const float x0 = std::min(a.x, mx), x1 = std::max(a.x, mx);
    const float y0 = std::min(a.y, my), y1 = std::max(a.y, my);
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) studioMultiClear();
    int last = -1;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.locked || !studioExplorerPartVisible(i)) continue;
        ImVec2 s;
        if (!studioWorldToScreen(p.position, s) || s.x < x0 || s.x > x1 || s.y < y0 || s.y > y1) continue;
        const int top = io.KeyAlt ? 0 : partgroups::topGroupOf(g_world, i);
        if (top) for (int m : partgroups::memberParts(g_world, top)) studioMultiAdd(m);
        else studioMultiAdd(i);
        last = i;
    }
    if (last >= 0) selectPart(last);
    g_exportMsg = std::to_string(g_multiParts.size()) + " part selezionate";
}

// ---- Disegno sopra la scena ---------------------------------------------------

static bool g_showConstraintLines = true;

static void drawStudioSelectionOverlays() {
    if (g_screen != AppScreen::Studio || g_studioTesting) {
        g_marquee.active = false;
        return;
    }
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Contorno di ogni part selezionata (la principale piu' marcata).
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.size() > 1) {
        for (int i : sel) {
            glm::vec3 c[8];
            studioPartCorners(g_world.parts[(size_t)i], c, 0.08f);
            studioDrawWireBox(dl, c, i == g_selectedPart ? IM_COL32(60, 170, 255, 255) : IM_COL32(60, 170, 255, 150),
                              i == g_selectedPart ? 2.f : 1.f);
        }
    }

    // Riquadro arancione (con nome) attorno a ogni Model selezionato per intero.
    for (int gid : studioSelectedWholeGroups()) {
        glm::vec3 mn(0.f), mx(0.f);
        if (!partgroups::boundsOf(g_world, partgroups::memberParts(g_world, gid), mn, mx)) continue;
        mn -= glm::vec3(0.15f);
        mx += glm::vec3(0.15f);
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k)
            c[k] = {(k & 1) ? mx.x : mn.x, (k & 2) ? mx.y : mn.y, (k & 4) ? mx.z : mn.z};
        studioDrawWireBox(dl, c, IM_COL32(255, 160, 40, 230), 1.5f);
        ImVec2 s;
        if (const PartGroup* g = partgroups::find(g_world, gid))
            if (studioWorldToScreen({(mn.x + mx.x) * 0.5f, mx.y, (mn.z + mx.z) * 0.5f}, s))
                dl->AddText(ImVec2(s.x + 4, s.y - 18), IM_COL32(255, 180, 70, 255), g->name.c_str());
    }

    // Vincoli: una linea fra le due part, colore per tipo; la cerniera mostra anche l'asse.
    if (g_showConstraintLines) {
        for (const WeldConstraint& w : g_world.welds) {
            if (!partconstraints::validPart(g_world, w.part0) || !partconstraints::validPart(g_world, w.part1))
                continue;
            const WorkspacePart& a = g_world.parts[(size_t)w.part0];
            const WorkspacePart& b = g_world.parts[(size_t)w.part1];
            ImU32 col = IM_COL32(200, 200, 200, 200);
            switch (w.kind) {
            case ConstraintKind::Weld:   col = IM_COL32(120, 220, 120, 220); break;
            case ConstraintKind::Hinge:  col = IM_COL32(240, 200, 60, 230); break;
            case ConstraintKind::Rope:   col = IM_COL32(200, 150, 90, 230); break;
            case ConstraintKind::Spring: col = IM_COL32(90, 200, 240, 230); break;
            case ConstraintKind::Rod:    col = IM_COL32(220, 120, 220, 230); break;
            case ConstraintKind::Count:  break;
            }
            if (!w.enabled) col = (col & 0x00FFFFFF) | 0x60000000;
            studioDrawLine3D(dl, a.position, b.position, col, 2.f);
            if (w.kind == ConstraintKind::Hinge) {
                const glm::mat3 r0(partRotationMatrix(a.rotation));
                const glm::vec3 pivot = a.position + r0 * w.pivot;
                const glm::vec3 axis = glm::length(w.axis) > 1e-4f ? glm::normalize(r0 * w.axis) : glm::vec3(0, 1, 0);
                studioDrawLine3D(dl, pivot - axis * 1.5f, pivot + axis * 1.5f, IM_COL32(255, 230, 90, 255), 3.f);
            }
        }
    }

    // Il riquadro mentre si trascina; finisce quando il tasto si rilascia
    // (anche sopra un pannello, dove onMouseBtn non arriva).
    if (g_marquee.active && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        studioEndMarquee(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
    }
    if (g_marquee.active) {
        const ImVec2 now = ImGui::GetIO().MousePos;
        dl->AddRectFilled(g_marquee.from, now, IM_COL32(60, 150, 255, 40));
        dl->AddRect(g_marquee.from, now, IM_COL32(60, 150, 255, 220), 0.f, 0, 1.5f);
    }
}

// ---- Appunti (Ctrl+C / Ctrl+X / Ctrl+V) --------------------------------------

static partclipboard::Clipboard g_clipboard;

static void studioCopySelection() {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.empty()) {
        g_exportMsg = "Copia: niente di selezionato";
        return;
    }
    g_clipboard = partclipboard::copy(g_world, sel);
    g_exportMsg = "Copiate " + std::to_string(g_clipboard.parts.size()) + " part"
                + (g_clipboard.groups.empty() ? std::string() : " (" + std::to_string(g_clipboard.groups.size()) + " Model)");
}

static void studioCutSelection() {
    studioCopySelection();
    if (!g_clipboard.parts.empty()) studioDeleteUnlockedSelection();
}

// Incolla appoggiando la copia sopra l'originale (come Roblox quando lo
// spazio e' occupato). I Model copiati diventano Model nuovi; una part presa da
// un Model torna in quel Model se esiste ancora.
static void studioPasteClipboard() {
    if (g_clipboard.parts.empty()) {
        g_exportMsg = "Incolla: appunti vuoti (Ctrl+C prima)";
        return;
    }
    const std::vector<PartGroup> groupsBefore = g_world.groups;
    std::vector<int> partGroupsBefore = UndoStack::partGroups(g_world);
    const int base = partclipboard::paste(g_world, g_clipboard, partclipboard::stackOffset(g_clipboard));

    g_undo.beginBatch();
    for (int i = base; i < (int)g_world.parts.size(); ++i) g_undo.recordInsert(i, g_world.parts[(size_t)i]);
    partGroupsBefore.resize(g_world.parts.size(), 0);
    g_undo.recordGroupChange(groupsBefore, partGroupsBefore, g_world);
    g_undo.endBatch();

    studioMultiClear();
    for (int i = base; i < (int)g_world.parts.size(); ++i) studioMultiAdd(i);
    selectPart(base);
    markPlaceDirty();
    g_exportMsg = "Incollate " + std::to_string(g_clipboard.parts.size()) + " part";
}

// ---- Scala ----------------------------------------------------------------------

// I bottoni "Ingrandisci/Riduci" del pannello: con piu' part (o un Model)
// scala tutto attorno al centro, distanze comprese; con una sola, come prima.
static void studioScaleSelection(float factor) {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.size() <= 1) {
        scaleSelected(factor);
        return;
    }
    std::vector<WorkspacePart> before;
    for (int i : sel) before.push_back(g_world.parts[(size_t)i]);
    partgroups::scaleParts(g_world, sel, factor);
    g_undo.beginBatch();
    for (size_t k = 0; k < sel.size(); ++k) g_undo.recordTransform(sel[k], before[k], g_world.parts[(size_t)sel[k]]);
    g_undo.endBatch();
    studioFollowRebaseline();
    markPlaceDirty();
}

// ---- Proprieta' su tutta la selezione ---------------------------------------------

// Una modifica = una voce di Undo per tutta la selezione, presa quando il
// controllo si attiva e salvata quando si rilascia (non a ogni frame del trascinamento).
struct MultiEditUndo {
    std::vector<int> idx;
    std::vector<WorkspacePart> before;
};
static MultiEditUndo g_multiEdit;

static void multiEditTrack(const std::vector<int>& sel) {
    if (ImGui::IsItemActivated()) {
        g_multiEdit.idx = sel;
        g_multiEdit.before.clear();
        for (int i : sel) g_multiEdit.before.push_back(g_world.parts[(size_t)i]);
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && !g_multiEdit.idx.empty()) {
        g_undo.beginBatch();
        for (size_t k = 0; k < g_multiEdit.idx.size(); ++k) {
            const int i = g_multiEdit.idx[k];
            if (i >= 0 && i < (int)g_world.parts.size())
                g_undo.recordEdit(i, g_multiEdit.before[k], g_world.parts[(size_t)i]);
        }
        g_undo.endBatch();
        g_multiEdit = MultiEditUndo{};
        markPlaceDirty();
    }
}

// In cima al pannello Properties quando ci sono 2+ part selezionate: i valori
// mostrati sono quelli della part principale, una modifica vale per tutte.
static void drawMultiPartPropertiesSection() {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.size() < 2 || !partIndexValid(g_selectedPart)) return;
    WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.2f, 0.32f, 0.6f));
    ImGui::BeginChild("##multi", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextColored({0.55f, 0.8f, 1.f, 1.f}, "%d part selezionate", (int)sel.size());
    ImGui::TextDisabled("Le modifiche qui valgono per tutte (un solo Ctrl+Z).");

    glm::vec3 col = p.color;
    if (ImGui::ColorEdit3("Colore (tutte)", &col.x))
        for (int i : sel) g_world.parts[(size_t)i].color = col;
    multiEditTrack(sel);

    int mat = (int)p.material;
    if (ImGui::BeginCombo("Materiale (tutte)", partMaterialName(p.material))) {
        for (int m = 0; m < (int)PartMaterial::Count; ++m) {
            if (ImGui::Selectable(partMaterialName((PartMaterial)m), m == mat)) {
                std::vector<WorkspacePart> before;
                for (int i : sel) before.push_back(g_world.parts[(size_t)i]);
                for (int i : sel) g_world.parts[(size_t)i].setMaterial((PartMaterial)m);
                g_undo.beginBatch();
                for (size_t k = 0; k < sel.size(); ++k) g_undo.recordEdit(sel[k], before[k], g_world.parts[(size_t)sel[k]]);
                g_undo.endBatch();
                markPlaceDirty();
            }
        }
        ImGui::EndCombo();
    }

    // Ogni proprieta' della tabella PartProps.def, gratis.
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help)                    \
    {                                                                                \
        T v = p.field;                                                               \
        if (partprops::editUi(luaName " (tutte)", v, mn, mx))                        \
            for (int i : sel) g_world.parts[(size_t)i].field = v;                    \
        multiEditTrack(sel);                                                         \
        partprops::helpTooltip(help);                                                \
    }
#include "engine/PartProps.def"
#undef PART_PROP

    if (ImGui::Button("Ingrandisci tutto x1.25")) studioScaleSelection(1.25f);
    ImGui::SameLine();
    if (ImGui::Button("Riduci tutto x0.8")) studioScaleSelection(0.8f);
    if (ImGui::Button("Raggruppa in Model (Ctrl+G)")) studioGroupSelection();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::TextDisabled("Sotto: solo la part principale.");
}

// ---- Tasti ------------------------------------------------------------------------

static void studioSelectionPollHotkeys() {
    if (g_studioTesting) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !io.KeyCtrl || io.KeyShift) return;
    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) studioCopySelection();
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) studioCutSelection();
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) studioPasteClipboard();
}
