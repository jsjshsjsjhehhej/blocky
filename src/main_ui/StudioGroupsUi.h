// Model di Roblox Studio in Blocky: Ctrl+G raggruppa, Ctrl+U separa, clic su
// una part di un Model seleziona tutto il Model (Alt+clic = solo la part),
// Ctrl+clic aggiunge/toglie, e il gizmo sposta/ruota tutta la selezione.
// La logica (dati, salvataggio, geometria) e' in engine/PartGroups.h.
//
// Non e' un header riusabile: usa le globali di main.cpp (g_world, g_undo,
// g_selectedPart) e la multi-selezione di StudioFeaturesUi.h. Va incluso
// subito dopo quel file.

#include "engine/PartGroups.h"

// ---- Selezione ------------------------------------------------------------

// Model selezionati per intero (tutte le loro part sono nella selezione).
static std::vector<int> studioSelectedWholeGroups() {
    const std::vector<int> sel = studioActivePartIndices();
    std::vector<int> out;
    for (int i : sel) {
        const int top = partgroups::topGroupOf(g_world, i);
        if (top <= 0 || std::find(out.begin(), out.end(), top) != out.end()) continue;
        bool all = true;
        for (int m : partgroups::memberParts(g_world, top))
            if (std::find(sel.begin(), sel.end(), m) == sel.end()) { all = false; break; }
        if (all) out.push_back(top);
    }
    return out;
}

static void studioSelectGroup(int gid, int primary = -1) {
    const std::vector<int> members = partgroups::memberParts(g_world, gid);
    if (members.empty()) return;
    studioMultiClear();
    for (int i : members) studioMultiAdd(i);
    selectPart(std::find(members.begin(), members.end(), primary) != members.end() ? primary : members.front());
    if (const PartGroup* g = partgroups::find(g_world, gid))
        g_exportMsg = "Model \"" + g->name + "\": " + std::to_string(members.size()) + " part";
}

// Clic nel viewport su una part (chiamato da applyScenePick, GizmoSystem.h).
static void studioPickPart(int i) {
    if (!partIndexValid(i)) return;
    const ImGuiIO& io = ImGui::GetIO();
    const int top = io.KeyAlt ? 0 : partgroups::topGroupOf(g_world, i);
    if (io.KeyCtrl) {
        // La selezione attuale (anche se e' una part sola) resta.
        if (g_multiParts.empty() && partIndexValid(g_selectedPart)) studioMultiAdd(g_selectedPart);
        const std::vector<int> add = top ? partgroups::memberParts(g_world, top) : std::vector<int>{i};
        bool allIn = true;
        for (int m : add) allIn = allIn && studioMultiContains(m);
        for (int m : add) {
            if (allIn) { if (studioMultiContains(m)) studioMultiToggle(m); }
            else studioMultiAdd(m);
        }
        if (!g_multiParts.empty())
            selectPart(studioMultiContains(i) ? i : g_multiParts.back());
        else
            selectPart(-1);
        return;
    }
    if (top) {
        studioSelectGroup(top, i);
        return;
    }
    studioMultiClear();
    selectPart(i);
}

// ---- Ctrl+G / Ctrl+U ------------------------------------------------------

static void studioGroupSelection() {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.empty()) {
        g_exportMsg = "Group: seleziona prima delle part (Ctrl+clic per piu' di una)";
        return;
    }
    const std::vector<PartGroup> before = g_world.groups;
    const std::vector<int> partsBefore = UndoStack::partGroups(g_world);
    const int gid = partgroups::group(g_world, sel, "Model");
    g_undo.recordGroupChange(before, partsBefore, g_world);
    studioSelectGroup(gid, g_selectedPart);
    markPlaceDirty();
}

