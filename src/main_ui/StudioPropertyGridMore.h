// Altre Proprieta' in stile Roblox Studio: Lighting (con Atmosphere e SunRays),
// GUI (TextLabel, TextButton, Frame, ImageLabel), Tool e MeshPart importati.
// Le opzioni che esistono solo in Blocky (preset grafici, rig, animazioni,
// script della GUI...) restano sotto, in "Altre opzioni (Blocky)", chiuse.
// Incluso da StudioPropertyGrid.h.

namespace pg {

// Righe su valori qualsiasi (non una Part): nessun Annulla per-proprieta',
// segnano solo il place come modificato.
static bool rawCheck(const char* label, bool& v, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    const bool c = ImGui::Checkbox("##v", &v);
    if (c) markPlaceDirty();
    ImGui::PopID();
    return c;
}
static bool rawNum(const char* label, float& v, float speed, float mn, float mx, const char* fmt = "%.3g", const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    bool c = ImGui::DragFloat("##v", &v, speed, mn, mx, fmt);
    if (c && mx > mn) v = std::clamp(v, mn, mx);
    if (c) markPlaceDirty();
    ImGui::PopID();
    return c;
}
static bool rawInt(const char* label, int& v, int mn, int mx, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    const bool c = ImGui::DragInt("##v", &v, 0.2f, mn, mx);
    if (c) markPlaceDirty();
    ImGui::PopID();
    return c;
}
static bool rawColor(const char* label, glm::vec3& c3, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    ImGui::ColorButton("##sw", {c3.r, c3.g, c3.b, 1.f}, ImGuiColorEditFlags_NoTooltip, {14, 14});
    ImGui::SameLine();
    char txt[48];
    std::snprintf(txt, sizeof(txt), "[%d, %d, %d]", (int)std::lround(c3.r * 255.f), (int)std::lround(c3.g * 255.f),
                  (int)std::lround(c3.b * 255.f));
    if (ImGui::Selectable(txt, false)) ImGui::OpenPopup("##cpick");
    bool c = false;
    if (ImGui::BeginPopup("##cpick")) {
        c = ImGui::ColorPicker3("##p", &c3.r, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8);
        ImGui::EndPopup();
    }
    if (c) markPlaceDirty();
    ImGui::PopID();
    return c;
}
static bool rawVec3(const char* label, glm::vec3& v, float speed, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s", fmt3(v).c_str());
    bool c = false;
    if (ImGui::InputText("##txt", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
        glm::vec3 nv = v;
        if (std::sscanf(buf, " %f , %f , %f", &nv.x, &nv.y, &nv.z) == 3 || std::sscanf(buf, " %f %f %f", &nv.x, &nv.y, &nv.z) == 3) {
            c = nv != v;
            v = nv;
        }
    }
    // Trascinando col tasto destro sul campo si cambia X (come una scorciatoia veloce).
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%sScrivi \"x, y, z\" e premi Invio", tip ? (std::string(tip) + "\n").c_str() : "");
    (void)speed;
    if (c) markPlaceDirty();
    ImGui::PopID();
    return c;
}
static bool rawText(const char* label, std::string& v, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", v.c_str());
    const bool c = ImGui::InputText("##v", buf, sizeof(buf));
    if (c) { v = buf; markPlaceDirty(); }
    ImGui::PopID();
    return c;
}

/** Fine della griglia Roblox: le opzioni solo di Blocky si aprono qui sotto (codice vecchio). */
static bool moreBlockyOptions(const char* id) {
    ImGui::Spacing();
    ImGui::PushID(id);
    const bool open = ImGui::CollapsingHeader("Altre opzioni (Blocky)");
    ImGui::PopID();
    return open;
}

} // namespace pg

// ---- Lighting ------------------------------------------------------------------------------------

