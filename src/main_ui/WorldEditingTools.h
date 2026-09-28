// Strumenti di editing del mondo in Studio: inserimento part/modelli/suoni/
// etichette, template di mappa, caricamento e rendering dei modelli
// riggati nel workspace, duplica/elimina selezione. Non e' un header
// riusabile: usa direttamente le globali di main.cpp (g_world, g_wsMeshes,
// g_wsRigged, g_undo, ecc.). Compila solo se incluso da main.cpp
// esattamente in questo punto.

static glm::vec3 insertPositionInFront() {
    if (g_mode == CamMode::Studio) {
        glm::vec3 p = g_cam.pos + g_cam.front() * 12.f;
        p.y = std::max(1.f, p.y);
        p.x = std::round(p.x);
        p.z = std::round(p.z);
        p.y = std::round(p.y * 2.f) * 0.5f;
        return p;
    }
    return g_playerPos + glm::vec3(0, 2, 6);
}

static void insertNewPart() {
    int n = g_world.nextPartIndex();
    std::string name = "Part" + std::to_string(n);
    auto& p = g_world.addPart(name, insertPositionInFront(), {4, 1, 4}, {0.64f, 0.64f, 0.64f});
    (void)p;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    markPlaceDirty();
}

static void insertShapedPart(PartShape shape) {
    int n = g_world.nextPartIndex();
    // Nome, dimensione e colore vengono dalla tabella unica in PartShape.h:
    // prima erano ripetuti qui a mano, ed erano gia sfasati dal nome che
    // partShapeName() restituiva altrove (bastava dimenticarsi una riga).
    const PartShapeInfo& info = partShapeInfo(shape);
    const std::string base = info.name;
    const glm::vec3 size{info.sizeX, info.sizeY, info.sizeZ};
    const glm::vec3 col{info.r, info.g, info.b};
    auto& p = g_world.addPart(base + std::to_string(n),
                              insertPositionInFront(), size, col);
    p.shape = shape;
    p.useStud = false;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    markPlaceDirty();
    g_exportMsg = std::string("Inserito ") + partShapeName(shape);
}

static void insertNeonPart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Neon" + std::to_string(n), insertPositionInFront(),
                              {2, 2, 2}, {0.25f, 1.f, 0.85f});
    p.setMaterial(PartMaterial::Neon);
    p.pointLight = true;
    p.lightRange = 22.f;
    p.lightBrightness = 1.4f;
    p.script = makeNeonBlockScript(p);
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "Neon Part + script Lua — Test (F5) per glow/pulse";
}

static void insertPointLightPart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Light" + std::to_string(n), insertPositionInFront(),
                              {1, 1, 1}, {1.f, 0.95f, 0.75f});
    p.setMaterial(PartMaterial::Neon);
    p.emissive = 1.2f;
    p.pointLight = true;
    p.lightRange = 28.f;
    p.lightBrightness = 1.8f;
    p.canCollide = false;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "PointLight — regola Range/Brightness in Properties";
}

static void insertExplodePart() {
    int n = g_world.nextPartIndex();
    auto& p = g_world.addPart("Explode" + std::to_string(n), insertPositionInFront(),
                              {3, 3, 3}, {0.85f, 0.18f, 0.12f});
    p.setMaterial(PartMaterial::Metal);
    p.canDestroy = true;
    p.boomOnTouch = false;
    p.boomAutoSecs = 0.f;
    p.canCollide = false;
    p.health = 100.f;
    p.maxHealth = 100.f;
    p.anchored = true;
    p.script = makeExplodeBlockScript(p);
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    selectPart((int)g_world.parts.size() - 1);
    syncPartEditors();
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
    g_exportMsg = "Explode Part — script Lua: tocco = monete + boom";
    markPlaceDirty();
}

static void insertNewLabel() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("Label" + std::to_string(n), "Label",
                               0.5f, 0.18f, {1.f, 1.f, 1.f}, "Label");
    l.bgColor = {0.f, 0.f, 0.f};
    l.bgTransparency = 0.35f;
    l.fontScale = 1.25f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "Label creata — testo + outline + bg nero";
}

