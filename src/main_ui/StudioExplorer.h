// Esploratore stile Roblox Studio: un albero solo (Workspace e poi i servizi
// nell'ordine di Studio), un'icona per classe, il Name come etichetta, "+"
// sulla riga per inserire dentro quell'oggetto, F2 / doppio clic rinomina,
// tasto destro = lo stesso menu che compare nel viewport.
//
// I nomi delle classi sono quelli di Roblox: Part, WedgePart, MeshPart
// (modello importato), Model, Script, LocalScript, ModuleScript...
// Incluso da StudioUiPanel.h, usa le globali di main.cpp.

#include "studio/StudioPlatform.h"

// ---- Icone (disegnate, una per classe) --------------------------------------------------

enum class ExIcon {
    Workspace, Camera, Terrain, Spawn, Part, Wedge, Ball, Cylinder, MeshPart, Model,
    Folder, Script, LocalScript, ModuleScript, Remote, Players, Player, Humanoid, BodyPart,
    Lighting, Sky, Atmosphere, Service, Storage, Gui, Label, Button, Frame, Image,
    Pack, Tool, Sound, Chat, Teams, Light
};

static void exDrawIcon(ImDrawList* dl, ImVec2 c, ExIcon ic) {
    const float r = 6.5f;
    auto col = [](int r_, int g, int b) { return IM_COL32(r_, g, b, 255); };
    const ImVec2 a{c.x - r, c.y - r}, b{c.x + r, c.y + r};
    switch (ic) {
    case ExIcon::Workspace:
        dl->AddCircleFilled(c, r, col(70, 150, 230));
        dl->AddCircle(c, r * 0.55f, col(150, 220, 120), 12, 2.f);
        break;
    case ExIcon::Camera:
        dl->AddRectFilled({a.x, c.y - 4}, {c.x + 3, c.y + 4}, col(150, 160, 175), 1.5f);
        dl->AddTriangleFilled({c.x + 3, c.y}, {b.x, c.y - 4}, {b.x, c.y + 4}, col(150, 160, 175));
        break;
    case ExIcon::Terrain:
        dl->AddTriangleFilled({a.x, b.y}, {c.x - 1, a.y + 2}, {c.x + 4, b.y}, col(90, 170, 90));
        dl->AddTriangleFilled({c.x - 2, b.y}, {c.x + 3, c.y - 2}, {b.x, b.y}, col(120, 200, 110));
        break;
    case ExIcon::Spawn:
        dl->AddRectFilled({a.x, c.y + 1}, {b.x, b.y - 1}, col(160, 165, 175), 2.f);
        dl->AddCircle({c.x, c.y - 2}, 3.f, col(90, 200, 250), 10, 2.f);
        break;
    case ExIcon::Part:
        dl->AddQuadFilled({c.x, a.y}, {b.x, c.y - 2}, {c.x, c.y + 1}, {a.x, c.y - 2}, col(150, 200, 255));
        dl->AddQuadFilled({a.x, c.y - 2}, {c.x, c.y + 1}, {c.x, b.y}, {a.x, c.y + 4}, col(80, 140, 220));
        dl->AddQuadFilled({c.x, c.y + 1}, {b.x, c.y - 2}, {b.x, c.y + 4}, {c.x, b.y}, col(55, 110, 190));
        break;
    case ExIcon::Wedge:
        dl->AddTriangleFilled({a.x, b.y}, {b.x, b.y}, {b.x, a.y + 1}, col(110, 170, 240));
        break;
    case ExIcon::Ball:
        dl->AddCircleFilled(c, r - 0.5f, col(110, 170, 240));
        dl->AddCircleFilled({c.x - 2, c.y - 2}, 2.f, col(200, 225, 255));
        break;
    case ExIcon::Cylinder:
        dl->AddRectFilled({a.x + 2, a.y + 2}, {b.x - 2, b.y - 2}, col(80, 140, 220));
        dl->AddEllipseFilled({c.x, a.y + 2}, {r - 2, 2.5f}, col(150, 200, 255));
        break;
    case ExIcon::MeshPart:
        dl->AddTriangleFilled({c.x, a.y}, {b.x, b.y}, {a.x, b.y}, col(80, 140, 220));
        dl->AddTriangle({c.x, a.y}, {b.x, b.y}, {a.x, b.y}, col(170, 210, 255), 1.2f);
        dl->AddLine({c.x, a.y}, {c.x, b.y}, col(170, 210, 255), 1.f);
        break;
    case ExIcon::Model:
        dl->AddRect({a.x, a.y + 1}, {c.x + 1, c.y + 2}, col(150, 200, 255), 1.f, 0, 1.5f);
        dl->AddRectFilled({c.x - 1, c.y - 1}, {b.x, b.y}, col(80, 140, 220), 1.f);
        break;
    case ExIcon::Folder:
    case ExIcon::Storage:
        dl->AddRectFilled({a.x, a.y + 2}, {c.x, a.y + 5}, col(230, 185, 80), 1.f);
        dl->AddRectFilled({a.x, a.y + 4}, {b.x, b.y - 1}, ic == ExIcon::Folder ? col(240, 200, 95) : col(225, 170, 70), 1.5f);
        break;
    case ExIcon::Script:
    case ExIcon::LocalScript:
    case ExIcon::ModuleScript: {
        const ImU32 k = ic == ExIcon::Script ? col(90, 190, 110) : ic == ExIcon::LocalScript ? col(90, 160, 240) : col(170, 120, 230);
        dl->AddRectFilled({a.x + 1, a.y}, {b.x - 1, b.y}, col(235, 238, 242), 1.5f);
        for (int l = 0; l < 3; ++l) dl->AddLine({a.x + 3, a.y + 3.5f + l * 3.f}, {b.x - 3 - (l == 1 ? 3.f : 0.f), a.y + 3.5f + l * 3.f}, k, 1.5f);
        break;
    }
    case ExIcon::Remote:
        dl->AddTriangleFilled({a.x, c.y - 4}, {c.x, c.y}, {a.x, c.y + 4}, col(240, 170, 60));
        dl->AddTriangleFilled({c.x, c.y - 4}, {b.x, c.y}, {c.x, c.y + 4}, col(240, 200, 90));
        break;
    case ExIcon::Players:
    case ExIcon::Player:
    case ExIcon::Teams:
        dl->AddCircleFilled({c.x - (ic == ExIcon::Player ? 0.f : 2.f), a.y + 3}, 2.6f, col(250, 200, 70));
        dl->AddRectFilled({c.x - (ic == ExIcon::Player ? 3.5f : 5.5f), c.y}, {c.x + (ic == ExIcon::Player ? 3.5f : 1.5f), b.y}, col(250, 200, 70), 2.f);
        if (ic != ExIcon::Player) {
            dl->AddCircleFilled({c.x + 3.5f, a.y + 4}, 2.2f, ic == ExIcon::Teams ? col(90, 200, 120) : col(240, 140, 60));
            dl->AddRectFilled({c.x + 1.5f, c.y + 1}, {b.x, b.y}, ic == ExIcon::Teams ? col(90, 200, 120) : col(240, 140, 60), 2.f);
        }
        break;
    case ExIcon::Humanoid:
    case ExIcon::BodyPart:
        dl->AddCircleFilled({c.x, a.y + 2.5f}, 2.5f, col(230, 190, 150));
        dl->AddRectFilled({c.x - 3, c.y - 1}, {c.x + 3, b.y}, ic == ExIcon::Humanoid ? col(90, 150, 230) : col(200, 170, 140), 1.f);
        break;
    case ExIcon::Lighting:
    case ExIcon::Light:
        dl->AddCircleFilled({c.x, c.y - 1.5f}, 4.5f, col(255, 210, 70));
        dl->AddRectFilled({c.x - 2, c.y + 3}, {c.x + 2, b.y}, col(170, 170, 180));
        break;
    case ExIcon::Sky:
        dl->AddCircleFilled({c.x - 2, c.y}, 3.5f, col(230, 240, 255));
        dl->AddCircleFilled({c.x + 2.5f, c.y + 1}, 3.f, col(200, 220, 250));
        dl->AddCircleFilled({b.x - 2, a.y + 2}, 2.f, col(255, 210, 70));
        break;
    case ExIcon::Atmosphere:
        dl->AddCircle(c, r - 1, col(120, 180, 255), 16, 1.5f);
        dl->AddCircle(c, r - 4, col(170, 210, 255), 12, 1.5f);
        break;
    case ExIcon::Gui:
    case ExIcon::Frame:
        dl->AddRect({a.x, a.y + 1}, {b.x, b.y - 1}, col(120, 190, 255), 1.5f, 0, 1.5f);
        dl->AddRectFilled({a.x, a.y + 1}, {b.x, a.y + 4}, col(120, 190, 255), 1.5f);
        break;
    case ExIcon::Label:
        dl->AddText({c.x - 3.5f, c.y - 7.5f}, col(120, 190, 255), "T");
        break;
    case ExIcon::Button:
        dl->AddRectFilled({a.x, c.y - 4}, {b.x, c.y + 4}, col(90, 160, 240), 3.f);
        dl->AddLine({a.x + 4, c.y}, {b.x - 4, c.y}, col(230, 240, 255), 1.5f);
        break;
    case ExIcon::Image:
        dl->AddRect({a.x, a.y + 1}, {b.x, b.y - 1}, col(120, 190, 255), 1.f);
        dl->AddTriangleFilled({a.x + 1, b.y - 2}, {c.x - 1, c.y}, {c.x + 3, b.y - 2}, col(110, 200, 120));
        break;
    case ExIcon::Pack:
        dl->AddRectFilled({a.x + 1, a.y + 3}, {b.x - 1, b.y}, col(170, 120, 70), 2.f);
        dl->AddRect({c.x - 3, a.y}, {c.x + 3, a.y + 4}, col(170, 120, 70), 1.f, 0, 1.5f);
        break;
    case ExIcon::Tool:
        dl->AddLine({a.x + 1, b.y - 1}, {b.x - 2, a.y + 2}, col(190, 195, 205), 2.5f);
        dl->AddLine({a.x + 2, c.y + 1}, {c.x - 1, b.y - 2}, col(150, 110, 70), 2.5f);
        break;
    case ExIcon::Sound:
        dl->AddRectFilled({a.x + 1, c.y - 2}, {a.x + 4, c.y + 2}, col(150, 160, 175));
        dl->AddTriangleFilled({a.x + 4, c.y - 2}, {c.x + 1, a.y + 1}, {c.x + 1, b.y - 1}, col(150, 160, 175));
        dl->AddLine({c.x + 3, c.y - 3}, {c.x + 3, c.y + 3}, col(150, 160, 175), 1.5f);
        dl->AddLine({b.x - 1, c.y - 5}, {b.x - 1, c.y + 5}, col(150, 160, 175), 1.5f);
        break;
    case ExIcon::Chat:
        dl->AddRectFilled({a.x, a.y + 1}, {b.x, c.y + 3}, col(120, 190, 255), 3.f);
        dl->AddTriangleFilled({a.x + 2, c.y + 2}, {a.x + 6, c.y + 2}, {a.x + 2, b.y}, col(120, 190, 255));
        break;
    case ExIcon::Service:
    default:
        dl->AddCircle(c, r - 1.5f, col(170, 175, 185), 8, 2.f);
        dl->AddCircleFilled(c, 2.f, col(170, 175, 185));
        break;
    }
}