// true se c'era un Model da separare (Ctrl+U senza Model resta "inserisci suono").
static bool studioUngroupSelection() {
    const std::vector<int> groups = studioSelectedWholeGroups();
    if (groups.empty()) return false;
    const std::vector<PartGroup> before = g_world.groups;
    const std::vector<int> partsBefore = UndoStack::partGroups(g_world);
    for (int gid : groups) partgroups::ungroup(g_world, gid);
    g_undo.recordGroupChange(before, partsBefore, g_world);
    markPlaceDirty();
    g_exportMsg = "Ungroup: " + std::to_string(groups.size()) + " Model separati";
    return true;
}

static void studioRenameGroup(int gid, const std::string& name) {
    PartGroup* g = partgroups::find(g_world, gid);
    if (!g || name.empty() || g->name == name) return;
    const std::vector<PartGroup> before = g_world.groups;
    const std::vector<int> partsBefore = UndoStack::partGroups(g_world);
    g->name = name;
    g_undo.recordGroupChange(before, partsBefore, g_world);
    markPlaceDirty();
}

// ---- Movimento di gruppo ---------------------------------------------------
//
// Il gizmo (e Properties, Transform Type-In...) muovono solo g_selectedPart.
// Ogni frame si guarda di quanto e' cambiata e si applica lo stesso
// spostamento e la stessa rotazione al resto della selezione: cosi' ogni
// strumento che muove una part muove anche il Model, senza riscriverli tutti.

struct StudioFollowState {
    int primary = -1;
    std::vector<int> set;
    partgroups::Pose last;
    bool dragging = false;
    std::vector<std::pair<int, WorkspacePart>> beforeDrag;
};
static StudioFollowState g_follow;

static bool studioGizmoDragging() {
    return g_moveDragging || g_rotateDragging || g_scaleDragging || g_freeDragging;
}

static void studioSyncGroupFollow() {
    StudioFollowState& f = g_follow;
    if (g_animStudioDriving) {   // le muove l animazione, non l utente
        f = StudioFollowState{};
        return;
    }
    std::vector<int> sel = studioActivePartIndices();
    for (int i : g_multiParts)
        if (!partIndexValid(i)) { studioMultiClear(); sel.clear(); break; }
    const bool active = sel.size() > 1 && partIndexValid(g_selectedPart) && studioMultiContains(g_selectedPart);
    if (!active) {
        f = StudioFollowState{};
        return;
    }
    const WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
    const partgroups::Pose now{p.position, p.rotation};
    std::sort(sel.begin(), sel.end());
    if (f.primary != g_selectedPart || f.set != sel) {
        f.primary = g_selectedPart;
        f.set = sel;
        f.last = now;
        f.dragging = false;
        f.beforeDrag.clear();
        return;
    }

    std::vector<int> followers;
    for (int i : sel)
        if (i != g_selectedPart && !g_world.parts[(size_t)i].locked) followers.push_back(i);

    const bool dragNow = studioGizmoDragging();
    if (dragNow && !f.dragging) {
        f.beforeDrag.clear();
        for (int i : followers) f.beforeDrag.push_back({i, g_world.parts[(size_t)i]});
    }

    if (now.position != f.last.position || now.rotation != f.last.rotation) {
        // Fuori da un trascinamento (Properties, type-in) l'Undo va salvato
        // subito, insieme alla voce della part principale se c'e'.
        std::vector<std::pair<int, WorkspacePart>> before;
        if (!dragNow)
            for (int i : followers) before.push_back({i, g_world.parts[(size_t)i]});
        partgroups::applyFollow(g_world.parts, followers, f.last, now);
        if (!dragNow) {
            g_undo.beginBatch();
            for (auto& [i, b] : before) g_undo.recordTransform(i, b, g_world.parts[(size_t)i]);
            g_undo.endBatch();
        }
        markPlaceDirty();
    }

    if (!dragNow && f.dragging) {
        // Fine trascinamento: il gizmo ha appena salvato la sua voce per la
        // part principale; quelle delle altre si legano a lei in un unico Ctrl+Z.
        const bool primaryRecorded = !g_undo.undo.empty()
            && g_undo.undo.back().kind == UndoKind::TransformPart
            && g_undo.undo.back().index == g_selectedPart;
        const uint32_t batch = g_undo.beginBatch();
        if (primaryRecorded) g_undo.joinLastToBatch(batch);
        for (auto& [i, b] : f.beforeDrag)
            if (partIndexValid(i)) g_undo.recordTransform(i, b, g_world.parts[(size_t)i]);
        g_undo.endBatch();
        f.beforeDrag.clear();
    }
    f.dragging = dragNow;
    f.last = now;
}