static void drawRobloxLightingProperties() {
    LightingService& L = g_lighting;
    if (pg::beginSection("Aspetto")) {
        pg::rawNum("Ambient", L.ambient, 0.005f, 0.f, 1.f, "%.3f", "Luce di base nelle zone in ombra (0-1)");
        pg::rawNum("Brightness", L.brightness, 0.02f, 0.f, 10.f, "%.2f", "Forza della luce del sole");
        pg::rawColor("ColorShift_Top", L.colorShiftTop, "Tinta della luce del sole sulle facce in alto");
        pg::rawColor("OutdoorAmbient", L.outdoorAmbient, "Colore della luce all'aperto");
        pg::rawCheck("GlobalShadows", L.globalShadows, "Ombre del sole e della luna");
        pg::endSection();
    }
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", "Lighting");
        pg::readonly("Name", "Lighting");
        pg::endSection();
    }
    if (pg::beginSection("Ambiente")) {
        pg::rawColor("FogColor", L.fogColor, "Colore della nebbia");
        pg::rawNum("FogEnd", L.fogEnd, 2.f, 10.f, 100000.f, "%.0f", "Distanza dove la nebbia copre tutto");
        pg::rawNum("FogStart", L.fogStart, 2.f, 0.f, 100000.f, "%.0f", "Distanza dove comincia la nebbia");
        pg::endSection();
    }
    if (pg::beginSection("Ora del giorno")) {
        pg::rawNum("ClockTime", L.clockTime, 0.02f, 0.f, 24.f, "%.2f", "Ora del giorno, 0-24 (14 = pomeriggio)");
        if (pg::row("TimeOfDay", "La stessa ora scritta come hh:mm:ss")) {
            const int h = (int)L.clockTime, m = (int)((L.clockTime - (float)h) * 60.f), sec = (int)(((L.clockTime - (float)h) * 60.f - (float)m) * 60.f);
            ImGui::TextDisabled("%02d:%02d:%02d", h, m, sec);
            ImGui::PopID();
        }
        pg::rawNum("GeographicLatitude", L.geographicLatitude, 0.2f, -90.f, 90.f, "%.1f", "Latitudine: cambia l'altezza del sole");
        pg::endSection();
    }
    if (pg::beginSection("Atmosphere", L.atmosphereEnabled)) {
        pg::rawCheck("Enabled", L.atmosphereEnabled, "In Roblox e' l'oggetto Atmosphere dentro Lighting");
        pg::rawNum("Density", L.atmosphereDensity, 0.005f, 0.f, 1.f, "%.3f", "Quanto e' densa l'aria (avvicina la nebbia)");
        pg::rawNum("Haze", L.atmosphereHaze, 0.005f, 0.f, 1.f, "%.3f", "Foschia sull'orizzonte");
        pg::rawNum("Offset", L.atmosphereOffset, 0.005f, 0.f, 1.f, "%.3f", "Spostamento della foschia");
        pg::endSection();
    }
    if (pg::beginSection("SunRaysEffect", L.sunRaysEnabled)) {
        pg::rawCheck("Enabled", L.sunRaysEnabled, "Raggi di sole (oggetto SunRaysEffect)");
        pg::rawNum("Intensity", L.sunRaysIntensity, 0.005f, 0.f, 1.f, "%.3f");
        pg::rawNum("Spread", L.sunRaysSpread, 0.01f, 0.f, 1.f, "%.2f");
        pg::endSection();
    }
    if (pg::beginSection("Blocky: giorno e notte", false)) {
        if (pg::row("DayNight", "Ciclo del giorno nel gioco")) {
            static const char* kModes[] = {"Ciclo", "Solo giorno", "Solo notte"};
            int m = std::clamp((int)L.dayNight, 0, 2);
            if (ImGui::Combo("##v", &m, kModes, 3)) { L.dayNight = (DayNightMode)m; markPlaceDirty(); }
            ImGui::PopID();
        }
        pg::rawNum("DayLengthMinutes", L.dayLengthMinutes, 0.1f, 0.5f, 120.f, "%.1f min", "Quanto dura un giorno intero nel gioco");
        pg::endSection();
    }
}

// ---- GUI (TextLabel, TextButton, Frame, ImageLabel...) ---------------------------------------------