/** Classe Roblox di una Part (il nome che compare nel tooltip e in Proprieta'). */
static const char* exPartClass(const WorkspacePart& p) {
    return p.shape == PartShape::Wedge ? "WedgePart" : "Part";
}

static ExIcon exPartIcon(const WorkspacePart& p) {
    if (p.pointLight) return ExIcon::Light;
    switch (p.shape) {
    case PartShape::Wedge: return ExIcon::Wedge;
    case PartShape::Ball: return ExIcon::Ball;
    case PartShape::Cylinder: return ExIcon::Cylinder;
    default: return ExIcon::Part;
    }
}

static const char* exScriptClass(ScriptType t) {
    return t == ScriptType::Client ? "LocalScript" : t == ScriptType::ModuleScript ? "ModuleScript" : "Script";
}
static ExIcon exScriptIcon(ScriptType t) {
    return t == ScriptType::Client ? ExIcon::LocalScript : t == ScriptType::ModuleScript ? ExIcon::ModuleScript : ExIcon::Script;
}
static ExIcon exClassIcon(const std::string& cls) {
    if (cls == "Script") return ExIcon::Script;
    if (cls == "LocalScript") return ExIcon::LocalScript;
    if (cls == "ModuleScript") return ExIcon::ModuleScript;
    if (cls == "RemoteEvent" || cls == "RemoteFunction" || cls == "BindableEvent") return ExIcon::Remote;
    if (cls == "TextLabel") return ExIcon::Label;
    if (cls == "TextButton" || cls == "ImageButton") return ExIcon::Button;
    if (cls == "Frame") return ExIcon::Frame;
    if (cls == "ImageLabel") return ExIcon::Image;
    return ExIcon::Folder;
}

