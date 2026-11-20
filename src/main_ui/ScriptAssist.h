// Aiuti dell'editor degli script, come lo Script Editor di Roblox Studio:
//   - completamento automatico (API Roblox/Luau, servizi, Enum, variabili dello
//     script) mentre scrivi: frecce per scegliere, Tab/Invio per inserire, Esc chiude
//   - errori di sintassi sottolineati in rosso mentre scrivi (compilatore Luau)
//   - Trova / Sostituisci (Ctrl+F / Ctrl+H), Vai alla riga (Ctrl+G),
//     Commenta / togli commento (Ctrl+/ o Ctrl+K)
//   - barra di stato: riga, colonna, errori
// Incluso da LuaSyntaxEditor.h: lavora sul TextEditor di ogni LuaScriptEditorState.

#include "imgui_internal.h"
#include <Luau/Compiler.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <unordered_set>
#include <string>
#include <vector>

namespace sa {

// ---- Dizionario dell'API -----------------------------------------------------------------------

struct Entry {
    const char* name;
    const char* kind;   // "keyword", "global", "function", "property", "method", "event", "enum", "service"
};

static const Entry kKeywords[] = {
    {"and", "keyword"}, {"break", "keyword"}, {"do", "keyword"}, {"else", "keyword"}, {"elseif", "keyword"},
    {"end", "keyword"}, {"false", "keyword"}, {"for", "keyword"}, {"function", "keyword"}, {"if", "keyword"},
    {"in", "keyword"}, {"local", "keyword"}, {"nil", "keyword"}, {"not", "keyword"}, {"or", "keyword"},
    {"repeat", "keyword"}, {"return", "keyword"}, {"then", "keyword"}, {"true", "keyword"}, {"until", "keyword"},
    {"while", "keyword"}, {"continue", "keyword"}, {"export", "keyword"}, {"type", "keyword"},
};

static const Entry kGlobals[] = {
    {"game", "global"}, {"workspace", "global"}, {"script", "global"}, {"Instance", "global"}, {"Vector3", "global"},
    {"Vector2", "global"}, {"CFrame", "global"}, {"Color3", "global"}, {"BrickColor", "global"}, {"UDim", "global"},
    {"UDim2", "global"}, {"Enum", "global"}, {"task", "global"}, {"math", "global"}, {"string", "global"},
    {"table", "global"}, {"coroutine", "global"}, {"os", "global"}, {"debug", "global"}, {"utf8", "global"},
    {"TweenInfo", "global"}, {"NumberRange", "global"}, {"NumberSequence", "global"}, {"ColorSequence", "global"},
    {"Ray", "global"}, {"RaycastParams", "global"}, {"OverlapParams", "global"}, {"Random", "global"},
    {"print", "function"}, {"warn", "function"}, {"error", "function"}, {"assert", "function"}, {"pairs", "function"},
    {"ipairs", "function"}, {"next", "function"}, {"select", "function"}, {"tostring", "function"},
    {"tonumber", "function"}, {"type", "function"}, {"typeof", "function"}, {"require", "function"},
    {"pcall", "function"}, {"xpcall", "function"}, {"setmetatable", "function"}, {"getmetatable", "function"},
    {"rawget", "function"}, {"rawset", "function"}, {"rawequal", "function"}, {"unpack", "function"},
    {"wait", "function"}, {"spawn", "function"}, {"delay", "function"}, {"tick", "function"}, {"time", "function"},
};

static const char* kServices[] = {
    "Players", "Workspace", "Lighting", "ReplicatedStorage", "ReplicatedFirst", "ServerStorage",
    "ServerScriptService", "StarterGui", "StarterPack", "StarterPlayer", "SoundService", "Teams",
    "RunService", "UserInputService", "TweenService", "Debris", "CollectionService", "DataStoreService",
    "HttpService", "PathfindingService", "MarketplaceService", "TextChatService", "ContextActionService",
    "BadgeService", "ProximityPromptService", "PhysicsService",
};

struct Members {
    const char* receiver;   // "math", "Vector3", "game", "Players"... o "*part" / "*signal" / "*instance"
    std::vector<Entry> list;
};

static const std::vector<Members>& members() {
    static const std::vector<Members> m = {
        {"task", {{"wait", "function"}, {"spawn", "function"}, {"delay", "function"}, {"defer", "function"}, {"cancel", "function"}}},
        {"math", {{"abs", "function"}, {"floor", "function"}, {"ceil", "function"}, {"sqrt", "function"}, {"sin", "function"},
                  {"cos", "function"}, {"tan", "function"}, {"asin", "function"}, {"acos", "function"}, {"atan", "function"},
                  {"atan2", "function"}, {"random", "function"}, {"randomseed", "function"}, {"clamp", "function"},
                  {"min", "function"}, {"max", "function"}, {"huge", "property"}, {"pi", "property"}, {"rad", "function"},
                  {"deg", "function"}, {"noise", "function"}, {"sign", "function"}, {"round", "function"}, {"exp", "function"},
                  {"log", "function"}, {"pow", "function"}, {"fmod", "function"}, {"modf", "function"}}},
        {"string", {{"format", "function"}, {"sub", "function"}, {"len", "function"}, {"upper", "function"},
                    {"lower", "function"}, {"find", "function"}, {"gsub", "function"}, {"match", "function"},
                    {"gmatch", "function"}, {"rep", "function"}, {"reverse", "function"}, {"split", "function"},
                    {"byte", "function"}, {"char", "function"}}},
        {"table", {{"insert", "function"}, {"remove", "function"}, {"find", "function"}, {"sort", "function"},
                   {"concat", "function"}, {"clear", "function"}, {"clone", "function"}, {"freeze", "function"},
                   {"isfrozen", "function"}, {"unpack", "function"}, {"pack", "function"}, {"create", "function"},
                   {"move", "function"}}},
        {"coroutine", {{"create", "function"}, {"resume", "function"}, {"yield", "function"}, {"wrap", "function"},
                       {"status", "function"}, {"running", "function"}, {"isyieldable", "function"}, {"close", "function"}}},
        {"os", {{"time", "function"}, {"clock", "function"}, {"date", "function"}, {"difftime", "function"}}},
        {"Instance", {{"new", "function"}}},
        {"Vector3", {{"new", "function"}, {"zero", "property"}, {"one", "property"}, {"xAxis", "property"},
                     {"yAxis", "property"}, {"zAxis", "property"}, {"FromNormalId", "function"}}},
        {"Vector2", {{"new", "function"}, {"zero", "property"}, {"one", "property"}}},
        {"CFrame", {{"new", "function"}, {"Angles", "function"}, {"lookAt", "function"}, {"fromEulerAnglesXYZ", "function"},
                    {"fromOrientation", "function"}, {"fromAxisAngle", "function"}, {"identity", "property"}}},
        {"Color3", {{"new", "function"}, {"fromRGB", "function"}, {"fromHSV", "function"}, {"fromHex", "function"}}},
        {"BrickColor", {{"new", "function"}, {"random", "function"}, {"Red", "function"}, {"Blue", "function"},
                        {"Green", "function"}, {"White", "function"}, {"Black", "function"}, {"Yellow", "function"}}},
        {"UDim2", {{"new", "function"}, {"fromScale", "function"}, {"fromOffset", "function"}}},
        {"UDim", {{"new", "function"}}},
        {"TweenInfo", {{"new", "function"}}},
        {"Random", {{"new", "function"}}},
        {"RaycastParams", {{"new", "function"}}},
        {"Enum", {{"KeyCode", "enum"}, {"Material", "enum"}, {"EasingStyle", "enum"}, {"EasingDirection", "enum"},
                  {"PartType", "enum"}, {"HumanoidStateType", "enum"}, {"UserInputType", "enum"}, {"RaycastFilterType", "enum"},
                  {"NormalId", "enum"}, {"SortOrder", "enum"}, {"Font", "enum"}, {"ContextActionResult", "enum"}}},
        {"Enum.KeyCode", {{"W", "enum"}, {"A", "enum"}, {"S", "enum"}, {"D", "enum"}, {"E", "enum"}, {"F", "enum"},
                          {"Q", "enum"}, {"R", "enum"}, {"Space", "enum"}, {"LeftShift", "enum"}, {"LeftControl", "enum"},
                          {"Return", "enum"}, {"Escape", "enum"}, {"Tab", "enum"}, {"One", "enum"}, {"Two", "enum"},
                          {"Three", "enum"}, {"Up", "enum"}, {"Down", "enum"}, {"Left", "enum"}, {"Right", "enum"},
                          {"ButtonA", "enum"}, {"ButtonB", "enum"}, {"ButtonX", "enum"}, {"ButtonY", "enum"}}},
        {"Enum.Material", {{"Plastic", "enum"}, {"SmoothPlastic", "enum"}, {"Neon", "enum"}, {"Metal", "enum"},
                           {"Wood", "enum"}, {"Concrete", "enum"}, {"Grass", "enum"}, {"Glass", "enum"}, {"Ice", "enum"},
                           {"Brick", "enum"}, {"ForceField", "enum"}, {"Fabric", "enum"}, {"Marble", "enum"},
                           {"Sand", "enum"}, {"Slate", "enum"}, {"DiamondPlate", "enum"}, {"CorrodedMetal", "enum"},
                           {"Granite", "enum"}, {"Asphalt", "enum"}}},
        {"Enum.EasingStyle", {{"Linear", "enum"}, {"Sine", "enum"}, {"Quad", "enum"}, {"Cubic", "enum"}, {"Quart", "enum"},
                              {"Quint", "enum"}, {"Exponential", "enum"}, {"Circular", "enum"}, {"Elastic", "enum"},
                              {"Back", "enum"}, {"Bounce", "enum"}}},
        {"Enum.EasingDirection", {{"In", "enum"}, {"Out", "enum"}, {"InOut", "enum"}}},
        {"Enum.PartType", {{"Block", "enum"}, {"Ball", "enum"}, {"Cylinder", "enum"}, {"Wedge", "enum"}}},
        {"Enum.UserInputType", {{"MouseButton1", "enum"}, {"MouseButton2", "enum"}, {"Keyboard", "enum"}, {"Touch", "enum"},
                                {"MouseMovement", "enum"}, {"Gamepad1", "enum"}}},
        {"Enum.HumanoidStateType", {{"Running", "enum"}, {"Jumping", "enum"}, {"Freefall", "enum"}, {"Landed", "enum"},
                                    {"Dead", "enum"}, {"Climbing", "enum"}, {"Swimming", "enum"}, {"Seated", "enum"}}},
        {"Enum.RaycastFilterType", {{"Exclude", "enum"}, {"Include", "enum"}}},
        {"game", {{"GetService", "method"}, {"Players", "service"}, {"Workspace", "service"}, {"Lighting", "service"},
                  {"ReplicatedStorage", "service"}, {"ServerStorage", "service"}, {"ServerScriptService", "service"},
                  {"StarterGui", "service"}, {"SoundService", "service"}, {"Teams", "service"}, {"PlaceId", "property"},
                  {"JobId", "property"}, {"Loaded", "event"}, {"IsLoaded", "method"}}},
        {"workspace", {{"Gravity", "property"}, {"CurrentCamera", "property"}, {"Terrain", "property"}, {"Raycast", "method"},
                       {"GetPartBoundsInBox", "method"}, {"GetPartsInPart", "method"}, {"GetServerTimeNow", "method"}}},
        {"Players", {{"LocalPlayer", "property"}, {"PlayerAdded", "event"}, {"PlayerRemoving", "event"}, {"GetPlayers", "method"},
                     {"GetPlayerFromCharacter", "method"}, {"MaxPlayers", "property"}}},
        {"RunService", {{"Heartbeat", "event"}, {"RenderStepped", "event"}, {"Stepped", "event"}, {"IsServer", "method"},
                        {"IsClient", "method"}, {"IsStudio", "method"}, {"BindToRenderStep", "method"}}},
        {"UserInputService", {{"InputBegan", "event"}, {"InputEnded", "event"}, {"InputChanged", "event"},
                              {"IsKeyDown", "method"}, {"TouchEnabled", "property"}, {"KeyboardEnabled", "property"},
                              {"MouseBehavior", "property"}, {"GetMouseLocation", "method"}}},
        {"TweenService", {{"Create", "method"}}},
        {"Debris", {{"AddItem", "method"}}},
        {"CollectionService", {{"GetTagged", "method"}, {"AddTag", "method"}, {"RemoveTag", "method"}, {"HasTag", "method"},
                               {"GetInstanceAddedSignal", "method"}, {"GetInstanceRemovedSignal", "method"}}},
        {"DataStoreService", {{"GetDataStore", "method"}, {"GetOrderedDataStore", "method"}}},
        {"HttpService", {{"JSONEncode", "method"}, {"JSONDecode", "method"}, {"GenerateGUID", "method"}, {"GetAsync", "method"},
                         {"PostAsync", "method"}}},
        {"Lighting", {{"ClockTime", "property"}, {"Brightness", "property"}, {"Ambient", "property"},
                      {"OutdoorAmbient", "property"}, {"FogStart", "property"}, {"FogEnd", "property"}, {"FogColor", "property"},
                      {"GlobalShadows", "property"}, {"TimeOfDay", "property"}}},
        {"*instance", {{"FindFirstChild", "method"}, {"WaitForChild", "method"}, {"GetChildren", "method"},
                       {"GetDescendants", "method"}, {"Destroy", "method"}, {"Clone", "method"}, {"IsA", "method"},
                       {"FindFirstChildOfClass", "method"}, {"FindFirstChildWhichIsA", "method"},
                       {"FindFirstAncestor", "method"}, {"FindFirstAncestorOfClass", "method"}, {"GetAttribute", "method"},
                       {"SetAttribute", "method"}, {"GetAttributes", "method"}, {"GetPropertyChangedSignal", "method"},
                       {"ClearAllChildren", "method"}, {"IsDescendantOf", "method"}, {"GetFullName", "method"},
                       {"PivotTo", "method"}, {"GetPivot", "method"}, {"MoveTo", "method"}, {"TakeDamage", "method"},
                       {"LoadAnimation", "method"}, {"Play", "method"}, {"Stop", "method"}, {"Fire", "method"},
                       {"FireServer", "method"}, {"FireClient", "method"}, {"FireAllClients", "method"},
                       {"InvokeServer", "method"}, {"Kick", "method"}, {"GetMouse", "method"}, {"ApplyImpulse", "method"},
                       {"SetPrimaryPartCFrame", "method"}, {"Connect", "method"}, {"Once", "method"}, {"Wait", "method"},
                       {"Disconnect", "method"}}},
        {"*property", {{"Name", "property"}, {"Parent", "property"}, {"ClassName", "property"}, {"Position", "property"},
                       {"Size", "property"}, {"CFrame", "property"}, {"Orientation", "property"}, {"Anchored", "property"},
                       {"CanCollide", "property"}, {"CanTouch", "property"}, {"Transparency", "property"},
                       {"Color", "property"}, {"BrickColor", "property"}, {"Material", "property"},
                       {"Reflectance", "property"}, {"AssemblyLinearVelocity", "property"}, {"Massless", "property"},
                       {"Shape", "property"}, {"Locked", "property"}, {"Touched", "event"}, {"TouchEnded", "event"},
                       {"Changed", "event"}, {"ChildAdded", "event"}, {"ChildRemoved", "event"}, {"Destroying", "event"},
                       {"AncestryChanged", "event"}, {"Humanoid", "property"}, {"Health", "property"},
                       {"MaxHealth", "property"}, {"WalkSpeed", "property"}, {"JumpPower", "property"},
                       {"Died", "event"}, {"HumanoidRootPart", "property"}, {"PrimaryPart", "property"},
                       {"Character", "property"}, {"CharacterAdded", "event"}, {"UserId", "property"},
                       {"DisplayName", "property"}, {"Team", "property"}, {"leaderstats", "property"},
                       {"Value", "property"}, {"Text", "property"}, {"TextColor3", "property"},
                       {"BackgroundColor3", "property"}, {"Visible", "property"}, {"MouseButton1Click", "event"},
                       {"Activated", "event"}, {"Enabled", "property"}, {"OnServerEvent", "event"},
                       {"OnClientEvent", "event"}, {"OnServerInvoke", "property"}, {"Event", "event"},
                       {"Triggered", "event"}, {"MouseClick", "event"}, {"Volume", "property"},
                       {"Looped", "property"}, {"Playing", "property"}, {"SoundId", "property"},
                       {"X", "property"}, {"Y", "property"}, {"Z", "property"}, {"Magnitude", "property"},
                       {"Unit", "property"}, {"LookVector", "property"}, {"RightVector", "property"},
                       {"UpVector", "property"}, {"KeyCode", "property"}, {"UserInputType", "property"}}},
    };
    return m;
}

static const Members* findMembers(const std::string& receiver) {
    for (const auto& m : members())
        if (receiver == m.receiver) return &m;
    return nullptr;
}

// ---- Stato per ogni editor ---------------------------------------------------------------------

struct Item {
    std::string text;
    std::string kind;
};

struct State {
    // completamento
    bool open = false;
    std::vector<Item> items;
    int sel = 0;
    int wordStartCol = 0;   // colonna (visiva) dove comincia la parola da completare
    // errori
    double lastEditTime = 0.0;
    bool checkPending = true;
    int errLine = 0;
    std::string errMsg;
    // trova / sostituisci / vai alla riga
    bool findOpen = false, replaceMode = false, findFocus = false, matchCase = false;
    char findBuf[128] = "";
    char replaceBuf[128] = "";
    std::string findStatus;
    bool gotoOpen = false;
    int gotoLine = 1;
    // punti di interruzione del debugger (righe da 1), F9
    std::set<int> breakpoints;
    int paletteLight = -1;   // la palette del TextEditor segue il tema dello Studio
    // finestra figlia del TextEditor (per la posizione del cursore e il fuoco)
    std::string childName;
    bool focused = false;
};

static std::map<const void*, State>& states() {
    static std::map<const void*, State> s;
    return s;
}

// ---- Utilita' sul testo -----------------------------------------------------------------------

static bool isIdent(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

/** Colonna visiva (tab espansi, UTF-8 = un carattere) -> indice nella riga, e viceversa. */
static int columnToIndex(const std::string& line, int column, int tabSize) {
    int col = 0;
    size_t i = 0;
    while (i < line.size() && col < column) {
        const unsigned char c = (unsigned char)line[i];
        if (c == '\t') col = (col / tabSize + 1) * tabSize;
        else ++col;
        size_t len = 1;
        if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
        i += len;
    }
    return (int)std::min(i, line.size());
}
static int indexToColumn(const std::string& line, int index, int tabSize) {
    int col = 0;
    size_t i = 0;
    while (i < line.size() && (int)i < index) {
        const unsigned char c = (unsigned char)line[i];
        if (c == '\t') col = (col / tabSize + 1) * tabSize;
        else ++col;
        size_t len = 1;
        if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
        i += len;
    }
    return col;
}

static std::vector<std::string> splitLines(const std::string& t) {
    std::vector<std::string> out;
    size_t s = 0;
    for (size_t i = 0; i <= t.size(); ++i)
        if (i == t.size() || t[i] == '\n') { out.push_back(t.substr(s, i - s)); s = i + 1; }
    return out;
}
static std::string joinLines(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += '\n'; out += v[i]; }
    return out;
}

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/** Variabili prese dallo script: local x / function f / local function f / parametri. */
static void scriptIdentifiers(const std::string& text, std::vector<std::string>& out) {
    auto add = [&](const std::string& w) {
        if (w.size() < 2) return;
        if (std::find(out.begin(), out.end(), w) == out.end()) out.push_back(w);
    };
    for (size_t i = 0; i < text.size();) {
        if (!isIdent(text[i]) || (i > 0 && isIdent(text[i - 1]))) { ++i; continue; }
        size_t j = i;
        while (j < text.size() && isIdent(text[j])) ++j;
        const std::string w = text.substr(i, j - i);
        if (w == "local" || w == "function" || w == "for") {
            // le parole dopo: local a, b = ... / function nome( / for i, v in
            size_t k = j;
            for (int n = 0; n < 4; ++n) {
                while (k < text.size() && (text[k] == ' ' || text[k] == '\t' || text[k] == ',')) ++k;
                size_t e = k;
                while (e < text.size() && (isIdent(text[e]) || text[e] == '.' || text[e] == ':')) ++e;
                std::string v = text.substr(k, e - k);
                const size_t dot = v.find_last_of(".:");
                if (dot != std::string::npos) v = v.substr(dot + 1);
                if (v.empty() || v == "function" || v == "in") break;
                add(v);
                k = e;
                if (k >= text.size() || (text[k] != ',' && text[k] != ' ')) break;
                while (k < text.size() && text[k] == ' ') ++k;
                if (k < text.size() && text[k] != ',') break;
            }
        }
        i = j;
    }
}

/** local Players = game:GetService("Players") -> {"Players" -> "Players"} */
static std::string serviceOfVariable(const std::string& text, const std::string& var) {
    const std::string key = var + " = game:GetService(\"";
    size_t p = text.find(key);
    if (p == std::string::npos) return "";
    p += key.size();
    const size_t e = text.find('"', p);
    return e == std::string::npos ? "" : text.substr(p, e - p);
}

// ---- Completamento -------------------------------------------------------------------------------

static void rebuild(State& s, TextEditor& ed) {
    s.items.clear();
    s.open = false;
    const TextEditor::Coordinates cur = ed.GetCursorPosition();
    const std::string line = ed.GetCurrentLineText();
    const int tab = ed.GetTabSize();
    const int ci = columnToIndex(line, cur.mColumn, tab);
    // Dentro una stringa o un commento non si suggerisce niente (tranne GetService(").
    bool inString = false;
    char quote = 0;
    for (int i = 0; i < ci; ++i) {
        const char c = line[(size_t)i];
        if (inString) { if (c == quote && (i == 0 || line[(size_t)i - 1] != '\\')) inString = false; }
        else if (c == '"' || c == '\'') { inString = true; quote = c; }
        else if (c == '-' && i + 1 < ci && line[(size_t)i + 1] == '-') return;
    }
    int ws = ci;
    while (ws > 0 && isIdent(line[(size_t)ws - 1])) --ws;
    const std::string prefix = line.substr((size_t)ws, (size_t)(ci - ws));
    s.wordStartCol = indexToColumn(line, ws, tab);

    std::vector<Entry> pool;
    std::vector<std::string> locals;
    if (inString) {
        // game:GetService("Pla|
        const std::string before = line.substr(0, (size_t)ws);
        if (before.size() >= 12 && before.compare(before.size() - 12, 12, "GetService(\"") == 0)
            for (const char* sv : kServices) pool.push_back({sv, "service"});
        else return;
    } else if (ws > 0 && (line[(size_t)ws - 1] == '.' || line[(size_t)ws - 1] == ':')) {
        const bool colon = line[(size_t)ws - 1] == ':';
        // il "ricevitore": a.b.c prima del punto
        int rs = ws - 1;
        while (rs > 0 && (isIdent(line[(size_t)rs - 1]) || line[(size_t)rs - 1] == '.')) --rs;
        std::string recv = line.substr((size_t)rs, (size_t)(ws - 1 - rs));
        const Members* m = findMembers(recv);
        if (!m) {
            const std::string svc = serviceOfVariable(ed.GetText(), recv);
            if (!svc.empty()) m = findMembers(svc);
        }
        if (!m && recv.rfind("game.", 0) == 0) m = findMembers(recv.substr(5));
        if (m) for (const auto& e : m->list) pool.push_back(e);
        // Oggetti qualunque (part, player...): metodi dopo ":", proprieta' ed eventi dopo "."
        if (!m || colon) {
            const Members* generic = findMembers(colon ? "*instance" : "*property");
            if (generic) for (const auto& e : generic->list) pool.push_back(e);
        }
        if (prefix.empty() && pool.empty()) return;
    } else {
        if (prefix.empty()) return;   // si apre solo mentre scrivi una parola
        for (const auto& e : kKeywords) pool.push_back(e);
        for (const auto& e : kGlobals) pool.push_back(e);
        scriptIdentifiers(ed.GetText(), locals);
    }

    const std::string lp = lower(prefix);
    auto consider = [&](const std::string& text, const char* kind, int& score) {
        const std::string lt = lower(text);
        if (lp.empty()) { score = 1; return true; }
        if (lt == lp) return false;   // gia' scritta tutta
        if (lt.rfind(lp, 0) == 0) { score = text.rfind(prefix, 0) == 0 ? 3 : 2; return true; }
        if (lp.size() >= 2 && lt.find(lp) != std::string::npos) { score = 1; return true; }
        (void)kind;
        return false;
    };
    struct Scored { Item it; int score; };
    std::vector<Scored> found;
    for (const auto& e : pool) {
        int sc = 0;
        if (consider(e.name, e.kind, sc)) found.push_back({{e.name, e.kind}, sc});
    }
    for (const auto& w : locals) {
        if (w.size() == prefix.size()) continue;
        int sc = 0;
        if (consider(w, "variable", sc)) found.push_back({{w, "variable"}, sc + 1});   // le tue variabili prima
    }
    std::stable_sort(found.begin(), found.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.it.text.size() < b.it.text.size();
    });
    for (const auto& f : found) {
        bool dup = false;
        for (const auto& x : s.items) if (x.text == f.it.text) { dup = true; break; }
        if (!dup) s.items.push_back(f.it);
        if (s.items.size() >= 12) break;
    }
    s.sel = 0;
    s.open = !s.items.empty();
}

