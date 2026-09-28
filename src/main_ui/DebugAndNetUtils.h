// Pannello diagnostico animazioni (F3) e apertura/chiusura porta UPnP per
// l'hosting LAN. Non e' un header riusabile: usa direttamente le globali
// di main.cpp (g_animator, g_menu, ecc.). Compila solo se incluso da
// main.cpp esattamente in questo punto.

// debugMode in assets/config/config.json decide se parte gia' acceso —
// prima dimostrazione reale che i flag esterni funzionano, non solo un
// nome in un file che nessuno legge.
static bool g_showAnimDebug = configFlags().get("debugMode", false);

static void drawAnimDebugPanel() {
    if (!g_showAnimDebug) return;

    const auto& h = g_player.humanoid;
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowPos({12, 90}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Diagnostica animazioni (F3)", &g_showAnimDebug,
                     ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Clip attiva      : %s", animClipName(g_animator.lastActive));
        ImGui::Separator();

        ImGui::TextColored(h.grounded ? ImVec4{1, 0.6f, 0.3f, 1} : ImVec4{0.4f, 1, 0.5f, 1},
                           "grounded        : %s", h.grounded ? "SI (a terra)" : "NO (in aria)");
        ImGui::Text("velocityY       : %.2f", h.velocityY);
        ImGui::Text("jumpAnimTime    : %.3f s", h.jumpAnimTime);
        ImGui::Text("jumpArmBlend    : %.3f", h.jumpArmBlend);
        ImGui::Separator();

        ImGui::Text("animator.playing: %s", g_animator.playing ? "si" : "NO");
        ImGui::Text("customClip      : %s%s", g_animator.hasCustomClip ? "si" : "no",
                    g_animator.hasCustomClip
                        ? (g_animator.customClip.looped ? " (loopata)" : " (one-shot)") : "");
        ImGui::Text("jumpOverride    : %s", g_animator.hasJumpOverride
                        ? (g_animator.jumpOverride.valid() ? "si (valida)" : "si (NON valida)")
                        : "no (posa procedurale)");
        ImGui::Separator();

        for (const auto& p : h.parts) {
            if (!p.isLimb) continue;
            ImGui::Text("%-10s rotX %7.2f  rotZ %7.2f", p.name.c_str(), p.rotX, p.rotZ);
        }
    }
    ImGui::End();
}

static std::atomic<bool> g_upnpBusy{false};
static std::atomic<bool> g_upnpDone{false};
static std::string g_upnpExternalIp;
static std::string g_upnpError;
static int g_upnpPort = 0;

static void openPortAsync(int port) {
    if (g_upnpBusy.load()) return;
    g_upnpBusy.store(true);
    g_upnpDone.store(false);
    g_upnpPort = port;

    std::thread([port]() {
        PortMap::Result res = PortMap::open(port, "Blocky");
        g_upnpExternalIp = res.externalIp;
        g_upnpError = res.mapped ? std::string() : res.error;
        g_upnpDone.store(true);
        g_upnpBusy.store(false);
    }).detach();
}

static void pollPortMapStatus(char* statusOut, size_t statusSize) {
    static bool reported = false;
    if (!g_upnpDone.load()) { reported = false; return; }
    if (reported) return;
    reported = true;

    if (!g_upnpError.empty()) {
        std::snprintf(statusOut, statusSize,
                      "Porta non aperta: %s — serve il port forwarding manuale",
                      g_upnpError.c_str());
    } else if (!g_upnpExternalIp.empty()) {
        std::snprintf(statusOut, statusSize,
                      "Server aperto — gli amici si collegano a %s:%d",
                      g_upnpExternalIp.c_str(), g_upnpPort);
    } else {
        std::snprintf(statusOut, statusSize, "Porta %d aperta sul router", g_upnpPort);
    }
}

static void closePortIfOpen() {
    if (g_upnpPort <= 0) return;
    const int port = g_upnpPort;
    g_upnpPort = 0;
    g_upnpDone.store(false);
    std::thread([port]() { PortMap::close(port); }).detach();
}
