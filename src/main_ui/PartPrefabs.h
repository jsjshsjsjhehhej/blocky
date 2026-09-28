// La Toolbox: oggetti pronti da inserire con un clic, come in Roblox Studio.
// Non e' un header riusabile: usa g_world, g_undo e insertPositionInFront()
// (WorldEditingTools.h). Compila solo se incluso da main.cpp dopo quel file.
//
// ── Perche' sono piu' Part insieme, non un "gruppo" ────────────────────────
// WorkspacePart non ha un concetto di gruppo/genitore: non esiste un modo di
// legare insieme piu' Part cosi' che Explorer, gizmo e selezione le trattino
// come una cosa sola. Aggiungerlo sarebbe un lavoro grande a se — Explorer,
// gizmo e selezione andrebbero tutti insegnati a gestire un gruppo, non solo
// l'inserimento.
//
// Ogni prefabbricato qui sotto e' quindi una LISTA di Part inserite insieme,
// ognuna poi selezionabile e modificabile singolarmente — esattamente come
// se le avessi costruite a mano una per una, solo piu' veloce. E' onesto
// sul cosa fa: non finge un raggruppamento che non c'e'.
//
// ── Perche' sono fatti di primitive, non mesh importate ────────────────────
// Il progetto non ha nessun pacchetto di asset pronti (nessun .glb di una
// scala o di un albero). Costruire i prefabbricati con le forme che il
// motore gia' disegna (Block/Ball/Cylinder/Wedge) significa che funzionano
// SUBITO, senza dover procurarsi o importare niente — e restano modificabili
// come una Part qualsiasi una volta inseriti.

struct PrefabPartSpec {
    glm::vec3 offset;      // relativo al punto di inserimento
    glm::vec3 size;
    glm::vec3 color;
    PartShape shape = PartShape::Block;
    PartMaterial material = PartMaterial::Plastic;
    float emissive = 0.f;
    bool pointLight = false;
};

struct PartPrefab {
    const char* name;
    const char* hint;
    std::vector<PrefabPartSpec> parts;
};

/**
 * Le forme sono liste letterali, non generate: si leggono e si modificano
 * guardandole, invece di dover seguire una formula.
 */
static const std::vector<PartPrefab>& partPrefabs() {
    static const std::vector<PartPrefab> kPrefabs = {
        {
            "Scala", "5 gradini, alti mezzo blocco l'uno",
            {
                {{0.0f, 0.25f, 0.0f}, {4.f, 0.5f, 2.f}, {0.62f, 0.60f, 0.58f}},
                {{0.0f, 0.75f, -2.f}, {4.f, 0.5f, 2.f}, {0.62f, 0.60f, 0.58f}},
                {{0.0f, 1.25f, -4.f}, {4.f, 0.5f, 2.f}, {0.62f, 0.60f, 0.58f}},
                {{0.0f, 1.75f, -6.f}, {4.f, 0.5f, 2.f}, {0.62f, 0.60f, 0.58f}},
                {{0.0f, 2.25f, -8.f}, {4.f, 0.5f, 2.f}, {0.62f, 0.60f, 0.58f}},
            }
        },
        {
            "Porta (telaio)", "Due montanti e un architrave: ci si passa sotto",
            {
                {{-2.0f, 2.5f, 0.f}, {0.5f, 5.f, 0.5f}, {0.42f, 0.28f, 0.18f}},
                {{ 2.0f, 2.5f, 0.f}, {0.5f, 5.f, 0.5f}, {0.42f, 0.28f, 0.18f}},
                {{ 0.0f, 5.25f, 0.f}, {4.5f, 0.5f, 0.5f}, {0.42f, 0.28f, 0.18f}},
            }
        },
        {
            "Albero", "Tronco cilindrico + chioma",
            {
                {{0.f, 2.f, 0.f}, {1.2f, 4.f, 1.2f}, {0.40f, 0.26f, 0.14f}, PartShape::Cylinder},
                {{0.f, 5.5f, 0.f}, {4.f, 4.f, 4.f},  {0.20f, 0.55f, 0.22f}, PartShape::Ball},
            }
        },
        {
            "Panchina", "Seduta, schienale e quattro gambe",
            {
                {{0.f, 1.0f, 0.f},  {4.f, 0.3f, 1.2f}, {0.45f, 0.30f, 0.18f}},
                {{0.f, 1.7f, -0.5f}, {4.f, 1.2f, 0.2f}, {0.45f, 0.30f, 0.18f}},
                {{-1.7f, 0.5f, 0.5f},  {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 0.2f}},
                {{ 1.7f, 0.5f, 0.5f},  {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 0.2f}},
                {{-1.7f, 0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 0.2f}},
                {{ 1.7f, 0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 0.2f}},
            }
        },
        {
            "Lampione", "Palo sottile con luce puntiforme in cima",
            {
                {{0.f, 3.f, 0.f}, {0.4f, 6.f, 0.4f}, {0.15f, 0.15f, 0.16f}, PartShape::Cylinder},
                {{0.f, 6.2f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.92f, 0.65f}, PartShape::Ball,
                 PartMaterial::Neon, /*emissive*/1.f, /*pointLight*/true},
            }
        },
        {
            "Tavolo", "Piano e quattro gambe",
            {
                {{0.f, 2.0f, 0.f}, {4.f, 0.3f, 2.5f}, {0.42f, 0.28f, 0.18f}},
                {{-1.7f, 1.0f, 1.0f},  {0.25f, 2.0f, 0.25f}, {0.30f, 0.20f, 0.13f}},
                {{ 1.7f, 1.0f, 1.0f},  {0.25f, 2.0f, 0.25f}, {0.30f, 0.20f, 0.13f}},
                {{-1.7f, 1.0f, -1.0f}, {0.25f, 2.0f, 0.25f}, {0.30f, 0.20f, 0.13f}},
                {{ 1.7f, 1.0f, -1.0f}, {0.25f, 2.0f, 0.25f}, {0.30f, 0.20f, 0.13f}},
            }
        },
        {
            "Recinzione", "Tre pali con due traverse orizzontali",
            {
                {{-3.f, 1.5f, 0.f}, {0.3f, 3.f, 0.3f}, {0.55f, 0.42f, 0.28f}},
                {{ 0.f, 1.5f, 0.f}, {0.3f, 3.f, 0.3f}, {0.55f, 0.42f, 0.28f}},
                {{ 3.f, 1.5f, 0.f}, {0.3f, 3.f, 0.3f}, {0.55f, 0.42f, 0.28f}},
                {{ 0.f, 1.0f, 0.f}, {6.4f, 0.3f, 0.2f}, {0.55f, 0.42f, 0.28f}},
                {{ 0.f, 2.2f, 0.f}, {6.4f, 0.3f, 0.2f}, {0.55f, 0.42f, 0.28f}},
            }
        },
        {
            "Rampa", "Un cuneo per salire senza gradini",
            {
                {{0.f, 1.f, 0.f}, {4.f, 2.f, 8.f}, {0.55f, 0.55f, 0.58f}, PartShape::Wedge},
            }
        },
    };
    return kPrefabs;
}