static void insertNewTextLabel() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("TextLabel" + std::to_string(n), "TextLabel",
                               0.5f, 0.12f, {1.f, 1.f, 1.f}, "TextLabel");
    l.bgColor = {0.f, 0.f, 0.f};
    l.bgTransparency = 0.35f;
    l.fontScale = 1.15f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "TextLabel creato — testo + outline + bg nero";
}

static void insertNewTextButton() {
    int n = g_world.nextLabelIndex();
    auto& l = g_world.addLabel("TextButton" + std::to_string(n), "Button",
                               0.5f, 0.55f, {1.f, 1.f, 1.f}, "TextButton");
    l.bgColor = {0.55f, 0.55f, 0.58f};
    l.bgTransparency = 0.f;
    l.fontScale = 1.1f;
    l.sizeW = 140.f;
    l.sizeH = 40.f;
    selectLabel((int)g_world.labels.size() - 1);
    g_exportMsg = "TextButton creato — cliccabile in Test/Play";
}

static void insertPropPreset(int kind) {
    glm::vec3 base = insertPositionInFront();
    base.y = 0.f;
    switch (kind) {
    case 0: {
        g_world.addPart("Trunk", base + glm::vec3(0, 2, 0), {1.2f, 4, 1.2f}, {0.42f, 0.26f, 0.12f});
        g_world.addPart("Leaves", base + glm::vec3(0, 5.5f, 0), {4, 3.5f, 4}, {0.18f, 0.55f, 0.22f});
        g_exportMsg = "Prop: Albero";
        break;
    }
    case 1: {
        g_world.addPart("HouseBody", base + glm::vec3(0, 3, 0), {10, 6, 8}, {0.85f, 0.78f, 0.65f});
        g_world.addPart("Roof", base + glm::vec3(0, 7.2f, 0), {12, 2, 10}, {0.75f, 0.28f, 0.22f});
        g_world.addPart("Door", base + glm::vec3(0, 1.5f, 4.1f), {2, 3, 0.3f}, {0.35f, 0.22f, 0.12f});
        g_exportMsg = "Prop: Casa";
        break;
    }
    case 2: {
        for (int i = 0; i < 6; ++i)
            g_world.addPart("Ramp" + std::to_string(i),
                base + glm::vec3(i * 3.f, 0.5f + i * 0.8f, 0), {3, 1, 4},
                {0.3f + i * 0.08f, 0.55f, 0.9f});
        g_exportMsg = "Prop: Rampa";
        break;
    }
    case 3: {
        g_world.addPart("Tower", base + glm::vec3(0, 8, 0), {5, 16, 5}, {0.55f, 0.58f, 0.65f});
        g_world.addPart("Top", base + glm::vec3(0, 17, 0), {7, 2, 7}, {0.9f, 0.4f, 0.25f});
        g_exportMsg = "Prop: Torre";
        break;
    }
    default: break;
    }
    if (!g_world.parts.empty())
        selectPart((int)g_world.parts.size() - 1);
}

static void applyMapTemplate(int kind) {
    g_sounds.stopAll();
    g_sounds.sounds.clear();
    g_wsMeshes.clear();
    g_showAnimEditor = false;
    g_animEditModel = -1;
    switch (kind) {
    case 0: g_world.loadBaseplatePlace(); break;
    case 1: g_world.loadGardenPlace(); break;
    case 2: g_world.loadObbyPlace(); break;
    case 3: g_world.loadArenaPlace(); break;
    case 4: g_world.loadParkPlace(); break;
    case 5: g_world.loadCityPlace(); break;
    default: g_world.loadBaseplatePlace(); break;
    }
    std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    clearAllSelection();
    g_playerPos = g_world.spawn.position;
    if (g_playerPos.y < 0.01f) g_playerPos = {0, 0, 0};
    acAllowTeleport();
    g_friends.clear();
    g_friendSeq = 1;
    g_exportMsg = std::string("Mappa: ") + g_world.placeName;
}