static void drawRobloxGuiProperties(WorkspaceLabel& l) {
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", l.className.c_str());
        pg::rawText("Name", l.name);
        pg::readonly("Parent", l.parentIndex >= 0 && l.parentIndex < (int)g_world.labels.size()
                                   ? g_world.labels[(size_t)l.parentIndex].name.c_str() : "ScreenGui");
        pg::endSection();
    }
    if (pg::beginSection("Aspetto")) {
        pg::rawColor("BackgroundColor3", l.bgColor, "Colore dello sfondo");
        pg::rawNum("BackgroundTransparency", l.bgTransparency, 0.01f, 0.f, 1.f, "%.2f", "0 = pieno, 1 = invisibile");
        pg::rawCheck("Visible", l.visible);
        pg::endSection();
    }
    if (!l.isFrame() && !l.isImage() && pg::beginSection("Testo")) {
        pg::rawText("Text", l.text, "Quello che c'e' scritto");
        pg::rawColor("TextColor3", l.color, "Colore del testo");
        pg::rawNum("TextSize", l.fontScale, 0.01f, 0.2f, 8.f, "x%.2f", "Grandezza del testo (1 = normale)");
        pg::endSection();
    }
    if (pg::beginSection("Posizione e misura")) {
        // In Roblox Position e Size sono UDim2 {scala, offset}: qui scala = anchor, offset in pixel.
        if (pg::row("Position", "UDim2: {scala X, offset X}, {scala Y, offset Y}")) {
            float v[4] = {l.anchorX, l.offsetX, l.anchorY, l.offsetY};
            if (ImGui::DragFloat4("##v", v, 0.002f, 0.f, 0.f, "%.3g")) {
                l.anchorX = v[0]; l.offsetX = v[1]; l.anchorY = v[2]; l.offsetY = v[3];
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        if (pg::row("Size", "Larghezza e altezza in pixel (0 = si adatta al testo)")) {
            float v[2] = {l.sizeW, l.sizeH};
            if (ImGui::DragFloat2("##v", v, 1.f, 0.f, 4000.f, "%.0f")) { l.sizeW = v[0]; l.sizeH = v[1]; markPlaceDirty(); }
            ImGui::PopID();
        }
        pg::rawInt("LayoutOrder", l.layoutOrder, -1000, 1000, "Ordine dentro un UIListLayout");
        pg::endSection();
    }
    if (l.isWorldGui() && pg::beginSection("BillboardGui")) {
        pg::readonly("Adornee", l.adorneePart >= 0 && l.adorneePart < (int)g_world.parts.size()
                                    ? g_world.parts[(size_t)l.adorneePart].name.c_str() : "(nessuna)");
        pg::rawNum("StudsOffset Y", l.studsOffsetY, 0.05f, -100.f, 100.f, "%.2f", "Altezza sopra la part");
        pg::endSection();
    }
}

// ---- Tool (StarterPack) --------------------------------------------------------------------------

static void drawRobloxToolProperties(BackpackItem& b) {
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", "Tool");
        pg::rawText("Name", b.name);
        pg::readonly("Parent", "StarterPack");
        pg::endSection();
    }
    if (pg::beginSection("Comportamento")) {
        if (pg::row("ToolType", "Che cosa fa quando lo usi (in Roblox lo decide lo script del Tool)")) {
            static const char* kTypes[] = {"Nessuno", "Spada", "Pistola", "Torcia"};
            int t = std::clamp((int)b.tool, 0, 3);
            if (ImGui::Combo("##v", &t, kTypes, 4)) { b.tool = (BackpackTool)t; markPlaceDirty(); }
            ImGui::PopID();
        }
        pg::rawText("TextureId", b.meshPath, "Modello 3D del Tool (file)");
        pg::rawText("AnimationId", b.animId, "Animazione quando lo usi");
        pg::endSection();
    }
    if (pg::beginSection("Impugnatura")) {
        pg::rawVec3("GripPos", b.gripOffset, 0.01f, "Dove sta rispetto alla mano (stud)");
        pg::rawVec3("GripRotation", b.gripRotation, 0.5f, "Rotazione nella mano (gradi)");
        pg::rawNum("GripScale", b.gripScale, 0.01f, 0.05f, 10.f, "%.2f", "Grandezza in mano");
        pg::endSection();
    }
}

// ---- MeshPart (modello importato) ---------------------------------------------------------------

static void drawRobloxMeshPartProperties(WorkspaceModel& m) {
    if (pg::beginSection("Aspetto")) {
        pg::rawColor("Color", m.tint, "Tinta del modello");
        pg::endSection();
    }
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", "MeshPart");
        pg::rawText("Name", m.name);
        pg::readonly("Parent", "Workspace");
        pg::endSection();
    }
    if (pg::beginSection("Collisione")) {
        pg::rawCheck("CanCollide", m.canCollide);
        pg::endSection();
    }
    if (pg::beginSection("Parte")) {
        const auto slash = m.sourcePath.find_last_of("\\/");
        const std::string file = slash == std::string::npos ? m.sourcePath : m.sourcePath.substr(slash + 1);
        pg::readonly("MeshId", file.empty() ? "(nessuno)" : file.c_str());
        pg::readonly("Rig", m.rigged ? "Si'" : "No");
        pg::endSection();
    }
    if (pg::beginSection("Trasformazione")) {
        pg::rawVec3("Position", m.position, 0.1f);
        pg::rawVec3("Orientation", m.rotation, 0.5f);
        pg::rawVec3("Size", m.scale, 0.01f, "Scala del modello (1 = misura del file)");
        pg::endSection();
    }
}

/**
 * Lighting, GUI, Tool, MeshPart: griglia Roblox, poi le opzioni di Blocky.
 * Vero = gia' disegnato tutto; falso = sotto va anche il pannello vecchio
 * (quando "Altre opzioni (Blocky)" e' aperto).
 */
static bool drawRobloxOtherProperties(bool& handled) {
    handled = false;
    if (g_selectedService == studio::platform::Svc_Lighting) {
        handled = true;
        drawRobloxLightingProperties();
        return !pg::moreBlockyOptions("light");
    }
    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
        handled = true;
        drawRobloxGuiProperties(g_world.labels[(size_t)g_selectedLabel]);
        return !pg::moreBlockyOptions("gui");
    }
    if (g_selectedBackpack >= 0 && g_selectedBackpack < (int)g_world.backpack.size()) {
        handled = true;
        drawRobloxToolProperties(g_world.backpack[(size_t)g_selectedBackpack]);
        return !pg::moreBlockyOptions("tool");
    }
    if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
        handled = true;
        drawRobloxMeshPartProperties(g_world.models[(size_t)g_selectedModel]);
        return !pg::moreBlockyOptions("mesh");
    }
    return false;
}
