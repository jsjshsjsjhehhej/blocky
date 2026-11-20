#pragma once

/**
 * Real Studio dock extras: Assets / Console bottom tabs, window search,
 * Explorer service tree, property fallback. No fake editor windows.
 */

#include <cstdio>
#include <string>

#include "studio/StudioPlatform.h"

static bool g_showWindowSearch = false;
static int  g_bottomDockTab = 0; // 0 Output · 1 Assets · 2 Console
static char g_windowSearchBuf[96] = {};
static char g_consoleInputBuf[256] = {};
static char g_assetFilterBuf[96] = {};

static bool* panelVisibilityFlag(studio::platform::PanelId id) {
    using studio::platform::PanelId;
    switch (id) {
    case PanelId::Explorer: return &g_showExplorer;
    case PanelId::Properties: return &g_showProps;
    case PanelId::Toolbox: return &g_showToolbox;
    case PanelId::Assets: return &g_showAssets;
    case PanelId::ScriptEditor: return &g_showScript;
    case PanelId::Output: return &g_showLuaOutput;
    case PanelId::Console: return &g_showConsole;
    case PanelId::AnimationEditor: return &g_showAnimEditor;
    case PanelId::Profiler: return &g_showProfiler;
    case PanelId::Count: return nullptr;
    }
    return nullptr;
}

static void openPlatformPanel(studio::platform::PanelId id) {
    if (bool* f = panelVisibilityFlag(id)) *f = true;
    using studio::platform::PanelId;
    if (id == PanelId::Output) g_bottomDockTab = 0;
    else if (id == PanelId::Assets) { g_showAssets = true; g_bottomDockTab = 1; }
    else if (id == PanelId::Console) { g_showConsole = true; g_bottomDockTab = 2; }
}