// ---- Riga dell'albero -----------------------------------------------------------------------

enum class ExInsert { None, Workspace, Group, Part, StarterGui, StarterPack, Sound, Shared };
struct ExInsertTarget {
    ExInsert kind = ExInsert::None;
    int index = -1;   // gid del Model, indice della Part, SharedService
};
static ExInsertTarget g_exInsert;
static ImGuiID g_exInsertPopupId = 0;   // preso al livello dell'albero: le righe hanno ID annidati
static int g_exRenamePart = -1;       // part in rinomina (F2 / doppio clic)
static bool g_exRenameFocus = false;
static char g_exRenameBuf[96] = {};

struct ExRow {
    bool open = false;       // da chiudere con ImGui::TreePop()
    bool clicked = false;    // clic sinistro sulla riga (non sulla freccia)
    bool rclicked = false;
    bool dbl = false;
};

/**
 * Una riga: freccia (se ha figli), icona, nome. `insert` = mostra il "+"
 * passando col mouse; cliccandolo si apre il menu "Inserisci" di quell'oggetto.
 */
static ExRow exRow(const char* id, ExIcon icon, const char* name, bool selected, bool hasChildren, bool defaultOpen,
                   const char* cls, ExInsertTarget insert = {}) {
    ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                          | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (!hasChildren) fl |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (defaultOpen) fl |= ImGuiTreeNodeFlags_DefaultOpen;
    if (selected) fl |= ImGuiTreeNodeFlags_Selected;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.f, 1.f});
    const bool open = ImGui::TreeNodeEx(id, fl, "      %s", name);
    ImGui::PopStyleVar();
    ExRow r;
    r.open = open && hasChildren;
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    r.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen();
    r.rclicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    r.dbl = hovered && ImGui::IsMouseDoubleClicked(0);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    exDrawIcon(dl, {mn.x + ImGui::GetTreeNodeToLabelSpacing() + 7.f, (mn.y + mx.y) * 0.5f}, icon);
    if (hovered && cls && cls[0] && !ImGui::IsMouseDown(0)) ImGui::SetTooltip("%s  (%s)", name, cls);
    if (insert.kind != ExInsert::None && hovered) {
        const ImVec2 pc{mx.x - 11.f, (mn.y + mx.y) * 0.5f};
        const ImVec2 m = ImGui::GetIO().MousePos;
        const bool over = std::fabs(m.x - pc.x) < 8.f && std::fabs(m.y - pc.y) < 8.f;
        dl->AddCircleFilled(pc, 7.f, over ? IM_COL32(90, 150, 230, 255) : IM_COL32(120, 125, 135, 230));
        dl->AddLine({pc.x - 3.5f, pc.y}, {pc.x + 3.5f, pc.y}, IM_COL32(255, 255, 255, 255), 1.6f);
        dl->AddLine({pc.x, pc.y - 3.5f}, {pc.x, pc.y + 3.5f}, IM_COL32(255, 255, 255, 255), 1.6f);
        if (over) {
            ImGui::SetTooltip("Inserisci oggetto in %s", name);
            if (ImGui::IsMouseClicked(0)) {
                g_exInsert = insert;
                ImGui::OpenPopup(g_exInsertPopupId);
                r.clicked = false;
            }
        }
    }
    return r;
}

