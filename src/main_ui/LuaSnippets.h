// Libreria di blocchi Lua pronti da inserire nell'editor con un clic, invece
// di scriverli a memoria. Non e' un header riusabile: si aggancia a
// drawLuaScriptEditor() (LuaSyntaxEditor.h) esattamente come gli altri file
// di main_ui/. Compila solo se incluso da main.cpp nello stesso punto.
//
// ── Perche' esiste ──────────────────────────────────────────────────────
// Studio aveva 5 generatori scritti in C++, uno per idea (salto, esplosione,
// neon, arma, vuoto): per aggiungerne uno nuovo bisognava ricompilare il
// motore. E per tutto il resto — un timer, un tocco che da monete, un
// teleport — non c'era niente: bisognava saperlo scrivere a memoria, con
// l'API giusta, senza sbagliare una virgola.
//
// Qui i blocchi sono DATI, non codice C++: una tabella di stringhe. Se ne
// puo' aggiungere uno nuovo modificando un array, senza toccare il resto del
// motore. E soprattutto: ogni frammento sotto e' preso o adattato da uno
// script che passa gia' test_lua (vedi assets/scripts/lua_api_completo.lua,
// monete_salvate.lua, explode_part.lua) — non e' API inventata a memoria,
// perche' un frammento sbagliato sarebbe peggio di doverlo scrivere a mano:
// sembrerebbe funzionare finche' non lo si prova.

struct LuaSnippet {
    const char* category;
    const char* title;
    const char* hint;   // una riga: cosa fa, per scegliere senza doverlo leggere
    const char* code;
};

// clang-format off
static const LuaSnippet kLuaSnippets[] = {

{"Tocco", "Raccogli al tocco",
 "Sparisce e stampa un messaggio quando qualcuno la tocca",
R"LUA(part.Touched:Connect(function(other)
    print("Toccato da: " .. tostring(other))
    part:Destroy()
end)
)LUA"},

{"Tocco", "Esplode al tocco",
 "part:Explode() quando qualcuno la tocca — vedi anche il preset Esplosione",
R"LUA(part.Touched:Connect(function()
    part:Explode()
end)
)LUA"},

{"Tocco", "Da monete salvate",
 "DataStore: aggiunge monete al tocco, persistono chiudendo il gioco",
R"LUA(local store = game:GetService("DataStoreService"):GetDataStore("Monete")
local IMPORTO = 10

part.Touched:Connect(function()
    local monete = (store:GetAsync("totale") or 0) + IMPORTO
    store:SetAsync("totale", monete)
    game.Particles:Coins(part.Position)
    print("Monete: " .. monete)
end)
)LUA"},

{"Tempo", "Ripeti ogni N secondi",
 "task.spawn + task.wait: un ciclo che si ripete da solo",
R"LUA(task.spawn(function()
    while true do
        task.wait(5)
        print("Sono passati 5 secondi")
    end
end)
)LUA"},

{"Tempo", "Aspetta e poi fai",
 "task.delay: esegue una volta sola dopo N secondi",
R"LUA(task.delay(3, function()
    print("Sono passati 3 secondi")
end)
)LUA"},

{"Tempo", "A ogni frame (Heartbeat)",
 "Gira ogni frame — muovere qualcosa di continuo, max 64 heartbeat attivi",
R"LUA(game.RunService.Heartbeat:Connect(function(dt)
    -- dt = secondi dall'ultimo frame
end)
)LUA"},

{"Movimento", "Teletrasporta",
 "Sposta una Part in un punto preciso",
R"LUA(part:SetPosition(Vector3.new(0, 5, 0))
)LUA"},

{"Movimento", "Sali e scendi (oscilla)",
 "Effetto piattaforma fluttuante, con Heartbeat",
R"LUA(local base = part:GetPosition()
local ALTEZZA = 3
local VELOCITA = 1.5
local t = 0

game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    local y = base.y + math.sin(t * VELOCITA) * ALTEZZA
    part:SetPosition(Vector3.new(base.x, y, base.z))
end)
)LUA"},

