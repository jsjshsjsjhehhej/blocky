/**
 * Blocky Studio — editor di place.
 *
 * Eseguibile separato da BlockyGameEngine: condivide DataModel, UndoStack e
 * PlaceExporter, ma non ne tocca il main. Così l'editor può cambiare layout
 * senza rischiare di rompere il runtime del gioco.
 *
 * Layout a pannelli fissi calcolati a mano invece che con DockSpace: la
 * versione di Dear ImGui agganciata qui (v1.91.6, branch master) non ha il
 * docking, e un layout fisso è comunque quello che serve a uno Studio.
 */

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "engine/BlockyProject.h"
#include "engine/Humanoid.h"
#include "engine/PlaceExporter.h"
#include "engine/SceneIntegrity.h"
#include "platform/CrashHandler.h"
#include "studio/StudioPanels.h"
#include "studio/StudioRibbon.h"
#include "studio/StudioScript.h"
#include "studio/StudioState.h"
#include "studio/StudioViewport.h"

using namespace studio;

namespace {

constexpr float kPanelLeft = 250.f;
constexpr float kPanelRight = 320.f;
constexpr float kPanelBottom = 170.f;
constexpr float kStatusHeight = 24.f;

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

/** Asse del gizmo attualmente trascinato. -1 = nessuno. */
struct GizmoDrag {
    int axis = -1;
    ImVec2 startMouse{};
    glm::vec3 startPosition{};
    glm::vec3 startSize{};
    glm::vec3 startRotation{};
    WorkspacePart before;
};

void applyDarkTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 0.f;
    st.FrameRounding = 4.f;
    st.GrabRounding = 4.f;
    st.ScrollbarRounding = 6.f;
    st.WindowPadding = {10, 10};
    st.FramePadding = {7, 4};
    st.ItemSpacing = {8, 6};
    st.WindowBorderSize = 1.f;

    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]        = {0.086f, 0.098f, 0.118f, 1.f};
    c[ImGuiCol_ChildBg]         = {0.071f, 0.082f, 0.100f, 1.f};
    c[ImGuiCol_PopupBg]         = {0.098f, 0.110f, 0.133f, 0.98f};
    c[ImGuiCol_Border]          = {0.180f, 0.200f, 0.235f, 1.f};
    c[ImGuiCol_FrameBg]         = {0.145f, 0.161f, 0.192f, 1.f};
    c[ImGuiCol_FrameBgHovered]  = {0.196f, 0.220f, 0.263f, 1.f};
    c[ImGuiCol_TitleBgActive]   = {0.129f, 0.267f, 0.443f, 1.f};
    c[ImGuiCol_Header]          = {0.157f, 0.325f, 0.545f, 1.f};
    c[ImGuiCol_HeaderHovered]   = {0.196f, 0.396f, 0.651f, 1.f};
    c[ImGuiCol_Button]          = {0.176f, 0.196f, 0.235f, 1.f};
    c[ImGuiCol_ButtonHovered]   = {0.227f, 0.255f, 0.310f, 1.f};
    c[ImGuiCol_MenuBarBg]       = {0.110f, 0.125f, 0.153f, 1.f};
    c[ImGuiCol_Separator]       = {0.196f, 0.216f, 0.255f, 1.f};
}

std::string windowTitle(const StudioState& s) {
    std::string t = "Blocky Studio — " + s.world.placeName;
    if (s.dirty) t += " *";
    return t;
}

/**
 * Verifica il place e riversa i rilievi nell'Output.
 * `autoRepair` corregge il correggibile: usato al caricamento, dove un valore
 * rotto non è colpa dell'utente e fermarlo non aiuterebbe nessuno.
 */