// ---- Azioni sulla selezione (menu tasto destro: Esploratore e viewport) -------------------

static int exPartTopGroup(int i) { return partIndexValid(i) ? partgroups::topGroupOf(g_world, i) : 0; }

static void exDeleteSelection() {
    if (g_multiParts.size() > 1) studioDeleteUnlockedSelection();
    else deleteSelected();
}

static void exBeginRename(int part) {
    if (!partIndexValid(part)) return;
    g_exRenamePart = part;
    g_exRenameFocus = true;
    std::snprintf(g_exRenameBuf, sizeof(g_exRenameBuf), "%s", g_world.parts[(size_t)part].name.c_str());
    g_showExplorer = true;
}

/** F2 come Roblox Studio: rinomina la part (o il Model) selezionato nell'Esploratore. */
static void studioExplorerBeginRename() {
    const std::vector<int> groups = studioSelectedWholeGroups();
    if (!groups.empty()) { studioStartRenameGroup(groups.front()); g_showExplorer = true; return; }
    exBeginRename(g_selectedPart);
}

static void exInsertPartInto(int gid, PartShape shape) {
    if (shape == PartShape::Block) insertNewPart(); else insertShapedPart(shape);
    if (gid > 0 && partIndexValid(g_selectedPart)) {
        studioMultiClear();
        studioMovePartsToGroup(g_selectedPart, gid);
    }
}

/** Il menu del tasto destro, come in Roblox Studio (Taglia, Copia, Incolla, Raggruppa...). */
static void drawStudioObjectMenuItems() {
    const std::vector<int> sel = studioActivePartIndices();
    const bool any = !sel.empty();
    const bool one = partIndexValid(g_selectedPart);
    const bool locked = one && g_world.parts[(size_t)g_selectedPart].locked;
    const int topGroup = one ? exPartTopGroup(g_selectedPart) : 0;
    const std::vector<int> wholeGroups = studioSelectedWholeGroups();

    if (ImGui::MenuItem("Taglia", "Ctrl+X", false, any)) studioCutSelection();
    if (ImGui::MenuItem("Copia", "Ctrl+C", false, any)) studioCopySelection();
    if (ImGui::MenuItem("Incolla", "Ctrl+V", false, !g_clipboard.empty())) studioPasteClipboard();
    if (ImGui::MenuItem("Incolla in", "Ctrl+Maiusc+V", false, !g_clipboard.empty() && topGroup > 0)) {
        studioPasteClipboard();
        if (partIndexValid(g_selectedPart)) studioMovePartsToGroup(g_selectedPart, topGroup);
    }
    if (ImGui::MenuItem("Duplica", "Ctrl+D", false, any)) studioDuplicateSelection();
    if (ImGui::MenuItem("Elimina", "Canc", false, any && !locked)) exDeleteSelection();
    if (ImGui::MenuItem("Rinomina", "F2", false, one || !wholeGroups.empty())) studioExplorerBeginRename();
    ImGui::Separator();
    if (ImGui::MenuItem("Raggruppa come modello", "Ctrl+G", false, any)) studioGroupSelection();
    if (ImGui::MenuItem("Sciogli gruppo", "Ctrl+U", false, !wholeGroups.empty())) studioUngroupSelection();
    ImGui::Separator();
    if (ImGui::MenuItem("Seleziona Parent", nullptr, false, topGroup > 0 && wholeGroups.empty())) studioSelectGroup(topGroup, g_selectedPart);
    if (ImGui::MenuItem("Seleziona simili (colore)", nullptr, false, one)) studioSelectSimilar(0);
    if (ImGui::MenuItem("Ingrandisci", "F", false, any)) studioFocusSelection();
    if (ImGui::BeginMenu("Allinea", sel.size() > 1)) {
        static const char* axes[3] = {"X", "Y", "Z"};
        for (int a = 0; a < 3; ++a) {
            char lab[48];
            std::snprintf(lab, sizeof(lab), "Centra su %s", axes[a]);
            if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Center);
            std::snprintf(lab, sizeof(lab), "Minimo su %s", axes[a]);
            if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Min);
            std::snprintf(lab, sizeof(lab), "Massimo su %s", axes[a]);
            if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Max);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (one) {
        WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
        if (ImGui::MenuItem("Anchored", nullptr, p.anchored, !locked)) studioSetAnchoredOnSelection(!p.anchored);
        if (ImGui::MenuItem("CanCollide", nullptr, p.canCollide, !locked)) studioSetCanCollideOnSelection(!p.canCollide);
        if (ImGui::MenuItem("Locked", nullptr, p.locked)) studioSetLockedOnSelection(!p.locked);
        if (ImGui::BeginMenu("Colore", !locked)) {
            static const char* quick[] = {"Medium stone grey", "White", "Really black", "Bright red", "Bright blue",
                                          "Bright yellow", "Bright green", "Bright orange", "Hot pink", "Lime green"};
            for (const char* nm : quick)
                if (ImGui::MenuItem(nm))
                    if (const auto* e = brickcolor::findByName(nm)) studioApplyColorToSelection(brickcolor::toLinear(*e));
            ImGui::Separator();
            if (ImGui::MenuItem("Altri colori...")) g_showBrickColorPicker = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Materiale", !locked)) {
            for (int mi = 0; mi < (int)PartMaterial::Count; ++mi)
                if (ImGui::MenuItem(partMaterialName((PartMaterial)mi), nullptr, p.material == (PartMaterial)mi))
                    studioApplyMaterialToSelection((PartMaterial)mi);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Inserisci nella Part")) {
            if (ImGui::MenuItem("Script")) studioAddScriptOfType(ScriptType::Server);
            if (ImGui::MenuItem("LocalScript")) studioAddScriptOfType(ScriptType::Client);
            if (ImGui::MenuItem("ModuleScript")) studioAddScriptOfType(ScriptType::ModuleScript);
            ImGui::Separator();
            if (ImGui::MenuItem("PointLight", nullptr, p.pointLight)) { p.pointLight = !p.pointLight; markPlaceDirty(); }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Tag...")) g_showTagsWindow = true;
        ImGui::Separator();
    }
    if (ImGui::MenuItem("Inserisci parte")) exInsertPartInto(topGroup, PartShape::Block);
    if (ImGui::MenuItem("Inserisci oggetto...", "Ctrl+I")) g_showInsertObject = true;
    if (ImGui::MenuItem("Proprieta'")) g_showProps = true;
}

