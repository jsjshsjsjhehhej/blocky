#pragma once


#include <imgui.h>

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#endif

#include "engine/LuaRuntime.h"
#include "platform/CrashHandler.h"
#include "studio/StudioIcons.h"
#include "studio/StudioPanels.h"
#include "studio/StudioState.h"
#include "ui/StudioChrome.h"

namespace studio {

struct StdoutCapture {
#ifdef _WIN32
    int readFd = -1;
    int writeFd = -1;
    int savedStdout = -1;
#endif
    bool active = false;

    void begin() {
#ifdef _WIN32
        if (active) return;
        int fds[2];
        if (_pipe(fds, 1 << 16, _O_BINARY) != 0) return;
        readFd = fds[0];
        writeFd = fds[1];
        savedStdout = _dup(_fileno(stdout));
        if (_dup2(writeFd, _fileno(stdout)) != 0) return;
        setvbuf(stdout, nullptr, _IONBF, 0);
        active = true;
#endif
    }

    void drain(OutputLog& log) {
#ifdef _WIN32
        if (!active) return;

        HANDLE h = (HANDLE)_get_osfhandle(readFd);
        if (h == INVALID_HANDLE_VALUE) return;

        DWORD available = 0;
        if (!PeekNamedPipe(h, nullptr, 0, nullptr, &available, nullptr) || available == 0) return;

        std::string buf;
        buf.resize(available);
        int got = _read(readFd, buf.data(), (unsigned)available);
        if (got <= 0) return;
        buf.resize((size_t)got);

        size_t start = 0;
        while (start < buf.size()) {
            size_t nl = buf.find('\n', start);
            if (nl == std::string::npos) nl = buf.size();
            std::string line = buf.substr(start, nl - start);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (!line.empty()) log.add(LogLevel::Script, "%s", line.c_str());
            start = nl + 1;
        }
#else
        (void)log;
#endif
    }

    void end() {
#ifdef _WIN32
        if (!active) return;
        _dup2(savedStdout, _fileno(stdout));
        _close(savedStdout);
        _close(readFd);
        _close(writeFd);
        active = false;
#endif
    }
};

struct ScriptEditor {
    LuaRuntime lua;
    StdoutCapture capture;
    std::vector<char> buffer;
    int boundPart = -1;
    bool runtimeReady = false;

    ScriptEditor() { buffer.resize(16 * 1024, '\0'); }

    void start(DataModel& world, OutputLog& log) {
        capture.begin();
        runtimeReady = lua.init(&world);
        if (runtimeReady) log.info("LuaRuntime pronto (sandbox, budget %d us/frame)", lua.budgetUs);
        else log.add(LogLevel::Error, "LuaRuntime non inizializzato: %s", lua.lastError.c_str());
    }

    void stop() { capture.end(); }

    void syncTo(StudioState& s) {
        if (s.selected == boundPart) return;
        boundPart = s.selected;

        std::fill(buffer.begin(), buffer.end(), '\0');
        if (const WorkspacePart* p = s.selectedPart()) {
            const size_t n = std::min(p->script.size(), buffer.size() - 1);
            std::copy(p->script.begin(), p->script.begin() + (long)n, buffer.begin());
        }
    }

    void run(StudioState& s) {
        if (!runtimeReady) {
            s.output.add(LogLevel::Error, "LuaRuntime non disponibile");
            return;
        }
        const std::string code(buffer.data());
        if (code.empty()) {
            s.output.add(LogLevel::Warn, "Script vuoto");
            return;
        }

        {
            std::string crumb = "esegue script";
            if (const WorkspacePart* p = s.selectedPart()) crumb += " di " + p->name;
            crumb += " (" + std::to_string(code.size()) + " byte)";
            CrashHandler::note(crumb.c_str());
        }

        s.output.add(LogLevel::Script, "--- esecuzione ---");
        const bool ok = (s.selected >= 0) ? lua.runPartScript(code, s.selected)
                                          : lua.runString(code);
        capture.drain(s.output);

        if (ok) s.output.add(LogLevel::Script, "--- ok ---");
        else s.output.add(LogLevel::Error, "%s", lua.lastError.c_str());
    }
};

inline void drawScriptPanel(StudioState& s, ScriptEditor& editor) {
    editor.syncTo(s);

    WorkspacePart* part = s.selectedPart();

    if (studioAccentButton("Esegui", {84, 26}, {0.18f, 0.52f, 0.28f, 1.f}))
        editor.run(s);
    ImGui::SameLine();

    if (studioAccentButton("Salva su Part", {120, 26}, {0.20f, 0.36f, 0.58f, 1.f})) {
        if (part) {
            part->script = editor.buffer.data();
            s.dirty = true;
            s.output.info("Script salvato su %s", part->name.c_str());
        } else {
            s.output.add(LogLevel::Warn, "Nessuna Part selezionata");
        }
    }
    ImGui::SameLine();

    if (part) {
        ImGui::SameLine();
        ImGui::TextDisabled("Script di %s", part->name.c_str());

        // Script (gira lato server) vs LocalScript (gira lato client), come
        // in Roblox — prima il tipo esisteva solo come dato salvato
        // (ScriptType) ma non si poteva scegliere da nessuna UI.
        static const char* const kTypes[] = {"Script (Legacy)", "Script", "LocalScript"};
        int typeIdx = (int)part->scriptType;
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::Combo("Tipo", &typeIdx, kTypes, IM_ARRAYSIZE(kTypes))) {
            part->scriptType = (ScriptType)typeIdx;
            s.dirty = true;
        }
    } else {
        ImGui::TextDisabled("Nessuna Part — lo script gira sul mondo");
    }

    ImGui::InputTextMultiline("##code", editor.buffer.data(), editor.buffer.size(),
                              {-FLT_MIN, -FLT_MIN}, ImGuiInputTextFlags_AllowTabInput);
}

}
