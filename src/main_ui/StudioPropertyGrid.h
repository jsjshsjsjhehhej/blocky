// Pannello Proprieta' come Roblox Studio: titolo "Proprieta' - Classe "Nome"",
// filtro in alto, sezioni richiudibili e righe "Nome | valore" con i nomi delle
// proprieta' di Roblox (gli stessi degli script). Vector3 scritti "4, 1, 2" ed
// espandibili in X/Y/Z, Color "[163, 162, 165]", sola lettura in grigio.
// Con piu' part selezionate una modifica vale per tutte; ogni modifica e' un
// Ctrl+Z. Script e Pubblica NON stanno qui (come in Roblox): lo script si apre
// in una finestra sua, Pubblica sta nel menu File / ribbon Home.
// Incluso da StudioUiPanel.h dopo StudioExplorer.h.

#include <cfloat>

static bool g_showPlacePublish = false;   // finestra Pubblica (StudioUiPanel.h)

namespace pg {

static char g_filter[64] = "";
static WorkspacePart* g_part = nullptr;   // part principale (quella mostrata)
static int g_index = -1;
static std::vector<std::pair<int, WorkspacePart>> g_beforeAll;   // stato prima di un trascinamento
static bool g_inTable = false;

struct Help { const char* name; const char* text; };
static const Help kHelp[] = {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) {luaName, help},
#include "engine/PartProps.def"
#undef PART_PROP
    {"Position", "Posizione del centro, in stud (part.Position)"},
    {"Size", "Misura in stud lungo X, Y, Z (part.Size)"},
    {"Orientation", "Rotazione in gradi attorno a X, Y, Z (part.Orientation)"},
    {"Color", "Colore RGB 0-255 (part.Color)"},
    {"BrickColor", "Colore dalla tavolozza BrickColor di Roblox"},
    {"Material", "Materiale: aspetto e suono dei passi"},
    {"Shape", "Block, Ball, Cylinder o Wedge"},
    {"Name", "Nome dell'oggetto: con questo lo trovi negli script"},
    {"Parent", "Dove sta l'oggetto nell'albero"},
    {"ClassName", "Classe dell'oggetto (sola lettura)"},
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

static bool beginSection(const char* title, bool defaultOpen = true) {
    if (g_filter[0]) ImGui::SetNextItemOpen(true);
    ImGui::PushStyleColor(ImGuiCol_Header, studio_theme::pal().sectionHeader);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, studio_theme::pal().sectionHeaderHover);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.f, 2.f});
    const bool open = ImGui::CollapsingHeader(title, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    if (!open) return false;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {4.f, 1.f});
    g_inTable = ImGui::BeginTable(title, 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp);
    if (g_inTable) {
        ImGui::TableSetupColumn("n", ImGuiTableColumnFlags_WidthStretch, 0.44f);
        ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.56f);
    } else {
        ImGui::PopStyleVar();
    }
    return g_inTable;
}
static void endSection() {
    if (g_inTable) { ImGui::EndTable(); ImGui::PopStyleVar(); }
    g_inTable = false;
}

/** Etichetta a sinistra; il widget va nella colonna destra. Falso = nascosta dal filtro. */
static bool row(const char* label, const char* tip = nullptr, bool dim = false) {
    if (!match(label)) return false;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    if (dim) ImGui::TextDisabled("%s", label); else ImGui::TextUnformatted(label);
    if (!tip) tip = help(label);
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushID(label);
    return true;
}

// ---- Piu' part selezionate: la modifica passa a tutte ----------------------------------------
// Posizione e rotazione no: quelle le sposta gia' il "follow" dei Model
// (StudioGroupsUi.h), che le muove tutte dello stesso tanto.
static void copyChanged(const WorkspacePart& b, const WorkspacePart& a, WorkspacePart& d) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) if (a.field != b.field) d.field = a.field;
#include "engine/PartProps.def"
#undef PART_PROP
    if (a.name != b.name) d.name = a.name;
    if (a.size != b.size) d.size = a.size;
    if (a.color != b.color) d.color = a.color;
    if (a.material != b.material) d.setMaterial(a.material);
    if (a.shape != b.shape) d.shape = a.shape;
    if (a.useStud != b.useStud) d.useStud = a.useStud;
    if (a.emissive != b.emissive) d.emissive = a.emissive;
    if (a.pointLight != b.pointLight) d.pointLight = a.pointLight;
    if (a.lightRange != b.lightRange) d.lightRange = a.lightRange;
    if (a.lightBrightness != b.lightBrightness) d.lightBrightness = a.lightBrightness;
    if (a.canDestroy != b.canDestroy) d.canDestroy = a.canDestroy;
    if (a.health != b.health) { d.health = a.health; d.maxHealth = std::max(d.maxHealth, d.health); }
    if (a.boomOnTouch != b.boomOnTouch) d.boomOnTouch = a.boomOnTouch;
    if (a.boomAutoSecs != b.boomAutoSecs) d.boomAutoSecs = a.boomAutoSecs;
}

static std::vector<int> others() {
    std::vector<int> out;
    for (int i : studioActivePartIndices())
        if (i != g_index && partIndexValid(i) && !g_world.parts[(size_t)i].locked) out.push_back(i);
    return out;
}

static void propagate(const WorkspacePart& before) {
    for (int i : others()) copyChanged(before, *g_part, g_world.parts[(size_t)i]);
}

static void recordAll(const std::vector<std::pair<int, WorkspacePart>>& befores) {
    g_undo.beginBatch();
    for (const auto& [i, b] : befores)
        if (partIndexValid(i)) g_undo.recordEdit(i, b, g_world.parts[(size_t)i]);
    g_undo.endBatch();
}

static std::vector<std::pair<int, WorkspacePart>> snapshotAll() {
    std::vector<std::pair<int, WorkspacePart>> v;
    v.push_back({g_index, *g_part});
    for (int i : others()) v.push_back({i, g_world.parts[(size_t)i]});
    return v;
}

