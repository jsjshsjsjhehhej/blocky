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

#include <cctype>
#include <cstring>
#include <string>
#include <unordered_set>

// Colori dello Script Editor di Roblox Studio (tema scuro di default):
//   keyword rosa, stringa verde, numero oro, commento grigio,
//   builtin e proprietà azzurri, metodo e nome di funzione gialli,
//   il resto del testo grigio chiaro. Gli operatori non urlano.
static ImU32 rbRgb(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

static TextEditor::Palette robloxScriptPalette(bool light) {
    TextEditor::Palette p = light ? TextEditor::GetLightPalette() : TextEditor::GetDarkPalette();
    auto set = [&](TextEditor::PaletteIndex i, int r, int g, int b, int a = 255) {
        p[(int)i] = rbRgb(r, g, b, a);
    };
    if (!light) {
        set(TextEditor::PaletteIndex::Default, 204, 204, 204);
        set(TextEditor::PaletteIndex::Keyword, 248, 109, 124);
        set(TextEditor::PaletteIndex::Number, 255, 198, 0);
        set(TextEditor::PaletteIndex::String, 173, 241, 149);
        set(TextEditor::PaletteIndex::CharLiteral, 132, 214, 247);
        set(TextEditor::PaletteIndex::Punctuation, 204, 204, 204);
        set(TextEditor::PaletteIndex::Preprocessor, 204, 204, 204);
        set(TextEditor::PaletteIndex::Identifier, 204, 204, 204);
        set(TextEditor::PaletteIndex::KnownIdentifier, 132, 214, 247);
        set(TextEditor::PaletteIndex::PreprocIdentifier, 253, 251, 172);
        set(TextEditor::PaletteIndex::Comment, 102, 102, 102);
        set(TextEditor::PaletteIndex::MultiLineComment, 102, 102, 102);
        set(TextEditor::PaletteIndex::Background, 30, 30, 30);
        set(TextEditor::PaletteIndex::Cursor, 255, 255, 255);
        set(TextEditor::PaletteIndex::Selection, 38, 79, 120, 170);
        set(TextEditor::PaletteIndex::LineNumber, 110, 110, 110);
        set(TextEditor::PaletteIndex::CurrentLineFill, 255, 255, 255, 14);
        set(TextEditor::PaletteIndex::CurrentLineFillInactive, 255, 255, 255, 8);
        set(TextEditor::PaletteIndex::CurrentLineEdge, 255, 255, 255, 24);
    } else {
        set(TextEditor::PaletteIndex::Default, 32, 32, 32);
        set(TextEditor::PaletteIndex::Keyword, 196, 42, 72);
        set(TextEditor::PaletteIndex::Number, 156, 98, 0);
        set(TextEditor::PaletteIndex::String, 28, 120, 48);
        set(TextEditor::PaletteIndex::CharLiteral, 16, 110, 176);
        set(TextEditor::PaletteIndex::Punctuation, 32, 32, 32);
        set(TextEditor::PaletteIndex::Preprocessor, 32, 32, 32);
        set(TextEditor::PaletteIndex::Identifier, 32, 32, 32);
        set(TextEditor::PaletteIndex::KnownIdentifier, 16, 110, 176);
        set(TextEditor::PaletteIndex::PreprocIdentifier, 140, 104, 0);
        set(TextEditor::PaletteIndex::Comment, 120, 120, 120);
        set(TextEditor::PaletteIndex::MultiLineComment, 120, 120, 120);
        set(TextEditor::PaletteIndex::Background, 250, 250, 250);
        set(TextEditor::PaletteIndex::Cursor, 0, 0, 0);
        set(TextEditor::PaletteIndex::Selection, 180, 210, 245, 180);
        set(TextEditor::PaletteIndex::LineNumber, 140, 140, 140);
    }
    return p;
}

static bool luauIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool luauTokenize(const char* begin, const char* end,
                         const char*& outBegin, const char*& outEnd,
                         TextEditor::PaletteIndex& color) {
    if (begin >= end) return false;
    // Ogni riga è un buffer nuovo: se il puntatore non continua, è un'altra riga.
    static const char* atteso = nullptr;
    static const char* inizioRiga = nullptr;
    if (begin != atteso) inizioRiga = begin;

    const char* s = begin;
    auto chiudi = [&](const char* e, TextEditor::PaletteIndex c) {
        outBegin = begin;
        outEnd = e;
        color = c;
        atteso = e;
        return true;
    };

    if (*s == ' ' || *s == '\t') {
        while (s < end && (*s == ' ' || *s == '\t')) ++s;
        return chiudi(s, TextEditor::PaletteIndex::Default);
    }
    if (*s == '-' && s + 1 < end && s[1] == '-')
        return chiudi(end, TextEditor::PaletteIndex::Comment);

    if (*s == '"' || *s == '\'' || *s == '`') {
        const char q = *s++;
        while (s < end && *s != q) {
            if (*s == '\\' && s + 1 < end) s += 2;
            else ++s;
        }
        if (s < end) ++s;
        return chiudi(s, TextEditor::PaletteIndex::String);
    }
    if (*s == '[' && s + 1 < end && s[1] == '[') {
        s += 2;
        while (s + 1 < end && !(*s == ']' && s[1] == ']')) ++s;
        if (s + 1 < end) s += 2;
        else s = end;
        return chiudi(s, TextEditor::PaletteIndex::String);
    }

    const bool numero = (*s >= '0' && *s <= '9')
        || (*s == '.' && s + 1 < end && s[1] >= '0' && s[1] <= '9');
    if (numero) {
        if (*s == '0' && s + 1 < end && (s[1] == 'x' || s[1] == 'X' || s[1] == 'b' || s[1] == 'B')) {
            const bool hex = s[1] == 'x' || s[1] == 'X';
            s += 2;
            while (s < end && (*s == '_' || (hex ? std::isxdigit((unsigned char)*s)
                                                 : (*s == '0' || *s == '1'))))
                ++s;
        } else {
            while (s < end && ((*s >= '0' && *s <= '9') || *s == '_')) ++s;
            if (s < end && *s == '.') {
                ++s;
                while (s < end && ((*s >= '0' && *s <= '9') || *s == '_')) ++s;
            }
            if (s < end && (*s == 'e' || *s == 'E')) {
                ++s;
                if (s < end && (*s == '+' || *s == '-')) ++s;
                while (s < end && ((*s >= '0' && *s <= '9') || *s == '_')) ++s;
            }
        }
        return chiudi(s, TextEditor::PaletteIndex::Number);
    }

    if ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_') {
        const char* e = s;
        while (e < end && luauIdentChar(*e)) ++e;
        static const std::unordered_set<std::string> chiavi = {
            "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
            "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true",
            "until", "while", "continue", "export", "type", "self"
        };
        static const std::unordered_set<std::string> builtin = {
            "game", "workspace", "script", "plugin", "shared",
            "Instance", "Vector2", "Vector3", "CFrame", "Color3", "BrickColor",
            "UDim", "UDim2", "Enum", "Ray", "TweenInfo", "NumberRange", "NumberSequence",
            "ColorSequence", "Random", "OverlapParams", "RaycastParams", "DateTime",
            "buffer", "vector", "math", "string", "table", "task", "coroutine", "os",
            "debug", "utf8", "bit32", "print", "warn", "error", "assert", "pairs", "ipairs",
            "next", "select", "tostring", "tonumber", "typeof", "require", "pcall", "xpcall",
            "unpack", "tick", "time", "wait", "spawn", "delay", "rawget", "rawset",
            "rawequal", "getmetatable", "setmetatable"
        };
        const std::string id(s, e);
        const char* p = s;
        while (p > inizioRiga && (p[-1] == ' ' || p[-1] == '\t')) --p;
        auto parolaPrima = [&](const char* w) {
            const size_t n = std::strlen(w);
            if ((size_t)(p - inizioRiga) < n) return false;
            if (std::memcmp(p - n, w, n) != 0) return false;
            return p - n == inizioRiga || !luauIdentChar((p - n)[-1]);
        };
        TextEditor::PaletteIndex c = TextEditor::PaletteIndex::Identifier;
        if (chiavi.count(id)) c = TextEditor::PaletteIndex::Keyword;
        else if (p > inizioRiga && p[-1] == ':' )
            c = TextEditor::PaletteIndex::PreprocIdentifier;
        else if (p > inizioRiga && p[-1] == '.' && !(p - 1 > inizioRiga && p[-2] == '.'))
            c = TextEditor::PaletteIndex::KnownIdentifier;
        else if (parolaPrima("function"))
            c = TextEditor::PaletteIndex::PreprocIdentifier;
        else if (builtin.count(id))
            c = TextEditor::PaletteIndex::KnownIdentifier;
        return chiudi(e, c);
    }

    ++s;
    while (s < end && !luauIdentChar(*s) && *s != ' ' && *s != '\t' && *s != '"' && *s != '\''
           && *s != '`' && *s != '-' && !(*s >= '0' && *s <= '9'))
        ++s;
    return chiudi(s, TextEditor::PaletteIndex::Punctuation);
}

static TextEditor::LanguageDefinition luauLanguage() {
    TextEditor::LanguageDefinition lang;
    lang.mName = "Luau";
    lang.mTokenize = luauTokenize;
    lang.mCommentStart = "--[[";
    lang.mCommentEnd = "]]";
    lang.mSingleLineComment = "--";
    lang.mCaseSensitive = true;
    lang.mAutoIndentation = false;
    return lang;
}

struct LuaScriptEditorState {
    TextEditor editor;
    std::string lastSynced{"\x01"};
    LuaScriptEditorState() {
        editor.SetLanguageDefinition(luauLanguage());
    }
};

#include "main_ui/ScriptAssist.h"   // completamento, errori, trova, commenta

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
    scriptAssistEditor(st.editor, imguiId, height);   // ScriptAssist.h
    if (st.editor.IsTextChanged()) {
        std::string cur = st.editor.GetText();
        if (cur.size() >= bufSize) cur.resize(bufSize - 1);
        std::snprintf(buf, bufSize, "%s", cur.c_str());
        st.lastSynced = buf;
    }
}