static bool modelWorldAabb(int index, glm::vec3& outMin, glm::vec3& outMax) {
    if (index < 0 || index >= (int)g_world.models.size()) return false;
    if (index >= (int)g_wsMeshes.size() || !g_wsMeshes[index].loaded) return false;

    const auto& m = g_world.models[index];
    const glm::vec3 lo = g_wsMeshes[index].boundsMin;
    const glm::vec3 hi = g_wsMeshes[index].boundsMax;

    const glm::mat4 xf = glm::translate(glm::mat4(1.f), m.position)
                       * partRotationMatrix(m.rotation)
                       * glm::scale(glm::mat4(1.f), m.scale);

    outMin = glm::vec3(1e30f);
    outMax = glm::vec3(-1e30f);
    for (int c = 0; c < 8; ++c) {
        const glm::vec3 corner((c & 1) ? hi.x : lo.x,
                               (c & 2) ? hi.y : lo.y,
                               (c & 4) ? hi.z : lo.z);
        const glm::vec3 w = glm::vec3(xf * glm::vec4(corner, 1.f));
        outMin = glm::min(outMin, w);
        outMax = glm::max(outMax, w);
    }
    return true;
}

static void rebuildNavGrid() {
    std::vector<glm::vec3> mins(g_world.models.size());
    std::vector<glm::vec3> maxs(g_world.models.size());
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (!modelWorldAabb(i, mins[(size_t)i], maxs[(size_t)i])) {
            mins[(size_t)i] = glm::vec3(0.f);
            maxs[(size_t)i] = glm::vec3(0.f);
        }
    }
    g_nav.build(g_world, mins, maxs);
}

static void setupRiggedModel(size_t index, WorkspaceModel& m, bool fileHasBones) {
    if (g_wsRigged.size() <= index) g_wsRigged.resize(index + 1);

    m.rigged = false;
    if (!fileHasBones || m.sourcePath.empty()) return;

    if (!g_wsRigged[index].load(m.sourcePath)) return;

    m.rigged = g_wsRigged[index].clipCount() > 0;
    if (!m.rigged) return;

    std::printf("[Model] '%s': %d clip animate disponibili\n",
                m.name.c_str(), g_wsRigged[index].clipCount());

    if (m.animator.tracks.empty()) {
        const int id = m.animator.load(g_wsRigged[index].activeClipName());
        if (AnimTrack* t = m.animator.get(id)) {
            t->looped = true;
            // NON partire da sola: molti modelli scaricati portano
            // un'animazione idle nel file anche quando si vuole solo un
            // oggetto statico. La clip resta pronta e selezionata — parte
            // solo se l'utente la avvia davvero (bottone "Anima questo
            // Model..." o controlli Animator in Properties).
            t->playing = false;
        }
    }
}

static void reloadWorkspaceModelMeshes() {
    g_wsMeshes.clear();
    g_wsRigged.clear();
    g_wsMeshes.reserve(g_world.models.size());
    g_wsRigged.resize(g_world.models.size());

    for (size_t i = 0; i < g_world.models.size(); ++i) {
        auto& m = g_world.models[i];

        StaticMesh mesh;
        mesh.exactImport = true;
        if (!m.sourcePath.empty())
            mesh.load(m.sourcePath, kWorldModelTargetSize);

        setupRiggedModel(i, m, mesh.hasBones);
        g_wsMeshes.push_back(std::move(mesh));
    }
}

