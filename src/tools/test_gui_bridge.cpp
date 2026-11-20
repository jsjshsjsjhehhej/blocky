// GUI in stile Roblox (ScreenGui > Frame > TextButton fatti dagli script): le
// label del motore prendono Size / Position (scala + pixel), AnchorPoint, colori,
// TextSize e Parent come Roblox, si vedono solo dentro una ScreenGui accesa in
// PlayerGui, e i clic tornano come MouseButton1Click / Activated.
// (La regola "si vede" e' la stessa di uiLabelShown in WorldLabelsUi.h.)

#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

/** La ScreenGui da cui viene la label (per saltare quelle delle CoreScripts, src/engine/lua/core). */
static const WorkspaceLabel* rootGui(const DataModel& dm, const WorkspaceLabel* l) {
    for (int guard = 0; l && guard < 64; ++guard) {
        if (l->className == "ScreenGui") return l;
        if (l->parentIndex < 0 || l->parentIndex >= (int)dm.labels.size()) return nullptr;
        l = &dm.labels[(size_t)l->parentIndex];
    }
    return nullptr;
}

static const WorkspaceLabel* findUi(const DataModel& dm, const char* cls, int* index = nullptr) {
    for (size_t i = 0; i < dm.labels.size(); ++i) {
        const WorkspaceLabel& l = dm.labels[i];
        if (!l.uiLayout || l.className != cls) continue;
        const WorkspaceLabel* r = rootGui(dm, &l);
        // GUI di una CoreScript (core/*.luau): DisplayOrder 100. (Il Name non si puo' usare:
        // per il motore resta "ScreenGui", docs/limiti_trovati.md n. 16.)
        if (r && r->zIndex >= 100) continue;
        if (index) *index = (int)i;
        return &l;
    }
    return nullptr;
}

static bool shown(const DataModel& dm, const WorkspaceLabel* l) {
    for (int guard = 0; l && guard < 64; ++guard) {
        if (!l->visible) return false;
        if (l->className == "ScreenGui") return l->uiAttached;
        if (l->parentIndex < 0 || l->parentIndex >= (int)dm.labels.size()) return false;
        l = &dm.labels[(size_t)l->parentIndex];
    }
    return false;
}

