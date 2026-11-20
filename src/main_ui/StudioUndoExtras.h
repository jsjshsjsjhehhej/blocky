// Annulla / Ripeti per tutto quello che non e' una Part: scritte e bottoni GUI,
// Tool dello StarterPack, oggetti dei servizi (Script, ModuleScript,
// RemoteEvent, Folder), SpawnLocation, Lighting, suoni e proprieta' dei
// MeshPart importati.
//
// Invece di toccare ogni pannello, si confronta un'"impronta" di questi dati
// quando il mouse e' libero: se e' cambiata, il prima e il dopo vanno nella
// pila di Annulla (g_undo) come voce Custom, nello stesso ordine delle
// modifiche alle Part. Un trascinamento lungo e' un passo solo.
// Incluso da StudioUiPanel.h; studioTrackExtrasUndo() gira ogni frame nello Studio.

struct ExtrasModelProps {
    std::string name;
    glm::vec3 position{0.f}, rotation{0.f}, scale{1.f}, tint{1.f};
    bool canCollide = false;
};
struct ExtrasSound {
    std::string name, soundId;
    float volume = 1.f;
    bool looped = false;
};
struct ExtrasSnapshot {
    std::vector<WorkspaceLabel> labels;
    std::vector<BackpackItem> backpack;
    std::vector<SharedInstance> instances;
    SpawnLocation spawn;
    LightingService lighting;
    std::vector<ExtrasModelProps> models;   // solo proprieta': i modelli 3D non si ricreano
    std::vector<ExtrasSound> sounds;
};

static std::shared_ptr<ExtrasSnapshot> g_extrasLast;
static uint64_t g_extrasHash = 0;
static bool g_extrasRestoring = false;   // le modifiche fatte da Annulla non diventano voci nuove

namespace extras_undo {

struct H {
    uint64_t h = 1469598103934665603ull;
    void b(const void* p, size_t n) { const unsigned char* c = (const unsigned char*)p; for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; } }
    void f(float v) { b(&v, sizeof v); }
    void i(int v) { b(&v, sizeof v); }
    void s(const std::string& v) { b(v.data(), v.size()); i((int)v.size()); }
    void v3(const glm::vec3& v) { f(v.x); f(v.y); f(v.z); }
};

static uint64_t fingerprint() {
    H h;
    h.i((int)g_world.labels.size());
    for (const auto& l : g_world.labels) {
        h.s(l.name); h.s(l.text); h.s(l.className); h.s(l.script);
        h.f(l.anchorX); h.f(l.anchorY); h.f(l.offsetX); h.f(l.offsetY); h.f(l.sizeW); h.f(l.sizeH);
        h.v3(l.color); h.f(l.fontScale); h.v3(l.bgColor); h.f(l.bgTransparency); h.i(l.visible);
        h.i(l.parentIndex); h.i(l.layoutOrder); h.i(l.adorneePart); h.f(l.studsOffsetY);
    }
    h.i((int)g_world.backpack.size());
    for (const auto& b : g_world.backpack) {
        h.s(b.name); h.i((int)b.tool); h.s(b.meshPath); h.s(b.animId); h.v3(b.gripOffset); h.v3(b.gripRotation); h.f(b.gripScale);
    }
    h.i((int)g_world.instances.size());
    for (const auto& s : g_world.instances) {
        h.s(s.name); h.s(s.className); h.i((int)s.service); h.i(s.parentIndex); h.s(s.source); h.i((int)s.scriptType);
    }
    h.v3(g_world.spawn.position); h.f(g_world.spawn.yaw);
    const LightingService& L = g_lighting;
    h.f(L.brightness); h.f(L.ambient); h.f(L.clockTime); h.f(L.geographicLatitude); h.v3(L.outdoorAmbient);
    h.v3(L.colorShiftTop); h.v3(L.fogColor); h.f(L.fogStart); h.f(L.fogEnd); h.i(L.globalShadows);
    h.i(L.atmosphereEnabled); h.f(L.atmosphereDensity); h.f(L.atmosphereHaze); h.f(L.atmosphereOffset);
    h.i(L.sunRaysEnabled); h.f(L.sunRaysIntensity); h.f(L.sunRaysSpread); h.i((int)L.dayNight); h.f(L.dayLengthMinutes);
    h.i((int)g_world.models.size());
    for (const auto& m : g_world.models) { h.s(m.name); h.v3(m.position); h.v3(m.rotation); h.v3(m.scale); h.v3(m.tint); h.i(m.canCollide); }
    h.i((int)g_sounds.sounds.size());
    for (const auto& s : g_sounds.sounds) { h.s(s.name); h.s(s.soundId); h.f(s.volume); h.i(s.looped); }
    return h.h;
}