static void accept(State& s, TextEditor& ed) {
    if (!s.open || s.items.empty()) return;
    const Item it = s.items[(size_t)std::clamp(s.sel, 0, (int)s.items.size() - 1)];
    const TextEditor::Coordinates cur = ed.GetCursorPosition();
    const TextEditor::Coordinates start(cur.mLine, s.wordStartCol);
    if (start < cur) {
        ed.SetSelection(start, cur);
        ed.Delete();
    }
    ed.InsertText(it.text);
    s.open = false;
}

// ---- Errori di sintassi -----------------------------------------------------------------------

/** Compila con Luau: se c'e' un errore, riga (da 1) e messaggio. */
static bool syntaxError(const std::string& src, int& line, std::string& msg) {
    const std::string bc = Luau::compile(src);
    if (bc.empty() || bc[0] != 0) return false;
    std::string e = bc.substr(1);   // ":12: Expected 'end' ..."
    line = 0;
    if (!e.empty() && e[0] == ':') {
        size_t i = 1;
        while (i < e.size() && std::isdigit((unsigned char)e[i])) line = line * 10 + (e[i++] - '0');
        if (i < e.size() && e[i] == ':') ++i;
        while (i < e.size() && e[i] == ' ') ++i;
        e = e.substr(i);
    }
    msg = e;
    return true;
}