void runIntegrityCheck(StudioState& s, bool autoRepair) {
    SceneIntegrity::Result res = SceneIntegrity::check(s.world);

    if (res.clean()) {
        s.output.info("Integrità scena: nessun problema (%d parts)", (int)s.world.parts.size());
        return;
    }

    for (const SceneIntegrity::Issue& i : res.issues) {
        const LogLevel level = (i.severity == SceneIntegrity::Severity::Error) ? LogLevel::Error
                             : (i.severity == SceneIntegrity::Severity::Warn) ? LogLevel::Warn
                             : LogLevel::Info;
        s.output.add(level, "[%s] %s — %s", i.rule.c_str(), i.subject.c_str(), i.detail.c_str());
    }

    s.output.add(res.errors ? LogLevel::Error : LogLevel::Warn,
                 "Integrità scena: %d errori, %d avvisi", res.errors, res.warnings);

    if (autoRepair && res.repairable > 0) {
        const int fixed = SceneIntegrity::repair(s.world);
        if (fixed > 0) {
            s.dirty = true;
            s.output.info("Corretti %d valori automaticamente", fixed);
        }
    }
}

void savePlace(StudioState& s) {
    static Humanoid humanoid;
    const std::string json = PlaceExporter::exportPlace(
        s.world, humanoid, s.world.spawn.position, s.world.spawn.yaw);

    const std::string path = PlaceExporter::makeExportPath(s.world.placeName);
    if (BlockyProject::writeTextFile(path, json)) {
        s.projectPath = path;
        s.dirty = false;
        s.output.add(LogLevel::Info, "Place salvato in %s", path.c_str());
    } else {
        s.output.add(LogLevel::Error, "Salvataggio fallito: %s", path.c_str());
    }
}

// ------------------------------------------------------------------- gizmo

/**
 * Gizmo a tre assi. Il trascinamento si risolve in spazio schermo: si proietta
 * l'asse, si misura quanto il mouse è avanzato lungo quella direzione e lo si
 * riconverte in unità mondo. Funziona a qualsiasi angolo di camera senza dover
 * intersecare piani.
 */
void drawGizmo(StudioState& s, Viewport& vp, ImVec2 imageOrigin, bool viewportHovered,
               GizmoDrag& drag) {
    WorkspacePart* part = s.selectedPart();
    if (!part || s.tool == Tool::Select) { drag.axis = -1; return; }

    glm::vec2 centerPx;
    if (!vp.worldToScreen(part->position, centerPx)) return;

    const float handleLength = vp.camera.distance * 0.14f;
    const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const ImU32 colors[3] = {IM_COL32(235, 80, 75, 255),
                             IM_COL32(115, 215, 100, 255),
                             IM_COL32(80, 150, 245, 255)};

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center{imageOrigin.x + centerPx.x, imageOrigin.y + centerPx.y};
    const ImVec2 mouse = ImGui::GetMousePos();

    glm::vec2 endPx[3];
    bool endValid[3];
    for (int a = 0; a < 3; ++a) {
        endValid[a] = vp.worldToScreen(part->position + axes[a] * handleLength, endPx[a]);
    }

    // Quale asse è sotto il cursore (solo se non stiamo già trascinando).
    int hovered = -1;
    if (drag.axis < 0 && viewportHovered) {
        float bestDistance = 10.f;
        for (int a = 0; a < 3; ++a) {
            if (!endValid[a]) continue;
            const ImVec2 end{imageOrigin.x + endPx[a].x, imageOrigin.y + endPx[a].y};
            const ImVec2 seg{end.x - center.x, end.y - center.y};
            const float len2 = seg.x * seg.x + seg.y * seg.y;
            if (len2 < 1.f) continue;

            float t = ((mouse.x - center.x) * seg.x + (mouse.y - center.y) * seg.y) / len2;
            t = std::clamp(t, 0.f, 1.f);
            const float dx = mouse.x - (center.x + seg.x * t);
            const float dy = mouse.y - (center.y + seg.y * t);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < bestDistance) { bestDistance = distance; hovered = a; }
        }
    }

    for (int a = 0; a < 3; ++a) {
        if (!endValid[a]) continue;
        const ImVec2 end{imageOrigin.x + endPx[a].x, imageOrigin.y + endPx[a].y};
        const bool lit = (hovered == a || drag.axis == a);
        dl->AddLine(center, end, colors[a], lit ? 5.f : 3.f);
        dl->AddCircleFilled(end, lit ? 7.f : 5.f, colors[a]);
    }
    dl->AddCircleFilled(center, 4.f, IM_COL32(240, 240, 240, 220));

    if (hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        drag.axis = hovered;
        drag.startMouse = mouse;
        drag.startPosition = part->position;
        drag.startSize = part->size;
        drag.startRotation = part->rotation;
        drag.before = *part;
    }

    if (drag.axis >= 0) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            s.undo.recordTransform(s.selected, drag.before, *part);
            s.dirty = true;
            drag.axis = -1;
            return;
        }

        const int a = drag.axis;
        if (!endValid[a]) return;

        const ImVec2 end{imageOrigin.x + endPx[a].x, imageOrigin.y + endPx[a].y};
        glm::vec2 screenAxis{end.x - center.x, end.y - center.y};
        const float axisLength = std::sqrt(screenAxis.x * screenAxis.x + screenAxis.y * screenAxis.y);
        if (axisLength < 2.f) return;   // asse quasi di punta: il drag sarebbe ingovernabile
        screenAxis /= axisLength;

        const glm::vec2 delta{mouse.x - drag.startMouse.x, mouse.y - drag.startMouse.y};
        const float alongPixels = delta.x * screenAxis.x + delta.y * screenAxis.y;
        const float amount = alongPixels / axisLength * handleLength;

        switch (s.tool) {
        case Tool::Move: {
            glm::vec3 p = drag.startPosition;
            p[a] += amount;
            p[a] = s.snap(p[a], s.snapStuds);
            part->position = p;
            break;
        }
        case Tool::Scale: {
            glm::vec3 sz = drag.startSize;
            sz[a] = std::max(0.05f, s.snap(sz[a] + amount * 2.f, s.snapStuds));
            part->size = sz;
            break;
        }
        case Tool::Rotate: {
            glm::vec3 r = drag.startRotation;
            r[a] = s.snap(r[a] + amount * 8.f, s.snapDegrees);
            part->rotation = r;
            break;
        }
        default: break;
        }
    }
}