static std::shared_ptr<ExtrasSnapshot> capture() {
    auto s = std::make_shared<ExtrasSnapshot>();
    s->labels = g_world.labels;
    s->backpack = g_world.backpack;
    s->instances = g_world.instances;
    s->spawn = g_world.spawn;
    s->lighting = g_lighting;
    for (const auto& m : g_world.models) s->models.push_back({m.name, m.position, m.rotation, m.scale, m.tint, m.canCollide});
    for (const auto& x : g_sounds.sounds) s->sounds.push_back({x.name, x.soundId, x.volume, x.looped});
    return s;
}

static void restore(const ExtrasSnapshot& s) {
    g_extrasRestoring = true;
    g_world.labels = s.labels;
    g_world.backpack = s.backpack;
    g_world.instances = s.instances;
    g_world.spawn = s.spawn;
    g_lighting = s.lighting;
    // I MeshPart: solo le proprieta' (se nel frattempo ne e' stato importato o
    // tolto uno, il modello 3D non si puo' ricreare da qui: restano com'erano).
    if (s.models.size() == g_world.models.size())
        for (size_t i = 0; i < s.models.size(); ++i) {
            auto& m = g_world.models[i];
            const auto& p = s.models[i];
            m.name = p.name; m.position = p.position; m.rotation = p.rotation; m.scale = p.scale; m.tint = p.tint; m.canCollide = p.canCollide;
        }
    // I suoni: prima si ferma l'audio, poi si rimettono come erano (fermi).
    for (auto& x : g_sounds.sounds) if (x.playing) g_sounds.stop(x);
    g_sounds.sounds.clear();
    for (const auto& p : s.sounds) {
        SoundInstance x;
        x.name = p.name; x.soundId = p.soundId; x.volume = p.volume; x.looped = p.looped;
        g_sounds.sounds.push_back(x);
    }
    // Selezioni che ora puntano fuori dagli elenchi.
    if (g_selectedLabel >= (int)g_world.labels.size()) g_selectedLabel = -1;
    if (g_selectedBackpack >= (int)g_world.backpack.size()) g_selectedBackpack = -1;
    if (g_selectedSharedInstance >= (int)g_world.instances.size()) g_selectedSharedInstance = -1;
    if (g_selectedSound >= (int)g_sounds.sounds.size()) g_selectedSound = -1;
    if (g_world.equippedBackpack >= (int)g_world.backpack.size()) g_world.equippedBackpack = -1;
    markPlaceDirty();
}

} // namespace extras_undo

/** Ogni frame nello Studio (non in Test): le modifiche diventano voci di Annulla. */
static void studioTrackExtrasUndo() {
    if (g_screen != AppScreen::Studio || g_studioTesting) { g_extrasLast.reset(); return; }
    if (ImGui::IsAnyItemActive() || ImGui::IsMouseDown(0)) return;   // a trascinamento finito
    const uint64_t h = extras_undo::fingerprint();
    if (!g_extrasLast || g_extrasRestoring || g_worldReplaced) {
        g_worldReplaced = false;
        // Primo frame, o ha appena lavorato Annulla/Ripeti: si riparte da qui.
        g_extrasLast = extras_undo::capture();
        g_extrasHash = h;
        g_extrasRestoring = false;
        return;
    }
    if (h == g_extrasHash) return;
    std::shared_ptr<ExtrasSnapshot> before = g_extrasLast;
    std::shared_ptr<ExtrasSnapshot> after = extras_undo::capture();
    g_undo.recordCustom([before, after](bool undoing) { extras_undo::restore(undoing ? *before : *after); });
    g_extrasLast = after;
    g_extrasHash = h;
}
