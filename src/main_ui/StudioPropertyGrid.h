// Proprieta' della Part come in Roblox Studio: filtro in alto, sezioni
// richiudibili (Aspetto, Dati, Collisione, Parte, Trasformazione...) e righe
// "Nome | valore" con i nomi delle proprieta' di Roblox (gli stessi degli
// script: part.Anchored, part.Transparency...). Ogni modifica e' un Ctrl+Z.
// Incluso da StudioUiPanel.h dopo StudioExplorer.h.

#include <cfloat>

namespace pg {

static char g_filter[64] = "";
static WorkspacePart* g_part = nullptr;
static int g_index = -1;
static WorkspacePart g_before;   // stato prima di un trascinamento
static bool g_inTable = false;

// Aiuto (tooltip) delle proprieta' di PartProps.def, per nome Roblox.
struct Help { const char* name; const char* text; };
static const Help kHelp[] = {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) {luaName, help},
#include "engine/PartProps.def"
#undef PART_PROP
};
static const char* help(const char* name) {
    for (const auto& h : kHelp)
        if (std::strcmp(h.name, name) == 0) return h.text;
    return nullptr;
}

static bool match(const char* label) {
    if (!g_filter[0]) return true;
    std::string a = label, q = g_filter;
    for (char& c : a) c = (char)std::tolower((unsigned char)c);
    for (char& c : q) c = (char)std::tolower((unsigned char)c);
    return a.find(q) != std::string::npos;
}

/** Sezione richiudibile con dentro la tabella a due colonne. Con il filtro attivo e' sempre aperta. */
static bool beginSection(const char* title, bool defaultOpen = true) {
    if (g_filter[0]) ImGui::SetNextItemOpen(true);
    ImGui::PushStyleColor(ImGuiCol_Header, {0.22f, 0.23f, 0.26f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.28f, 0.30f, 0.34f, 1.f});
    const bool open = ImGui::CollapsingHeader(title, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor(2);
    if (!open) return false;
    g_inTable = ImGui::BeginTable(title, 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp);
    if (g_inTable) {
        ImGui::TableSetupColumn("n", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.58f);
    }
    return g_inTable;
}
static void endSection() {
    if (g_inTable) ImGui::EndTable();
    g_inTable = false;
}

/** Inizio riga: etichetta a sinistra, poi il widget va nella colonna destra. Falso = nascosta dal filtro. */
static bool row(const char* label, const char* tip = nullptr) {
    if (!match(label)) return false;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (!tip) tip = help(label);
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushID(label);
    return true;
}

// Checkbox, combo...: cambiano in un colpo solo -> una voce di Annulla subito.
template <class F> static bool instant(const char* label, F widget, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    const WorkspacePart before = *g_part;
    const bool ch = widget();
    ImGui::PopID();
    if (ch) {
        g_undo.recordEdit(g_index, before, *g_part);
        markPlaceDirty();
    }
    return ch;
}

// Trascinamenti e campi di testo: una voce di Annulla quando lasci il mouse.
template <class F> static bool dragged(const char* label, F widget, const char* tip = nullptr) {
    if (!row(label, tip)) return false;
    const WorkspacePart before = *g_part;
    const bool ch = widget();
    if (ImGui::IsItemActivated()) g_before = before;
    if (ch) markPlaceDirty();
    if (ImGui::IsItemDeactivatedAfterEdit()) g_undo.recordEdit(g_index, g_before, *g_part);
    ImGui::PopID();
    return ch;
}

static void readonly(const char* label, const char* value) {
    if (!row(label)) return;
    ImGui::TextDisabled("%s", value);
    ImGui::PopID();
}

static bool check(const char* label, bool& v) {
    return instant(label, [&] { return ImGui::Checkbox("##v", &v); });
}
static bool num(const char* label, float& v, float speed, float mn, float mx, const char* fmt = "%.2f") {
    return dragged(label, [&] {
        const bool c = ImGui::DragFloat("##v", &v, speed, mn, mx, fmt);
        if (c && mx > mn) v = std::clamp(v, mn, mx);
        return c;
    });
}
static bool vec3(const char* label, glm::vec3& v, float speed, float mn = 0.f, float mx = 0.f, const char* fmt = "%.3f") {
    return dragged(label, [&] { return ImGui::DragFloat3("##v", &v.x, speed, mn, mx, fmt); });
}

} // namespace pg

/** Il nome del contenitore della part, come la proprieta' Parent di Roblox. */
static std::string pgParentName(const WorkspacePart& p) {
    if (const PartGroup* g = partgroups::find(g_world, p.groupId)) return g->name;
    return "Workspace";
}