// Dopo Ctrl+Z / Ctrl+Y le part sono gia' tutte al loro posto: si riparte da
// li', senza "seguire" la principale (le altre si sposterebbero due volte).
static void studioFollowRebaseline() {
    g_follow = StudioFollowState{};
}

// Ctrl+D: duplica tutta la selezione. I Model selezionati per intero
// diventano Model nuovi; un unico Ctrl+Z toglie tutte le copie.
static void studioDuplicateSelection() {
    std::vector<int> originals = studioActivePartIndices();
    if (originals.size() <= 1 && partgroups::topGroupOf(g_world, g_selectedPart) == 0) {
        duplicateSelected();   // una part sciolta: il comportamento di sempre
        return;
    }
    std::sort(originals.begin(), originals.end());
    const std::vector<PartGroup> groupsBefore = g_world.groups;
    const std::vector<int> partGroupsBefore = UndoStack::partGroups(g_world);
    glm::vec3 mn(0.f), mx(0.f);
    partgroups::boundsOf(g_world, originals, mn, mx);
    const float shift = (mx.x - mn.x) + 1.f;   // accanto all'originale, come duplicateSelected

    std::vector<int> copies;
    for (int i : originals) {
        WorkspacePart c = g_world.parts[(size_t)i];
        c.netId = 0;
        c.position.x += shift;
        g_world.parts.push_back(std::move(c));
        copies.push_back((int)g_world.parts.size() - 1);
    }
    partgroups::remapCopies(g_world, originals, copies);

    g_undo.beginBatch();
    for (int c : copies) g_undo.recordInsert(c, g_world.parts[(size_t)c]);
    std::vector<int> partGroupsPadded = partGroupsBefore;
    partGroupsPadded.resize(g_world.parts.size(), 0);
    g_undo.recordGroupChange(groupsBefore, partGroupsPadded, g_world);
    g_undo.endBatch();

    studioMultiClear();
    for (int c : copies) studioMultiAdd(c);
    selectPart(copies.front());
    markPlaceDirty();
    g_exportMsg = "Duplicato: " + std::to_string(copies.size()) + " part";
}

// ---- Tasti -----------------------------------------------------------------

static void studioGroupsPollHotkeys() {
    if (g_studioTesting) return;
    partgroups::pruneEmpty(g_world);
    studioSyncGroupFollow();
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !io.KeyCtrl || io.KeyShift) return;
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) studioGroupSelection();
}

// ---- Explorer ----------------------------------------------------------------

// Trascina e rilascia: le part (la trascinata, o tutta la selezione se ne fa
// parte) entrano nel Model gid; gid = 0 le toglie dai Model.
static void studioMovePartsToGroup(int dragged, int gid) {
    if (!partIndexValid(dragged)) return;
    std::vector<int> idx = studioMultiContains(dragged) ? studioActivePartIndices() : std::vector<int>{dragged};
    const std::vector<PartGroup> before = g_world.groups;
    const std::vector<int> partsBefore = UndoStack::partGroups(g_world);
    for (int i : idx)
        if (partIndexValid(i) && !g_world.parts[(size_t)i].locked) g_world.parts[(size_t)i].groupId = gid;
    g_undo.recordGroupChange(before, partsBefore, g_world);
    markPlaceDirty();
    const PartGroup* g = partgroups::find(g_world, gid);
    g_exportMsg = std::to_string(idx.size()) + (g ? " part in \"" + g->name + "\"" : std::string(" part fuori dai Model"));
}