// ---- Trova / sostituisci ----------------------------------------------------------------------

static size_t findIn(const std::string& hay, const std::string& needle, size_t from, bool matchCase, bool backward) {
    if (needle.empty()) return std::string::npos;
    const std::string h = matchCase ? hay : lower(hay);
    const std::string n = matchCase ? needle : lower(needle);
    if (!backward) {
        size_t p = h.find(n, from);
        if (p == std::string::npos) p = h.find(n);   // ricomincia dall'inizio
        return p;
    }
    size_t p = from == 0 ? std::string::npos : h.rfind(n, from - 1);
    if (p == std::string::npos) p = h.rfind(n);
    return p;
}

/** Offset nel testo intero -> coordinate (riga, colonna visiva). */
static TextEditor::Coordinates offsetToCoords(const std::string& text, size_t off, int tab) {
    int line = 0;
    size_t ls = 0;
    for (size_t i = 0; i < off && i < text.size(); ++i)
        if (text[i] == '\n') { ++line; ls = i + 1; }
    const size_t le = text.find('\n', ls);
    const std::string l = text.substr(ls, (le == std::string::npos ? text.size() : le) - ls);
    return {line, indexToColumn(l, (int)(off - ls), tab)};
}
static size_t coordsToOffset(const std::string& text, const TextEditor::Coordinates& c, int tab) {
    size_t ls = 0;
    for (int line = 0; line < c.mLine; ++line) {
        const size_t nl = text.find('\n', ls);
        if (nl == std::string::npos) return text.size();
        ls = nl + 1;
    }
    const size_t le = text.find('\n', ls);
    const std::string l = text.substr(ls, (le == std::string::npos ? text.size() : le) - ls);
    return ls + (size_t)columnToIndex(l, c.mColumn, tab);
}