static void drawWorkspaceModels(GLuint meshP, GLuint skinP, const glm::mat4& view,
                                const glm::mat4& proj, const glm::vec3& sun) {
    if (g_wsMeshes.size() != g_world.models.size())
        reloadWorkspaceModelMeshes();

    static double lastAnimTime = glfwGetTime();
    const double nowAnim = glfwGetTime();
    const float animDt = std::min((float)(nowAnim - lastAnimTime), 0.05f);
    lastAnimTime = nowAnim;

    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (i >= (int)g_wsMeshes.size() || !g_wsMeshes[i].loaded) continue;
        auto& m = g_world.models[i];
        glm::mat4 model = glm::translate(glm::mat4(1.f), m.position)
                        * partRotationMatrix(m.rotation)
                        * glm::scale(glm::mat4(1.f), m.scale);
        glm::vec3 tint = m.tint;
        if (i == g_selectedModel)
            tint = glm::mix(tint, glm::vec3(1.f, 0.85f, 0.2f), 0.3f);

        const int track = m.animator.dominant();
        if (m.rigged && track >= 0 && i < (int)g_wsRigged.size() && g_wsRigged[i].loaded) {
            AnimTrack* t = m.animator.get(track);
            AnimatedModel& rig = g_wsRigged[i];

            if (t && rig.activeClipName() != t->clipName)
                rig.setClip(t->clipName);

            m.animator.advance(animDt);
            rig.update(animDt * (t ? std::max(0.f, t->speed) : 1.f));

            const glm::mat4 skinModel = model * rig.correction;

            glUseProgram(skinP);
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(skinModel));
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
            glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
            glUniform3fv(glGetUniformLocation(skinP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
            glUniform1f(glGetUniformLocation(skinP,"uEmissive"), 0.f);
            glUniform1f(glGetUniformLocation(skinP,"uSpecular"), 0.12f);
            glUniform1f(glGetUniformLocation(skinP,"uRoughness"), 0.75f);
            glUniform1f(glGetUniformLocation(skinP,"uAlpha"), 1.f);
            glUniform1i(glGetUniformLocation(skinP,"uMaterial"), (int)PartMaterial::Plastic);
            uploadDynamicLights(skinP);
            rig.draw(skinP);
            continue;
        }

        glUseProgram(meshP);
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(meshP,"uSun"),1,glm::value_ptr(sun));
        glUniform3fv(glGetUniformLocation(meshP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
        uploadDynamicLights(meshP);
        g_wsMeshes[i].drawTextured(meshP, tint, 1.f);
    }
}

static void ensureDefaultBeepWav() {
#ifdef _WIN32
    std::string dir = blockyDataDir() + "/sounds";
    _mkdir(dir.c_str());
    std::string path = dir + "/beep.wav";
    if (FILE* exists = std::fopen(path.c_str(), "rb")) { std::fclose(exists); return; }
    const int sampleRate = 22050;
    const int nSamples = sampleRate / 5;
    const int dataBytes = nSamples * 2;
    std::vector<char> buf(44 + dataBytes);
    auto* hdr = reinterpret_cast<unsigned char*>(buf.data());
    std::memcpy(hdr, "RIFF", 4);
    unsigned chunkSize = 36 + (unsigned)dataBytes;
    std::memcpy(hdr + 4, &chunkSize, 4);
    std::memcpy(hdr + 8, "WAVEfmt ", 8);
    unsigned sub1 = 16; short fmt = 1, ch = 1; unsigned rate = (unsigned)sampleRate;
    unsigned byteRate = (unsigned)sampleRate * 2; short align = 2, bits = 16;
    std::memcpy(hdr + 16, &sub1, 4);
    std::memcpy(hdr + 20, &fmt, 2);
    std::memcpy(hdr + 22, &ch, 2);
    std::memcpy(hdr + 24, &rate, 4);
    std::memcpy(hdr + 28, &byteRate, 4);
    std::memcpy(hdr + 32, &align, 2);
    std::memcpy(hdr + 34, &bits, 2);
    std::memcpy(hdr + 36, "data", 4);
    std::memcpy(hdr + 40, &dataBytes, 4);
    short* samples = reinterpret_cast<short*>(buf.data() + 44);
    for (int i = 0; i < nSamples; ++i) {
        float t = (float)i / (float)sampleRate;
        samples[i] = (short)(std::sin(2.f * 3.14159265f * 880.f * t) * 12000.f);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f); }
    if (!g_soundPathBuf[0])
        std::snprintf(g_soundPathBuf, sizeof(g_soundPathBuf), "%s", path.c_str());
#endif
}