{"Effetti", "Scintille + suono",
 "Un piccolo impatto: scintille sul punto e un suono",
R"LUA(game.Particles:Sparks(part:GetPosition(), 6)
game.SoundService:Play("assets/sounds/beep.wav", false)
)LUA"},

{"Effetti", "Scossa camera",
 "Camera.Shake(intensita, durata) — per esplosioni o impatti",
R"LUA(Camera.Shake(5.0, 0.25)
)LUA"},

{"Effetti", "Fumo continuo",
 "Un pennacchio di fumo dalla Part, finche' non si ferma a mano",
R"LUA(game.Particles:Smoke(part:GetPosition(), 3)
)LUA"},

{"Giocatore", "Cambia velocita' di corsa",
 "Humanoid.SetWalkSpeed — limite 28",
R"LUA(Humanoid.SetWalkSpeed(20)
)LUA"},

{"Giocatore", "Disattiva il salto",
 "Utile per un'area, un minigioco o una cutscene",
R"LUA(Humanoid.SetJumpEnabled(false)
-- Humanoid.SetJumpEnabled(true) per riattivarlo
)LUA"},

{"Giocatore", "Fai danno",
 "Humanoid.TakeDamage — per trappole o armi",
R"LUA(Humanoid.TakeDamage(10)
)LUA"},

{"Rete", "Evento personalizzato",
 "Events.Create/Connect/Fire — comunicazione fra script dello stesso posto",
R"LUA(Events.Create("MioEvento")
Events.Connect("MioEvento", function(...)
    print("MioEvento ricevuto:", ...)
end)

-- altrove, per farlo scattare:
-- Events.Fire("MioEvento", "un dato")
)LUA"},

{"Rete", "Raggio (raycast)",
 "workspace:Raycast — per armi, mira, rilevare cosa c'e' davanti",
R"LUA(local origine = Humanoid.GetPosition() + Vector3.new(0, 2, 0)
local direzione = Humanoid.GetLook()
local hit = workspace:Raycast(origine, direzione, 50)
if hit and hit.Instance then
    print("Colpito: " .. hit.Instance.Name)
end
)LUA"},

};
// clang-format on

static const int kLuaSnippetCount = (int)(sizeof(kLuaSnippets) / sizeof(kLuaSnippets[0]));

/**
 * Il pulsante "Inserisci blocco...", da disegnare accanto a un editor Lua.
 *
 * Inserisce al CURSORE (TextEditor::InsertText), non in coda al testo: cosi'
 * si puo' mettere un blocco esattamente dove si sta scrivendo, non solo
 * aggiungerlo alla fine. Dopo l'inserimento risincronizza st.lastSynced,
 * altrimenti drawLuaScriptEditor() al giro successivo vedrebbe buf diverso
 * dall'editor e riscriverebbe sopra il testo appena inserito.
 */
static void drawSnippetInserter(LuaScriptEditorState& st, char* buf, size_t bufSize) {
    if (ImGui::Button(LB("btn.insert_snippet", "inssnip"))) {
        ImGui::OpenPopup("##snippetPicker");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Blocchi di codice pronti: si inseriscono dove sta il cursore.");

    if (ImGui::BeginPopup("##snippetPicker")) {
        const char* lastCat = nullptr;
        for (int i = 0; i < kLuaSnippetCount; ++i) {
            const LuaSnippet& s = kLuaSnippets[i];
            if (!lastCat || std::strcmp(lastCat, s.category) != 0) {
                if (lastCat) ImGui::Separator();
                ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "%s", s.category);
                lastCat = s.category;
            }
            if (ImGui::Selectable(s.title)) {
                st.editor.InsertText(s.code);
                std::string cur = st.editor.GetText();
                if (cur.size() >= bufSize) cur.resize(bufSize - 1);
                std::snprintf(buf, bufSize, "%s", cur.c_str());
                st.lastSynced = buf;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s.hint);
        }
        ImGui::EndPopup();
    }
}
