// Callback errori GLFW e aggiornamento del contesto per il CrashHandler
// (schermata/place/utente/fps correnti). Non e' un header riusabile: usa
// direttamente le globali di main.cpp (g_screen, g_world, g_menu, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

static void onGlfwError(int code, const char* desc) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "GLFW error %d: %s", code, desc ? desc : "?");
    CrashHandler::note(buf);
    std::fprintf(stderr, "[GLFW] %s\n", buf);
}

static void updateCrashContext() {
    const char* screen = "Unknown";
    switch (g_screen) {
    case AppScreen::MainMenu: screen = "MainMenu"; break;
    case AppScreen::Loading: screen = "Loading"; break;
    case AppScreen::Playing: screen = "Playing"; break;
    case AppScreen::Studio: screen = g_studioTesting ? "Studio/Test" : "Studio"; break;
    case AppScreen::AvatarPublisher: screen = "AvatarPublisher"; break;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s | place=%s | user=%s | fps=%.0f",
                  screen,
                  g_world.placeName.empty() ? "-" : g_world.placeName.c_str(),
                  g_menu.username[0] ? g_menu.username : "-",
                  g_fps);
    CrashHandler::setContext(buf);
}

/**
 * La tabella "dove finiscono i millisecondi".
 *
 * E' la risposta alla domanda che gli FPS non possono soddisfare: non "quanto
 * va veloce", ma "che cosa lo sta rallentando". Si legge la colonna Media per
 * capire il costo normale, e la colonna Picco per capire gli scatti — sono
 * due problemi diversi e si curano in modi diversi.
 *
 * La riga "Non misurato" e' la piu' importante: se e' grande, il tempo se ne
 * va dove non stiamo guardando. Se e' grande INSIEME a Presentazione, il
 * collo di bottiglia e' la GPU e ottimizzare il codice C++ non servira' a
 * niente.
 */
inline void drawFrameProfiler() {
    using namespace plat::prof;

    ImGui::SetNextWindowBgAlpha(0.85f);
    if (!ImGui::Begin("Prestazioni", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    const State& s = state();
    const float target = g_vsync ? 16.67f : 0.f;

    ImGui::Text("Frame: %.2f ms  (%.0f fps)", s.frameAvgMs,
                s.frameAvgMs > 0.f ? 1000.f / s.frameAvgMs : 0.f);
    if (target > 0.f) {
        ImGui::SameLine();
        ImGui::TextColored(s.frameAvgMs <= target ? ImVec4{0.4f, 0.9f, 0.5f, 1.f}
                                                  : ImVec4{0.95f, 0.5f, 0.35f, 1.f},
                           s.frameAvgMs <= target ? "  entro i 60 fps" : "  sopra il budget");
    }
    ImGui::Separator();

    if (ImGui::BeginTable("fasi", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Fase");
        ImGui::TableSetupColumn("Media");
        ImGui::TableSetupColumn("Picco");
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)Phase::Count; ++i) {
            const PhaseStat& ph = s.phase[i];
            // Una fase mai misurata non si mostra: una riga a zero fa credere
            // che sia gratis, mentre invece non la stiamo guardando affatto.
            if (ph.avgMs <= 0.f && ph.peakMs <= 0.f) continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(phaseName((Phase)i));
            ImGui::TableNextColumn();
            // Rosso oltre un quarto del budget: a quel punto una fase sola si
            // sta prendendo una fetta che si vede.
            const bool grossa = (target > 0.f && ph.avgMs > target * 0.25f);
            ImGui::TextColored(grossa ? ImVec4{0.95f, 0.6f, 0.35f, 1.f}
                                      : ImVec4{0.85f, 0.87f, 0.9f, 1.f},
                               "%6.2f ms", ph.avgMs);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%6.2f ms", ph.peakMs);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    const float rest = unaccountedMs();
    ImGui::TextColored(rest > 2.f ? ImVec4{0.95f, 0.75f, 0.35f, 1.f}
                                  : ImVec4{0.6f, 0.62f, 0.66f, 1.f},
                       "Non misurato: %.2f ms", rest);
    if (rest > 2.f && s.phase[(int)Phase::Present].avgMs > 2.f)
        ImGui::TextDisabled("Molto tempo in Presentazione: il freno e' la GPU.");

    {
        const bphys::Stats& ph = g_bphys.stats();
        ImGui::Separator();
        ImGui::Text("Fisica [%s]: attive %d/%d (%d dormono)", ph.tier, ph.active,
                    g_bphys.budget().maxActive, ph.sleeping);
        ImGui::TextDisabled("congelate %d  lontane %d  statiche %d", ph.frozen, ph.culled, ph.statics);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Scena %dx%d  ->  schermo %dx%d", g_renderW, g_renderH, g_w, g_h);
    if (g_renderW < g_w)
        ImGui::TextDisabled("Render scale attivo: %.0f%% dei pixel",
                            100.f * (float)(g_renderW * g_renderH) / (float)(g_w * g_h));

    ImGui::End();
}