/**
 * Esegue i widget di una riga e gestisce Annulla + selezione multipla:
 * scatto (checkbox, combo) = voce subito; trascinamento/testo = voce al rilascio.
 */
static bool g_changedInDrag = false;

template <class F> static bool edit(F widgets) {
    const WorkspacePart before = *g_part;
    bool started = false, ended = false;
    const bool ch = widgets(started, ended);
    if (started) {
        g_beforeAll = snapshotAll();   // le altre part non sono ancora cambiate (propagate e' sotto)
        g_beforeAll[0].second = before;
        g_changedInDrag = false;
    }
    if (ch) {
        const bool inDrag = !g_beforeAll.empty();
        std::vector<std::pair<int, WorkspacePart>> b;
        if (!inDrag) { b = snapshotAll(); b[0].second = before; }
        propagate(before);
        markPlaceDirty();
        if (inDrag) g_changedInDrag = true;
        else recordAll(b);             // checkbox, combo, popup: voce subito
    }
    if (ended && !g_beforeAll.empty()) {
        if (g_changedInDrag) recordAll(g_beforeAll);   // trascinamento / testo: voce al rilascio
        g_beforeAll.clear();
        g_changedInDrag = false;
    }
    return ch;
}

// Dopo ogni widget: segna se e' partito o finito un trascinamento.
#define PG_TRACK(a, d) do { if (ImGui::IsItemActivated()) a = true; if (ImGui::IsItemDeactivated()) d = true; } while (0)

static void readonly(const char* label, const char* value) {
    if (!row(label, nullptr, true)) return;
    ImGui::TextDisabled("%s", value);
    ImGui::PopID();
}

static bool check(const char* label, bool& v) {
    if (!row(label)) return false;
    const bool r = edit([&](bool& a, bool& d) { const bool c = ImGui::Checkbox("##v", &v); PG_TRACK(a, d); return c; });
    ImGui::PopID();
    return r;
}

static bool num(const char* label, float& v, float speed, float mn, float mx, const char* fmt = "%.3g") {
    if (!row(label)) return false;
    const bool r = edit([&](bool& a, bool& d) {
        bool c = ImGui::DragFloat("##v", &v, speed, mn, mx, fmt);
        if (c && mx > mn) v = std::clamp(v, mn, mx);
        PG_TRACK(a, d);
        return c;
    });
    ImGui::PopID();
    return r;
}

static std::string fmt3(const glm::vec3& v) {
    auto one = [](float f) {
        char b[24];
        std::snprintf(b, sizeof(b), "%.4f", f);
        std::string s = b;   // toglie gli zeri in fondo: 4.000 -> 4, 0.500 -> 0.5
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        if (s == "-0") s = "0";
        return s;
    };
    return one(v.x) + ", " + one(v.y) + ", " + one(v.z);
}

/** Vector3 come in Roblox: "4, 1, 2" (si scrive e Invio), freccia per X / Y / Z. */
static bool vec3(const char* label, glm::vec3& v, float speed, float mn = 0.f, float mx = 0.f) {
    if (!match(label)) return false;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    const bool open = ImGui::TreeNodeEx("##t", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label);
    if (const char* tip = help(label); tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    const bool r = edit([&](bool& a, bool& d) {
        bool c = false;
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s", fmt3(v).c_str());
        if (ImGui::InputText("##txt", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            glm::vec3 nv = v;
            if (std::sscanf(buf, " %f , %f , %f", &nv.x, &nv.y, &nv.z) == 3
                || std::sscanf(buf, " %f %f %f", &nv.x, &nv.y, &nv.z) == 3) {
                if (mx > mn) nv = glm::clamp(nv, glm::vec3(mn), glm::vec3(mx));
                c = nv != v;
                v = nv;
            }
        }
        if (open) {
            static const char* kAx[3] = {"X", "Y", "Z"};
            for (int k = 0; k < 3; ++k) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Indent(18.f);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(kAx[k]);
                ImGui::Unindent(18.f);
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(k);
                if (ImGui::DragFloat("##c", &v[k], speed, mn, mx, "%.4f")) {
                    if (mx > mn) v[k] = std::clamp(v[k], mn, mx);
                    c = true;
                }
                PG_TRACK(a, d);
                ImGui::PopID();
            }
        }
        return c;
    });
    ImGui::PopID();
    return r;
}

/** Color3 come in Roblox: quadratino + "[163, 162, 165]"; clic = selettore. */
static bool color(const char* label, glm::vec3& c3) {
    if (!row(label)) return false;
    const bool r = edit([&](bool& a, bool& d) {
        ImGui::ColorButton("##sw", {c3.r, c3.g, c3.b, 1.f}, ImGuiColorEditFlags_NoTooltip, {14, 14});
        ImGui::SameLine();
        char txt[48];
        std::snprintf(txt, sizeof(txt), "[%d, %d, %d]", (int)std::lround(c3.r * 255.f), (int)std::lround(c3.g * 255.f),
                      (int)std::lround(c3.b * 255.f));
        if (ImGui::Selectable(txt, false)) ImGui::OpenPopup("##cpick");
        bool c = false;
        if (ImGui::BeginPopup("##cpick")) {
            c = ImGui::ColorPicker3("##p", &c3.r, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8);
            PG_TRACK(a, d);
            ImGui::EndPopup();
        }
        return c;
    });
    ImGui::PopID();
    return r;
}

/** Riga con widget libero (combo, testo...). */
template <class F> static bool custom(const char* label, F widget) {
    if (!row(label)) return false;
    const bool r = edit([&](bool& a, bool& d) { const bool c = widget(); PG_TRACK(a, d); return c; });
    ImGui::PopID();
    return r;
}

} // namespace pg

static std::string pgParentName(const WorkspacePart& p) {
    if (const PartGroup* g = partgroups::find(g_world, p.groupId)) return g->name;
    return "Workspace";
}

