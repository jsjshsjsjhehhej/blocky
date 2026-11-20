// Due finestre come in Roblox Studio:
//   - Analisi script: controlla da sola tutti gli script del place (errori di
//     sintassi veri dal compilatore Luau, API deprecate, cicli senza attesa...),
//     riga per riga; clic = apre lo script alla riga giusta.
//   - Inserisci oggetto: classi di Roblox con icona, ricerca (Invio inserisce
//     il primo), frecce per scegliere, "recenti", inserimento dentro l'oggetto
//     selezionato (script e luce nella Part, Remote nei servizi...).
// Incluso da StudioUiPanel.h dopo le schede degli script (usa openScriptTab).

// ---- Analisi script --------------------------------------------------------------------------------

struct AnalysisIssue {
    int severity = 1;     // 2 errore, 1 avviso, 0 suggerimento
    std::string where;    // "Script in Part \"Porta\""
    int part = -1;        // part con lo script (-1 = script in un servizio)
    int shared = -1;      // indice in g_world.instances
    int line = 0;
    std::string msg;
};

static std::vector<AnalysisIssue> g_analysis;
static uint64_t g_analysisHash = 0;
static double g_analysisTime = -10.0;
static bool g_analysisShowErr = true, g_analysisShowWarn = true, g_analysisShowHint = true;

/** Una riga dello script con una delle API vecchie o pericolose. */
static void analyzeLines(const std::string& src, std::vector<AnalysisIssue>& out, const AnalysisIssue& base) {
    struct Rule { const char* needle; int sev; const char* msg; };
    static const Rule kRules[] = {
        {"wait(", 0, "wait() e' deprecato: usa task.wait()"},
        {"spawn(", 0, "spawn() e' deprecato: usa task.spawn()"},
        {"delay(", 0, "delay() e' deprecato: usa task.delay()"},
        {":connect(", 1, ":connect() minuscolo e' deprecato: usa :Connect()"},
        {":Remove()", 1, ":Remove() e' deprecato: usa :Destroy()"},
        {"loadstring", 1, "loadstring non e' permesso negli script del gioco"},
        {"getfenv", 1, "getfenv/setfenv rallentano Luau e sono quasi sempre evitabili"},
        {"setfenv", 1, "getfenv/setfenv rallentano Luau e sono quasi sempre evitabili"},
        {"game.Workspace", 0, "Si puo' scrivere solo workspace"},
        {"Instance.new(\"Part\", ", 0, "Instance.new con il Parent e' lento: imposta Parent per ultimo"},
    };
    std::vector<std::string> lines;
    {
        size_t s = 0;
        for (size_t i = 0; i <= src.size(); ++i)
            if (i == src.size() || src[i] == '\n') { lines.push_back(src.substr(s, i - s)); s = i + 1; }
    }
    for (size_t l = 0; l < lines.size(); ++l) {
        std::string code = lines[l];
        const size_t cm = code.find("--");
        if (cm != std::string::npos) code = code.substr(0, cm);   // i commenti non contano
        for (const auto& r : kRules) {
            const size_t p = code.find(r.needle);
            if (p == std::string::npos) continue;
            // "task.wait(" contiene "wait(": il carattere prima non deve far parte di un nome
            if (p > 0 && (std::isalnum((unsigned char)code[p - 1]) || code[p - 1] == '_' || code[p - 1] == '.') && r.needle[0] != ':'
                && r.needle[0] != 'g' && r.needle[0] != 'I')
                continue;
            AnalysisIssue is = base;
            is.severity = r.sev;
            is.line = (int)l + 1;
            is.msg = r.msg;
            out.push_back(is);
        }
        // while true do ... senza attesa nelle righe successive del ciclo
        if (code.find("while true do") != std::string::npos || code.find("while wait() do") != std::string::npos) {
            bool waits = false;
            for (size_t k = l; k < lines.size() && k < l + 40; ++k) {
                if (lines[k].find("wait") != std::string::npos || lines[k].find("Wait") != std::string::npos
                    || lines[k].find("Heartbeat") != std::string::npos) { waits = true; break; }
                if (k > l && lines[k].find("end") == 0) break;
            }
            if (!waits) {
                AnalysisIssue is = base;
                is.severity = 1;
                is.line = (int)l + 1;
                is.msg = "Ciclo infinito senza task.wait(): bloccherebbe il gioco";
                out.push_back(is);
            }
        }
    }
}