static bool closeTo(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
    std::printf("=== GUI in stile Roblox ===\n");
    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    lua.runIsClient = true;
    lua.runIsServer = true;
    if (!lua.init(&dm)) { std::printf("init: %s\n", lua.lastError.c_str()); return 1; }
    const bool ok = lua.runString(R"(
        lonely = Instance.new("TextLabel")   -- fuori da ogni ScreenGui: non si vede
        local gui = Instance.new("ScreenGui")
        gui.Parent = game:GetService("Players").LocalPlayer:WaitForChild("PlayerGui")
        local frame = Instance.new("Frame")
        frame.Size = UDim2.new(0.5, 0, 0.25, 20)
        frame.Position = UDim2.new(0.5, 0, 0.5, 0)
        frame.AnchorPoint = Vector2.new(0.5, 0.5)
        frame.BackgroundColor3 = Color3.new(0.2, 0.4, 0.8)
        frame.Parent = gui
        local button = Instance.new("TextButton")
        button.Size = UDim2.new(1, -20, 0, 40)
        button.Position = UDim2.new(0, 10, 0, 10)
        button.Text = "Gioca"
        button.TextColor3 = Color3.new(1, 1, 1)
        button.TextSize = 28
        button.Parent = frame
        clicks = 0
        button.MouseButton1Click:Connect(function() clicks += 1 end)
        activated = 0
        button.Activated:Connect(function() activated += 1 end)
        theButton = button
        theFrame = frame
        theGui = gui
        readSize = frame.Size
        readOk = readSize.X.Scale == 0.5 and readSize.Y.Offset == 20 and frame.AnchorPoint.X == 0.5
                 and math.abs(button.TextColor3.R - 1) < 1e-3 and button.TextSize == 28
    )");
    if (!ok) std::printf("  script: %s\n", lua.lastError.c_str());

    int fi = -1;
    const WorkspaceLabel* f = findUi(dm, "Frame", &fi);
    const WorkspaceLabel* b = findUi(dm, "TextButton");
    check(f && b, "Frame e TextButton sono label del motore");
    if (f && b) {
        check(closeTo(f->sizeScaleX, 0.5f) && closeTo(f->sizeScaleY, 0.25f) && closeTo(f->sizeH, 20.f), "Size del Frame (scala + pixel)");
        check(closeTo(f->anchorX, 0.5f) && closeTo(f->anchorPointX, 0.5f) && closeTo(f->anchorPointY, 0.5f), "Position e AnchorPoint");
        check(closeTo(f->bgColor.y, 0.4f) && closeTo(f->bgColor.z, 0.8f), "BackgroundColor3");
        check(b->parentIndex == fi, "il bottone sta dentro il Frame (padre giusto)");
        check(b->text == "Gioca" && closeTo(b->sizeScaleX, 1.f) && closeTo(b->sizeW, -20.f) && closeTo(b->fontScale, 2.f),
              "testo, misura e TextSize del bottone");
        check(shown(dm, f) && shown(dm, b), "dentro la ScreenGui in PlayerGui: si vedono");
    }
    check(lua.evalWatch("readOk") == "true", "gli script rileggono Size, AnchorPoint, TextColor3, TextSize");
    const WorkspaceLabel* lonely = findUi(dm, "TextLabel");
    check(lonely && !shown(dm, lonely), "una TextLabel fuori dalle ScreenGui non va sullo schermo");

    if (b) lua.fireGuiButtonActivated(b->name);   // come fa il motore quando premi il bottone
    check(lua.evalWatch("clicks") == "1" && lua.evalWatch("activated") == "1", "il clic arriva come MouseButton1Click e Activated");

    // Rifiniture: UICorner / UIStroke (Instance Luau, il disegno le riceve), testo, ordine.
    lua.runString(R"(
        local corner = Instance.new("UICorner")
        corner.CornerRadius = UDim.new(0, 12)
        corner.Parent = theButton
        local stroke = Instance.new("UIStroke", theButton)
        stroke.Thickness = 3
        stroke.Color = Color3.new(1, 0, 0)
        theButton.TextScaled = true
        theButton.TextXAlignment = Enum.TextXAlignment.Left
        theButton.ZIndex = 5
        polishOk = theButton.TextScaled == true and theButton.TextXAlignment == Enum.TextXAlignment.Left
                   and theButton.ZIndex == 5 and corner.CornerRadius.Offset == 12 and stroke.Thickness == 3
    )");
    int bIdx = -1;
    b = findUi(dm, "TextButton", &bIdx);
    const WorkspaceLabel* corner = nullptr;
    const WorkspaceLabel* stroke = nullptr;
    for (const auto& l : dm.labels) {
        if (l.parentIndex != bIdx) continue;
        if (l.className == "UICorner") corner = &l;
        if (l.className == "UIStroke") stroke = &l;
    }
    check(corner && closeTo(corner->sizeW, 12.f), "UICorner arriva al disegno (raggio 12 sul bottone)");
    check(stroke && closeTo(stroke->sizeW, 3.f) && closeTo(stroke->color.x, 1.f) && closeTo(stroke->color.y, 0.f), "UIStroke arriva al disegno (spessore e colore)");
    check(b && b->textScaled && b->textAlignX == 0 && b->zIndex == 5, "TextScaled, TextXAlignment, ZIndex");
    check(lua.evalWatch("polishOk") == "true", "gli script rileggono tutto");

    lua.runString("theButton.Text = 'Esci' theFrame.Visible = false");
    b = findUi(dm, "TextButton");
    f = findUi(dm, "Frame");
    check(b && b->text == "Esci", "Text cambiato dallo script");
    check(f && b && !shown(dm, f) && !shown(dm, b), "Frame nascosto: sparisce anche il bottone dentro");
    lua.runString("theFrame.Visible = true theGui.Enabled = false");
    f = findUi(dm, "Frame");
    check(f && !shown(dm, f), "ScreenGui.Enabled = false nasconde tutto");
    lua.runString("theGui.Enabled = true theButton.Parent = nil");
    b = findUi(dm, "TextButton");
    f = findUi(dm, "Frame");
    check(f && shown(dm, f) && b && !shown(dm, b), "tolto dal Frame (Parent = nil): non si vede piu'");

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
