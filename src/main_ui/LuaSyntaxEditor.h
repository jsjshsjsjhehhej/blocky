// Editor Lua con evidenziazione sintattica (libreria ImGuiColorTextEdit) al
// posto del semplice ImGui::InputTextMultiline usato finora per gli script
// di Part/Model e per la finestra "Script (Safe Bytecode)". Non e' un
// header riusabile: si aggancia ai buffer char[] gia' esistenti in
// main.cpp (g_partScriptBuf, g_modelScriptBuf, g_luaEditBuf), che restano
// la sorgente di verita' — tutto il resto del codice (salvataggio, compile,
// preset, sincronizzazione alla selezione) continua a leggerli/scriverli
// esattamente come prima. Compila solo se incluso da main.cpp esattamente
// in questo punto (dopo la dichiarazione di quei tre buffer).

#include "TextEditor.h"

struct LuaScriptEditorState {
    TextEditor editor;
    std::string lastSynced{"\x01"};
    LuaScriptEditorState() {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    }
};

static LuaScriptEditorState g_partScriptEditor;
static LuaScriptEditorState g_modelScriptEditor;
static LuaScriptEditorState g_luaEditEditor;
static LuaScriptEditorState g_labelScriptEditor;

// Sostituisce una ImGui::InputTextMultiline(id, buf, bufSize, {-1,height}, ...):
// stesso contratto (dopo la chiamata `buf` contiene il testo corrente),
// ma con le parole Lua colorate mentre si scrive.
static void drawLuaScriptEditor(LuaScriptEditorState& st, char* buf, size_t bufSize,
                                 const char* imguiId, float height) {
    if (st.lastSynced != buf) {
        st.editor.SetText(buf);
        st.lastSynced = buf;
    }
    st.editor.Render(imguiId, {-1.f, height});
    if (st.editor.IsTextChanged()) {
        std::string cur = st.editor.GetText();
        if (cur.size() >= bufSize) cur.resize(bufSize - 1);
        std::snprintf(buf, bufSize, "%s", cur.c_str());
        st.lastSynced = buf;
    }
}