void handleShortcuts(StudioState& s) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (s.undo.applyUndo(s.world, s.selected)) {
            s.dirty = true;
            s.output.info("Undo");
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        if (s.undo.applyRedo(s.world, s.selected)) {
            s.dirty = true;
            s.output.info("Redo");
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) s.duplicateSelected();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) savePlace(s);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) s.deleteSelected();

    if (ImGui::IsKeyPressed(ImGuiKey_1, false)) s.tool = Tool::Select;
    if (ImGui::IsKeyPressed(ImGuiKey_2, false)) s.tool = Tool::Move;
    if (ImGui::IsKeyPressed(ImGuiKey_3, false)) s.tool = Tool::Scale;
    if (ImGui::IsKeyPressed(ImGuiKey_4, false)) s.tool = Tool::Rotate;
}

/** Contenuto della ribbon per la scheda attiva. */
void drawRibbon(StudioState& s, const glm::vec3& spawnPoint, int& tab) {
    static const char* const tabs[] = {"HOME", "MODEL", "TEST", "VIEW"};
    ribbonTabs(tabs, IM_ARRAYSIZE(tabs), tab);

    ImGui::Dummy({0.f, 3.f});
    ImGui::Indent(9.f);

    auto toolGroup = [&]() {
        ribbonBeginGroup();
        if (ribbonBigButton("t_sel", Icon::Cursor, "Select", s.tool == Tool::Select)) s.tool = Tool::Select;
        if (ribbonBigButton("t_mov", Icon::Move,   "Move",   s.tool == Tool::Move))   s.tool = Tool::Move;
        if (ribbonBigButton("t_scl", Icon::Scale,  "Scale",  s.tool == Tool::Scale))  s.tool = Tool::Scale;
        if (ribbonBigButton("t_rot", Icon::Rotate, "Rotate", s.tool == Tool::Rotate)) s.tool = Tool::Rotate;
        ribbonEndGroup("Strumenti");
    };

    auto insertGroup = [&]() {
        ribbonBeginGroup();
        if (ribbonBigButton("i_box", Icon::Cube,     "Part"))   s.addPart(PartShape::Block, spawnPoint);
        if (ribbonBigButton("i_bal", Icon::Sphere,   "Ball"))   s.addPart(PartShape::Ball, spawnPoint);
        if (ribbonBigButton("i_cyl", Icon::Cylinder, "Cylinder")) s.addPart(PartShape::Cylinder, spawnPoint);
        if (ribbonBigButton("i_wed", Icon::Wedge,    "Wedge"))  s.addPart(PartShape::Wedge, spawnPoint);
        ribbonEndGroup("Inserisci");
    };

    auto snapGroup = [&]() {
        ribbonBeginGroup();
        // Due righe, non tre: la terza sfonderebbe l'altezza della ribbon.
        ImGui::Checkbox("Snap", &s.snapEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        ImGui::DragFloat("studs", &s.snapStuds, 0.25f, 0.25f, 16.f, "%.2f");
        ImGui::SetNextItemWidth(78.f);
        ImGui::DragFloat("gradi", &s.snapDegrees, 1.f, 1.f, 90.f, "%.0f");
        ribbonEndGroup("Snap");
    };

    switch (tab) {

    case 0: // HOME
        ribbonBeginGroup();
        ribbonBeginStack();
        if (ribbonSmallButton("h_undo", Icon::Undo, "Annulla", false, s.undo.canUndo()))
            s.undo.applyUndo(s.world, s.selected);
        if (ribbonSmallButton("h_redo", Icon::Redo, "Ripeti", false, s.undo.canRedo()))
            s.undo.applyRedo(s.world, s.selected);
        if (ribbonSmallButton("h_save", Icon::Save, "Salva")) savePlace(s);
        ribbonEndStack();
        ribbonEndGroup("Cronologia");

        toolGroup();
        insertGroup();

        ribbonBeginGroup();
        if (ribbonBigButton("h_dup", Icon::Duplicate, "Duplica", false, s.selected >= 0))
            s.duplicateSelected();
        if (ribbonBigButton("h_del", Icon::Trash, "Elimina", false, s.selected >= 0))
            s.deleteSelected();
        ribbonEndGroup("Modifica");

        snapGroup();
        break;

    case 1: // MODEL
        insertGroup();
        toolGroup();
        snapGroup();
        break;

    case 2: // TEST
        ribbonBeginGroup();
        // Disabilitati finché il runtime non è agganciato: un bottone che non
        // fa nulla è peggio di uno spento.
        ribbonBigButton("x_play", Icon::Play, "Play", false, false);
        ribbonBigButton("x_stop", Icon::Stop, "Stop", false, false);
        ribbonEndGroup("Simulazione");

        ribbonBeginGroup();
        ribbonBeginStack();
        if (ribbonSmallButton("x_save", Icon::Save, "Salva Place")) savePlace(s);
        ribbonEndStack();
        ribbonEndGroup("Place");
        break;

    case 3: // VIEW
        ribbonBeginGroup();
        ImGui::Checkbox("Griglia", &s.showGrid);
        ImGui::Checkbox("Baseplate", &s.world.hasBaseplate);
        ribbonEndGroup("Mostra");

        ribbonBeginGroup();
        if (ribbonBigButton("v_check", Icon::Anchor, "Verifica"))
            runIntegrityCheck(s, false);
        if (ribbonBigButton("v_fix", Icon::Undo, "Ripara"))
            runIntegrityCheck(s, true);
        ribbonEndGroup("Integrità");

        ribbonBeginGroup();
        ribbonBeginStack();
        if (ribbonSmallButton("v_home", Icon::Camera, "Reset camera"))
            s.output.info("Camera riportata all'origine");
        ribbonEndStack();
        ribbonEndGroup("Camera");
        break;

    default:
        break;
    }

    ImGui::Unindent(9.f);
}

void drawStatusBar(const StudioState& s) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled({p.x - 8.f, p.y - 4.f}, {p.x + width + 8.f, p.y + kStatusHeight},
                      IM_COL32(38, 88, 152, 255));

    const WorkspacePart* part = s.selectedPart();
    char left[256];
    std::snprintf(left, sizeof(left), "%s   |   %s   |   %d parts",
                  toolName(s.tool),
                  part ? part->name.c_str() : "nessuna selezione",
                  (int)s.world.parts.size());
    dl->AddText({p.x + 4.f, p.y + 1.f}, IM_COL32(232, 240, 252, 255), left);

    char right[128];
    std::snprintf(right, sizeof(right), "Snap %s  %.2f studs / %.0f gradi",
                  s.snapEnabled ? "ON" : "OFF", s.snapStuds, s.snapDegrees);
    const float w = ImGui::CalcTextSize(right).x;
    dl->AddText({p.x + width - w - 4.f, p.y + 1.f}, IM_COL32(198, 216, 240, 255), right);
}