static void drawWindowSearchPopup() {
    if (!g_showWindowSearch) return;
    ImGui::OpenPopup("Window Search##winsrch");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({400.f, 320.f}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Window Search##winsrch", &g_showWindowSearch,
                               ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##winsrchq", "Search windows...",
                                 g_windowSearchBuf, sizeof(g_windowSearchBuf));
        ImGui::Separator();
        ImGui::BeginChild("##winsrchlist", {0, -36}, false);
        int n = 0;
        const auto* all = studio::platform::panels(&n);
        for (int i = 0; i < n; ++i) {
            if (g_windowSearchBuf[0]) {
                std::string hay = std::string(all[i].title) + " " + all[i].searchKeys;
                for (char& ch : hay) ch = (char)std::tolower((unsigned char)ch);
                std::string q = g_windowSearchBuf;
                for (char& ch : q) ch = (char)std::tolower((unsigned char)ch);
                if (hay.find(q) == std::string::npos) continue;
            }
            if (ImGui::Selectable(all[i].title)) {
                openPlatformPanel(all[i].id);
                g_showWindowSearch = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Close", {-1, 0})) {
            g_showWindowSearch = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void drawServicePropertyFallback(int svcId) {
    const auto* info = studio::platform::findService(svcId);
    if (!info) {
        ImGui::TextDisabled("Service id %d", svcId);
        return;
    }
    ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "Services · %s", info->name);
    ImGui::TextDisabled("%s", info->category);
    ImGui::Separator();
    ImGui::TextWrapped("%s", info->hint);
    ImGui::TextDisabled("Luau: game:GetService(\"%s\")", info->luaName);
}

static int g_selectedSharedInstance = -1;

static void drawSharedServiceChildren(SharedService svc) {
    ImGui::Indent();
    for (int i = 0; i < (int)g_world.instances.size(); ++i) {
        const auto& s = g_world.instances[i];
        if (s.service != svc || s.parentIndex >= 0) continue;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s [%s]##sh%d", s.name.c_str(), s.className.c_str(), i);
        if (ImGui::Selectable(buf, g_selectedSharedInstance == i)) {
            g_selectedSharedInstance = i;
            g_showProps = true;
        }
        // Nested children
        ImGui::Indent();
        for (int j = 0; j < (int)g_world.instances.size(); ++j) {
            const auto& c = g_world.instances[j];
            if (c.service != svc || c.parentIndex != i) continue;
            char cbuf[128];
            std::snprintf(cbuf, sizeof(cbuf), "%s [%s]##sh%d", c.name.c_str(), c.className.c_str(), j);
            if (ImGui::Selectable(cbuf, g_selectedSharedInstance == j)) {
                g_selectedSharedInstance = j;
                g_showProps = true;
            }
        }
        ImGui::Unindent();
    }
    ImGui::Unindent();
}

static void drawOutputPanelPro();   // StudioToolsPro.h

static void drawExplorerServiceTree() {
    using namespace studio::platform;
    auto svc = [](int id, const char* label) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s##svc%d", label, id);
        if (ImGui::Selectable(buf, g_selectedService == id))
            toggleService(id);
    };

    svc(Svc_Lighting, "Lighting");
    svc(Svc_RunService, "RunService");
    svc(Svc_TweenService, "TweenService");
    svc(Svc_UserInputService, "UserInputService");
    svc(Svc_ReplicatedStorage, "ReplicatedStorage");
    if (g_selectedService == Svc_ReplicatedStorage)
        drawSharedServiceChildren(SharedService::ReplicatedStorage);
    svc(Svc_ServerStorage, "ServerStorage");
    if (g_selectedService == Svc_ServerStorage)
        drawSharedServiceChildren(SharedService::ServerStorage);
    svc(Svc_ServerScriptService, "ServerScriptService");
    if (g_selectedService == Svc_ServerScriptService)
        drawSharedServiceChildren(SharedService::ServerScriptService);
    svc(Svc_DataStoreService, "DataStoreService");
    svc(Svc_CollectionService, "CollectionService");
    svc(Svc_Debris, "Debris");
    svc(Svc_Chat, "Chat");
}

static void drawAssetsBrowserBody() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##assetfilter", "Search assets...",
                             g_assetFilterBuf, sizeof(g_assetFilterBuf));
    ImGui::Separator();

    if (ImGui::BeginTabBar("##assetcats")) {
        if (ImGui::BeginTabItem("Models")) {
            ImGui::TextDisabled("%d models", (int)g_world.models.size());
            for (int i = 0; i < (int)g_world.models.size(); ++i) {
                auto& m = g_world.models[i];
                if (g_assetFilterBuf[0] && m.name.find(g_assetFilterBuf) == std::string::npos)
                    continue;
                if (ImGui::Selectable(m.name.c_str(), g_selectedModel == i)) {
                    g_selectedModel = i;
                    g_showProps = true;
                }
            }
            if (ImGui::Button("+ Import Model", {-1, 0})) insertImportedModel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sounds")) {
            for (int i = 0; i < (int)g_sounds.sounds.size(); ++i) {
                auto& s = g_sounds.sounds[i];
                if (g_assetFilterBuf[0] && s.name.find(g_assetFilterBuf) == std::string::npos)
                    continue;
                if (ImGui::Selectable(s.name.c_str(), g_selectedSound == i)) {
                    selectSound(i);
                    g_showProps = true;
                }
            }
            if (ImGui::Button("+ Sound", {-1, 0})) insertSound();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scripts")) {
            if (ImGui::Button("Open Script Editor", {-1, 0})) g_showScript = true;
            if (ImGui::Button("Add Script to selection", {-1, 0})) addScriptToSelected();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void drawConsoleBody() {
    ImGui::TextDisabled("Console — log command (see Output)");
    if (ImGui::InputText("##conin", g_consoleInputBuf, sizeof(g_consoleInputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (g_consoleInputBuf[0]) {
            g_lua.printLog.push_back(std::string("> ") + g_consoleInputBuf);
            g_consoleInputBuf[0] = 0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Run") && g_consoleInputBuf[0]) {
        g_lua.printLog.push_back(std::string("> ") + g_consoleInputBuf);
        g_consoleInputBuf[0] = 0;
    }
}

static void drawBottomDockTabs(float x, float y, float w, float h,
                               ImGuiWindowFlags flags) {
    if (h < 40.f || w < 64.f) return;
    const bool any = g_showLuaOutput || g_showAssets || g_showConsole;
    if (!any) return;

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.14f, 0.14f, 0.15f, 1.f});
    bool open = true;
    ImGui::Begin("##BottomDock", &open,
                 flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::BeginTabBar("##bottomdocktabs", ImGuiTabBarFlags_Reorderable)) {
        if (g_showLuaOutput && ImGui::BeginTabItem("Output")) {
            g_bottomDockTab = 0;
            drawOutputPanelPro();   // StudioToolsPro.h: Output come Roblox Studio
            ImGui::EndTabItem();
        }
        if (g_showAssets && ImGui::BeginTabItem("Assets")) {
            g_bottomDockTab = 1;
            drawAssetsBrowserBody();
            ImGui::EndTabItem();
        }
        if (g_showConsole && ImGui::BeginTabItem("Console")) {
            g_bottomDockTab = 2;
            drawConsoleBody();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

static void drawViewMenuPlatformItems() {
    ImGui::Separator();
    ImGui::MenuItem("Toolbox", nullptr, &g_showToolbox);
    ImGui::MenuItem("Assets", nullptr, &g_showAssets);
    ImGui::MenuItem("Console", nullptr, &g_showConsole);
    ImGui::Separator();
    if (ImGui::MenuItem("Window Search...", "Ctrl+Shift+P"))
        g_showWindowSearch = true;
}