static void analyzeScript(const std::string& src, const AnalysisIssue& base, std::vector<AnalysisIssue>& out) {
    if (src.empty()) return;
    int line = 0;
    std::string msg;
    if (sa::syntaxError(src, line, msg)) {
        AnalysisIssue is = base;
        is.severity = 2;
        is.line = line;
        is.msg = msg;
        out.push_back(is);
        return;   // con un errore di sintassi il resto non e' affidabile
    }
    analyzeLines(src, out, base);
    if (src.size() > 60000) {
        AnalysisIssue is = base;
        is.severity = 0;
        is.msg = "Script molto lungo: conviene dividerlo in ModuleScript";
        out.push_back(is);
    }
}

static uint64_t analysisFingerprint() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const std::string& s) { for (char c : s) { h ^= (unsigned char)c; h *= 1099511628211ull; } h ^= s.size(); };
    for (const auto& p : g_world.parts) mix(p.script);
    for (const auto& i : g_world.instances) mix(i.source);
    return h;
}

static void runScriptAnalysis() {
    std::vector<AnalysisIssue> out;
    bool fires = false, listens = false;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.script.empty()) continue;
        AnalysisIssue base;
        base.where = std::string(exScriptClass(p.scriptType)) + " in \"" + p.name + "\"";
        base.part = i;
        analyzeScript(p.script, base, out);
        fires |= p.script.find("FireServer") != std::string::npos;
        listens |= p.script.find("OnServerEvent") != std::string::npos;
    }
    for (int i = 0; i < (int)g_world.instances.size(); ++i) {
        const SharedInstance& s = g_world.instances[(size_t)i];
        if (s.source.empty()) continue;
        AnalysisIssue base;
        base.where = s.className + " \"" + s.name + "\"";
        base.shared = i;
        analyzeScript(s.source, base, out);
        fires |= s.source.find("FireServer") != std::string::npos;
        listens |= s.source.find("OnServerEvent") != std::string::npos;
    }
    if (fires && !listens) {
        AnalysisIssue is;
        is.severity = 1;
        is.where = "Place";
        is.msg = "C'e' FireServer ma nessuno script ascolta OnServerEvent";
        out.push_back(is);
    }
    std::stable_sort(out.begin(), out.end(), [](const AnalysisIssue& a, const AnalysisIssue& b) { return a.severity > b.severity; });
    g_analysis = std::move(out);
}

static void analysisCounts(int& err, int& warn, int& hint) {
    err = warn = hint = 0;
    for (const auto& a : g_analysis) (a.severity == 2 ? err : a.severity == 1 ? warn : hint)++;
}

/** Ricontrolla da sola quando gli script cambiano (al massimo ogni 1,5 secondi). */
static void tickScriptAnalysis() {
    if (ImGui::GetTime() - g_analysisTime < 1.5) return;
    const uint64_t h = analysisFingerprint();
    if (h == g_analysisHash) return;
    g_analysisHash = h;
    g_analysisTime = ImGui::GetTime();
    runScriptAnalysis();
}