static void findNext(State& s, TextEditor& ed, bool backward) {
    const std::string text = ed.GetText();
    const std::string needle = s.findBuf;
    const int tab = ed.GetTabSize();
    const size_t from = coordsToOffset(text, ed.GetCursorPosition(), tab);
    const size_t start = backward && ed.HasSelection() ? from - std::min(from, needle.size()) : from;
    const size_t p = findIn(text, needle, start, s.matchCase, backward);
    if (p == std::string::npos) { s.findStatus = "Non trovato"; return; }
    const TextEditor::Coordinates a = offsetToCoords(text, p, tab), b = offsetToCoords(text, p + needle.size(), tab);
    ed.SetSelection(a, b);
    ed.SetCursorPosition(b);
    s.findStatus = "Riga " + std::to_string(a.mLine + 1);
}

static void replaceOne(State& s, TextEditor& ed) {
    const std::string sel = ed.GetSelectedText();
    const bool same = s.matchCase ? sel == s.findBuf : lower(sel) == lower(s.findBuf);
    if (ed.HasSelection() && same && s.findBuf[0]) {
        ed.Delete();
        ed.InsertText(s.replaceBuf);
    }
    findNext(s, ed, false);
}

static int replaceAll(State& s, TextEditor& ed) {
    const std::string needle = s.findBuf;
    if (needle.empty()) return 0;
    const std::string text = ed.GetText();
    const std::string hay = s.matchCase ? text : lower(text);
    const std::string n = s.matchCase ? needle : lower(needle);
    std::string out;
    size_t p = 0;
    int count = 0;
    for (size_t q; (q = hay.find(n, p)) != std::string::npos; p = q + needle.size(), ++count) {
        out.append(text, p, q - p);
        out += s.replaceBuf;
    }
    out.append(text, p, std::string::npos);
    if (count) {
        const TextEditor::Coordinates cur = ed.GetCursorPosition();
        ed.SetText(out);
        ed.SetCursorPosition(cur);
    }
    return count;
}