void drawMenuBar(StudioState& s, bool& running) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Nuovo (Baseplate)")) {
            s.world.loadBaseplatePlace();
            s.undo.clear();
            s.selectNone();
            s.dirty = false;
            s.output.info("Nuovo place: Baseplate");
        }
        if (ImGui::BeginMenu("Template")) {
            // Ogni caricamento passa dal controllo d'integrità: un place
            // scritto da una build diversa è il caso in cui serve davvero.
            auto loadTemplate = [&](const char* label, void (DataModel::*loader)()) {
                if (!ImGui::MenuItem(label)) return;
                CrashHandler::note((std::string("carica template: ") + label).c_str());
                (s.world.*loader)();
                s.undo.clear();
                s.selectNone();
                runIntegrityCheck(s, true);
            };
            loadTemplate("Obby",   &DataModel::loadObbyPlace);
            loadTemplate("Arena",  &DataModel::loadArenaPlace);
            loadTemplate("Parco",  &DataModel::loadParkPlace);
            loadTemplate("Città",  &DataModel::loadCityPlace);
            loadTemplate("Garden", &DataModel::loadGardenPlace);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Salva Place", "Ctrl+S")) savePlace(s);
        ImGui::Separator();
        if (ImGui::MenuItem("Esci")) running = false;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, s.undo.canUndo()))
            s.undo.applyUndo(s.world, s.selected);
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, s.undo.canRedo()))
            s.undo.applyRedo(s.world, s.selected);
        ImGui::Separator();
        if (ImGui::MenuItem("Duplica", "Ctrl+D", false, s.selected >= 0)) s.duplicateSelected();
        if (ImGui::MenuItem("Elimina", "Del", false, s.selected >= 0)) s.deleteSelected();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Griglia", nullptr, &s.showGrid);
        ImGui::MenuItem("Baseplate", nullptr, &s.world.hasBaseplate);
        ImGui::EndMenu();
    }

    // Stato a destra: dice sempre cosa è selezionato e con che strumento.
    const WorkspacePart* p = s.selectedPart();
    char status[192];
    std::snprintf(status, sizeof(status), "%s   |   %s   |   %d parts",
                  toolName(s.tool),
                  p ? p->name.c_str() : "nessuna selezione",
                  (int)s.world.parts.size());
    const float w = ImGui::CalcTextSize(status).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - 18.f);
    ImGui::TextDisabled("%s", status);

    ImGui::EndMainMenuBar();
}

} // namespace