// ---- Menu "+" (Inserisci dentro l'oggetto della riga) --------------------------------------

static void drawExplorerInsertPopup() {
    if (!ImGui::BeginPopup("##exinsert")) return;
    const ExInsertTarget t = g_exInsert;
    ImGui::TextDisabled("Inserisci");
    ImGui::Separator();
    switch (t.kind) {
    case ExInsert::Workspace:
    case ExInsert::Group: {
        const int gid = t.kind == ExInsert::Group ? t.index : 0;
        if (ImGui::MenuItem("Part")) exInsertPartInto(gid, PartShape::Block);
        if (ImGui::MenuItem("Part (sfera)")) exInsertPartInto(gid, PartShape::Ball);
        if (ImGui::MenuItem("Part (cilindro)")) exInsertPartInto(gid, PartShape::Cylinder);
        if (ImGui::MenuItem("WedgePart")) exInsertPartInto(gid, PartShape::Wedge);
        if (gid == 0) {
            ImGui::Separator();
            if (ImGui::MenuItem("MeshPart (importa modello)")) insertImportedModel();
            if (ImGui::MenuItem("SpawnLocation")) studioRunInsertKind(7);
            if (ImGui::MenuItem("Part con PointLight")) insertPointLightPart();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Altro...", "Ctrl+I")) g_showInsertObject = true;
        break;
    }
    case ExInsert::Part:
        if (partIndexValid(t.index)) {
            selectPart(t.index);
            if (ImGui::MenuItem("Script")) studioAddScriptOfType(ScriptType::Server);
            if (ImGui::MenuItem("LocalScript")) studioAddScriptOfType(ScriptType::Client);
            if (ImGui::MenuItem("ModuleScript")) studioAddScriptOfType(ScriptType::ModuleScript);
            ImGui::Separator();
            WorkspacePart& p = g_world.parts[(size_t)t.index];
            if (ImGui::MenuItem("PointLight", nullptr, p.pointLight)) { p.pointLight = !p.pointLight; markPlaceDirty(); }
        }
        break;
    case ExInsert::StarterGui:
        if (ImGui::MenuItem("TextLabel")) insertNewTextLabel();
        if (ImGui::MenuItem("TextButton")) insertNewTextButton();
        if (ImGui::MenuItem("Frame")) insertNewFrame();
        if (ImGui::MenuItem("ImageLabel")) insertNewImageLabel();
        break;
    case ExInsert::StarterPack:
        if (ImGui::MenuItem("Tool")) g_world.addBackpackItem("Tool", (int)g_world.backpack.size());
        if (ImGui::MenuItem("Tool: spada")) g_world.addBackpackItem("Sword", 3, BackpackTool::Sword);
        if (ImGui::MenuItem("Tool: pistola")) g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol);
        break;
    case ExInsert::Sound:
        if (ImGui::MenuItem("Sound")) insertSound();
        break;
    case ExInsert::Shared: {
        const SharedService svc = (SharedService)t.index;
        static const char* kCls[] = {"Script", "LocalScript", "ModuleScript", "RemoteEvent", "RemoteFunction", "Folder"};
        for (const char* c : kCls) {
            if (svc == SharedService::ServerScriptService && std::string(c) == "LocalScript") continue;
            if (ImGui::MenuItem(c)) {
                g_world.addShared(svc, c, c);
                g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                markPlaceDirty();
            }
        }
        break;
    }
    default:
        break;
    }
    ImGui::EndPopup();
}

// ---- Albero ---------------------------------------------------------------------------------

static void exPartRow(int i);