// ---- Commenti ---------------------------------------------------------------------------------

/** Ctrl+/: mette o toglie "-- " alle righe selezionate (o a quella del cursore). */
static void toggleComment(TextEditor& ed) {
    std::vector<std::string> lines = splitLines(ed.GetText());
    TextEditor::Coordinates a = ed.GetCursorPosition(), b = a;
    if (ed.HasSelection()) {
        // la selezione: dall'inizio della prima alla fine dell'ultima riga toccata
        const std::string sel = ed.GetSelectedText();
        const int nl = (int)std::count(sel.begin(), sel.end(), '\n');
        const TextEditor::Coordinates c = ed.GetCursorPosition();
        a = TextEditor::Coordinates(std::max(0, c.mLine - nl), 0);
        b = c;
        if (sel.size() && sel.back() == '\n' && b.mLine > a.mLine) b.mLine -= 1;
    }
    const int l0 = std::clamp(std::min(a.mLine, b.mLine), 0, (int)lines.size() - 1);
    const int l1 = std::clamp(std::max(a.mLine, b.mLine), 0, (int)lines.size() - 1);
    bool allCommented = true;
    for (int l = l0; l <= l1; ++l) {
        const std::string& s = lines[(size_t)l];
        const size_t f = s.find_first_not_of(" \t");
        if (f == std::string::npos) continue;
        if (s.compare(f, 2, "--") != 0) { allCommented = false; break; }
    }
    for (int l = l0; l <= l1; ++l) {
        std::string& s = lines[(size_t)l];
        const size_t f = s.find_first_not_of(" \t");
        if (f == std::string::npos) continue;
        if (allCommented) {
            s.erase(f, s.compare(f, 3, "-- ") == 0 ? 3 : 2);
        } else {
            s.insert(f, "-- ");
        }
    }
    const TextEditor::Coordinates cur = ed.GetCursorPosition();
    ed.SetText(joinLines(lines));
    ed.SetCursorPosition(cur);
}