static void studioDropTargetForGroup(int gid) {
    if (!ImGui::BeginDragDropTarget()) return;
    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("BLOCKY_PART"))
        studioMovePartsToGroup(*(const int*)pl->Data, gid);
    ImGui::EndDragDropTarget();
}

static int g_groupRenaming = 0;
static bool g_groupRenameFocus = false;
static char g_groupRenameBuf[64] = {};

static void studioStartRenameGroup(int gid) {
    const PartGroup* g = partgroups::find(g_world, gid);
    if (!g) return;
    g_groupRenaming = gid;
    g_groupRenameFocus = true;
    std::snprintf(g_groupRenameBuf, sizeof(g_groupRenameBuf), "%s", g->name.c_str());
}

static void drawExplorerGroupNode(int gid);

static void drawExplorerGroupChildren(int gid) {
    for (int child : partgroups::childGroups(g_world, gid)) drawExplorerGroupNode(child);
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (g_world.parts[(size_t)i].groupId == gid) drawExplorerPartRow(i);
}

static void drawExplorerGroupNode(int gid) {
    const PartGroup* g = partgroups::find(g_world, gid);
    if (!g) return;
    const std::vector<int> members = partgroups::memberParts(g_world, gid);
    bool whole = !members.empty();
    for (int m : members) whole = whole && studioMultiContains(m);

    ImGui::PushID(gid);
    if (g_groupRenaming == gid) {
        if (g_groupRenameFocus) {
            ImGui::SetKeyboardFocusHere();
            g_groupRenameFocus = false;
        }
        if (ImGui::InputText("##rename", g_groupRenameBuf, sizeof(g_groupRenameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            studioRenameGroup(gid, g_groupRenameBuf);
            g_groupRenaming = 0;
        } else if (ImGui::IsItemDeactivated()) {
            g_groupRenaming = 0;
        }
        ImGui::PopID();
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (whole) flags |= ImGuiTreeNodeFlags_Selected;
    const std::string label = "[Model] " + g->name + "  (" + std::to_string(members.size()) + ")";
    const bool open = ImGui::TreeNodeEx("##g", flags, "%s", label.c_str());
    studioDropTargetForGroup(gid);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (ImGui::GetIO().KeyCtrl) {
            for (int m : members) {
                if (whole) { if (studioMultiContains(m)) studioMultiToggle(m); }
                else studioMultiAdd(m);
            }
            if (!g_multiParts.empty()) selectPart(g_multiParts.back());
        } else {
            studioSelectGroup(gid);
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) studioStartRenameGroup(gid);
    if (ImGui::BeginPopupContextItem("##gctx")) {
        if (ImGui::MenuItem("Seleziona tutto il Model")) studioSelectGroup(gid);
        if (ImGui::MenuItem("Rinomina", "doppio clic")) studioStartRenameGroup(gid);
        if (ImGui::MenuItem("Separa (Ungroup)", "Ctrl+U")) {
            studioSelectGroup(gid);
            studioUngroupSelection();
        }
        ImGui::EndPopup();
    }
    if (open) {
        drawExplorerGroupChildren(gid);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

// Al posto dell'elenco piatto: prima i Model (con dentro le loro part), poi
// le part sciolte.
static void drawExplorerPartsTree() {
    for (int gid : partgroups::childGroups(g_world, 0)) drawExplorerGroupNode(gid);
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (!partgroups::find(g_world, g_world.parts[(size_t)i].groupId)) drawExplorerPartRow(i);
    // Mentre si trascina una part: la zona per toglierla dai Model.
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType("BLOCKY_PART")) {
        ImGui::Selectable("  -> rilascia qui: fuori dai Model", false);
        studioDropTargetForGroup(0);
    }
}