static void exGroupRow(int gid) {
    const PartGroup* g = partgroups::find(g_world, gid);
    if (!g) return;
    const std::vector<int> members = partgroups::memberParts(g_world, gid);
    bool whole = !members.empty();
    for (int m : members) whole = whole && studioMultiContains(m);
    ImGui::PushID(gid);
    if (g_groupRenaming == gid) {
        if (g_groupRenameFocus) { ImGui::SetKeyboardFocusHere(); g_groupRenameFocus = false; }
        ImGui::SetNextItemWidth(-1);
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
    const ExRow r = exRow("##g", ExIcon::Model, g->name.c_str(), whole, true, false, "Model", {ExInsert::Group, gid});
    studioDropTargetForGroup(gid);
    if (r.clicked) {
        if (ImGui::GetIO().KeyCtrl) {
            for (int m : members) { if (whole) { if (studioMultiContains(m)) studioMultiToggle(m); } else studioMultiAdd(m); }
            if (!g_multiParts.empty()) selectPart(g_multiParts.back());
        } else {
            studioSelectGroup(gid);
        }
    }
    if (r.rclicked && !whole) studioSelectGroup(gid);
    if (r.dbl) studioStartRenameGroup(gid);
    if (ImGui::BeginPopupContextItem("##gctx")) {
        drawStudioObjectMenuItems();
        ImGui::EndPopup();
    }
    if (r.open) {
        for (int child : partgroups::childGroups(g_world, gid)) exGroupRow(child);
        for (int i = 0; i < (int)g_world.parts.size(); ++i)
            if (g_world.parts[(size_t)i].groupId == gid) exPartRow(i);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void exPartRow(int i) {
    if (!studioExplorerPartVisible(i)) return;
    WorkspacePart& p = g_world.parts[(size_t)i];
    ImGui::PushID(i);
    if (g_exRenamePart == i) {
        if (g_exRenameFocus) { ImGui::SetKeyboardFocusHere(); g_exRenameFocus = false; }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##prn", g_exRenameBuf, sizeof(g_exRenameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (g_exRenameBuf[0]) {
                p.name = g_exRenameBuf;
                if (g_selectedPart == i) std::snprintf(g_partNameBuf, sizeof(g_partNameBuf), "%s", p.name.c_str());
                markPlaceDirty();
            }
            g_exRenamePart = -1;
        } else if (ImGui::IsItemDeactivated()) {
            g_exRenamePart = -1;
        }
        ImGui::PopID();
        return;
    }
    const bool sel = (i == g_selectedPart && !g_editingPartScript) || studioMultiContains(i);
    const bool hasScript = !p.script.empty();
    const ExRow r = exRow("##p", exPartIcon(p), p.locked ? (p.name + " (bloccata)").c_str() : p.name.c_str(), sel,
                          hasScript, false, exPartClass(p), {ExInsert::Part, i});
    if (r.clicked) {
        if (ImGui::GetIO().KeyCtrl) { studioMultiToggle(i); selectPart(i); }
        else if (ImGui::GetIO().KeyShift && partIndexValid(g_selectedPart)) {
            // Shift: tutte le part fra la selezionata e questa (come l'Esploratore di Studio)
            const int a = std::min(g_selectedPart, i), b = std::max(g_selectedPart, i);
            for (int k = a; k <= b; ++k) if (studioExplorerPartVisible(k)) studioMultiAdd(k);
            selectPart(i);
        } else { studioMultiClear(); selectPart(i); g_editingPartScript = false; }
    }
    if (r.rclicked && !studioMultiContains(i) && i != g_selectedPart) { studioMultiClear(); selectPart(i); }
    if (r.dbl) exBeginRename(i);
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("BLOCKY_PART", &i, sizeof(int));
        ImGui::Text("%s%s", p.name.c_str(), studioMultiContains(i) && g_multiParts.size() > 1 ? " (+ selezione)" : "");
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("##pctx")) {
        drawStudioObjectMenuItems();
        ImGui::EndPopup();
    }
    if (r.open) {
        const bool scriptSel = i == g_selectedPart && g_editingPartScript;
        const ExRow s = exRow("##s", exScriptIcon(p.scriptType), exScriptClass(p.scriptType), scriptSel, false, false,
                              exScriptClass(p.scriptType));
        if (s.clicked || s.dbl) { selectPart(i); g_editingPartScript = true; g_showProps = true; }
        if (s.dbl) g_showScript = true;
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void exSharedChildren(SharedService svc, int parent) {
    for (int i = 0; i < (int)g_world.instances.size(); ++i) {
        const SharedInstance& s = g_world.instances[(size_t)i];
        if (s.service != svc || s.parentIndex != parent) continue;
        bool kids = false;
        for (const auto& c : g_world.instances) if (c.service == svc && c.parentIndex == i) { kids = true; break; }
        ImGui::PushID(1000 + i);
        const ExRow r = exRow("##sh", exClassIcon(s.className), s.name.c_str(), g_selectedSharedInstance == i, kids, true,
                              s.className.c_str());
        if (r.clicked) {
            const int svcId = svc == SharedService::ReplicatedStorage ? studio::platform::Svc_ReplicatedStorage
                            : svc == SharedService::ServerStorage ? studio::platform::Svc_ServerStorage
                                                                   : studio::platform::Svc_ServerScriptService;
            selectService(svcId);
            g_selectedSharedInstance = i;
        }
        if (r.open) { exSharedChildren(svc, i); ImGui::TreePop(); }
        ImGui::PopID();
    }
}

static bool exServiceRow(const char* id, ExIcon icon, const char* name, int svcId, bool hasChildren, bool open = false,
                         ExInsertTarget ins = {}) {
    const ExRow r = exRow(id, icon, name, svcId >= 0 && g_selectedService == svcId, hasChildren, open, name, ins);
    if (r.clicked && svcId >= 0) toggleService(svcId);
    return r.open;
}

static void drawRobloxExplorer() {
    using namespace studio::platform;
    // Barra in alto: cerca + menu filtri ("...").
    ImGui::SetNextItemWidth(-30.f);
    ImGui::InputTextWithHint("##exfilter", "Cerca", g_explorerFilterBuf, sizeof(g_explorerFilterBuf));
    ImGui::SameLine();
    if (ImGui::Button("...", {24.f, 0.f})) ImGui::OpenPopup("##exfilters");
    if (ImGui::BeginPopup("##exfilters")) {
        ImGui::TextDisabled("Mostra solo");
        ImGui::Checkbox("Part con script", &g_explorerScriptsOnly);
        ImGui::Checkbox("Part bloccate", &g_explorerLockedOnly);
        ImGui::Checkbox("Part non ancorate", &g_explorerUnanchoredOnly);
        ImGui::Separator();
        if (ImGui::MenuItem("Rinomina", "F2")) studioExplorerBeginRename();
        if (ImGui::MenuItem("Inserisci oggetto...", "Ctrl+I")) g_showInsertObject = true;
        ImGui::EndPopup();
    }
    ImGui::BeginChild("##extree", {0, 0}, ImGuiChildFlags_None);
    g_exInsertPopupId = ImGui::GetID("##exinsert");
    // Con l'Esploratore a fuoco i tasti globali dello Studio sono spenti: questi
    // tre servono proprio qui (come in Roblox Studio). Ctrl+C/X/V vanno gia'.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && !g_studioTesting) {
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) studioExplorerBeginRename();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !studioActivePartIndices().empty()) exDeleteSelection();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) studioDuplicateSelection();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.f, 1.f});
    const bool filtering = g_explorerFilterBuf[0] || g_explorerScriptsOnly || g_explorerLockedOnly || g_explorerUnanchoredOnly;

    // Workspace
    if (exRow("Workspace##ws", ExIcon::Workspace, "Workspace", false, true, true, "Workspace", {ExInsert::Workspace, 0}).open) {
        if (!filtering) {
            exRow("##cam", ExIcon::Camera, "Camera", false, false, false, "Camera");
            if (ImGui::IsItemClicked()) g_showCameraPanel = true;
            char tbuf[64];
            std::snprintf(tbuf, sizeof(tbuf), "Terrain");
            const ExRow tr = exRow("##terr", ExIcon::Terrain, tbuf, g_showTerrainEditor && g_terrainToolActive, false, false, "Terrain");
            if (tr.clicked) g_showTerrainEditor = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Terrain (%d voxel): pennelli in Modello > Terreno", g_terrain.blockCount());
            const ExRow sp = exRow("##spawn", ExIcon::Spawn, "SpawnLocation", g_selectedSpawn, false, false, "SpawnLocation");
            if (sp.clicked) selectSpawn();
            for (int i = 0; i < (int)g_world.models.size(); ++i) {
                ImGui::PushID(5000 + i);
                const ExRow m = exRow("##mesh", ExIcon::MeshPart, g_world.models[(size_t)i].name.c_str(), i == g_selectedModel,
                                      false, false, "MeshPart");
                if (m.clicked || m.rclicked) { selectModel(i); g_showProps = true; }
                if (ImGui::BeginPopupContextItem("##mctx")) {
                    if (ImGui::MenuItem("Ingrandisci", "F")) studioFocusSelection();
                    if (ImGui::MenuItem("Elimina", "Canc")) deleteSelected();
                    if (ImGui::MenuItem("Proprieta'")) g_showProps = true;
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            for (int gid : partgroups::childGroups(g_world, 0)) exGroupRow(gid);
        }
        for (int i = 0; i < (int)g_world.parts.size(); ++i)
            if (filtering || !partgroups::find(g_world, g_world.parts[(size_t)i].groupId)) exPartRow(i);
        if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType("BLOCKY_PART")) {
            ImGui::Selectable("      (rilascia qui: fuori dai Model)", false);
            studioDropTargetForGroup(0);
        }
        ImGui::TreePop();
    }

    // Players
    if (exServiceRow("Players##svc", ExIcon::Players, "Players", Svc_Players, true)) {
        const ExRow lp = exRow("##lp", ExIcon::Player, "LocalPlayer", g_selectedPlayer && g_selectedPlayerPart < 0, true, false, "Player");
        if (lp.clicked) selectPlayer(-1);
        if (lp.open) {
            for (int i = 0; i < (int)g_player.humanoid.parts.size(); ++i) {
                ImGui::PushID(7000 + i);
                const ExRow bp = exRow("##bp", ExIcon::BodyPart, g_player.humanoid.parts[(size_t)i].name.c_str(),
                                       g_selectedPlayer && g_selectedPlayerPart == i, false, false, "Part");
                if (bp.clicked) selectPlayer(i);
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Rigga il player...")) beginRigPlayer();
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    // Lighting
    if (exServiceRow("Lighting##svc", ExIcon::Lighting, "Lighting", Svc_Lighting, true)) {
        if (exRow("##atm", ExIcon::Atmosphere, "Atmosphere", g_showAtmospherePanel, false, false, "Atmosphere").clicked)
            g_showAtmospherePanel = true;
        if (exRow("##sky", ExIcon::Sky, "Sky", false, false, false, "Sky").clicked) g_showLightingPresets = true;
        ImGui::TreePop();
    }
    exServiceRow("MaterialService##svc", ExIcon::Service, "MaterialService", -1, false);
    if (ImGui::IsItemClicked()) g_showMaterialPalette = true;
    exServiceRow("ReplicatedFirst##svc", ExIcon::Storage, "ReplicatedFirst", -1, false);

    struct SharedSvc { const char* id; const char* name; int svc; SharedService shared; ExIcon icon; };
    const SharedSvc shared[] = {
        {"ReplicatedStorage##svc", "ReplicatedStorage", Svc_ReplicatedStorage, SharedService::ReplicatedStorage, ExIcon::Storage},
        {"ServerScriptService##svc", "ServerScriptService", Svc_ServerScriptService, SharedService::ServerScriptService, ExIcon::Script},
        {"ServerStorage##svc", "ServerStorage", Svc_ServerStorage, SharedService::ServerStorage, ExIcon::Storage},
    };
    for (const auto& s : shared) {
        bool kids = false;
        for (const auto& c : g_world.instances) if (c.service == s.shared) { kids = true; break; }
        if (exServiceRow(s.id, s.icon, s.name, s.svc, kids, false, {ExInsert::Shared, (int)s.shared})) {
            exSharedChildren(s.shared, -1);
            ImGui::TreePop();
        }
    }

    // StarterGui: le etichette e i bottoni a schermo
    if (exServiceRow("StarterGui##svc", ExIcon::Gui, "StarterGui", Svc_StarterGui, true, false, {ExInsert::StarterGui, 0})) {
        if (exRow("##sgui", ExIcon::Gui, "ScreenGui", false, !g_world.labels.empty(), true, "ScreenGui", {ExInsert::StarterGui, 0}).open) {
            for (int i = 0; i < (int)g_world.labels.size(); ++i) {
                const auto& l = g_world.labels[(size_t)i];
                ImGui::PushID(9000 + i);
                if (exRow("##lab", exClassIcon(l.className), l.name.c_str(), i == g_selectedLabel, false, false, l.className.c_str()).clicked)
                    selectLabel(i);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    // StarterPack: i Tool
    if (exServiceRow("StarterPack##svc", ExIcon::Pack, "StarterPack", Svc_StarterPack, !g_world.backpack.empty(), false,
                     {ExInsert::StarterPack, 0})) {
        for (int i = 0; i < (int)g_world.backpack.size(); ++i) {
            ImGui::PushID(11000 + i);
            if (exRow("##tool", ExIcon::Tool, g_world.backpack[(size_t)i].name.c_str(), i == g_selectedBackpack, false, false, "Tool").clicked) {
                clearAllSelection();
                g_selectedBackpack = i;
                g_showProps = true;
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if (exServiceRow("StarterPlayer##svc", ExIcon::Players, "StarterPlayer", Svc_StarterPlayer, true)) {
        if (exRow("##scs", ExIcon::Storage, "StarterCharacterScripts", g_selectedService == Svc_StarterCharacter, false, false,
                  "StarterCharacterScripts").clicked)
            toggleService(Svc_StarterCharacter);
        exRow("##sps", ExIcon::Storage, "StarterPlayerScripts", false, false, false, "StarterPlayerScripts");
        ImGui::TreePop();
    }
    exServiceRow("Teams##svc", ExIcon::Teams, "Teams", -1, false);
    if (ImGui::IsItemClicked()) g_showTeamsEditor = true;
    if (exServiceRow("SoundService##svc", ExIcon::Sound, "SoundService", Svc_SoundService, !g_sounds.sounds.empty(), false,
                     {ExInsert::Sound, 0})) {
        for (int i = 0; i < (int)g_sounds.sounds.size(); ++i) {
            ImGui::PushID(13000 + i);
            const auto& s = g_sounds.sounds[(size_t)i];
            if (exRow("##snd", ExIcon::Sound, s.name.c_str(), i == g_selectedSound, false, false, "Sound").clicked) {
                selectSound(i);
                g_showProps = true;
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    exServiceRow("TextChatService##svc", ExIcon::Chat, "TextChatService", Svc_Chat, false);

    // Gli altri servizi che gli script usano con GetService (in Studio sono nascosti).
    if (exRow("##more", ExIcon::Service, "Altri servizi", false, true, false, "").open) {
        exServiceRow("RunService##svc", ExIcon::Service, "RunService", Svc_RunService, false);
        exServiceRow("TweenService##svc", ExIcon::Service, "TweenService", Svc_TweenService, false);
        exServiceRow("UserInputService##svc", ExIcon::Service, "UserInputService", Svc_UserInputService, false);
        exServiceRow("DataStoreService##svc", ExIcon::Service, "DataStoreService", Svc_DataStoreService, false);
        exServiceRow("CollectionService##svc", ExIcon::Service, "CollectionService", Svc_CollectionService, false);
        exServiceRow("Debris##svc", ExIcon::Service, "Debris", Svc_Debris, false);
        ImGui::TreePop();
    }
    drawExplorerInsertPopup();
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ---- Tasto destro nel viewport --------------------------------------------------------------
// SelectionAndInput.h segna la richiesta (clic destro senza trascinare sopra
// una part); qui si apre lo stesso menu dell'Esploratore dove si e' cliccato.
// (g_viewportCtxRequest / g_viewportCtxPos stanno in SelectionAndInput.h)
static void drawViewportContextMenu() {
    if (g_screen != AppScreen::Studio || g_studioTesting) { g_viewportCtxRequest = false; return; }
    if (g_viewportCtxRequest) {
        g_viewportCtxRequest = false;
        ImGui::OpenPopup("##viewportctx");
        ImGui::SetNextWindowPos(g_viewportCtxPos);
    }
    if (ImGui::BeginPopup("##viewportctx")) {
        drawStudioObjectMenuItems();
        ImGui::EndPopup();
    }
}