// ---- Disegno: barra sopra, editor, popup, barra di stato ----------------------------------------

static void toolbar(State& s, TextEditor& ed) {
    if (ImGui::SmallButton("Trova")) { s.findOpen = true; s.replaceMode = false; s.findFocus = true; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+F");
    ImGui::SameLine();
    if (ImGui::SmallButton("Sostituisci")) { s.findOpen = true; s.replaceMode = true; s.findFocus = true; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+H");
    ImGui::SameLine();
    if (ImGui::SmallButton("Vai alla riga")) { s.gotoOpen = true; s.gotoLine = ed.GetCursorPosition().mLine + 1; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+G");
    ImGui::SameLine();
    if (ImGui::SmallButton("Interruzione")) {
        const int l = ed.GetCursorPosition().mLine + 1;
        if (!s.breakpoints.erase(l)) s.breakpoints.insert(l);
        ed.SetBreakpoints(TextEditor::Breakpoints(s.breakpoints.begin(), s.breakpoints.end()));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("F9: il Test si ferma su questa riga (debugger)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Commenta")) toggleComment(ed);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+/ o Ctrl+K: mette o toglie -- alle righe");
    ImGui::SameLine();
    ImGui::BeginDisabled(!ed.CanUndo());
    if (ImGui::SmallButton("Annulla")) ed.Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!ed.CanRedo());
    if (ImGui::SmallButton("Ripeti")) ed.Redo();
    ImGui::EndDisabled();

    if (s.findOpen) {
        if (s.findFocus) { ImGui::SetKeyboardFocusHere(); s.findFocus = false; }
        ImGui::SetNextItemWidth(170);
        const bool enter = ImGui::InputTextWithHint("##find", "Trova", s.findBuf, sizeof(s.findBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter) { findNext(s, ed, ImGui::GetIO().KeyShift); ImGui::SetKeyboardFocusHere(-1); }
        ImGui::SameLine();
        if (ImGui::SmallButton("<")) findNext(s, ed, true);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) findNext(s, ed, false);
        ImGui::SameLine();
        ImGui::Checkbox("Aa", &s.matchCase);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maiuscole/minuscole uguali");
        if (s.replaceMode) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::InputTextWithHint("##repl", "Sostituisci con", s.replaceBuf, sizeof(s.replaceBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Sostituisci##one")) replaceOne(s, ed);
            ImGui::SameLine();
            if (ImGui::SmallButton("Tutti")) {
                const int n = replaceAll(s, ed);
                s.findStatus = std::to_string(n) + " sostituiti";
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", s.findStatus.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x##closefind") || (ImGui::IsKeyPressed(ImGuiKey_Escape) && !s.open)) s.findOpen = false;
    }
    if (s.gotoOpen) {
        ImGui::SetNextItemWidth(90);
        if (ImGui::InputInt("##goto", &s.gotoLine, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
            const int l = std::clamp(s.gotoLine, 1, ed.GetTotalLines()) - 1;
            ed.SetCursorPosition({l, 0});
            ed.SetSelection({l, 0}, {l, 0});
            s.gotoOpen = false;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Invio = vai alla riga (1-%d)", ed.GetTotalLines());
        ImGui::SameLine();
        if (ImGui::SmallButton("x##closegoto")) s.gotoOpen = false;
    }
}

static void statusBar(State& s, TextEditor& ed) {
    const TextEditor::Coordinates c = ed.GetCursorPosition();
    ImGui::TextDisabled("Riga %d, Col %d  |  %d righe", c.mLine + 1, c.mColumn + 1, ed.GetTotalLines());
    if (!s.breakpoints.empty()) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "%d interruzioni", (int)s.breakpoints.size());
    }
    ImGui::SameLine();
    if (s.errMsg.empty()) {
        ImGui::TextColored({0.45f, 0.85f, 0.5f, 1.f}, "Nessun errore di sintassi");
    } else {
        ImGui::TextColored({1.f, 0.45f, 0.4f, 1.f}, "Riga %d: %s", s.errLine, s.errMsg.c_str());
        if (ImGui::IsItemClicked() && s.errLine > 0) ed.SetCursorPosition({s.errLine - 1, 0});
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clic: vai alla riga");
    }
}

/** Dove disegnare il popup: sotto il cursore di testo, dentro la finestra del TextEditor. */
static ImVec2 caretScreenPos(State& s, TextEditor& ed) {
    ImGuiWindow* w = s.childName.empty() ? nullptr : ImGui::FindWindowByName(s.childName.c_str());
    if (!w) return ImGui::GetMousePos();
    const float fs = ImGui::GetFontSize();
    char buf[16];
    std::snprintf(buf, sizeof(buf), " %d ", ed.GetTotalLines());
    const float textStart = ImGui::CalcTextSize(buf).x + 10.f;
    const TextEditor::Coordinates c = ed.GetCursorPosition();
    std::string line = ed.GetCurrentLineText();
    const int idx = columnToIndex(line, c.mColumn, ed.GetTabSize());
    std::string pre;
    for (int i = 0; i < idx; ++i) {
        if (line[(size_t)i] == '\t') pre.append((size_t)ed.GetTabSize(), ' ');
        else pre += line[(size_t)i];
    }
    const float x = w->Pos.x + textStart + ImGui::CalcTextSize(pre.c_str()).x - w->Scroll.x;
    const float y = w->Pos.y + (float)(c.mLine + 1) * fs - w->Scroll.y + 2.f;
    return {x, y};
}

/**
 * Il resto della parola suggerita, in grigio dopo il cursore, e sotto la riga
 * "Tab  Accetta": come i suggerimenti dello Script Editor di Roblox Studio.
 */
static void ghost(State& s, TextEditor& ed) {
    if (!s.open || s.items.empty() || !s.focused) return;
    const Item& it = s.items[(size_t)std::clamp(s.sel, 0, (int)s.items.size() - 1)];
    const TextEditor::Coordinates cur = ed.GetCursorPosition();
    const std::string line = ed.GetCurrentLineText();
    const int tab = ed.GetTabSize();
    const int ws = columnToIndex(line, s.wordStartCol, tab), ci = columnToIndex(line, cur.mColumn, tab);
    const std::string typed = line.substr((size_t)ws, (size_t)std::max(0, ci - ws));
    if (typed.size() > it.text.size() || lower(it.text.substr(0, typed.size())) != lower(typed)) return;
    const std::string rest = it.text.substr(typed.size());
    ImGuiWindow* w = s.childName.empty() ? nullptr : ImGui::FindWindowByName(s.childName.c_str());
    if (!w) return;
    const ImVec2 cp = caretScreenPos(s, ed);
    const float fs = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->PushClipRect(w->InnerRect.Min, w->InnerRect.Max, true);
    dl->AddText({cp.x, cp.y - fs - 2.f}, IM_COL32(150, 155, 165, 200), rest.c_str());
    const ImVec2 k = ImGui::CalcTextSize("Tab");
    const float y = cp.y + 3.f;
    dl->AddRectFilled({cp.x, y}, {cp.x + k.x + 10.f, y + fs + 2.f}, IM_COL32(70, 74, 84, 230), 3.f);
    dl->AddText({cp.x + 5.f, y + 1.f}, IM_COL32(220, 224, 232, 255), "Tab");
    dl->AddText({cp.x + k.x + 18.f, y + 1.f}, IM_COL32(170, 175, 185, 255), "Accetta");
    dl->PopClipRect();
}

static void popup(State& s, TextEditor& ed) {
    if (!s.open || s.items.empty() || !s.focused) return;
    const ImVec2 cp = caretScreenPos(s, ed);
    ImGui::SetNextWindowPos({cp.x, cp.y + ImGui::GetFontSize() + 6.f});
    ImGui::SetNextWindowBgAlpha(0.97f);
    const ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing
                              | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_Tooltip;
    if (ImGui::Begin("##scriptassist", nullptr, fl)) {
        for (int i = 0; i < (int)s.items.size(); ++i) {
            const Item& it = s.items[(size_t)i];
            ImGui::PushID(i);
            if (ImGui::Selectable("##it", i == s.sel, 0, {220.f, 0.f})) {
                s.sel = i;
                accept(s, ed);
                if (!s.childName.empty()) ImGui::SetWindowFocus(s.childName.c_str());
            }
            ImGui::SameLine(6.f);
            ImGui::TextUnformatted(it.text.c_str());
            ImGui::SameLine(170.f);
            ImGui::TextDisabled("%s", it.kind.c_str());
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::TextDisabled("Tab / Invio inserisce  ·  Esc chiude");
    }
    ImGui::End();
}

} // namespace sa

/**
 * L'editor completo: barra (trova, vai alla riga, commenta), il TextEditor con
 * completamento ed errori, barra di stato. `height` = altezza del solo testo.
 */
static void scriptAssistEditor(TextEditor& ed, const char* imguiId, float height) {
    sa::State& s = sa::states()[&ed];
    const ImGuiIO& io = ImGui::GetIO();

    if (s.paletteLight != (studio_theme::isLight() ? 1 : 0)) {
        s.paletteLight = studio_theme::isLight() ? 1 : 0;
        ed.SetPalette(robloxScriptPalette(s.paletteLight != 0));
    }
    sa::toolbar(s, ed);

    // Scorciatoie dell'editor (solo col testo a fuoco).
    if (s.focused) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) { s.findOpen = true; s.replaceMode = false; s.findFocus = true;
            if (ed.HasSelection()) std::snprintf(s.findBuf, sizeof(s.findBuf), "%s", ed.GetSelectedText().c_str()); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_H, false)) { s.findOpen = true; s.replaceMode = true; s.findFocus = true; }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) { s.gotoOpen = true; s.gotoLine = ed.GetCursorPosition().mLine + 1; }
        if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Slash, false) || ImGui::IsKeyPressed(ImGuiKey_K, false)))
            sa::toggleComment(ed);
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) sa::findNext(s, ed, io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {   // come Roblox Studio
            const int l = ed.GetCursorPosition().mLine + 1;
            if (!s.breakpoints.erase(l)) s.breakpoints.insert(l);
            ed.SetBreakpoints(TextEditor::Breakpoints(s.breakpoints.begin(), s.breakpoints.end()));
        }
    }

    // Con il popup aperto, frecce / Tab / Invio / Esc sono del popup, non del testo.
    bool consumed = false;
    if (s.open && s.focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { s.sel = (s.sel + 1) % (int)s.items.size(); consumed = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { s.sel = (s.sel + (int)s.items.size() - 1) % (int)s.items.size(); consumed = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)
            || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) { sa::accept(s, ed); consumed = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { s.open = false; consumed = true; }
    }
    if (consumed) ed.SetHandleKeyboardInputs(false);

    // Nome della finestra figlia che crea TextEditor::Render (stesso formato di ImGui::BeginChild).
    {
        ImGuiWindow* parent = ImGui::GetCurrentWindow();
        const ImGuiID id = parent->GetID(imguiId);
        char name[256];
        std::snprintf(name, sizeof(name), "%s/%s_%08X", parent->Name, imguiId, id);
        s.childName = name;
    }
    const float statusH = ImGui::GetFrameHeightWithSpacing();
    ed.Render(imguiId, {-1.f, std::max(60.f, height - statusH)});
    ed.SetHandleKeyboardInputs(true);
    {
        ImGuiWindow* w = ImGui::FindWindowByName(s.childName.c_str());
        s.focused = w && GImGui->NavWindow == w;
    }

    if (ed.IsTextChanged()) {
        s.lastEditTime = ImGui::GetTime();
        s.checkPending = true;
        sa::rebuild(s, ed);
    } else if (ed.IsCursorPositionChanged() && s.open && !consumed) {
        sa::rebuild(s, ed);   // il cursore e' andato altrove: si rifa' (o si chiude)
    }
    if (!s.focused) s.open = false;

    // Controllo della sintassi poco dopo che smetti di scrivere.
    if (s.checkPending && ImGui::GetTime() - s.lastEditTime > 0.35) {
        s.checkPending = false;
        int line = 0;
        std::string msg;
        TextEditor::ErrorMarkers markers;
        if (sa::syntaxError(ed.GetText(), line, msg)) {
            s.errLine = line;
            s.errMsg = msg;
            if (line > 0) markers[line] = msg;
        } else {
            s.errLine = 0;
            s.errMsg.clear();
        }
        ed.SetErrorMarkers(markers);
    }

    sa::statusBar(s, ed);
    sa::ghost(s, ed);
    sa::popup(s, ed);
}

/** I punti di interruzione messi con F9 in questo editor (righe da 1). */
static std::set<int>& scriptAssistBreakpoints(TextEditor& ed) { return sa::states()[&ed].breakpoints; }