static void drawScriptAnalysisWindowPro() {
    tickScriptAnalysis();
    if (!g_showScriptAnalysis) return;
    ImGui::SetNextWindowSize({620.f, 300.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Analisi script###scriptanalysis", &g_showScriptAnalysis)) { ImGui::End(); return; }
    int e = 0, w = 0, h = 0;
    analysisCounts(e, w, h);
    char lab[48];
    std::snprintf(lab, sizeof(lab), "Errori (%d)", e);
    ImGui::Checkbox(lab, &g_analysisShowErr);
    ImGui::SameLine();
    std::snprintf(lab, sizeof(lab), "Avvisi (%d)", w);
    ImGui::Checkbox(lab, &g_analysisShowWarn);
    ImGui::SameLine();
    std::snprintf(lab, sizeof(lab), "Suggerimenti (%d)", h);
    ImGui::Checkbox(lab, &g_analysisShowHint);
    ImGui::SameLine();
    if (ImGui::SmallButton("Ricontrolla")) { runScriptAnalysis(); g_analysisHash = analysisFingerprint(); }
    if (ImGui::BeginTable("##an", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          {0.f, ImGui::GetContentRegionAvail().y})) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 18.f);
        ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch, 0.32f);
        ImGui::TableSetupColumn("Riga", ImGuiTableColumnFlags_WidthFixed, 40.f);
        ImGui::TableSetupColumn("Problema", ImGuiTableColumnFlags_WidthStretch, 0.68f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)g_analysis.size(); ++i) {
            const AnalysisIssue& a = g_analysis[(size_t)i];
            if ((a.severity == 2 && !g_analysisShowErr) || (a.severity == 1 && !g_analysisShowWarn) || (a.severity == 0 && !g_analysisShowHint))
                continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const ImVec2 c = ImGui::GetCursorScreenPos();
            const ImU32 col = a.severity == 2 ? IM_COL32(235, 80, 70, 255) : a.severity == 1 ? IM_COL32(240, 190, 60, 255) : IM_COL32(110, 170, 240, 255);
            ImGui::GetWindowDrawList()->AddCircleFilled({c.x + 7.f, c.y + ImGui::GetTextLineHeight() * 0.5f + 1.f}, 5.f, col);
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(i);
            const bool clicked = ImGui::Selectable(a.where.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::PopID();
            ImGui::TableSetColumnIndex(2);
            if (a.line > 0) ImGui::Text("%d", a.line); else ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", a.msg.c_str());
            if (clicked) {
                if (a.part >= 0 && partIndexValid(a.part)) {
                    studioMultiClear();
                    selectPart(a.part);
                    g_editingPartScript = true;
                    openScriptTab(a.part);
                    g_gotoScriptLine = a.line;
                } else if (a.shared >= 0 && a.shared < (int)g_world.instances.size()) {
                    g_selectedSharedInstance = a.shared;
                    g_showProps = true;
                }
            }
        }
        if (g_analysis.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored({0.45f, 0.85f, 0.5f, 1.f}, "Nessun problema negli script del place");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---- Inserisci oggetto ------------------------------------------------------------------------------

struct InsertClass {
    const char* name;      // classe Roblox
    const char* category;
    const char* hint;
    ExIcon icon;
    int action;            // vedi insertClassRun
};

static const InsertClass kInsertClasses[] = {
    {"Part", "Parti", "Blocco 4x1x4", ExIcon::Part, 0},
    {"Part (Ball)", "Parti", "Sfera", ExIcon::Ball, 1},
    {"Part (Cylinder)", "Parti", "Cilindro", ExIcon::Cylinder, 2},
    {"WedgePart", "Parti", "Cuneo / rampa", ExIcon::Wedge, 3},
    {"SpawnLocation", "Parti", "Dove nascono i giocatori", ExIcon::Spawn, 7},
    {"MeshPart", "Parti", "Importa un modello 3D (FBX, GLB, OBJ)", ExIcon::MeshPart, 11},
    {"Model", "Parti", "Raggruppa la selezione in un Model (Ctrl+G)", ExIcon::Model, 100},
    {"Script", "Script", "Script del server dentro la Part selezionata", ExIcon::Script, 101},
    {"LocalScript", "Script", "Script del giocatore dentro la Part selezionata", ExIcon::LocalScript, 102},
    {"ModuleScript", "Script", "Modulo per require() dentro la Part selezionata", ExIcon::ModuleScript, 103},
    {"RemoteEvent", "Rete", "In ReplicatedStorage: client <-> server", ExIcon::Remote, 110},
    {"RemoteFunction", "Rete", "In ReplicatedStorage: chiamata con risposta", ExIcon::Remote, 111},
    {"Folder", "Rete", "Cartella in ReplicatedStorage", ExIcon::Folder, 112},
    {"PointLight", "Luci", "Luce dentro la Part selezionata", ExIcon::Light, 104},
    {"Explosion", "Effetti", "Part che esplode", ExIcon::Lighting, 6},
    {"Sound", "Audio", "Suono in SoundService", ExIcon::Sound, 10},
    {"TextLabel", "GUI", "Scritta sullo schermo", ExIcon::Label, 8},
    {"TextButton", "GUI", "Bottone sullo schermo", ExIcon::Button, 9},
    {"Frame", "GUI", "Riquadro sullo schermo", ExIcon::Frame, 120},
    {"ImageLabel", "GUI", "Immagine sullo schermo", ExIcon::Image, 121},
    {"BillboardGui", "GUI", "Scritta sopra una Part, nel mondo", ExIcon::Gui, 122},
    {"Tool", "Strumenti", "Oggetto da tenere in mano (StarterPack)", ExIcon::Tool, 130},
    {"WeldConstraint", "Vincoli", "Salda le 2 part selezionate", ExIcon::Model, 140},
    {"HingeConstraint", "Vincoli", "Cerniera fra le 2 part selezionate", ExIcon::Model, 141},
    {"RopeConstraint", "Vincoli", "Corda fra le 2 part selezionate", ExIcon::Model, 142},
    {"SpringConstraint", "Vincoli", "Molla fra le 2 part selezionate", ExIcon::Model, 143},
    {"RodConstraint", "Vincoli", "Asta fra le 2 part selezionate", ExIcon::Model, 144},
};

static std::vector<std::string> g_insertRecent;

static void insertClassRun(const InsertClass& c) {
    switch (c.action) {
    case 100: studioGroupSelection(); break;
    case 101: studioAddScriptOfType(ScriptType::Server); break;
    case 102: studioAddScriptOfType(ScriptType::Client); break;
    case 103: studioAddScriptOfType(ScriptType::ModuleScript); break;
    case 104:
        if (partIndexValid(g_selectedPart)) { g_world.parts[(size_t)g_selectedPart].pointLight = true; markPlaceDirty(); }
        else insertPointLightPart();
        break;
    case 110: case 111: case 112: {
        static const char* kCls[] = {"RemoteEvent", "RemoteFunction", "Folder"};
        g_world.addShared(SharedService::ReplicatedStorage, kCls[c.action - 110], kCls[c.action - 110]);
        g_selectedSharedInstance = (int)g_world.instances.size() - 1;
        markPlaceDirty();
        break;
    }
    case 120: insertNewFrame(); break;
    case 121: insertNewImageLabel(); break;
    case 122: insertNewLabel(); break;
    case 130: g_world.addBackpackItem("Tool", (int)g_world.backpack.size()); markPlaceDirty(); break;
    case 140: studioCreateWeldFromSelection(); break;
    case 141: studioCreateConstraintFromSelection(ConstraintKind::Hinge); break;
    case 142: studioCreateConstraintFromSelection(ConstraintKind::Rope); break;
    case 143: studioCreateConstraintFromSelection(ConstraintKind::Spring); break;
    case 144: studioCreateConstraintFromSelection(ConstraintKind::Rod); break;
    default: studioRunInsertKind(c.action); break;
    }
    auto it = std::find(g_insertRecent.begin(), g_insertRecent.end(), c.name);
    if (it != g_insertRecent.end()) g_insertRecent.erase(it);
    g_insertRecent.insert(g_insertRecent.begin(), c.name);
    if (g_insertRecent.size() > 6) g_insertRecent.pop_back();
}

static void drawInsertObjectWindowPro() {
    if (!g_showInsertObject) return;
    static int sel = 0;
    static bool focusSearch = true;
    ImGui::SetNextWindowSize({420.f, 520.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inserisci oggetto###insertobject", &g_showInsertObject)) { ImGui::End(); return; }
    if (ImGui::IsWindowAppearing()) { focusSearch = true; sel = 0; }
    // Dove andra': come la riga "Inserisci in" di Roblox.
    std::string target = "Workspace";
    if (partIndexValid(g_selectedPart)) target = g_world.parts[(size_t)g_selectedPart].name + " (Part)";
    ImGui::TextDisabled("Inserisci in: %s", target.c_str());
    if (focusSearch) { ImGui::SetKeyboardFocusHere(); focusSearch = false; }
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool enter = ImGui::InputTextWithHint("##insq", "Cerca una classe (Part, Script, RemoteEvent...)", g_insertSearchBuf,
                                                sizeof(g_insertSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    std::string q = g_insertSearchBuf;
    for (char& ch : q) ch = (char)std::tolower((unsigned char)ch);
    std::vector<const InsertClass*> list;
    if (q.empty())
        for (const auto& r : g_insertRecent)
            for (const auto& c : kInsertClasses)
                if (r == c.name) list.push_back(&c);
    const size_t recentCount = list.size();
    for (const auto& c : kInsertClasses) {
        std::string hay = std::string(c.name) + " " + c.category + " " + c.hint;
        for (char& ch : hay) ch = (char)std::tolower((unsigned char)ch);
        if (q.empty() || hay.find(q) != std::string::npos) list.push_back(&c);
    }
    // I prefabbricati di Blocky (kill brick, jump pad...) in fondo, anche loro cercabili.
    std::vector<const StudioInsertItem*> extras;
    for (int i = 0; i < kInsertItemCount; ++i) {
        const auto& it = kInsertItems[i];
        if (it.kind < 15) continue;
        std::string hay = std::string(it.name) + " " + it.category + " " + it.hint;
        for (char& ch : hay) ch = (char)std::tolower((unsigned char)ch);
        if (q.empty() || hay.find(q) != std::string::npos) extras.push_back(&it);
    }
    const int total = (int)(list.size() + extras.size());
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) sel = std::min(sel + 1, total - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) sel = std::max(sel - 1, 0);
    sel = std::clamp(sel, 0, std::max(0, total - 1));
    auto runIndex = [&](int k) {
        if (k < (int)list.size()) insertClassRun(*list[(size_t)k]);
        else if (k - (int)list.size() < (int)extras.size()) studioRunInsertKind(extras[(size_t)(k - (int)list.size())]->kind);
        if (!ImGui::GetIO().KeyShift) g_showInsertObject = false;   // Shift = resta aperta per inserirne altri
    };
    if (enter && total > 0) runIndex(sel);

    ImGui::BeginChild("##inslist2", {0.f, -ImGui::GetFrameHeightWithSpacing()});
    const char* lastCat = nullptr;
    for (int k = 0; k < total; ++k) {
        const bool isExtra = k >= (int)list.size();
        const char* cat = isExtra ? "Blocky: prefabbricati" : ((size_t)k < recentCount ? "Recenti" : list[(size_t)k]->category);
        if (!lastCat || std::strcmp(lastCat, cat) != 0) {
            ImGui::TextColored({0.55f, 0.8f, 1.f, 1.f}, "%s", cat);
            lastCat = cat;
        }
        ImGui::PushID(k);
        const char* name = isExtra ? extras[(size_t)(k - (int)list.size())]->name : list[(size_t)k]->name;
        const char* hint = isExtra ? extras[(size_t)(k - (int)list.size())]->hint : list[(size_t)k]->hint;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##row", k == sel, ImGuiSelectableFlags_AllowDoubleClick, {0.f, 20.f})) {
            sel = k;
            runIndex(k);
        }
        if (k == sel && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) ImGui::SetScrollHereY(0.5f);
        exDrawIcon(ImGui::GetWindowDrawList(), {pos.x + 10.f, pos.y + 10.f}, isExtra ? ExIcon::Part : list[(size_t)k]->icon);
        ImGui::GetWindowDrawList()->AddText({pos.x + 24.f, pos.y + 3.f}, ImGui::GetColorU32(ImGuiCol_Text), name);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
        ImGui::PopID();
    }
    if (total == 0) ImGui::TextDisabled("Nessuna classe con questo nome");
    ImGui::EndChild();
    ImGui::TextDisabled("Invio inserisce  ·  frecce scelgono  ·  Shift = resta aperta");
    ImGui::End();
}

// ---- Output (come la finestra Output di Roblox Studio) ----------------------------------------
// Ogni riga con l'orario d'arrivo, i doppioni di fila diventano "(x3)",
// filtro per tipo e per testo, clic su un errore "Nome:12:" = apre lo script
// alla riga 12, tasto destro = copia / pulisci.

struct OutputLine {
    std::string text;
    double time = 0.0;   // secondi da quando lo Studio e' partito
    int count = 1;       // ripetizioni di fila
    int kind = 0;        // 0 messaggio, 1 avviso, 2 errore, 3 info
};
static std::vector<OutputLine> g_outLines;
static uint64_t g_outSeen = 0;
static int g_outFilter = 0;         // 0 tutti, 1 messaggi, 2 avvisi, 3 errori
static char g_outSearch[96] = "";
static bool g_outTimestamps = true, g_outAutoScroll = true;

static int outputKind(const std::string& l) {
    std::string s = l;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s.find("error") != std::string::npos || s.find("errore") != std::string::npos) return 2;
    if (s.find("warn") != std::string::npos || s.find("avviso") != std::string::npos) return 1;
    if (s.rfind("[info]", 0) == 0) return 3;
    return 0;
}

/** Le righe nuove del registro Luau finiscono qui, con l'orario. */
static void syncOutputLines() {
    const auto& log = g_lua.printLog;
    if (log.empty() && g_lua.printCount == 0) { g_outLines.clear(); g_outSeen = 0; return; }
    if (g_lua.printCount < g_outSeen) g_outSeen = 0;   // runtime ripartito (nuovo Test)
    uint64_t fresh = g_lua.printCount - g_outSeen;
    if (fresh == 0) return;
    fresh = std::min<uint64_t>(fresh, log.size());
    for (size_t i = log.size() - (size_t)fresh; i < log.size(); ++i) {
        if (!g_outLines.empty() && g_outLines.back().text == log[i]) { ++g_outLines.back().count; g_outLines.back().time = ImGui::GetTime(); continue; }
        g_outLines.push_back({log[i], ImGui::GetTime(), 1, outputKind(log[i])});
    }
    if (g_outLines.size() > 2000) g_outLines.erase(g_outLines.begin(), g_outLines.begin() + (long)(g_outLines.size() - 2000));
    g_outSeen = g_lua.printCount;
}

/** "Porta:12: ..." o "[string \"Porta\"]:12:" -> la part "Porta" con uno script e la riga 12. */
static bool outputSourceOf(const std::string& l, int& part, int& line) {
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.script.empty() || p.name.empty()) continue;
        size_t at = l.find(p.name + ":");
        if (at == std::string::npos) at = l.find(p.name + "\"]:");
        if (at == std::string::npos) continue;
        size_t k = l.find(':', at + p.name.size());
        if (k == std::string::npos) continue;
        int n = 0;
        size_t d = k + 1;
        while (d < l.size() && std::isdigit((unsigned char)l[d])) n = n * 10 + (l[d++] - '0');
        if (n <= 0) continue;
        part = i;
        line = n;
        return true;
    }
    return false;
}

static void drawOutputPanelPro() {
    syncOutputLines();
    static const char* kFilters[] = {"Tutti i messaggi", "Messaggi", "Avvisi", "Errori"};
    ImGui::SetNextItemWidth(140.f);
    ImGui::Combo("##outflt", &g_outFilter, kFilters, 4);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.f);
    ImGui::InputTextWithHint("##outsearch", "Filtra", g_outSearch, sizeof(g_outSearch));
    ImGui::SameLine();
    ImGui::Checkbox("Orari", &g_outTimestamps);
    ImGui::SameLine();
    ImGui::Checkbox("Scorri in fondo", &g_outAutoScroll);
    ImGui::SameLine();
    if (ImGui::SmallButton("Pulisci")) { g_lua.printLog.clear(); g_outLines.clear(); g_outSeen = g_lua.printCount; }
    int errs = 0, warns = 0;
    for (const auto& o : g_outLines) { if (o.kind == 2) errs += o.count; else if (o.kind == 1) warns += o.count; }
    ImGui::SameLine();
    ImGui::TextColored(errs ? ImVec4(1.f, 0.45f, 0.4f, 1.f) : ImVec4(0.55f, 0.6f, 0.65f, 1.f), "%d errori", errs);
    ImGui::SameLine();
    ImGui::TextColored(warns ? ImVec4(1.f, 0.82f, 0.35f, 1.f) : ImVec4(0.55f, 0.6f, 0.65f, 1.f), "%d avvisi", warns);

    std::string q = g_outSearch;
    for (char& c : q) c = (char)std::tolower((unsigned char)c);
    ImGui::BeginChild("##outlines", {0.f, std::max(1.f, ImGui::GetContentRegionAvail().y)}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.f;
    for (int i = 0; i < (int)g_outLines.size(); ++i) {
        const OutputLine& o = g_outLines[(size_t)i];
        if (g_outFilter == 1 && o.kind != 0 && o.kind != 3) continue;
        if (g_outFilter == 2 && o.kind != 1) continue;
        if (g_outFilter == 3 && o.kind != 2) continue;
        if (!q.empty()) {
            std::string l = o.text;
            for (char& c : l) c = (char)std::tolower((unsigned char)c);
            if (l.find(q) == std::string::npos) continue;
        }
        ImGui::PushID(i);
        if (g_outTimestamps) {
            const int t = (int)o.time;
            ImGui::TextDisabled("%02d:%02d:%02d.%03d", (t / 3600) % 24, (t / 60) % 60, t % 60, (int)((o.time - t) * 1000.0));
            ImGui::SameLine();
        }
        const ImVec4 col = o.kind == 2 ? ImVec4(1.f, 0.45f, 0.4f, 1.f) : o.kind == 1 ? ImVec4(1.f, 0.82f, 0.35f, 1.f)
                         : o.kind == 3 ? ImVec4(0.55f, 0.75f, 1.f, 1.f) : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        int srcPart = -1, srcLine = 0;
        const bool hasSrc = o.kind == 2 && outputSourceOf(o.text, srcPart, srcLine);
        if (ImGui::Selectable(o.text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && hasSrc) {
            studioMultiClear();
            selectPart(srcPart);
            g_editingPartScript = true;
            openScriptTab(srcPart);
            g_gotoScriptLine = srcLine;
        }
        ImGui::PopStyleColor();
        if (hasSrc && ImGui::IsItemHovered()) ImGui::SetTooltip("Clic: apri lo script alla riga %d", srcLine);
        if (o.count > 1) {
            ImGui::SameLine();
            ImGui::TextDisabled("(x%d)", o.count);
        }
        if (ImGui::BeginPopupContextItem("##outctx")) {
            if (ImGui::MenuItem("Copia riga")) ImGui::SetClipboardText(o.text.c_str());
            if (ImGui::MenuItem("Copia tutto")) {
                std::string all;
                for (const auto& x : g_outLines) { all += x.text; all += '\n'; }
                ImGui::SetClipboardText(all.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Pulisci")) { g_lua.printLog.clear(); g_outLines.clear(); g_outSeen = g_lua.printCount; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (g_outLines.empty()) ImGui::TextDisabled("Qui escono print(), warn() ed errori degli script quando provi il gioco (F5).");
    if (g_outAutoScroll && atBottom) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
}

// ---- Debugger (come il debugger degli script di Roblox Studio) -----------------------------------
// I punti di interruzione (F9 nell'editor) vanno al runtime; quando il Test ci
// passa, lo script si ferma e qui si vedono riga, variabili e chiamate.
// Continua (F8) / Passo (F10) / Ferma (F5 nello Studio ferma il Test). La fisica resta ferma finche' non continui.

static void drawDebuggerWindow() {
    g_lua.debugBreakpoints = g_scriptBreakpoints;   // valgono per gli script caricati da qui in poi
    if (!g_studioTesting) {
        if (g_lua.debugPause.active || g_lua.debugIsPaused()) g_lua.debugAbandon();
        return;
    }
    LuaRuntime::DebugPause& d = g_lua.debugPause;
    if (!d.active) return;
    const bool paused = g_lua.debugIsPaused();
    static bool wasPaused = false;
    if (paused && !wasPaused) g_simPaused = true;   // il mondo si ferma con lo script
    wasPaused = paused;

    ImGui::SetNextWindowSize({420.f, 360.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({(float)g_w - 440.f, 120.f}, ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("Debugger###luadebugger", &open)) {
        if (paused) ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f}, "In pausa: %s, riga %d", d.chunk.c_str(), d.line);
        else ImGui::TextColored({0.6f, 0.75f, 1.f, 1.f}, "Passato da %s, riga %d (%d volte)", d.chunk.c_str(), d.line, d.hits);
        if (!paused)
            ImGui::TextWrapped("Questo punto e' dentro un evento (Touched, Heartbeat...): qui lo script non si puo' fermare, "
                               "ma vedi le variabili di quel momento.");
        ImGui::BeginDisabled(!paused);
        if (ImGui::Button("Continua (F8)") || (paused && ImGui::IsKeyPressed(ImGuiKey_F8, false))) {
            g_simPaused = false;
            g_lua.debugContinue(false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Passo (F10)") || (paused && ImGui::IsKeyPressed(ImGuiKey_F10, false))) g_lua.debugContinue(true);
        ImGui::SameLine();
        if (ImGui::Button("Ferma lo script")) { g_lua.debugAbandon(); g_simPaused = false; }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Apri lo script")) {
            const std::string want = d.chunk.size() > 7 && d.chunk.compare(d.chunk.size() - 7, 7, ".Script") == 0
                                         ? d.chunk.substr(0, d.chunk.size() - 7) : d.chunk;
            for (int i = 0; i < (int)g_world.parts.size(); ++i)
                if (g_world.parts[(size_t)i].name == want && !g_world.parts[(size_t)i].script.empty()) {
                    openScriptTab(i);
                    g_gotoScriptLine = d.line;
                    break;
                }
        }
        ImGui::SeparatorText("Variabili locali");
        if (ImGui::BeginTable("##dbgvars", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY,
                              {0.f, 150.f})) {
            ImGui::TableSetupColumn("Nome", ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("Valore", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableHeadersRow();
            for (const auto& v : d.locals) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(v.name.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(v.value.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("%s", v.type.c_str());
            }
            if (d.locals.empty()) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("(nessuna)"); }
            ImGui::EndTable();
        }
        ImGui::SeparatorText("Chiamate");
        for (const auto& fr : d.stack) ImGui::BulletText("%s", fr.c_str());
    }
    ImGui::End();
    if (!open) {   // chiudere la finestra = continuare
        if (paused) { g_simPaused = false; g_lua.debugContinue(false); }
        d.active = false;
    }
}