// Il parsing Assimp + upload GPU (in processPendingModelImport) blocca il
// thread principale per tutta la sua durata — su un file pesante puo'
// essere lungo. Per non lasciare l'editor bloccato senza nessun segnale,
// separiamo in due fasi: qui scegliamo il file e mostriamo subito il
// messaggio "in corso" (che fa in tempo a disegnarsi ed essere presentato
// su schermo), il lavoro pesante vero e proprio parte al frame successivo
// (vedi la chiamata a processPendingModelImport() in Blocky_RunEngine) —
// cosi' l'ultimo frame mostrato all'utente e' quello col messaggio, non uno
// congelato a meta'.
static void insertImportedModel() {
    // La scelta del file e' una richiesta, non una domanda con risposta
    // immediata: su Android il selettore e' del sistema e l'utente esce dal
    // gioco per usarlo. Quello che c'era qui sotto vive ora nella richiamata.
    plat::requestFile("Blocky — importa modello", "*/*", [](const std::string& path) {
        if (path.empty()) {
            g_exportMsg = "Import modello annullato";
            return;
        }
        auto mod = Moderation::checkAvatarOrModelName(path);
        if (mod.verdict != Moderation::Verdict::Ok) {
            g_exportMsg = mod.reason;
            return;
        }
        g_pendingModelImportPath = path;
        g_pendingModelImport = true;
        g_exportMsg = "Importazione in corso...";
    });
}

static void processPendingModelImport() {
    if (!g_pendingModelImport) return;
    g_pendingModelImport = false;
    std::string path = g_pendingModelImportPath;
    g_pendingModelImportPath.clear();

    StaticMesh mesh;
    mesh.exactImport = true;
    bool ok = false;
    try {
        ok = mesh.load(path, kWorldModelTargetSize);
    } catch (...) {
        ok = false;
    }
    if (!ok || !mesh.loaded) {
        g_exportMsg = "Import fallito (file non supportato o troppo pesante)";
        std::fprintf(stderr, "[Import] FAIL %s\n", path.c_str());
        return;
    }

    auto slash = path.find_last_of("\\/");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::string name = "Model" + std::to_string(g_world.nextModelIndex());
    auto& wm = g_world.addModel(name, path, insertPositionInFront());
    wm.scale = {1, 1, 1};
    wm.rig.fileHasBones = mesh.hasBones;
    wm.rig.simpleRig = false;
    g_wsMeshes.push_back(std::move(mesh));
    setupRiggedModel(g_world.models.size() - 1, wm, wm.rig.fileHasBones);

    g_selectedPart = -1;
    g_selectedSound = -1;
    int idx = (int)g_world.models.size() - 1;
    g_selectedModel = idx;
    g_showExplorer = true;
    g_showProps = true;
    g_exportMsg = "Importato: " + base + " — conferma Rig";
    askRigOrOpenAnimator(idx);
    offerBackpackForGunImport(path);
}

static void insertSound() {
    ensureDefaultBeepWav();
    auto& s = g_sounds.createSound();
    s.soundId = g_soundPathBuf[0] ? g_soundPathBuf : (blockyDataDir() + "/sounds/beep.wav");
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = (int)g_sounds.sounds.size() - 1;
    g_exportMsg = "Sound creato — Play dalla Properties";
}

static void duplicateSelected() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    WorkspacePart copy = g_world.parts[g_selectedPart];
    copy.name += "_Copy";
    copy.position.x += copy.size.x + 1.f;
    g_world.parts.push_back(std::move(copy));
    selectPart((int)g_world.parts.size() - 1);
    g_undo.recordInsert(g_selectedPart, g_world.parts[g_selectedPart]);
}

static void deleteSelected() {
    if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
        if (g_animEditModel == g_selectedModel) {
            g_showAnimEditor = false;
            g_showRigPrompt = false;
            g_animPlaying = false;
            g_animEditModel = -1;
        }
        g_world.models.erase(g_world.models.begin() + g_selectedModel);
        if (g_selectedModel < (int)g_wsMeshes.size())
            g_wsMeshes.erase(g_wsMeshes.begin() + g_selectedModel);
        if (g_selectedModel < (int)g_wsRigged.size())
            g_wsRigged.erase(g_wsRigged.begin() + g_selectedModel);
        selectModel(-1);
        return;
    }
    if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
        g_world.labels.erase(g_world.labels.begin() + g_selectedLabel);
        selectLabel(-1);
        return;
    }
    if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) {
        g_sounds.stopAll();
        g_sounds.sounds.erase(g_sounds.sounds.begin() + g_selectedSound);
        selectSound(-1);
        return;
    }
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    g_undo.recordDelete(g_selectedPart, g_world.parts[g_selectedPart]);
    g_world.parts.erase(g_world.parts.begin() + g_selectedPart);
    g_editingPartScript = false;
    selectPart(-1);
    markPlaceDirty();
}