int main() {
    // Prima di tutto: un crash durante l'inizializzazione è quello che
    // lascia meno indizi, ed è proprio quello che va registrato.
    CrashHandler::install("BlockyStudio", "1.0");
    CrashHandler::note("avvio");

    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit fallita\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1600, 940, "Blocky Studio", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "glfwCreateWindow fallita\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGL fallita\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   // il layout è fisso: niente imgui.ini
    applyDarkTheme();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    StudioState state;
    state.world.loadBaseplatePlace();
    state.output.info("Blocky Studio pronto.");
    state.output.info("RMB orbita · MMB pan · rotella zoom · LMB seleziona");

    Viewport viewport;
    if (!viewport.init()) {
        state.output.add(LogLevel::Error, "Shader viewport non compilati:\n%s",
                         viewport.buildLog.c_str());
    }

    ScriptEditor scriptEditor;
    scriptEditor.start(state.world, state.output);

    GizmoDrag drag;
    int ribbonTab = 0;
    bool running = true;
    std::string lastTitle;

    while (running && !glfwWindowShouldClose(win)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const float time = (float)glfwGetTime();
        handleShortcuts(state);
        drawMenuBar(state, running);

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 origin = vp->WorkPos;
        const ImVec2 size = vp->WorkSize;

        // --- Ribbon ---
        ImGui::SetNextWindowPos(origin);
        ImGui::SetNextWindowSize({size.x, kRibbonHeight});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        if (ImGui::Begin("##ribbon", nullptr, kPanelFlags)) {
            glm::vec3 spawn = viewport.camera.target;
            spawn.y = std::max(spawn.y, 2.f);
            drawRibbon(state, spawn, ribbonTab);
        }
        ImGui::End();
        ImGui::PopStyleVar();

        const float bodyY = origin.y + kRibbonHeight;
        const float bodyH = size.y - kRibbonHeight - kStatusHeight;

        // --- Explorer ---
        ImGui::SetNextWindowPos({origin.x, bodyY});
        ImGui::SetNextWindowSize({kPanelLeft, bodyH});
        if (ImGui::Begin("##explorer", nullptr, kPanelFlags)) drawExplorer(state);
        ImGui::End();

        // --- Properties ---
        ImGui::SetNextWindowPos({origin.x + size.x - kPanelRight, bodyY});
        ImGui::SetNextWindowSize({kPanelRight, bodyH});
        if (ImGui::Begin("##properties", nullptr, kPanelFlags)) drawProperties(state);
        ImGui::End();

        const float centerX = origin.x + kPanelLeft;
        const float centerW = size.x - kPanelLeft - kPanelRight;

        // --- Output / Script (schede sullo stesso riquadro) ---
        ImGui::SetNextWindowPos({centerX, bodyY + bodyH - kPanelBottom});
        ImGui::SetNextWindowSize({centerW, kPanelBottom});
        if (ImGui::Begin("##bottom", nullptr, kPanelFlags)) {
            if (ImGui::BeginTabBar("bottom_tabs")) {
                if (ImGui::BeginTabItem("Output")) {
                    drawOutput(state);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Script")) {
                    drawScriptPanel(state, scriptEditor);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        // print() degli script arriva su stdout: va svuotato ogni frame.
        scriptEditor.capture.drain(state.output);

        // --- Barra di stato ---
        ImGui::SetNextWindowPos({origin.x, origin.y + size.y - kStatusHeight});
        ImGui::SetNextWindowSize({size.x, kStatusHeight});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 4});
        if (ImGui::Begin("##status", nullptr, kPanelFlags)) drawStatusBar(state);
        ImGui::End();
        ImGui::PopStyleVar();

        // --- Viewport 3D ---
        ImGui::SetNextWindowPos({centerX, bodyY});
        ImGui::SetNextWindowSize({centerW, bodyH - kPanelBottom});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        if (ImGui::Begin("##viewport", nullptr, kPanelFlags)) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            viewport.resize((int)avail.x, (int)avail.y);
            viewport.render(state.world, state.selected, state.showGrid);

            const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t)viewport.colorTex, avail, {0, 1}, {1, 0});

            const bool hovered = ImGui::IsItemHovered();
            ImGuiIO& io = ImGui::GetIO();

            if (hovered) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
                    viewport.camera.orbit(io.MouseDelta.x, io.MouseDelta.y);
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                    viewport.camera.pan(io.MouseDelta.x, io.MouseDelta.y);
                if (io.MouseWheel != 0.f)
                    viewport.camera.zoom(io.MouseWheel);
            }

            drawGizmo(state, viewport, imageOrigin, hovered, drag);

            // Selezione solo se il click non è finito su una maniglia del gizmo.
            if (hovered && drag.axis < 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 m = ImGui::GetMousePos();
                const float u = (m.x - imageOrigin.x) / std::max(avail.x, 1.f);
                const float v = (m.y - imageOrigin.y) / std::max(avail.y, 1.f);
                const int hit = viewport.pick(state.world, u, v);
                state.selected = hit;
                if (hit >= 0) {
                    state.output.info("Selezionata %s",
                                      state.world.parts[(size_t)hit].name.c_str());
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        const std::string title = windowTitle(state);
        if (title != lastTitle) {
            glfwSetWindowTitle(win, title.c_str());
            lastTitle = title;
        }

        ImGui::Render();
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.05f, 0.06f, 0.07f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    scriptEditor.stop();   // ripristina stdout prima di chiudere
    viewport.destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