static void drawRobloxPartProperties(WorkspacePart& p, int index) {
    pg::g_part = &p;
    pg::g_index = index;

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##propfilter", "Filtra proprieta'", pg::g_filter, sizeof(pg::g_filter));

    if (pg::beginSection("Aspetto")) {
        if (pg::row("BrickColor")) {
            const brickcolor::Entry* e = brickcolor::nearest(p.color);
            ImGui::ColorButton("##bcs", {p.color.r, p.color.g, p.color.b, 1.f}, ImGuiColorEditFlags_NoTooltip, {14, 14});
            ImGui::SameLine();
            if (ImGui::Selectable(e ? e->name : "?", false)) ImGui::OpenPopup("##bcpop");
            if (ImGui::BeginPopup("##bcpop")) {
                const WorkspacePart before = p;
                drawPartColorPicker(p.color);
                if (p.color != before.color) { g_undo.recordEdit(index, before, p); markPlaceDirty(); }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        pg::check("CastShadow", p.castShadow);
        pg::dragged("Color", [&] {
            return ImGui::ColorEdit3("##v", &p.color.r, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB);
        });
        pg::instant("Material", [&] {
            bool ch = false;
            if (ImGui::BeginCombo("##v", partMaterialName(p.material))) {
                for (int i = 0; i < (int)PartMaterial::Count; ++i)
                    if (ImGui::Selectable(partMaterialName((PartMaterial)i), p.material == (PartMaterial)i)) {
                        p.setMaterial((PartMaterial)i);
                        ch = true;
                    }
                ImGui::EndCombo();
            }
            return ch;
        });
        pg::num("Reflectance", p.reflectance, 0.01f, 0.f, 1.f);
        pg::num("Transparency", p.transparency, 0.01f, 0.f, 1.f);
        pg::check("Studs", p.useStud);
        pg::endSection();
    }

    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", exPartClass(p));
        pg::check("Locked", p.locked);
        pg::dragged("Name", [&] {
            const bool ch = ImGui::InputText("##v", g_partNameBuf, sizeof(g_partNameBuf));
            if (ch && g_partNameBuf[0]) p.name = g_partNameBuf;
            return ch;
        });
        pg::readonly("Parent", pgParentName(p).c_str());
        pg::endSection();
    }

    if (pg::beginSection("Collisione")) {
        pg::check("CanCollide", p.canCollide);
        pg::check("CanTouch", p.canTouch);
        pg::check("CanQuery", p.canQuery);
        pg::instant("CollisionGroup", [&] {
            int g = (int)p.collisionGroup;
            const bool ch = ImGui::SliderInt("##v", &g, 0, 31);
            if (ch) p.collisionGroup = (float)g;
            return ch;
        });
        pg::endSection();
    }

    if (pg::beginSection("Parte")) {
        pg::check("Anchored", p.anchored);
        pg::instant("Shape", [&] {
            static const char* kShapes[] = {"Block", "Ball", "Cylinder", "Wedge"};
            int sh = std::clamp((int)p.shape, 0, 3);
            const bool ch = ImGui::Combo("##v", &sh, kShapes, 4);
            if (ch) {
                p.shape = (PartShape)sh;
                if (p.shape != PartShape::Block) p.useStud = false;
            }
            return ch;
        });
        pg::check("VehicleSeat", p.vehicleSeat);
        pg::endSection();
    }

    if (pg::beginSection("Trasformazione")) {
        pg::vec3("Size", p.size, 0.1f, 0.05f, 100000.f);
        pg::vec3("Position", p.position, 0.1f);
        pg::vec3("Orientation", p.rotation, 0.5f, 0.f, 0.f, "%.1f");
        pg::endSection();
    }

    if (pg::beginSection("Fisica", false)) {
        pg::num("Density", p.density, 0.01f, 0.01f, 100.f);
        pg::num("Friction", p.friction, 0.01f, 0.f, 2.f);
        pg::num("Elasticity", p.elasticity, 0.01f, 0.f, 1.f);
        pg::num("LinearDamping", p.linearDamping, 0.01f, 0.f, 2.f);
        pg::num("AngularDamping", p.angularDamping, 0.01f, 0.f, 2.f);
        pg::endSection();
    }

    if (pg::beginSection("Luce", p.pointLight || p.emissive > 0.f)) {
        pg::num("Emissive", p.emissive, 0.01f, 0.f, 2.f);
        pg::check("PointLight", p.pointLight);
        if (p.pointLight) {
            pg::num("Range", p.lightRange, 0.2f, 4.f, 60.f, "%.1f");
            pg::num("Brightness", p.lightBrightness, 0.02f, 0.2f, 4.f);
        }
        pg::endSection();
    }

    if (pg::beginSection("Blocky: distruzione", false)) {
        pg::check("CanDestroy", p.canDestroy);
        if (p.canDestroy) {
            if (pg::num("Health", p.health, 1.f, 1.f, 100000.f, "%.0f")) p.maxHealth = std::max(p.maxHealth, p.health);
        }
        pg::check("BoomOnTouch", p.boomOnTouch);
        pg::num("BoomAutoSecs", p.boomAutoSecs, 0.1f, 0.f, 60.f, "%.1f s");
        pg::endSection();
    }

    if (pg::beginSection("Blocky: hitbox", false)) {
        pg::num("HitboxScaleX", p.hitboxScaleX, 0.01f, 0.05f, 4.f);
        pg::num("HitboxScaleY", p.hitboxScaleY, 0.01f, 0.05f, 4.f);
        pg::num("HitboxScaleZ", p.hitboxScaleZ, 0.01f, 0.05f, 4.f);
        pg::endSection();
    }
    pg::g_part = nullptr;
}