/**
 * Inserisce tutte le Part di un prefabbricato, ancorate al punto davanti
 * alla camera (stesso punto che usa insertNewPart/insertShapedPart).
 *
 * Non seleziona nessuna Part alla fine — sono N, non una — ma registra
 * ognuna nell'Undo separatamente: Ctrl+Z toglie l'ultima messa, esattamente
 * come se le avessi piazzate a mano una per una.
 */
static void insertPrefab(const PartPrefab& prefab) {
    const glm::vec3 base = insertPositionInFront();
    int firstIdx = -1;
    for (size_t i = 0; i < prefab.parts.size(); ++i) {
        const PrefabPartSpec& s = prefab.parts[i];
        std::string name = std::string(prefab.name) + "_" + std::to_string(i + 1);
        auto& p = g_world.addPart(name, base + s.offset, s.size, s.color);
        p.shape = s.shape;
        p.material = s.material;
        p.emissive = s.emissive;
        p.pointLight = s.pointLight;
        p.useStud = false;
        const int idx = (int)g_world.parts.size() - 1;
        if (firstIdx < 0) firstIdx = idx;
        g_undo.recordInsert(idx, g_world.parts[idx]);
    }
    if (firstIdx >= 0) selectPart(firstIdx);   // almeno la prima resta comoda da spostare
    markPlaceDirty();
    g_exportMsg = std::string(prefab.name) + ": " + std::to_string(prefab.parts.size()) + " part inserite";
}

/**
 * La finestra Toolbox. Un elenco semplice: nome, suggerimento al passaggio
 * del mouse, un pulsante che inserisce.
 */
static bool g_showToolbox = false;

static void drawToolboxWindow() {
    if (!g_showToolbox) return;
    ImGui::SetNextWindowSize({280, 360}, ImGuiCond_Appearing);
    if (!ImGui::Begin("Toolbox", &g_showToolbox)) { ImGui::End(); return; }

    ImGui::TextDisabled("Oggetti pronti, costruiti dalle forme del motore.");
    ImGui::Separator();

    for (const PartPrefab& prefab : partPrefabs()) {
        if (ImGui::Button(prefab.name, {-1, 34}))
            insertPrefab(prefab);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n(%d part)", prefab.hint, (int)prefab.parts.size());
    }

    ImGui::End();
}