// ---- Etichette e Attributi (in fondo alle Proprieta', come Roblox Studio) -------------------------
// Etichette = i tag di CollectionService (g_studioTags); Attributi = SetAttribute /
// GetAttribute (g_partAttributes, chiave "indice:nome"). Il "+" ne aggiunge uno.

static void drawRobloxTagsAndAttributes(int index) {
    // Etichette
    {
        if (pg::g_filter[0]) ImGui::SetNextItemOpen(true);
        const bool open = ImGui::CollapsingHeader("Etichette", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.f);
        if (ImGui::SmallButton("+##addtag")) ImGui::OpenPopup("##newtag");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Aggiungi un'etichetta (CollectionService:AddTag)");
        if (ImGui::BeginPopup("##newtag")) {
            static char buf[48] = "";
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputTextWithHint("##t", "Nome dell'etichetta", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0]) {
                studioAddTagToSelection(buf);
                markPlaceDirty();
                buf[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (open) {
            int n = 0;
            for (size_t k = 0; k < g_studioTags.size(); ++k) {
                if (g_studioTags[k].partIndex != index) continue;
                ++n;
                ImGui::PushID((int)k);
                ImGui::Bullet();
                ImGui::TextUnformatted(g_studioTags[k].tag);
                ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.f);
                if (ImGui::SmallButton("x")) {
                    g_studioTags.erase(g_studioTags.begin() + (long)k);
                    markPlaceDirty();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (!n) ImGui::TextDisabled("Nessuna etichetta: usa il + qui sopra");
        }
    }
    // Attributi
    {
        if (pg::g_filter[0]) ImGui::SetNextItemOpen(true);
        const bool open = ImGui::CollapsingHeader("Attributi", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 22.f);
        if (ImGui::SmallButton("+##addattr")) ImGui::OpenPopup("##newattr");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Aggiungi un attributo (part:SetAttribute)");
        if (ImGui::BeginPopup("##newattr")) {
            static char name[48] = "";
            static int type = 0;
            static const char* kTypes[] = {"string", "number", "boolean"};
            ImGui::InputTextWithHint("##n", "Nome", name, sizeof(name));
            ImGui::Combo("Tipo", &type, kTypes, 3);
            if (ImGui::Button("Aggiungi", {-FLT_MIN, 0}) && name[0]) {
                const char* def = type == 0 ? "" : type == 1 ? "0" : "false";
                const std::string key = std::to_string(index) + ":" + name;
                g_partAttributes[key] = def;
                studio_pro_apis::attributes()[key] = def;
                markPlaceDirty();
                name[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (open) {
            const std::string prefix = std::to_string(index) + ":";
            int n = 0;
            if (ImGui::BeginTable("##attrs", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("n", ImGuiTableColumnFlags_WidthStretch, 0.42f);
                ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.50f);
                ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthFixed, 22.f);
                for (auto it = g_partAttributes.begin(); it != g_partAttributes.end();) {
                    if (it->first.compare(0, prefix.size(), prefix) != 0) { ++it; continue; }
                    ++n;
                    const std::string key = it->first.substr(prefix.size());
                    std::string& val = it->second;
                    ImGui::PushID(it->first.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(key.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    bool changed = false;
                    char* end = nullptr;
                    const double num = std::strtod(val.c_str(), &end);
                    if (val == "true" || val == "false") {
                        bool b = val == "true";
                        if (ImGui::Checkbox("##b", &b)) { val = b ? "true" : "false"; changed = true; }
                    } else if (!val.empty() && end && *end == 0) {
                        float fv = (float)num;
                        if (ImGui::DragFloat("##f", &fv, 0.1f, 0.f, 0.f, "%.3g")) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "%g", fv);
                            val = buf;
                            changed = true;
                        }
                    } else {
                        char buf[256];
                        std::snprintf(buf, sizeof(buf), "%s", val.c_str());
                        if (ImGui::InputText("##s", buf, sizeof(buf))) { val = buf; changed = true; }
                    }
                    if (changed) { studio_pro_apis::attributes()[it->first] = val; markPlaceDirty(); }
                    ImGui::TableSetColumnIndex(2);
                    const bool del = ImGui::SmallButton("x");
                    ImGui::PopID();
                    if (del) {
                        studio_pro_apis::attributes().erase(it->first);
                        it = g_partAttributes.erase(it);
                        markPlaceDirty();
                    } else {
                        ++it;
                    }
                }
                ImGui::EndTable();
            }
            if (!n) ImGui::TextDisabled("Nessun attributo: usa il + qui sopra");
        }
    }
}

// ---- Part ----------------------------------------------------------------------------------

static void drawRobloxPartProperties(WorkspacePart& p, int index) {
    pg::g_part = &p;
    pg::g_index = index;

    if (pg::beginSection("Aspetto")) {
        pg::custom("BrickColor", [&] {
            const brickcolor::Entry* e = brickcolor::nearest(p.color);
            ImGui::ColorButton("##bcs", {p.color.r, p.color.g, p.color.b, 1.f}, ImGuiColorEditFlags_NoTooltip, {14, 14});
            ImGui::SameLine();
            if (ImGui::Selectable(e ? e->name : "?", false)) ImGui::OpenPopup("##bcpop");
            bool ch = false;
            if (ImGui::BeginPopup("##bcpop")) {
                const glm::vec3 before = p.color;
                drawPartColorPicker(p.color);
                ch = p.color != before;
                ImGui::EndPopup();
            }
            return ch;
        });
        pg::check("CastShadow", p.castShadow);
        pg::color("Color", p.color);
        pg::custom("Material", [&] {
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
        pg::endSection();
    }

    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", exPartClass(p));
        pg::check("Locked", p.locked);
        pg::custom("Name", [&] {
            const bool ch = ImGui::InputText("##v", g_partNameBuf, sizeof(g_partNameBuf));
            if (ch && g_partNameBuf[0]) p.name = g_partNameBuf;
            return ch;
        });
        pg::readonly("Parent", pgParentName(p).c_str());
        pg::endSection();
    }

    if (pg::beginSection("Comportamento")) {
        pg::check("Anchored", p.anchored);
        pg::check("CanQuery", p.canQuery);
        pg::endSection();
    }

    if (pg::beginSection("Collisione")) {
        pg::check("CanCollide", p.canCollide);
        pg::check("CanTouch", p.canTouch);
        pg::custom("CollisionGroup", [&] {
            int g = (int)p.collisionGroup;
            const bool ch = ImGui::SliderInt("##v", &g, 0, 31);
            if (ch) p.collisionGroup = (float)g;
            return ch;
        });
        pg::endSection();
    }

    if (pg::beginSection("Parte")) {
        pg::custom("Shape", [&] {
            static const char* kShapes[] = {"Block", "Ball", "Cylinder", "Wedge"};
            int sh = std::clamp((int)p.shape, 0, 3);
            const bool ch = ImGui::Combo("##v", &sh, kShapes, 4);
            if (ch) {
                p.shape = (PartShape)sh;
                if (p.shape != PartShape::Block) p.useStud = false;
            }
            return ch;
        });
        pg::check("Studs", p.useStud);
        pg::check("VehicleSeat", p.vehicleSeat);
        pg::endSection();
    }

    if (pg::beginSection("Aspetto extra")) {
        pg::check("Decal", p.decalEnabled);
        if (p.decalEnabled) {
            pg::num("DecalFace", p.decalFace, 1.f, 0.f, 5.f, "%.0f");
            pg::num("DecalR", p.decalR, 0.01f, 0.f, 1.f);
            pg::num("DecalG", p.decalG, 0.01f, 0.f, 1.f);
            pg::num("DecalB", p.decalB, 0.01f, 0.f, 1.f);
            pg::num("DecalTransparency", p.decalTransparency, 0.01f, 0.f, 1.f);
        }
        pg::check("Highlight", p.highlightEnabled);
        if (p.highlightEnabled) {
            pg::num("HighlightR", p.highlightR, 0.01f, 0.f, 1.f);
            pg::num("HighlightG", p.highlightG, 0.01f, 0.f, 1.f);
            pg::num("HighlightB", p.highlightB, 0.01f, 0.f, 1.f);
        }
        pg::check("SelectionBox", p.selectionBox);
        pg::check("SelectionSphere", p.selectionSphere);
        pg::check("DragDetector", p.dragDetector);
        pg::check("Texture", p.textureEnabled);
        if (p.textureEnabled) {
            pg::num("TextureFace", p.textureFace, 1.f, 0.f, 5.f, "%.0f");
            pg::num("TextureStuds", p.textureStuds, 0.1f, 1.f, 16.f, "%.0f");
            pg::num("TextureR", p.textureR, 0.01f, 0.f, 1.f);
            pg::num("TextureG", p.textureG, 0.01f, 0.f, 1.f);
            pg::num("TextureB", p.textureB, 0.01f, 0.f, 1.f);
        }
        pg::check("Seat", p.seat);
        pg::check("Ladder", p.ladder);
        pg::check("Spawn", p.spawnPoint);
        {
            static char teamBuf[32] = {};
            static int teamPart = -1;
            if (teamPart != g_selectedPart) {
                teamPart = g_selectedPart;
                std::snprintf(teamBuf, sizeof(teamBuf), "%s", p.spawnTeam.c_str());
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("SpawnTeam");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##spawnteam", teamBuf, sizeof(teamBuf))) {
                p.spawnTeam = teamBuf;
                if (!p.spawnTeam.empty()) p.spawnPoint = true;
                markPlaceDirty();
            }
        }
        auto imageRow = [&](const char* label, std::string& image, bool& enabled) {
            static const char* kImg[] = {
                "(colore)", "brick", "wood", "metal", "concrete", "grass", "fabric",
                "marble", "sand", "slate", "diamondplate", "granite", "asphalt", "corrodedmetal"
            };
            int cur = 0;
            for (int i = 1; i < 14; ++i)
                if (image == kImg[i]) cur = i;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo(label[0] == 'D' ? "##decalimg" : "##teximg", &cur, kImg, 14)) {
                image = cur == 0 ? "" : kImg[cur];
                if (cur != 0) enabled = true;
                markPlaceDirty();
            }
        };
        if (p.decalEnabled) imageRow("DecalImage", p.decalImage, p.decalEnabled);
        if (p.textureEnabled) imageRow("TextureImage", p.textureImage, p.textureEnabled);
        if (ImGui::Button("Altro decal")) {
            if ((int)p.extraDecals.size() < 4) {
                WorkspacePart::FaceDecal fd;
                fd.face = 1;
                fd.image = "brick";
                p.extraDecals.push_back(fd);
                markPlaceDirty();
            }
        }
        pg::endSection();
    }

    if (pg::beginSection("Tag", false)) {
        static char tagBuf[80];
        static int tagFor = -2;
        if (tagFor != g_selectedPart) {
            std::string tags;
            for (const auto& name : g_tag.allTagNames()) {
                if (!g_tag.has(name, g_selectedPart)) continue;
                if (!tags.empty()) tags += ",";
                tags += name;
            }
            std::snprintf(tagBuf, sizeof(tagBuf), "%s", tags.c_str());
            tagFor = g_selectedPart;
        }
        if (pg::row("Tags", "CollectionService. Invio per applicare. Separati da virgola.")) {
            if (ImGui::InputText("##v", tagBuf, sizeof(tagBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                for (const auto& name : g_tag.allTagNames()) g_tag.remove(name, g_selectedPart);
                std::string s = tagBuf;
                for (size_t i = 0; i < s.size();) {
                    const size_t c = s.find(',', i);
                    std::string one = s.substr(i, c == std::string::npos ? std::string::npos : c - i);
                    while (!one.empty() && one.front() == ' ') one.erase(one.begin());
                    while (!one.empty() && one.back() == ' ') one.pop_back();
                    if (!one.empty()) g_tag.add(one, g_selectedPart);
                    if (c == std::string::npos) break;
                    i = c + 1;
                }
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        pg::endSection();
    }

    if (pg::beginSection("Trasformazione")) {
        pg::vec3("Size", p.size, 0.001f, kMinPartSize, 100000.f);
        pg::vec3("Position", p.position, 0.001f);
        pg::vec3("Orientation", p.rotation, 0.5f);
        pg::endSection();
    }

    if (pg::beginSection("Proprieta' fisiche", false)) {
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
            pg::num("Range", p.lightRange, 0.2f, 4.f, 60.f);
            pg::num("Brightness", p.lightBrightness, 0.02f, 0.2f, 4.f);
        }
        pg::endSection();
    }

    if (pg::beginSection("Blocky: distruzione", false)) {
        pg::check("CanDestroy", p.canDestroy);
        if (p.canDestroy && pg::num("Health", p.health, 1.f, 1.f, 100000.f, "%.0f")) p.maxHealth = std::max(p.maxHealth, p.health);
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
    if (pg::beginSection("Solidi", p.negative)) {
        pg::check("IsNegative", p.negative);   // Nega: in un'Unione fa un buco
        pg::endSection();
    }
    drawRobloxTagsAndAttributes(index);
    pg::g_part = nullptr;
}

// ---- Script di una part --------------------------------------------------------------------


static void drawRobloxScriptProperties(WorkspacePart& p) {
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", exScriptClass(p.scriptType));
        pg::readonly("Name", exScriptClass(p.scriptType));
        pg::readonly("Parent", p.name.c_str());
        pg::endSection();
    }
    if (pg::beginSection("Comportamento")) {
        if (pg::row("RunContext", "Dove gira: Server (Script), Client (LocalScript), o solo con require (ModuleScript)")) {
            static const char* kTypes[] = {"Legacy", "Server", "Client", "ModuleScript"};
            int st = std::clamp((int)p.scriptType, 0, 3);
            if (ImGui::Combo("##v", &st, kTypes, 4)) { p.scriptType = (ScriptType)st; markPlaceDirty(); }
            ImGui::PopID();
        }
        if (pg::row("Source", "Il codice: si apre nell'editor (doppio clic sullo script nell'Esploratore)")) {
            if (ImGui::Button("Apri nell'editor", {-FLT_MIN, 0})) g_showPartScriptWin = true;
            ImGui::PopID();
        }
        char lines[32];
        std::snprintf(lines, sizeof(lines), "%d", (int)std::count(p.script.begin(), p.script.end(), '\n') + 1);
        pg::readonly("Righe", lines);
        pg::endSection();
    }
}

// ---- Model (gruppo di part) ----------------------------------------------------------------

static void drawRobloxModelProperties(int gid) {
    const PartGroup* g = partgroups::find(g_world, gid);
    if (!g) return;
    const std::vector<int> members = partgroups::memberParts(g_world, gid);
    glm::vec3 mn(0.f), mx(0.f);
    partgroups::boundsOf(g_world, members, mn, mx);
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", g->folder ? "Folder" : "Model");
        if (pg::row("Name")) {
            static char buf[96];
            static int bufFor = -1;
            static bool wasActive = false;
            if (bufFor != gid || !wasActive) std::snprintf(buf, sizeof(buf), "%s", g->name.c_str());
            bufFor = gid;
            if (ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue) && buf[0]) studioRenameGroup(gid, buf);
            wasActive = ImGui::IsItemActive();
            ImGui::PopID();
        }
        const PartGroup* parent = partgroups::find(g_world, g->parent);
        pg::readonly("Parent", parent ? parent->name.c_str() : "Workspace");
        char n[32];
        std::snprintf(n, sizeof(n), "%d part", (int)members.size());
        pg::readonly("Figli", n);
        pg::endSection();
    }
    if (!g->folder && pg::beginSection("Comportamento")) {
        if (pg::row("DragDetector", "In Play, trascinare una part sposta tutto il Model insieme")) {
            PartGroup* mg = partgroups::find(g_world, gid);
            bool on = mg && mg->dragDetector;
            if (ImGui::Checkbox("##v", &on) && mg) { mg->dragDetector = on; markPlaceDirty(); }
            ImGui::PopID();
        }
        if (pg::row("Pivot Offset", "Sposta solo il pivot. Le part restano dove sono.")) {
            PartGroup* mg = partgroups::find(g_world, gid);
            if (mg && ImGui::DragFloat3("##v", &mg->pivotOffset.x, 0.05f, 0.f, 0.f, "%.2f")) markPlaceDirty();
            ImGui::PopID();
        }
        pg::endSection();
    }
    if (!g->folder && pg::beginSection("Pivot")) {
        // Spostare il pivot sposta tutto il Model (come WorldPivot di Roblox).
        if (pg::match("Origin Position")) {
            glm::vec3 c = (mn + mx) * 0.5f;
            const glm::vec3 old = c;
            static std::vector<std::pair<int, WorkspacePart>> before;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Origin Position");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool ch = ImGui::DragFloat3("##org", &c.x, 0.1f, 0.f, 0.f, "%.2f");
            if (ImGui::IsItemActivated()) {
                before.clear();
                for (int i : members) before.push_back({i, g_world.parts[(size_t)i]});
            }
            if (ch) {
                const glm::vec3 d = c - old;
                for (int i : members)
                    if (!g_world.parts[(size_t)i].locked) g_world.parts[(size_t)i].position += d;
                studioFollowRebaseline();
                markPlaceDirty();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                g_undo.beginBatch();
                for (const auto& [i, b] : before)
                    if (partIndexValid(i)) g_undo.recordTransform(i, b, g_world.parts[(size_t)i]);
                g_undo.endBatch();
            }
        }
        pg::readonly("Size", pg::fmt3(mx - mn).c_str());
        pg::endSection();
    }
}

// ---- SpawnLocation, Sound ------------------------------------------------------------------

static void drawRobloxSpawnProperties() {
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", "SpawnLocation");
        pg::readonly("Name", "SpawnLocation");
        pg::readonly("Parent", "Workspace");
        pg::endSection();
    }
    if (pg::beginSection("Trasformazione")) {
        if (pg::row("Position")) {
            if (ImGui::DragFloat3("##v", &g_world.spawn.position.x, 0.1f, 0.f, 0.f, "%.2f")) markPlaceDirty();
            ImGui::PopID();
        }
        if (pg::row("Orientation", "Rotazione attorno a Y: dove guarda il giocatore quando entra")) {
            if (ImGui::DragFloat("##v", &g_world.spawn.yaw, 0.5f, -360.f, 360.f, "%.1f")) markPlaceDirty();
            ImGui::PopID();
        }
        pg::endSection();
    }
}

static void drawRobloxSoundProperties(int i) {
    auto& s = g_sounds.sounds[(size_t)i];
    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", "Sound");
        if (pg::row("Name")) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%s", s.name.c_str());
            if (ImGui::InputText("##v", buf, sizeof(buf)) && buf[0]) s.name = buf;
            ImGui::PopID();
        }
        pg::readonly("Parent", "SoundService");
        pg::endSection();
    }
    if (pg::beginSection("Riproduzione")) {
        if (pg::row("SoundId", "File audio (WAV consigliato)")) {
            if (ImGui::InputText("##v", g_soundPathBuf, sizeof(g_soundPathBuf))) s.soundId = g_soundPathBuf;
            ImGui::PopID();
        }
        if (pg::row("Looped")) { ImGui::Checkbox("##v", &s.looped); ImGui::PopID(); }
        if (pg::row("Volume")) { ImGui::SliderFloat("##v", &s.volume, 0.f, 1.f, "%.2f"); ImGui::PopID(); }
        if (pg::row("Playing", "Spunta per ascoltarlo qui nello Studio")) {
            bool playing = s.playing;
            if (ImGui::Checkbox("##v", &playing)) {
                if (playing) { s.soundId = g_soundPathBuf; if (!g_sounds.play(s)) g_exportMsg = g_sounds.lastError; }
                else g_sounds.stop(s);
            }
            ImGui::PopID();
        }
        pg::endSection();
    }
}

#include "main_ui/StudioPropertyGridMore.h"   // Lighting, GUI, Tool, MeshPart

// ---- Smistamento -----------------------------------------------------------------------------

/** Il gruppo selezionato per intero, se la selezione e' esattamente un Model. */
static int pgSelectedModelGroup() {
    const std::vector<int> groups = studioSelectedWholeGroups();
    if (groups.size() != 1) return 0;
    return partgroups::memberParts(g_world, groups.front()).size() == studioActivePartIndices().size() ? groups.front() : 0;
}

static const char* exChildClassName(ExChild k) {
    switch (k) {
    case ExChild::PointLight: return "PointLight";
    case ExChild::Decal: return "Decal";
    case ExChild::Texture: return "Texture";
    case ExChild::Highlight: return "Highlight";
    case ExChild::SelectionBox: return "SelectionBox";
    case ExChild::SelectionSphere: return "SelectionSphere";
    case ExChild::DragDetector: return "DragDetector";
    case ExChild::Seat: return "Seat";
    case ExChild::VehicleSeat: return "VehicleSeat";
    case ExChild::Ladder: return "Ladder";
    case ExChild::Spawn: return "SpawnLocation";
    case ExChild::Mesh: return "SpecialMesh";
    case ExChild::Weld: return "Constraint";
    case ExChild::Emitter: return "ParticleEmitter";
    case ExChild::Attachment: return "Attachment";
    case ExChild::Trail: return "Trail";
    case ExChild::Click: return "ClickDetector";
    default: return "Instance";
    }
}

static void pgFace(const char* label, float& face) {
    static const char* kFaces[] = {"Right", "Top", "Back", "Left", "Bottom", "Front"};
    if (!pg::row(label, "0 Right, 1 Top, 2 Back, 3 Left, 4 Bottom, 5 Front")) return;
    int f = std::clamp((int)face, 0, 5);
    if (ImGui::Combo("##v", &f, kFaces, 6)) { face = (float)f; markPlaceDirty(); }
    ImGui::PopID();
}

static void pgRgb(const char* label, float& r, float& g, float& b) {
    if (!pg::row(label)) return;
    glm::vec3 c{r, g, b};
    if (ImGui::ColorEdit3("##v", &c.x, ImGuiColorEditFlags_NoInputs)) {
        r = c.x; g = c.y; b = c.z;
        markPlaceDirty();
    }
    ImGui::PopID();
}

/** Proprieta' del figlio scelto nell'Esploratore (la luce, il decal, il vincolo...). */
static bool drawExChildProperties(WorkspacePart& p) {
    pg::g_part = &p;
    pg::g_index = g_selectedPart;
    const char* cls = exChildClassName(g_exChild);
    if (g_exChild == ExChild::Weld && g_exChildIndex >= 0 && g_exChildIndex < (int)g_world.welds.size())
        cls = constraintKindName(g_world.welds[(size_t)g_exChildIndex].kind);

    if (pg::beginSection("Dati")) {
        pg::readonly("ClassName", cls);
        pg::readonly("Parent", p.name.c_str());
        bool on = true;
        if (pg::row("Enabled", "Spento: toglie questo oggetto dalla Part")) {
            if (ImGui::Checkbox("##v", &on) && !on) exRemoveSelectedChild();
            ImGui::PopID();
        }
        pg::endSection();
    }
    if (g_exChild == ExChild::None) { pg::g_part = nullptr; return true; }

    if (g_exChild == ExChild::PointLight && pg::beginSection("Luce")) {
        pg::num("Range", p.lightRange, 0.2f, 4.f, 60.f);
        pg::num("Brightness", p.lightBrightness, 0.02f, 0.2f, 4.f);
        pg::color("Color", p.color);
        pg::endSection();
    } else if (g_exChild == ExChild::Decal && pg::beginSection("Decal")) {
        pgFace("Face", p.decalFace);
        pgRgb("Color", p.decalR, p.decalG, p.decalB);
        pg::num("Transparency", p.decalTransparency, 0.01f, 0.f, 1.f);
        static const char* kImg[] = {"(colore)", "brick", "wood", "metal", "concrete", "grass", "fabric", "marble", "sand", "slate"};
        int cur = 0;
        for (int i = 1; i < 10; ++i) if (p.decalImage == kImg[i]) cur = i;
        if (pg::row("Texture")) {
            if (ImGui::Combo("##v", &cur, kImg, 10)) {
                p.decalImage = cur == 0 ? "" : kImg[cur];
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Texture && pg::beginSection("Texture")) {
        pgFace("Face", p.textureFace);
        pg::num("StudsPerTile", p.textureStuds, 0.1f, 1.f, 16.f, "%.0f");
        pgRgb("Color", p.textureR, p.textureG, p.textureB);
        static const char* kImg[] = {"(righe)", "brick", "wood", "metal", "concrete", "grass", "fabric", "marble", "sand", "slate"};
        int cur = 0;
        for (int i = 1; i < 10; ++i) if (p.textureImage == kImg[i]) cur = i;
        if (pg::row("Texture")) {
            if (ImGui::Combo("##v", &cur, kImg, 10)) {
                p.textureImage = cur == 0 ? "" : kImg[cur];
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Highlight && pg::beginSection("Highlight")) {
        pgRgb("FillColor", p.highlightR, p.highlightG, p.highlightB);
        pg::endSection();
    } else if (g_exChild == ExChild::Mesh && pg::beginSection("Mesh")) {
        static char buf[260];
        static int bufFor = -2;
        if (bufFor != g_selectedPart) {
            std::snprintf(buf, sizeof(buf), "%s", p.meshFile.c_str());
            bufFor = g_selectedPart;
        }
        if (pg::row("MeshId", "File obj, glb o fbx. Se non si trova resta il cubo.")) {
            if (ImGui::InputText("##v", buf, sizeof(buf))) { p.meshFile = buf; markPlaceDirty(); }
            ImGui::PopID();
        }
        pg::vec3("Scale", p.meshScale, 0.01f);
        pg::endSection();
    } else if (g_exChild == ExChild::Spawn && pg::beginSection("Spawn")) {
        static char teamBuf[32] = {};
        static int teamPart = -2;
        if (teamPart != g_selectedPart) {
            std::snprintf(teamBuf, sizeof(teamBuf), "%s", p.spawnTeam.c_str());
            teamPart = g_selectedPart;
        }
        if (pg::row("Team", "Vuoto = va bene per tutti")) {
            if (ImGui::InputText("##v", teamBuf, sizeof(teamBuf))) { p.spawnTeam = teamBuf; markPlaceDirty(); }
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::VehicleSeat && pg::beginSection("VehicleSeat")) {
        pg::check("Anchored", p.anchored);
        pg::readonly("Guida", "In Play: F per salire, W A S D per guidare");
        pg::endSection();
    } else if (g_exChild == ExChild::Emitter
               && g_exChildIndex >= 0 && g_exChildIndex < (int)g_lua.scriptEmitters_.size()
               && pg::beginSection("Particelle")) {
        auto& e = g_lua.scriptEmitters_[(size_t)g_exChildIndex];
        pg::readonly("ClassName", exEmitterName(e));
        if (pg::row("Enabled")) { if (ImGui::Checkbox("##v", &e.enabled)) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Rate")) { if (ImGui::DragFloat("##v", &e.rate, 0.2f, 0.f, 60.f, "%.1f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Speed")) { if (ImGui::DragFloat("##v", &e.speed, 0.1f, 0.f, 40.f, "%.1f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Lifetime")) { if (ImGui::DragFloat("##v", &e.life, 0.05f, 0.1f, 8.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (e.color.x >= 0.f && pg::row("Color")) {
            if (ImGui::ColorEdit3("##v", &e.color.x, ImGuiColorEditFlags_NoInputs)) markPlaceDirty();
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Attachment
               && g_exChildIndex >= 0 && g_exChildIndex < (int)scriptControls().attachments.size()
               && pg::beginSection("Attachment")) {
        auto& a = scriptControls().attachments[(size_t)g_exChildIndex];
        if (pg::row("Position")) {
            float p3[3] = {a.ox, a.oy, a.oz};
            if (ImGui::DragFloat3("##v", p3, 0.05f, 0.f, 0.f, "%.2f")) {
                a.ox = p3[0]; a.oy = p3[1]; a.oz = p3[2];
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Trail
               && g_exChildIndex >= 0 && g_exChildIndex < (int)scriptControls().trails.size()
               && pg::beginSection("Trail")) {
        auto& tr = scriptControls().trails[(size_t)g_exChildIndex];
        if (pg::row("Enabled")) { if (ImGui::Checkbox("##v", &tr.enabled)) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Width")) { if (ImGui::DragFloat("##v", &tr.width, 0.02f, 0.05f, 4.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Lifetime")) { if (ImGui::DragFloat("##v", &tr.life, 0.02f, 0.05f, 4.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Color")) {
            float c[3] = {tr.r, tr.g, tr.b};
            if (ImGui::ColorEdit3("##v", c, ImGuiColorEditFlags_NoInputs)) {
                tr.r = c[0]; tr.g = c[1]; tr.b = c[2];
                markPlaceDirty();
            }
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Click && pg::beginSection("ClickDetector")) {
        float& reach = scriptControls().clickReach[g_selectedPart];
        if (pg::row("MaxActivationDistance", "Distanza in stud. In Play il clic sulla Part arriva allo script.")) {
            if (ImGui::DragFloat("##v", &reach, 0.2f, 1.f, 200.f, "%.1f")) markPlaceDirty();
            ImGui::PopID();
        }
        pg::endSection();
    } else if (g_exChild == ExChild::Weld
               && g_exChildIndex >= 0 && g_exChildIndex < (int)g_world.welds.size()
               && pg::beginSection("Vincolo")) {
        WeldConstraint& w = g_world.welds[(size_t)g_exChildIndex];
        auto partName = [](int i) {
            return partIndexValid(i) ? g_world.parts[(size_t)i].name.c_str() : "(nessuna)";
        };
        pg::readonly("Part0", partName(w.part0));
        pg::readonly("Part1", partName(w.part1));
        if (pg::row("Enabled")) {
            if (ImGui::Checkbox("##v", &w.enabled)) markPlaceDirty();
            ImGui::PopID();
        }
        if (pg::row("Name")) {
            static char nbuf[96];
            static int nfor = -2;
            if (nfor != g_exChildIndex) { std::snprintf(nbuf, sizeof(nbuf), "%s", w.name.c_str()); nfor = g_exChildIndex; }
            if (ImGui::InputText("##v", nbuf, sizeof(nbuf)) && nbuf[0]) { w.name = nbuf; markPlaceDirty(); }
            ImGui::PopID();
        }
        if (pg::row("Length")) { if (ImGui::DragFloat("##v", &w.length, 0.05f, 0.f, 500.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Stiffness")) { if (ImGui::DragFloat("##v", &w.stiffness, 0.05f, 0.f, 200.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Damping")) { if (ImGui::DragFloat("##v", &w.damping, 0.01f, 0.f, 5.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        if (pg::row("Axis")) { if (ImGui::DragFloat3("##v", &w.axis.x, 0.02f, -1.f, 1.f, "%.2f")) markPlaceDirty(); ImGui::PopID(); }
        pg::endSection();
    }
    pg::g_part = nullptr;
    return true;
}

/** Titolo come Roblox: "Proprieta' - Part "Nome"". */
static std::string robloxPropertiesTitle() {
    std::string t = std::string(L("menu.properties"));
    auto quoted = [](const std::string& cls, const std::string& name) { return " - " + cls + " \"" + name + "\""; };
    if (g_selectedSpawn) return t + quoted("SpawnLocation", "SpawnLocation");
    if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) return t + quoted("Sound", g_sounds.sounds[(size_t)g_selectedSound].name);
    if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) return t + quoted("MeshPart", g_world.models[(size_t)g_selectedModel].name);
    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size())
        return t + quoted(g_world.labels[(size_t)g_selectedLabel].className, g_world.labels[(size_t)g_selectedLabel].name);
    if (g_selectedService >= 0) {
        const auto* info = studio::platform::findService(g_selectedService);
        return t + " - " + (info ? info->name : "Service");
    }
    if (partIndexValid(g_selectedPart)) {
        const WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
        if (g_editingPartScript && !p.script.empty()) return t + quoted(exScriptClass(p.scriptType), exScriptClass(p.scriptType));
        if (g_exChild != ExChild::None) {
            const char* cls = exChildClassName(g_exChild);
            const char* nm = cls;
            if (g_exChild == ExChild::Weld && g_exChildIndex >= 0 && g_exChildIndex < (int)g_world.welds.size()) {
                cls = constraintKindName(g_world.welds[(size_t)g_exChildIndex].kind);
                nm = g_world.welds[(size_t)g_exChildIndex].name.c_str();
            } else if (g_exChild == ExChild::Emitter && g_exChildIndex >= 0
                       && g_exChildIndex < (int)g_lua.scriptEmitters_.size()) {
                cls = exEmitterName(g_lua.scriptEmitters_[(size_t)g_exChildIndex]);
                nm = cls;
            }
            return t + quoted(cls, nm);
        }
        if (const int gid = pgSelectedModelGroup()) {
            const PartGroup* g = partgroups::find(g_world, gid);
            return t + quoted(g && g->folder ? "Folder" : "Model", g ? g->name : "Model");
        }
        const size_t n = studioActivePartIndices().size();
        if (n > 1) return t + " - " + std::to_string(n) + " elementi";
        return t + quoted(exPartClass(p), p.name);
    }
    return t;
}

/**
 * Disegna le proprieta' in stile Roblox per la selezione, se e' un tipo che le
 * ha (Part, Model, Script, SpawnLocation, Sound). Falso = la mostra il codice
 * vecchio (Lighting, servizi, MeshPart, GUI, Tool, Player).
 */
static bool drawRobloxSelectionProperties() {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##propfilter", "Filtra proprieta'", pg::g_filter, sizeof(pg::g_filter));
    if (g_selectedSpawn) { drawRobloxSpawnProperties(); return true; }
    if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) { drawRobloxSoundProperties(g_selectedSound); return true; }
    {
        bool handled = false;
        const bool done = drawRobloxOtherProperties(handled);
        if (handled) return done;   // falso = sotto anche le opzioni di Blocky
    }
    const bool other = g_selectedService >= 0 || g_selectedLabel >= 0 || g_selectedBackpack >= 0 || g_selectedPlayer
                    || g_selectedModel >= 0;
    if (other || !partIndexValid(g_selectedPart)) return false;
    WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
    if (g_editingPartScript && !p.script.empty()) { drawRobloxScriptProperties(p); return true; }
    if (g_exChild != ExChild::None) { drawExChildProperties(p); return true; }
    if (const int gid = pgSelectedModelGroup()) { drawRobloxModelProperties(gid); return true; }
    drawRobloxPartProperties(p, g_selectedPart);
    return true;
}
