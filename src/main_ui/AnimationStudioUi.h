// Animation Studio: rig e animazione stile Blender / Moon Animator dentro
// Studio. Il motore e' in engine/anim/ (testato da test_animation); qui c'e'
// solo l'interfaccia e il collegamento con le Part del mondo.
//
// Come si usa:
//   1. seleziona un personaggio (Model o parti R6/R15) e premi "Auto Rig",
//      oppure "Rig vuoto" e costruisci le ossa a mano (Edit Mode)
//   2. Pose Mode: clicca una giuntura nel viewport e trascina per ruotare,
//      oppure scrivi i numeri nel pannello
//   3. Animate Mode: sposta la testina sulla timeline, metti la posa, I
//      (inserisci chiave). Spazio = play/pausa.
//
// Le Part si muovono davvero (come Moon Animator) solo mentre la finestra e'
// aperta; chiudendola (o con "Riposo") tornano esattamente dov'erano.
//
// Usa le globali di main.cpp: incluso da StudioUiPanel.h.

#include "engine/anim/AnimBlend.h"
#include "engine/anim/AnimCurve.h"
#include "engine/anim/AnimIO.h"
#include "engine/anim/AnimStateMachine.h"
#include "engine/anim/AutoRig.h"
#include "engine/anim/BoneConstraints.h"
#include "engine/anim/Ik.h"

namespace an = blk::anim;

struct AnimStudioState {
    an::Armature arm;
    std::vector<an::PartBinding> binds;
    std::vector<int> bindPart;             // indice in g_world.parts per ogni legame
    std::vector<an::Transform> partRest;   // dove stavano le parti prima del rig
    std::vector<an::BoneConstraint> constraints;
    std::vector<an::Action> actions;
    std::vector<an::NamedPose> poses;
    an::Pose pose;                         // posa di Pose Mode / ultima mostrata
    int activeAction = 0;
    int bone = -1;                         // osso attivo
    an::Mode mode = an::Mode::Pose;
    float frame = 0.f;
    bool playing = false;
    bool rigged = false;
    bool appliedToWorld = false;
    // timeline / curve editor
    float zoom = 10.f;       // pixel per frame
    float viewStart = -2.f;  // primo frame visibile
    float curveVCenter = 0.f, curveVScale = 4.f; // pixel per unita'
    bool curveCh[9] = {true, true, true, true, true, true, false, false, false};
    bool globalRotate = false;
    bool showNames = true, showAxes = false;
    char saveName[64] = "animazione";
    char poseName[48] = "Posa";
    char eventParam[48] = "";
    int eventType = 0;
    an::NamedPose clipboard;
    std::string status;
    // anteprima State Machine
    an::AnimStateMachine machine;
    an::AnimParams params;
    bool machinePreview = false;
    // riproduzione, onion skin, auto-key, appunti delle chiavi
    float speed = 1.f;
    // Come l'Animation Editor di Roblox: muovere un osso mette subito la chiave.
    bool autoKey = true;
    // Posa toccata in Animate e non ancora in chiave: si vede quella (non
    // l'animazione) finche' non si mette la chiave o si cambia frame. Prima
    // toccare un osso faceva passare a Pose Mode e l'animazione "spariva".
    bool poseDirty = false;
    float lastFrameShown = -1e9f;
    int gizmo = 0;            // 0 anelli (ruota), 1 frecce (sposta), 2 nessuno
    int lastSelPart = -2;     // per seguire il clic sulle part del rig
    bool onion = false;
    int onionCount = 2, onionStep = 5;
    std::vector<an::Action::KeyClip> keyClip;
    float timeScale = 2.f;     // per "Ridimensiona tempo"
};

// Annulla / Ripeti dell'Animation Studio (Ctrl+Z / Ctrl+Y con la finestra a
// fuoco): un'istantanea di Action, posa, vincoli e libreria pose. Le modifiche
// si riconoscono da sole confrontando un'impronta quando il mouse e' libero,
// cosi' un trascinamento lungo e' un solo passo.
struct AnimSnapshot {
    std::vector<an::Action> actions;
    an::Pose pose;
    std::vector<an::BoneConstraint> constraints;
    std::vector<an::NamedPose> poses;
    int activeAction = 0;
};
struct AnimUndoState {
    std::vector<AnimSnapshot> undo, redo;
    AnimSnapshot last;
    uint64_t lastData = 0, lastPose = 0;
    size_t lastBones = (size_t)-1;
};
static AnimUndoState g_asUndo;

static AnimStudioState g_as;
static bool g_showAnimStudio = false;

static an::Action& asAction() {
    if (g_as.actions.empty()) {
        an::Action a;
        a.name = "Idle";
        g_as.actions.push_back(a);
    }
    g_as.activeAction = std::clamp(g_as.activeAction, 0, (int)g_as.actions.size() - 1);
    return g_as.actions[(size_t)g_as.activeAction];
}

static an::Transform asPartTransform(const WorkspacePart& p) {
    an::Transform t;
    t.pos = p.position;
    t.rot = partgroups::toQuat(p.rotation);
    return t;
}

/** Le parti da riggare: il Model selezionato, le parti selezionate, o tutto il mondo. */
static std::vector<int> asCandidateParts() {
    std::vector<int> out;
    const std::vector<int> groups = studioSelectedWholeGroups();
    if (!groups.empty()) {
        out = partgroups::memberParts(g_world, groups.front());
    } else {
        for (int i : g_multiParts) out.push_back(i);
        if (g_selectedPart >= 0 && std::find(out.begin(), out.end(), g_selectedPart) == out.end()) out.push_back(g_selectedPart);
    }
    if (out.size() <= 1) {
        out.clear();
        for (int i = 0; i < (int)g_world.parts.size(); ++i) out.push_back(i);
    }
    return out;
}

/** Rimette le parti dove stavano prima del rig (chiusura, Edit Mode, nuovo rig). */
static void asRestoreWorld() {
    if (!g_as.appliedToWorld) return;
    for (size_t i = 0; i < g_as.bindPart.size() && i < g_as.partRest.size(); ++i) {
        const int pi = g_as.bindPart[i];
        if (pi < 0 || pi >= (int)g_world.parts.size()) continue;
        g_world.parts[(size_t)pi].position = g_as.partRest[i].pos;
        g_world.parts[(size_t)pi].rotation = partgroups::toEuler(g_as.partRest[i].rot);
    }
    g_as.appliedToWorld = false;
}

static void asResetRig() {
    asRestoreWorld();
    g_as.arm = an::Armature{};
    g_as.binds.clear();
    g_as.bindPart.clear();
    g_as.partRest.clear();
    g_as.constraints.clear();
    g_as.pose.reset(0);
    g_as.bone = -1;
    g_as.rigged = false;
}

static void asAutoRig() {
    asResetRig();
    std::vector<an::PartBox> boxes;
    std::vector<int> idx = asCandidateParts();
    for (int i : idx) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        boxes.push_back({p.name, p.position, p.size, partgroups::toQuat(p.rotation)});
    }
    an::AutoRigResult r = an::autoRig(boxes);
    if (r.kind == an::RigKind::None) {
        g_as.status = "Auto Rig: " + r.error;
        return;
    }
    g_as.arm = std::move(r.armature);
    g_as.binds = std::move(r.bindings);
    for (const auto& b : g_as.binds) {
        int found = -1;
        for (int i : idx)
            if (g_world.parts[(size_t)i].name == b.part) { found = i; break; }
        g_as.bindPart.push_back(found);
        g_as.partRest.push_back(found >= 0 ? asPartTransform(g_world.parts[(size_t)found]) : an::Transform{});
    }
    g_as.pose.reset(g_as.arm.bones.size());
    g_as.rigged = true;
    g_as.bone = 0;
    g_as.status = std::string("Auto Rig ") + (r.kind == an::RigKind::R15 ? "R15" : "R6") + ": "
                + std::to_string(g_as.arm.bones.size()) + " ossa, " + std::to_string(g_as.binds.size()) + " parti legate";
}

/**
 * Rig Builder (come Avatar > Rig Builder di Roblox): mette davanti alla
 * camera un manichino di Part con le misure del player, lo raggruppa in un
 * Model e lo rigga. Il player vero non e' fatto di Part nel mondo, quindi si
 * anima questo manichino e l'animazione vale poi per chi ha lo stesso rig.
 */
static void asInsertRig(bool r15) {
    struct P { std::string name; glm::vec3 pos, size, color; };
    const glm::vec3 skin{0.95f, 0.95f, 0.97f}, shirt{0.18f, 0.45f, 0.92f}, pants{0.22f, 0.72f, 0.32f};
    std::vector<P> parts;
    if (!r15) {
        // Le stesse misure di Humanoid::createR6Blocky (il corpo del player).
        for (const auto& d : Humanoid::createR6Blocky().parts) {
            if (d.name == "HumanoidRootPart") continue;
            const glm::vec3 col = d.name == "Torso" ? shirt : (d.name.find("Leg") != std::string::npos ? pants : skin);
            parts.push_back({d.name, d.offset, d.size, col});
        }
    } else {
        parts = {
            {"LowerTorso", {0, 2.6f, 0}, {2, 0.4f, 1}, shirt},   {"UpperTorso", {0, 3.4f, 0}, {2, 1.2f, 1}, shirt},
            {"Head", {0, 4.6f, 0}, {1.2f, 1.2f, 1.2f}, skin},
            {"LeftUpperArm", {-1.5f, 3.6f, 0}, {1, 0.8f, 1}, skin}, {"LeftLowerArm", {-1.5f, 2.9f, 0}, {1, 0.6f, 1}, skin},
            {"LeftHand", {-1.5f, 2.4f, 0}, {1, 0.4f, 1}, skin},
            {"RightUpperArm", {1.5f, 3.6f, 0}, {1, 0.8f, 1}, skin}, {"RightLowerArm", {1.5f, 2.9f, 0}, {1, 0.6f, 1}, skin},
            {"RightHand", {1.5f, 2.4f, 0}, {1, 0.4f, 1}, skin},
            {"LeftUpperLeg", {-0.5f, 2.0f, 0}, {1, 0.8f, 1}, pants}, {"LeftLowerLeg", {-0.5f, 1.2f, 0}, {1, 0.8f, 1}, pants},
            {"LeftFoot", {-0.5f, 0.6f, 0}, {1, 0.4f, 1}, pants},
            {"RightUpperLeg", {0.5f, 2.0f, 0}, {1, 0.8f, 1}, pants}, {"RightLowerLeg", {0.5f, 1.2f, 0}, {1, 0.8f, 1}, pants},
            {"RightFoot", {0.5f, 0.6f, 0}, {1, 0.4f, 1}, pants},
        };
    }
    glm::vec3 base = insertPositionInFront();
    base.y = 0.f;
    std::vector<int> idx;
    for (size_t i = 0; i < parts.size(); ++i) {
        g_world.addPart(parts[i].name, base + parts[i].pos, parts[i].size, parts[i].color);
        idx.push_back((int)g_world.parts.size() - 1);
        g_undo.recordInsert(idx.back(), g_world.parts.back());
    }
    const std::vector<PartGroup> before = g_world.groups;
    const std::vector<int> partsBefore = UndoStack::partGroups(g_world);
    const int gid = partgroups::group(g_world, idx, r15 ? "R15 Rig" : "R6 Rig");
    g_undo.recordGroupChange(before, partsBefore, g_world);
    studioSelectGroup(gid, idx.front());
    markPlaceDirty();
    asAutoRig();
    g_as.mode = an::Mode::Pose;
}

static void asEmptyRig() {
    asResetRig();
    glm::vec3 c(0.f);
    if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size()) c = g_world.parts[(size_t)g_selectedPart].position;
    an::Transform t;
    t.pos = c;
    g_as.arm.addBone("Root", -1, t, 1.f);
    g_as.pose.reset(1);
    g_as.rigged = true;
    g_as.bone = 0;
    g_as.mode = an::Mode::Edit;
    g_as.status = "Rig vuoto: aggiungi ossa in Edit Mode e lega le parti";
}

/** Lega la parte selezionata all'osso attivo (rig personalizzati). */
static void asBindSelectedPart() {
    if (!g_as.arm.valid(g_as.bone) || g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) return;
    asRestoreWorld();
    std::vector<an::Transform> g;
    g_as.arm.restGlobals(g);
    const WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
    for (size_t i = 0; i < g_as.bindPart.size(); ++i)
        if (g_as.bindPart[i] == g_selectedPart) { // gia' legata: cambia osso
            g_as.binds[i].bone = g_as.bone;
            g_as.binds[i].offset = an::compose(an::inverse(g[(size_t)g_as.bone]), g_as.partRest[i]);
            return;
        }
    const an::Transform world = asPartTransform(p);
    g_as.binds.push_back({g_as.bone, p.name, an::compose(an::inverse(g[(size_t)g_as.bone]), world)});
    g_as.bindPart.push_back(g_selectedPart);
    g_as.partRest.push_back(world);
    g_as.status = "\"" + p.name + "\" legata a " + g_as.arm.bones[(size_t)g_as.bone].name;
}

/** La posa da mostrare adesso: animazione (Animate), posa a mano (Pose), riposo (Edit). */
static an::Pose asEvaluate(float dt) {
    an::Pose p;
    if (g_as.mode == an::Mode::Edit) {
        p.reset(g_as.arm.bones.size());
        return p;
    }
    if (g_as.machinePreview && !g_as.machine.states.empty()) {
        p = g_as.machine.update(g_as.arm, dt, g_as.params);
    } else if (g_as.mode == an::Mode::Animate && !(g_as.poseDirty && !g_as.playing)) {
        p = asAction().sample(g_as.arm, g_as.frame);
    } else {
        p = g_as.pose;
    }
    std::vector<an::Transform> g;
    g_as.arm.poseGlobals(p, g);
    an::applyConstraints(g_as.arm, p, g, g_as.constraints);
    return p;
}

static void asApplyToWorld(const an::Pose& p) {
    const std::vector<an::Transform> w = an::partWorlds(g_as.arm, p, g_as.binds);
    for (size_t i = 0; i < w.size() && i < g_as.bindPart.size(); ++i) {
        const int pi = g_as.bindPart[i];
        if (pi < 0 || pi >= (int)g_world.parts.size()) continue;
        g_world.parts[(size_t)pi].position = w[i].pos;
        g_world.parts[(size_t)pi].rotation = partgroups::toEuler(w[i].rot);
    }
    g_as.appliedToWorld = !w.empty();
}

/**
 * Se il rig e' stato spostato nello Studio a finestra chiusa (o dopo un
 * salvataggio), lo scheletro lo segue: prima l'animazione lo riportava di
 * scatto dov'era quando era stato riggato.
 */
static void asSyncRigToWorld() {
    for (size_t k = 0; k < g_as.bindPart.size() && k < g_as.partRest.size(); ++k) {
        const int pi = g_as.bindPart[k];
        if (pi < 0 || pi >= (int)g_world.parts.size()) continue;
        const an::Transform cur = asPartTransform(g_world.parts[(size_t)pi]);
        const an::Transform& old = g_as.partRest[k];
        if (glm::length(cur.pos - old.pos) < 1e-4f && std::fabs(glm::dot(cur.rot, old.rot)) > 0.999999f) return;
        an::Transform d = an::compose(cur, an::inverse(old));
        d.scale = glm::vec3(1.f);
        for (auto& bone : g_as.arm.bones)
            if (bone.parent < 0) bone.rest = an::compose(d, bone.rest);
        for (size_t j = 0; j < g_as.bindPart.size() && j < g_as.partRest.size(); ++j) {
            const int pj = g_as.bindPart[j];
            if (pj >= 0 && pj < (int)g_world.parts.size()) g_as.partRest[j] = asPartTransform(g_world.parts[(size_t)pj]);
        }
        g_as.status = "Il rig e' stato spostato: lo scheletro l'ha seguito";
        return;
    }
}

/** Ogni frame: playback e parti del mondo nella posa giusta. */
static void animStudioTick(float dt) {
    // Salvataggi, autosalvataggi e versioni esportano il mondo a riposo;
    // il frame dopo la posa torna da sola.
    if (!PlaceExporter::beforeExportHook()) PlaceExporter::beforeExportHook() = &asRestoreWorld;
    g_animStudioGizmoBlock = false;
    g_animStudioDriving = false;
    if (!g_showAnimStudio || g_screen != AppScreen::Studio || g_studioTesting) {
        asRestoreWorld();
        g_as.playing = false;
        return;
    }
    if (!g_as.rigged) return;

    // Clic su una part del rig nel viewport = seleziona il suo osso (come Roblox);
    // il gizmo dello Studio li' non serve: si usa quello dell'osso.
    if (g_selectedPart != g_as.lastSelPart) {
        g_as.lastSelPart = g_selectedPart;
        for (size_t k = 0; k < g_as.bindPart.size() && k < g_as.binds.size(); ++k)
            if (g_as.bindPart[k] == g_selectedPart && g_as.arm.valid(g_as.binds[k].bone)) { g_as.bone = g_as.binds[k].bone; break; }
    }
    if (g_as.mode != an::Mode::Edit)
        for (int bp : g_as.bindPart)
            if (bp >= 0 && bp == g_selectedPart) { g_animStudioGizmoBlock = true; break; }
    g_animStudioDriving = g_as.mode != an::Mode::Edit;

    an::Action& a = asAction();
    // Cambiare frame (o riprodurre) butta via la posa non messa in chiave, come Blender.
    const float shown = std::round(g_as.frame);
    if (shown != g_as.lastFrameShown) { g_as.lastFrameShown = shown; g_as.poseDirty = false; }
    if (g_as.playing) g_as.poseDirty = false;
    if (g_as.playing && g_as.mode == an::Mode::Animate) {
        g_as.frame += dt * a.fps * g_as.speed;
        if (g_as.frame > a.frameEnd) {
            if (a.loop) g_as.frame = a.frameStart + std::fmod(g_as.frame - a.frameStart, std::max(1.f, a.frameEnd - a.frameStart));
            else { g_as.frame = a.frameEnd; g_as.playing = false; }
        }
    }
    if (g_as.mode == an::Mode::Edit) {
        asRestoreWorld();
        return;
    }
    if (!g_as.appliedToWorld) asSyncRigToWorld();
    const an::Pose p = asEvaluate(dt);
    if ((g_as.mode == an::Mode::Animate && !g_as.poseDirty) || g_as.machinePreview) g_as.pose = p;
    asApplyToWorld(p);
}

/**
 * La posa e' stata toccata a mano. In Animate si resta in Animate (la timeline
 * non sparisce): la posa toccata si vede finche' non va in chiave (subito con
 * l'auto-key, o col tasto I).
 */
static void asPoseEdited() {
    if (g_as.mode == an::Mode::Animate) {
        g_as.poseDirty = true;
        g_as.playing = false;
        if (!g_as.autoKey) g_as.status = "Posa modificata: I per metterla in chiave (o accendi Auto-key)";
    } else if (g_as.mode == an::Mode::Edit) {
        g_as.mode = an::Mode::Pose;
    }
}

// ---- Annulla / Ripeti -----------------------------------------------------------------

namespace as_undo {
struct Hash {
    uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, size_t n) {
        const unsigned char* c = (const unsigned char*)p;
        for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; }
    }
    void f(float v) { bytes(&v, sizeof v); }
    void i(int v) { bytes(&v, sizeof v); }
    void s(const std::string& v) { bytes(v.data(), v.size()); i((int)v.size()); }
    void v3(const glm::vec3& v) { f(v.x); f(v.y); f(v.z); }
    void t(const an::Transform& x) { v3(x.pos); f(x.rot.w); f(x.rot.x); f(x.rot.y); f(x.rot.z); v3(x.scale); }
};

inline uint64_t dataHash() {
    Hash h;
    h.i((int)g_as.actions.size());
    h.i(g_as.activeAction);
    for (const auto& a : g_as.actions) {
        h.s(a.name); h.f(a.fps); h.f(a.frameStart); h.f(a.frameEnd); h.i(a.loop);
        for (const auto& c : a.curves) {
            h.s(c.bone); h.i((int)c.channel); h.i(c.mute); h.i(c.locked);
            for (const auto& k : c.keys) {
                h.f(k.frame); h.f(k.value); h.i((int)k.interp); h.i((int)k.handles);
                h.f(k.hIn.x); h.f(k.hIn.y); h.f(k.hOut.x); h.f(k.hOut.y);
            }
        }
        for (const auto& e : a.events) { h.f(e.frame); h.s(e.name); h.s(e.param); }
        for (const auto& m : a.markers) { h.f(m.frame); h.s(m.name); }
    }
    for (const auto& c : g_as.constraints) {
        h.i((int)c.type); h.s(c.owner); h.s(c.target); h.s(c.pole); h.i(c.enabled); h.f(c.influence);
        h.v3(c.targetPoint); h.v3(c.offset); h.v3(c.minV); h.v3(c.maxV); h.v3(c.lookAxis);
        h.i(c.chainLength); h.f(c.twist); h.i(c.axis[0]); h.i(c.axis[1]); h.i(c.axis[2]);
    }
    h.i((int)g_as.poses.size());
    for (const auto& p : g_as.poses) h.s(p.name);
    return h.h;
}

inline uint64_t poseHash() {
    Hash h;
    for (const auto& t : g_as.pose.local) h.t(t);
    return h.h;
}

inline AnimSnapshot capture() {
    return {g_as.actions, g_as.pose, g_as.constraints, g_as.poses, g_as.activeAction};
}

inline void restore(const AnimSnapshot& s) {
    g_as.actions = s.actions;
    g_as.pose = s.pose;
    g_as.constraints = s.constraints;
    g_as.poses = s.poses;
    g_as.activeAction = s.activeAction;
    g_asUndo.last = s;
    g_asUndo.lastData = dataHash();
    g_asUndo.lastPose = poseHash();
}
} // namespace as_undo

/** Auto-key (registra): le ossa che la posa ha cambiato ricevono una chiave alla testina. */
static int asAutoKeyChangedBones() {
    an::Action& a = asAction();
    const float f = std::round(g_as.frame);
    const an::Pose ref = a.sample(g_as.arm, f);
    int n = 0;
    for (size_t i = 0; i < g_as.arm.bones.size() && i < g_as.pose.local.size(); ++i) {
        const an::Transform& p = g_as.pose.local[i];
        const an::Transform r = ref.get((int)i);
        const bool same = glm::length(p.pos - r.pos) < 1e-4f && std::fabs(glm::dot(p.rot, r.rot)) > 0.99999f
                       && glm::length(p.scale - r.scale) < 1e-4f;
        if (same) continue;
        a.insertKeys(g_as.arm.bones[i].name, p, f, an::Action::KeyAll);
        ++n;
    }
    return n;
}

/** Ogni frame (dopo la finestra): se qualcosa e' cambiato, il passo prima va in Annulla. */
static void asTrackUndo() {
    if (!g_as.rigged) { g_asUndo = AnimUndoState{}; return; }
    if (g_asUndo.lastBones != g_as.arm.bones.size()) {   // rig nuovo o ossa cambiate
        g_asUndo = AnimUndoState{};
        g_asUndo.lastBones = g_as.arm.bones.size();
        as_undo::restore(as_undo::capture());
        return;
    }
    if (ImGui::IsAnyItemActive() || ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1)) return;
    const bool poseEditable = (g_as.mode == an::Mode::Pose || (g_as.mode == an::Mode::Animate && g_as.poseDirty))
                           && !g_as.machinePreview;
    uint64_t hp = as_undo::poseHash();
    const bool poseEdited = poseEditable && hp != g_asUndo.lastPose;
    if (poseEdited && g_as.autoKey) {
        const int n = asAutoKeyChangedBones();
        g_as.poseDirty = false;
        if (n) {
            g_as.mode = an::Mode::Animate;
            g_as.status = "Auto-key: " + std::to_string(n) + " ossa in chiave al frame " + std::to_string((int)std::round(g_as.frame));
        }
    }
    const uint64_t hd = as_undo::dataHash();
    if (hd != g_asUndo.lastData || poseEdited) {
        g_asUndo.undo.push_back(std::move(g_asUndo.last));
        if (g_asUndo.undo.size() > 80) g_asUndo.undo.erase(g_asUndo.undo.begin());
        g_asUndo.redo.clear();
        g_asUndo.last = as_undo::capture();
        g_asUndo.lastData = hd;
        g_asUndo.lastPose = hp;
    } else if (hp != g_asUndo.lastPose) {
        // La posa cambiata dalla riproduzione o dalla testina non e' una modifica.
        g_asUndo.last.pose = g_as.pose;
        g_asUndo.lastPose = hp;
    }
}

static void asUndo() {
    if (g_asUndo.undo.empty()) { g_as.status = "Niente da annullare"; return; }
    g_asUndo.redo.push_back(as_undo::capture());
    AnimSnapshot s = std::move(g_asUndo.undo.back());
    g_asUndo.undo.pop_back();
    as_undo::restore(s);
    g_as.status = "Annullato (" + std::to_string(g_asUndo.undo.size()) + " passi rimasti)";
}

static void asRedo() {
    if (g_asUndo.redo.empty()) { g_as.status = "Niente da ripetere"; return; }
    g_asUndo.undo.push_back(as_undo::capture());
    AnimSnapshot s = std::move(g_asUndo.redo.back());
    g_asUndo.redo.pop_back();
    as_undo::restore(s);
    g_as.status = "Ripetuto";
}

// ---- Navigazione tra le chiavi -------------------------------------------------------------

static void asJumpKey(int dir) {
    an::Action& a = asAction();
    const std::vector<float> fr = a.keyedFrames();
    const float cur = std::round(g_as.frame);
    float best = cur;
    if (dir > 0) {
        for (float f : fr) if (f > cur + 0.01f) { best = f; break; }
    } else {
        for (auto it = fr.rbegin(); it != fr.rend(); ++it) if (*it < cur - 0.01f) { best = *it; break; }
    }
    g_as.frame = best;
    g_as.playing = false;
    g_as.mode = an::Mode::Animate;
}

static void asStepFrame(int d) {
    an::Action& a = asAction();
    g_as.frame = glm::clamp(std::round(g_as.frame) + (float)d, a.frameStart, a.frameEnd);
    g_as.playing = false;
    g_as.mode = an::Mode::Animate;
}

/**
 * Bake: l'Action con vincoli e IK applicati diventa chiavi vere (una ogni
 * `step` frame) in una nuova Action, come "Bake Action" di Blender. Serve
 * perche' in gioco l'animazione si riproduce senza i vincoli dell'editor.
 */
static void asBakeAction(int step) {
    const an::Action src = asAction();
    an::Action out = src;
    out.curves.clear();
    out.name = src.name + " (bake)";
    step = std::max(1, step);
    for (float f = src.frameStart; f <= src.frameEnd + 0.01f; f += (float)step) {
        an::Pose p = src.sample(g_as.arm, f);
        std::vector<an::Transform> g;
        g_as.arm.poseGlobals(p, g);
        an::applyConstraints(g_as.arm, p, g, g_as.constraints);
        for (size_t i = 0; i < g_as.arm.bones.size(); ++i) out.insertKeys(g_as.arm.bones[i].name, p.get((int)i), f, an::Action::KeyAll);
    }
    g_as.actions.push_back(out);
    g_as.activeAction = (int)g_as.actions.size() - 1;
    g_as.status = "Bake: " + std::to_string(out.keyCount()) + " chiavi in \"" + out.name + "\" (vincoli e IK inclusi)";
}

/** Cambia la durata: tutte le chiavi (ed eventi) scalano dal primo frame. */
static void asSetDuration(float newEnd) {
    an::Action& a = asAction();
    const float len = a.frameEnd - a.frameStart;
    const float nl = std::max(1.f, newEnd - a.frameStart);
    if (len <= 0.f) return;
    const float k = nl / len;
    a.selectAll(true);
    a.scaleSelected(k, a.frameStart);
    a.selectAll(false);
    for (auto& e : a.events) e.frame = a.frameStart + (e.frame - a.frameStart) * k;
    for (auto& m : a.markers) m.frame = a.frameStart + (m.frame - a.frameStart) * k;
    a.frameEnd = a.frameStart + nl;
    g_as.frame = std::min(g_as.frame, a.frameEnd);
}

static void asCopyKeys() {
    g_as.keyClip = asAction().copySelected();
    g_as.status = g_as.keyClip.empty() ? "Seleziona delle chiavi da copiare" : std::to_string(g_as.keyClip.size()) + " chiavi copiate";
}

static void asPasteKeys() {
    if (g_as.keyClip.empty()) { g_as.status = "Niente da incollare"; return; }
    const int n = asAction().paste(g_as.keyClip, std::round(g_as.frame));
    g_as.mode = an::Mode::Animate;
    g_as.status = std::to_string(n) + " chiavi incollate al frame " + std::to_string((int)std::round(g_as.frame));
}

// ---- Tasti -------------------------------------------------------------------------

static void asInsertKey(int mask, bool allBones) {
    if (!g_as.rigged) return;
    an::Action& a = asAction();
    const float f = std::round(g_as.frame);
    if (allBones) {
        for (size_t i = 0; i < g_as.arm.bones.size(); ++i)
            a.insertKeys(g_as.arm.bones[i].name, g_as.pose.get((int)i), f, mask);
    } else if (g_as.arm.valid(g_as.bone)) {
        a.insertKeys(g_as.arm.bones[(size_t)g_as.bone].name, g_as.pose.get(g_as.bone), f, mask);
    }
    g_as.poseDirty = false;
    g_as.status = "Chiave al frame " + std::to_string((int)f);
}

static void asPollHotkeys(bool windowFocused) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !g_as.rigged) return;
    // Tasti attivi con la finestra a fuoco o col mouse sul viewport.
    if (!windowFocused && io.WantCaptureKeyboard) return;
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
        if (g_as.mode != an::Mode::Animate) g_as.mode = an::Mode::Animate;
        asInsertKey(an::Action::KeyAll, io.KeyShift);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_K, false)) ImGui::OpenPopup("##askeymenu");
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && windowFocused) g_as.playing = !g_as.playing;
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && windowFocused) {
        const int n = asAction().deleteSelected();
        if (n) g_as.status = std::to_string(n) + " chiavi eliminate";
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && windowFocused) {
        an::Action& a = asAction();
        float minF = 1e9f;
        for (const auto& r : a.selected()) minF = std::min(minF, a.curves[(size_t)r.curve].keys[(size_t)r.key].frame);
        if (minF < 1e8f) g_as.status = std::to_string(a.duplicateSelected(std::round(g_as.frame) - minF)) + " chiavi duplicate alla testina";
    }
    if (!windowFocused) return;
    // Come l'Animation Editor di Roblox e Blender.
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) asRedo(); else asUndo(); }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) asRedo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) asCopyKeys();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) asPasteKeys();
    if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        // A = seleziona tutto; Alt+A = deseleziona.
        asAction().selectAll(!io.KeyAlt);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Comma, true)) asJumpKey(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_Period, true)) asJumpKey(1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) asStepFrame(io.KeyShift ? -10 : -1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) asStepFrame(io.KeyShift ? 10 : 1);
    if (ImGui::IsKeyPressed(ImGuiKey_O, false) && !io.KeyCtrl) g_as.onion = !g_as.onion;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.KeyCtrl) g_as.autoKey = !g_as.autoKey;
}

/** Voci per le chiavi selezionate: nel menu K, nel menu Chiavi e col tasto destro sul dope sheet. */
static void asKeyEditItems() {
    an::Action& a = asAction();
    const bool any = !a.selected().empty();
    if (ImGui::BeginMenu("Easing", any)) {
        if (ImGui::MenuItem("Constant (scalino)")) a.setInterpolationSelected(an::Interp::Constant);
        if (ImGui::MenuItem("Linear")) a.setInterpolationSelected(an::Interp::Linear);
        if (ImGui::MenuItem("Cubic / Bezier (morbida)")) a.setEaseSelected(true);
        if (ImGui::MenuItem("Ease In-Out (parte e arriva piano)")) a.setEaseSelected(false);
        ImGui::Separator();
        if (ImGui::MenuItem("Elastic (molla all'arrivo)")) a.setInterpolationSelected(an::Interp::Elastic);
        if (ImGui::MenuItem("Bounce (rimbalza)")) a.setInterpolationSelected(an::Interp::Bounce);
        if (ImGui::MenuItem("Back (sfora e torna)")) a.setInterpolationSelected(an::Interp::Back);
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Copia chiavi", "Ctrl+C", false, any)) asCopyKeys();
    if (ImGui::MenuItem("Incolla alla testina", "Ctrl+V", false, !g_as.keyClip.empty())) asPasteKeys();
    if (ImGui::MenuItem("Duplica alla testina", "Ctrl+D", false, any)) {
        float minF = 1e9f;
        for (const auto& r : a.selected()) minF = std::min(minF, a.curves[(size_t)r.curve].keys[(size_t)r.key].frame);
        a.duplicateSelected(std::round(g_as.frame) - minF);
    }
    if (ImGui::MenuItem("Elimina", "Canc", false, any)) a.deleteSelected();
    ImGui::Separator();
    if (ImGui::MenuItem("Inverti nel tempo", nullptr, false, any)) a.reverseSelected();
    ImGui::SetNextItemWidth(70);
    ImGui::DragFloat("##tsc", &g_as.timeScale, 0.05f, 0.1f, 10.f, "x%.2f");
    ImGui::SameLine();
    if (ImGui::MenuItem("Scala tempo dalla testina", nullptr, false, any)) a.scaleSelected(g_as.timeScale, std::round(g_as.frame));
    ImGui::Separator();
    if (ImGui::MenuItem("Seleziona tutto", "A")) a.selectAll(true);
    if (ImGui::MenuItem("Seleziona chiavi alla testina")) a.selectAtFrame(std::round(g_as.frame), ImGui::GetIO().KeyShift);
    if (ImGui::MenuItem("Deseleziona", "Alt+A")) a.selectAll(false);
}

static void asKeyMenu() {
    if (!ImGui::BeginPopup("##askeymenu")) return;
    ImGui::TextDisabled("Inserisci chiave (I)");
    if (ImGui::MenuItem("Posizione + Rotazione + Scala")) asInsertKey(an::Action::KeyAll, false);
    if (ImGui::MenuItem("Solo Posizione")) asInsertKey(an::Action::KeyLoc, false);
    if (ImGui::MenuItem("Solo Rotazione")) asInsertKey(an::Action::KeyRot, false);
    if (ImGui::MenuItem("Solo Scala")) asInsertKey(an::Action::KeyScale, false);
    if (ImGui::MenuItem("Tutte le ossa")) asInsertKey(an::Action::KeyAll, true);
    ImGui::Separator();
    ImGui::TextDisabled("Interpolazione (chiavi selezionate)");
    asKeyEditItems();
    ImGui::EndPopup();
}

// ---- Pannello ossa (Outliner + proprieta') ---------------------------------------------

static void asBoneTree(int parent, int depth) {
    for (int c : g_as.arm.children(parent)) {
        const an::Bone& b = g_as.arm.bones[(size_t)c];
        ImGui::PushID(c);
        const bool kids = !g_as.arm.children(c).empty();
        ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!kids) fl |= ImGuiTreeNodeFlags_Leaf;
        if (c == g_as.bone) fl |= ImGuiTreeNodeFlags_Selected;
        const bool open = ImGui::TreeNodeEx("##b", fl, "%s%s", b.name.c_str(), b.deform ? "" : "  (controllo)");
        if (ImGui::IsItemClicked()) g_as.bone = c;
        if (open) {
            asBoneTree(c, depth + 1);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

static void asBonePanel() {
    an::Armature& arm = g_as.arm;
    ImGui::TextDisabled("%s  -  %d ossa", arm.name.c_str(), (int)arm.bones.size());
    ImGui::BeginChild("##asbones", {0, 170}, ImGuiChildFlags_Borders);
    asBoneTree(-1, 0);
    ImGui::EndChild();
    if (!arm.valid(g_as.bone)) {
        ImGui::TextDisabled("Nessun osso selezionato");
        return;
    }
    an::Bone& b = arm.bones[(size_t)g_as.bone];
    if (g_as.mode == an::Mode::Edit) {
        static char nameBuf[64];
        static int nameFor = -1;
        if (nameFor != g_as.bone) { std::snprintf(nameBuf, sizeof(nameBuf), "%s", b.name.c_str()); nameFor = g_as.bone; }
        if (ImGui::InputText("Nome", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // I legami usano l'indice dell'osso, le curve il nome: rinominando, le curve seguono.
            const std::string oldName = b.name;
            if (!arm.renameBone(g_as.bone, nameBuf)) g_as.status = "Nome gia' usato";
            else
                for (auto& act : g_as.actions)
                    for (auto& c : act.curves)
                        if (c.bone == oldName) { c.bone = nameBuf; if (c.group == oldName) c.group = nameBuf; }
        }
        // Padre
        const char* pname = b.parent >= 0 ? arm.bones[(size_t)b.parent].name.c_str() : "(nessuno)";
        if (ImGui::BeginCombo("Padre", pname)) {
            if (ImGui::Selectable("(nessuno)", b.parent < 0)) arm.setParent(g_as.bone, -1);
            for (int i = 0; i < (int)arm.bones.size(); ++i) {
                if (i == g_as.bone || arm.isAncestor(g_as.bone, i)) continue;
                if (ImGui::Selectable(arm.bones[(size_t)i].name.c_str(), b.parent == i)) arm.setParent(g_as.bone, i);
            }
            ImGui::EndCombo();
        }
        an::Bone& bb = arm.bones[(size_t)g_as.bone];
        ImGui::DragFloat3("Posizione (riposo)", &bb.rest.pos.x, 0.05f);
        glm::vec3 e = glm::degrees(glm::eulerAngles(bb.rest.rot));
        if (ImGui::DragFloat3("Rotazione (riposo)", &e.x, 0.5f)) bb.rest.rot = glm::quat(glm::radians(e));
        ImGui::DragFloat("Lunghezza", &bb.length, 0.02f, 0.05f, 100.f);
        ImGui::Checkbox("Deforma (skinning)", &bb.deform);
        if (ImGui::Button("+ Osso figlio")) {
            an::Transform t;
            t.pos = {0.f, bb.length, 0.f};
            const int nb = arm.addBone("Bone", g_as.bone, t, bb.length);
            g_as.pose.reset(arm.bones.size());
            g_as.bone = nb;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplica")) { g_as.bone = arm.duplicateBone(g_as.bone, ImGui::GetIO().KeyShift); g_as.pose.reset(arm.bones.size()); }
        ImGui::SameLine();
        if (ImGui::Button("Elimina") && arm.bones.size() > 1) {
            const int removed = g_as.bone;
            arm.removeBone(removed);
            for (auto& bd : g_as.binds) bd.bone = bd.bone == removed ? -1 : (bd.bone > removed ? bd.bone - 1 : bd.bone);
            g_as.pose.reset(arm.bones.size());
            g_as.bone = std::min(removed, (int)arm.bones.size() - 1);
        }
        if (ImGui::Button("Lega la parte selezionata a quest'osso", {-1, 0})) asBindSelectedPart();
        return;
    }

    // Pose / Animate
    if (g_as.pose.local.size() < arm.bones.size()) g_as.pose.local.resize(arm.bones.size());
    an::Transform& t = g_as.pose.local[(size_t)g_as.bone];
    bool changed = false;
    changed |= ImGui::DragFloat3("Location", &t.pos.x, 0.02f);
    glm::vec3 e = glm::degrees(glm::eulerAngles(t.rot));
    if (ImGui::DragFloat3("Rotation (locale)", &e.x, 0.5f)) { t.rot = glm::quat(glm::radians(e)); changed = true; }
    changed |= ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.01f, 50.f);
    ImGui::Checkbox("Gizmo negli assi del mondo", &g_as.globalRotate);
    // Rotazione globale: attorno agli assi del MONDO, non dell'osso (15 gradi, Shift = 5).
    ImGui::TextDisabled("Ruota nel mondo:");
    static const char* kAxBtn[6] = {"-X", "+X", "-Y", "+Y", "-Z", "+Z"};
    for (int b = 0; b < 6; ++b) {
        ImGui::SameLine();
        if (!ImGui::SmallButton(kAxBtn[b])) continue;
        glm::vec3 axis(0.f);
        axis[b / 2] = 1.f;
        const float deg = (ImGui::GetIO().KeyShift ? 5.f : 15.f) * (b % 2 ? 1.f : -1.f);
        std::vector<an::Transform> g;
        arm.poseGlobals(g_as.pose, g);
        arm.setGlobalRotation(g_as.pose, g, g_as.bone, glm::angleAxis(glm::radians(deg), axis) * g[(size_t)g_as.bone].rot);
        changed = true;
    }
    if (ImGui::Button("Azzera osso")) { t = an::Transform{}; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Azzera tutto")) { g_as.pose.reset(arm.bones.size()); changed = true; }
    if (changed) asPoseEdited();
}

// ---- Timeline + Dope Sheet ------------------------------------------------------------

static float asFrameToX(float f, float x0) { return x0 + (f - g_as.viewStart) * g_as.zoom; }
static float asXToFrame(float x, float x0) { return g_as.viewStart + (x - x0) / g_as.zoom; }

static void asTransportBar() {
    an::Action& a = asAction();
    if (ImGui::Button("|<")) g_as.frame = a.frameStart;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Inizio");
    ImGui::SameLine();
    if (ImGui::Button("<<")) asJumpKey(-1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chiave precedente (,)");
    ImGui::SameLine();
    if (ImGui::Button("<")) asStepFrame(-1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frame precedente (freccia sinistra, Shift = 10)");
    ImGui::SameLine();
    if (ImGui::Button(g_as.playing ? "Pausa" : "Play")) {
        g_as.playing = !g_as.playing;
        if (g_as.playing) g_as.mode = an::Mode::Animate;
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) asStepFrame(1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frame successivo (freccia destra, Shift = 10)");
    ImGui::SameLine();
    if (ImGui::Button(">>")) asJumpKey(1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chiave successiva (.)");
    ImGui::SameLine();
    if (ImGui::Button(">|")) g_as.frame = a.frameEnd;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fine");
    ImGui::SameLine();
    if (ImGui::Button("Stop")) { g_as.playing = false; g_as.frame = a.frameStart; }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &a.loop);
    ImGui::SameLine();
    {
        static const float kSpeeds[] = {0.1f, 0.25f, 0.5f, 1.f, 1.5f, 2.f};
        char cur[16];
        std::snprintf(cur, sizeof(cur), "x%g", g_as.speed);
        ImGui::SetNextItemWidth(62);
        if (ImGui::BeginCombo("##aspeed", cur)) {
            for (float s : kSpeeds) {
                char lab[16];
                std::snprintf(lab, sizeof(lab), "x%g", s);
                if (ImGui::Selectable(lab, g_as.speed == s)) g_as.speed = s;
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Velocita' di riproduzione nell'editor (non cambia l'animazione)");
    }
    ImGui::SameLine();
    {
        const bool rec = g_as.autoKey;
        if (rec) ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.18f, 1.f});
        if (ImGui::Button(rec ? "REC" : "Auto-key")) g_as.autoKey = !g_as.autoKey;
        if (rec) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Auto-key (R): ogni posa che cambi va in chiave da sola alla testina");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Onion", &g_as.onion);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Onion skin (O): fantasmi dello scheletro prima (blu) e dopo (verde)");
    ImGui::SameLine();
    ImGui::BeginDisabled(g_asUndo.undo.empty());
    if (ImGui::Button("Annulla")) asUndo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(g_asUndo.redo.empty());
    if (ImGui::Button("Ripeti")) asRedo();
    ImGui::EndDisabled();
    // seconda riga: tempo e intervallo
    ImGui::SetNextItemWidth(70);
    ImGui::DragFloat("##frame", &g_as.frame, 0.2f, a.frameStart, a.frameEnd, "frame %.0f");
    ImGui::SameLine();
    ImGui::Text("%.2f s", (g_as.frame - a.frameStart) / std::max(1.f, a.fps));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("fps", &a.fps, 0.5f, 1.f, 240.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    float range[2] = {a.frameStart, a.frameEnd};
    if (ImGui::DragFloat2("inizio/fine", range, 0.5f, 0.f, 100000.f, "%.0f")) {
        a.frameStart = std::min(range[0], range[1] - 1.f);
        a.frameEnd = std::max(range[1], a.frameStart + 1.f);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::SliderFloat("zoom", &g_as.zoom, 2.f, 60.f, "%.0f px");
    ImGui::SameLine();
    // Durata in secondi che stira tutta l'animazione (chiavi ed eventi), non la taglia.
    static float durEdit = 0.f;
    if (ImGui::Button("Durata...")) { durEdit = a.durationSeconds(); ImGui::OpenPopup("##asdur"); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Allunga o accorcia tutta l'animazione");
    if (ImGui::BeginPopup("##asdur")) {
        ImGui::Text("Ora: %.2f s", a.durationSeconds());
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("secondi", &durEdit, 0.02f, 0.05f, 600.f, "%.2f");
        if (ImGui::Button("Stira l'animazione")) {
            asSetDuration(a.frameStart + durEdit * std::max(1.f, a.fps));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Fine = ultima chiave")) {
            const std::vector<float> fr = a.keyedFrames();
            if (!fr.empty()) a.frameEnd = std::max(a.frameStart + 1.f, fr.back());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/** Righe: riepilogo + una per osso con chiavi (o tutte le ossa). Ritorna l'osso di ogni riga (-1 = riepilogo). */
static std::vector<int> asDopeRows() {
    std::vector<int> rows = {-1};
    for (int i : g_as.arm.order()) rows.push_back(i);
    return rows;
}

static void asDopeSheet() {
    an::Action& a = asAction();
    const std::vector<int> rows = asDopeRows();
    const float labelW = 150.f, rowH = 20.f, rulerH = 22.f, eventH = 18.f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float h = std::max(120.f, std::min(avail.y, rulerH + eventH + rowH * (float)rows.size() + 8.f));
    ImGui::BeginChild("##dope", {0, h}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float x0 = o.x + labelW;
    const float right = o.x + w;
    const ImU32 cGrid = IM_COL32(70, 74, 84, 255), cText = IM_COL32(200, 205, 215, 255);

    // Righello: tacche adattive (ogni 1/5/10/30 frame a seconda dello zoom).
    dl->AddRectFilled({o.x, o.y}, {right, o.y + rulerH}, IM_COL32(40, 43, 50, 255));
    const float step = g_as.zoom >= 30 ? 1.f : g_as.zoom >= 12 ? 5.f : g_as.zoom >= 5 ? 10.f : 30.f;
    for (float f = std::floor(g_as.viewStart / step) * step; asFrameToX(f, x0) < right; f += step) {
        const float x = asFrameToX(f, x0);
        if (x < x0) continue;
        dl->AddLine({x, o.y + rulerH - 6}, {x, o.y + rulerH}, cText);
        dl->AddLine({x, o.y + rulerH}, {x, o.y + h}, cGrid);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", f);
        dl->AddText({x + 2, o.y + 2}, cText, buf);
    }
    // Zona fuori dall'intervallo dell'Action, piu' scura.
    const float xs = asFrameToX(a.frameStart, x0), xe = asFrameToX(a.frameEnd, x0);
    if (xs > x0) dl->AddRectFilled({x0, o.y + rulerH}, {std::min(xs, right), o.y + h}, IM_COL32(0, 0, 0, 60));
    if (xe < right) dl->AddRectFilled({std::max(xe, x0), o.y + rulerH}, {right, o.y + h}, IM_COL32(0, 0, 0, 60));

    // Clic/trascinamento sul righello = sposta la testina.
    ImGui::SetCursorScreenPos({x0, o.y});
    ImGui::InvisibleButton("##ruler", {std::max(1.f, right - x0), rulerH});
    if (ImGui::IsItemActive()) {
        g_as.frame = std::round(glm::clamp(asXToFrame(ImGui::GetIO().MousePos.x, x0), a.frameStart, a.frameEnd));
        g_as.mode = an::Mode::Animate;
        g_as.playing = false;
    }

    // Riga eventi.
    const float ey = o.y + rulerH;
    dl->AddText({o.x + 6, ey + 2}, IM_COL32(255, 200, 120, 255), "Eventi");
    for (const auto& ev : a.events) {
        const float x = asFrameToX(ev.frame, x0);
        if (x < x0 || x > right) continue;
        dl->AddTriangleFilled({x, ey + eventH - 3}, {x - 5, ey + 3}, {x + 5, ey + 3}, IM_COL32(255, 170, 60, 255));
        if (ImGui::IsMouseHoveringRect({x - 6, ey}, {x + 6, ey + eventH})) ImGui::SetTooltip("%s %s (frame %.0f)", ev.name.c_str(), ev.param.c_str(), ev.frame);
    }
    for (const auto& m : a.markers) {
        const float x = asFrameToX(m.frame, x0);
        if (x >= x0 && x <= right) {
            dl->AddLine({x, ey}, {x, o.y + h}, IM_COL32(120, 200, 255, 90));
            dl->AddText({x + 3, ey + 2}, IM_COL32(120, 200, 255, 255), m.name.c_str());
        }
    }

    // Righe ossa e rombi.
    const float rowsY = ey + eventH;
    static bool dragging = false, boxing = false;
    static float dragStartFrame = 0.f, dragApplied = 0.f;
    static ImVec2 boxStart;
    const ImGuiIO& io = ImGui::GetIO();
    bool clickedKey = false;
    for (size_t r = 0; r < rows.size(); ++r) {
        const int bone = rows[r];
        const float y = rowsY + rowH * (float)r;
        if (r % 2) dl->AddRectFilled({o.x, y}, {right, y + rowH}, IM_COL32(255, 255, 255, 8));
        const std::string label = bone < 0 ? std::string("Riepilogo") : g_as.arm.bones[(size_t)bone].name;
        const bool sel = bone == g_as.bone;
        dl->AddText({o.x + 6 + (bone < 0 ? 0.f : 10.f), y + 3}, sel ? IM_COL32(255, 210, 90, 255) : cText, label.c_str());
        if (bone >= 0 && ImGui::IsMouseHoveringRect({o.x, y}, {x0, y + rowH}) && ImGui::IsMouseClicked(0)) g_as.bone = bone;

        // Rombi: un frame con chiavi = un rombo (selezionato se lo e' una sua chiave).
        const std::string filter = bone < 0 ? std::string() : g_as.arm.bones[(size_t)bone].name;
        for (float f : a.keyedFrames(filter)) {
            const float x = asFrameToX(f, x0);
            if (x < x0 - 6 || x > right + 6) continue;
            bool anySel = false;
            for (const auto& c : a.curves) {
                if (!filter.empty() && c.bone != filter) continue;
                const int k = c.findKey(f);
                if (k >= 0 && c.keys[(size_t)k].selected) { anySel = true; break; }
            }
            const float cy = y + rowH * 0.5f, s = 5.f;
            const ImU32 col = anySel ? IM_COL32(255, 180, 40, 255) : IM_COL32(220, 220, 230, 255);
            dl->AddQuadFilled({x, cy - s}, {x + s, cy}, {x, cy + s}, {x - s, cy}, col);
            dl->AddQuad({x, cy - s}, {x + s, cy}, {x, cy + s}, {x - s, cy}, IM_COL32(20, 20, 20, 255));
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect({x - s - 1, cy - s - 1}, {x + s + 1, cy + s + 1})) {
                clickedKey = true;
                if (!io.KeyShift && !anySel) a.selectAll(false);
                for (auto& c : a.curves) {
                    if (!filter.empty() && c.bone != filter) continue;
                    const int k = c.findKey(f);
                    if (k >= 0) c.keys[(size_t)k].selected = io.KeyShift && anySel ? false : true;
                }
                if (bone >= 0) g_as.bone = bone;
                dragging = true;
                dragStartFrame = asXToFrame(io.MousePos.x, x0);
                dragApplied = 0.f;
            }
        }
    }

    // Trascinare le chiavi selezionate (a frame interi), o box select su uno spazio vuoto.
    if (dragging) {
        if (ImGui::IsMouseDown(0)) {
            const float want = std::round(asXToFrame(io.MousePos.x, x0) - dragStartFrame);
            if (want != dragApplied) { a.moveSelected(want - dragApplied); dragApplied = want; }
        } else {
            dragging = false;
        }
    } else if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !clickedKey && io.MousePos.x > x0 && io.MousePos.y > rowsY) {
        boxing = true;
        boxStart = io.MousePos;
    }
    if (boxing) {
        dl->AddRect(boxStart, io.MousePos, IM_COL32(255, 200, 80, 200));
        dl->AddRectFilled(boxStart, io.MousePos, IM_COL32(255, 200, 80, 30));
        if (!ImGui::IsMouseDown(0)) {
            boxing = false;
            const float f0 = asXToFrame(std::min(boxStart.x, io.MousePos.x), x0), f1 = asXToFrame(std::max(boxStart.x, io.MousePos.x), x0);
            const int r0 = (int)((std::min(boxStart.y, io.MousePos.y) - rowsY) / rowH), r1 = (int)((std::max(boxStart.y, io.MousePos.y) - rowsY) / rowH);
            std::vector<int> curveIdx;
            for (int r = std::max(0, r0); r <= r1 && r < (int)rows.size(); ++r) {
                for (size_t c = 0; c < a.curves.size(); ++c) {
                    if (rows[(size_t)r] < 0 || a.curves[c].bone == g_as.arm.bones[(size_t)rows[(size_t)r]].name) curveIdx.push_back((int)c);
                }
            }
            a.selectFrames(f0, f1, curveIdx, io.KeyShift);
        }
    }

    // Rotella: Ctrl = zoom, altrimenti scorre nel tempo.
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.f) {
        if (io.KeyCtrl) {
            const float fAtMouse = asXToFrame(io.MousePos.x, x0);
            g_as.zoom = glm::clamp(g_as.zoom * (io.MouseWheel > 0 ? 1.15f : 1.f / 1.15f), 2.f, 60.f);
            g_as.viewStart = fAtMouse - (io.MousePos.x - x0) / g_as.zoom;
        } else {
            g_as.viewStart -= io.MouseWheel * 30.f / g_as.zoom * 4.f;
        }
    }
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(2)) g_as.viewStart -= io.MouseDelta.x / g_as.zoom;

    // Testina.
    const float px = asFrameToX(g_as.frame, x0);
    if (px >= x0 && px <= right) {
        dl->AddLine({px, o.y}, {px, o.y + h}, IM_COL32(255, 70, 70, 255), 2.f);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", g_as.frame);
        dl->AddRectFilled({px - 12, o.y}, {px + 12, o.y + 16}, IM_COL32(200, 50, 50, 255), 3.f);
        dl->AddText({px - 8, o.y + 1}, IM_COL32(255, 255, 255, 255), buf);
    }
    // Tasto destro: menu delle chiavi (easing, copia/incolla, inverti...).
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1)) ImGui::OpenPopup("##asdopectx");
    if (ImGui::BeginPopup("##asdopectx")) {
        asKeyEditItems();
        ImGui::EndPopup();
    }
    ImGui::SetCursorScreenPos({o.x, o.y + h - 4});
    ImGui::Dummy({1, 1});
    ImGui::EndChild();
}

// ---- Curve editor (Graph Editor) ----------------------------------------------------------

static void asCurveEditor() {
    an::Action& a = asAction();
    if (!g_as.arm.valid(g_as.bone)) { ImGui::TextDisabled("Seleziona un osso"); return; }
    const std::string bone = g_as.arm.bones[(size_t)g_as.bone].name;
    static const ImU32 chCol[9] = {IM_COL32(240, 80, 80, 255), IM_COL32(90, 220, 90, 255), IM_COL32(90, 140, 255, 255),
                                   IM_COL32(255, 130, 130, 255), IM_COL32(140, 255, 140, 255), IM_COL32(140, 180, 255, 255),
                                   IM_COL32(200, 90, 90, 255), IM_COL32(80, 180, 80, 255), IM_COL32(80, 110, 200, 255)};
    for (int c = 0; c < 9; ++c) {
        if (c % 3) ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(chCol[c]));
        ImGui::Checkbox(an::channelName((an::Channel)c), &g_as.curveCh[c]);
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    const bool frameAll = ImGui::Button("Inquadra (Home)") || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Home));

    // La chiave selezionata, con i numeri da scrivere (come le proprieta' di una chiave in Roblox).
    {
        const std::vector<an::KeyRef> sel = a.selected();
        if (sel.size() == 1 && !a.curves[(size_t)sel[0].curve].locked) {
            an::FCurve& c = a.curves[(size_t)sel[0].curve];
            an::Keyframe& k = c.keys[(size_t)sel[0].key];
            ImGui::TextDisabled("%s %s:", c.bone.c_str(), an::channelName(c.channel));
            ImGui::SameLine();
            float fr = k.frame, val = k.value;
            ImGui::SetNextItemWidth(80);
            const bool fch = ImGui::DragFloat("frame##ks", &fr, 0.2f, -10000.f, 100000.f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            const bool vch = ImGui::DragFloat("valore##ks", &val, 0.05f, 0.f, 0.f, "%.3f");
            if ((fch && c.findKey(std::round(fr)) < 0) || vch) {
                const glm::vec2 d(std::round(fr) - k.frame, val - k.value);
                k.frame = std::round(fr);
                k.value = val;
                k.hIn += d;
                k.hOut += d;
                c.sort();
                c.recalcHandles();
            }
            ImGui::SameLine();
            int ip = (int)k.interp;
            const char* names[an::kInterpCount];
            for (int i = 0; i < an::kInterpCount; ++i) names[i] = an::interpName((an::Interp)i);
            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("##ksi", &ip, names, an::kInterpCount)) {
                // la chiave puo' essersi spostata col sort: si cerca di nuovo
                for (auto& kk : c.keys) if (kk.selected) kk.interp = (an::Interp)ip;
            }
        } else if (sel.size() > 1) {
            ImGui::TextDisabled("%d chiavi selezionate (tasto destro sul Dope Sheet per easing, copia, inverti)", (int)sel.size());
        }
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##curves", {0, std::max(160.f, avail.y)}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImVec2 sz = ImGui::GetContentRegionAvail();
    const float x0 = o.x + 40.f, right = o.x + sz.x, cy = o.y + sz.y * 0.5f;
    auto vToY = [&](float v) { return cy - (v - g_as.curveVCenter) * g_as.curveVScale; };
    auto yToV = [&](float y) { return g_as.curveVCenter - (y - cy) / g_as.curveVScale; };

    if (frameAll) {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& c : a.curves)
            if (c.bone == bone && g_as.curveCh[(int)c.channel])
                for (const auto& k : c.keys) { lo = std::min(lo, k.value); hi = std::max(hi, k.value); }
        if (lo <= hi) {
            g_as.curveVCenter = 0.5f * (lo + hi);
            g_as.curveVScale = glm::clamp((sz.y * 0.8f) / std::max(0.5f, hi - lo), 0.05f, 400.f);
        }
        g_as.viewStart = a.frameStart - 2.f;
        g_as.zoom = glm::clamp((sz.x - 60.f) / std::max(1.f, a.frameEnd - a.frameStart + 4.f), 2.f, 60.f);
    }

    // Griglia
    for (float f = std::floor(g_as.viewStart / 10.f) * 10.f; asFrameToX(f, x0) < right; f += 10.f) {
        const float x = asFrameToX(f, x0);
        if (x < x0) continue;
        dl->AddLine({x, o.y}, {x, o.y + sz.y}, IM_COL32(60, 64, 74, 255));
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", f);
        dl->AddText({x + 2, o.y + sz.y - 16}, IM_COL32(150, 155, 165, 255), buf);
    }
    const float vStep = std::pow(10.f, std::floor(std::log10(std::max(1e-3f, 60.f / g_as.curveVScale))));
    for (float v = std::floor(yToV(o.y + sz.y) / vStep) * vStep; vToY(v) > o.y; v += vStep) {
        const float y = vToY(v);
        dl->AddLine({x0, y}, {right, y}, std::fabs(v) < vStep * 0.01f ? IM_COL32(110, 110, 120, 255) : IM_COL32(52, 56, 64, 255));
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.3g", v);
        dl->AddText({o.x + 2, y - 7}, IM_COL32(150, 155, 165, 255), buf);
    }

    const ImGuiIO& io = ImGui::GetIO();
    static int dragCurve = -1, dragKey = -1, dragHandle = 0; // 0 chiave, -1 in, 1 out
    bool hitSomething = false;
    for (size_t ci = 0; ci < a.curves.size(); ++ci) {
        an::FCurve& c = a.curves[ci];
        if (c.bone != bone || !g_as.curveCh[(int)c.channel] || c.keys.empty()) continue;
        const ImU32 col = chCol[(int)c.channel];
        // Curva campionata ogni 3 pixel.
        ImVec2 prev{};
        bool first = true;
        for (float x = x0; x < right; x += 3.f) {
            const ImVec2 p{x, vToY(c.eval(asXToFrame(x, x0)))};
            if (!first) dl->AddLine(prev, p, c.mute ? IM_COL32(120, 120, 120, 120) : col, 1.6f);
            prev = p;
            first = false;
        }
        for (size_t k = 0; k < c.keys.size(); ++k) {
            an::Keyframe& kf = c.keys[k];
            const ImVec2 kp{asFrameToX(kf.frame, x0), vToY(kf.value)};
            if (kf.selected && kf.interp == an::Interp::Bezier) {
                const ImVec2 hi{asFrameToX(kf.hIn.x, x0), vToY(kf.hIn.y)}, ho{asFrameToX(kf.hOut.x, x0), vToY(kf.hOut.y)};
                dl->AddLine(hi, kp, IM_COL32(200, 200, 200, 160));
                dl->AddLine(kp, ho, IM_COL32(200, 200, 200, 160));
                dl->AddCircleFilled(hi, 3.5f, IM_COL32(230, 230, 230, 255));
                dl->AddCircleFilled(ho, 3.5f, IM_COL32(230, 230, 230, 255));
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !c.locked) {
                    const auto isNear = [&](ImVec2 p) { return std::fabs(io.MousePos.x - p.x) < 6 && std::fabs(io.MousePos.y - p.y) < 6; };
                    if (isNear(hi)) { dragCurve = (int)ci; dragKey = (int)k; dragHandle = -1; hitSomething = true; }
                    else if (isNear(ho)) { dragCurve = (int)ci; dragKey = (int)k; dragHandle = 1; hitSomething = true; }
                }
            }
            dl->AddRectFilled({kp.x - 4, kp.y - 4}, {kp.x + 4, kp.y + 4}, kf.selected ? IM_COL32(255, 180, 40, 255) : IM_COL32(20, 20, 20, 255));
            dl->AddRect({kp.x - 4, kp.y - 4}, {kp.x + 4, kp.y + 4}, col);
            if (!hitSomething && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && std::fabs(io.MousePos.x - kp.x) < 6
                && std::fabs(io.MousePos.y - kp.y) < 6) {
                if (!io.KeyShift && !kf.selected) a.selectAll(false);
                kf.selected = true;
                if (!c.locked) { dragCurve = (int)ci; dragKey = (int)k; dragHandle = 0; }
                hitSomething = true;
            }
        }
    }
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !hitSomething && !io.KeyShift) a.selectAll(false);

    if (dragCurve >= 0 && dragCurve < (int)a.curves.size()) {
        an::FCurve& c = a.curves[(size_t)dragCurve];
        if (ImGui::IsMouseDown(0) && dragKey >= 0 && dragKey < (int)c.keys.size()) {
            an::Keyframe& kf = c.keys[(size_t)dragKey];
            const float mf = asXToFrame(io.MousePos.x, x0), mv = yToV(io.MousePos.y);
            if (dragHandle == 0) {
                // La chiave si muove a frame interi e non scavalca le vicine.
                const float lo = dragKey > 0 ? c.keys[(size_t)dragKey - 1].frame + 1.f : -1e9f;
                const float hi = dragKey + 1 < (int)c.keys.size() ? c.keys[(size_t)dragKey + 1].frame - 1.f : 1e9f;
                const float nf = glm::clamp(std::round(mf), lo, hi);
                const glm::vec2 d(nf - kf.frame, mv - kf.value);
                kf.frame = nf;
                kf.value = mv;
                kf.hIn += d;
                kf.hOut += d;
            } else {
                // Maniglia: diventa libera; quella opposta resta allineata (come Aligned di Blender).
                kf.handles = an::HandleType::Free;
                glm::vec2& h = dragHandle < 0 ? kf.hIn : kf.hOut;
                glm::vec2& other = dragHandle < 0 ? kf.hOut : kf.hIn;
                h = {dragHandle < 0 ? std::min(mf, kf.frame) : std::max(mf, kf.frame), mv};
                const glm::vec2 dir = glm::vec2(kf.frame, kf.value) - h;
                const float len = glm::length(other - glm::vec2(kf.frame, kf.value));
                if (glm::length(dir) > 1e-4f) other = glm::vec2(kf.frame, kf.value) + glm::normalize(dir) * len;
            }
            c.recalcHandles();
        } else {
            dragCurve = -1;
        }
    }
    // Zoom (rotella; Ctrl = verticale) e pan (tasto centrale).
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.f) {
        const float k = io.MouseWheel > 0 ? 1.15f : 1.f / 1.15f;
        if (io.KeyCtrl) g_as.curveVScale = glm::clamp(g_as.curveVScale * k, 0.05f, 400.f);
        else {
            const float fAt = asXToFrame(io.MousePos.x, x0);
            g_as.zoom = glm::clamp(g_as.zoom * k, 2.f, 60.f);
            g_as.viewStart = fAt - (io.MousePos.x - x0) / g_as.zoom;
        }
    }
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(2)) {
        g_as.viewStart -= io.MouseDelta.x / g_as.zoom;
        g_as.curveVCenter += io.MouseDelta.y / g_as.curveVScale;
    }
    const float px = asFrameToX(g_as.frame, x0);
    if (px >= x0 && px <= right) dl->AddLine({px, o.y}, {px, o.y + sz.y}, IM_COL32(255, 70, 70, 200), 1.5f);
    ImGui::Dummy(sz);
    ImGui::EndChild();
}

// ---- Eventi, Pose, Vincoli, State Machine --------------------------------------------------

static void asEventsTab() {
    an::Action& a = asAction();
    static const char* kTypes[] = {"Footstep", "Jump", "Land", "Attack", "PlaySound", "SpawnParticle", "CustomEvent"};
    ImGui::SetNextItemWidth(140);
    ImGui::Combo("Tipo", &g_as.eventType, kTypes, IM_ARRAYSIZE(kTypes));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::InputTextWithHint("##evp", "parametro (es. id suono)", g_as.eventParam, sizeof(g_as.eventParam));
    ImGui::SameLine();
    if (ImGui::Button("Aggiungi alla testina")) {
        a.events.push_back({std::round(g_as.frame), kTypes[g_as.eventType], g_as.eventParam});
        std::sort(a.events.begin(), a.events.end(), [](const an::AnimEvent& x, const an::AnimEvent& y) { return x.frame < y.frame; });
    }
    if (ImGui::Button("+ Marker alla testina")) a.markers.push_back({std::round(g_as.frame), "Marker " + std::to_string(a.markers.size() + 1)});
    ImGui::Separator();
    for (size_t i = 0; i < a.events.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::Text("frame %3.0f  %s  %s", a.events[i].frame, a.events[i].name.c_str(), a.events[i].param.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { a.events.erase(a.events.begin() + (long)i); ImGui::PopID(); break; }
        ImGui::PopID();
    }
    ImGui::TextDisabled("Gli script li ricevono durante la riproduzione (come GetMarkerReachedSignal).");
}

static void asPosesTab() {
    an::Armature& arm = g_as.arm;
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##pn", g_as.poseName, sizeof(g_as.poseName));
    ImGui::SameLine();
    if (ImGui::Button("Salva posa nella libreria")) g_as.poses.push_back(an::capturePose(arm, g_as.pose, g_as.poseName));
    if (ImGui::Button("Copia posa")) g_as.clipboard = an::capturePose(arm, g_as.pose, "clipboard");
    ImGui::SameLine();
    if (ImGui::Button("Copia osso")) g_as.clipboard = an::capturePose(arm, g_as.pose, "clipboard", {g_as.bone});
    ImGui::SameLine();
    if (ImGui::Button("Incolla") && !g_as.clipboard.bones.empty()) { an::applyNamedPose(arm, g_as.pose, g_as.clipboard); asPoseEdited(); }
    ImGui::SameLine();
    if (ImGui::Button("Incolla specchiata") && !g_as.clipboard.bones.empty()) {
        an::Pose tmp;
        tmp.reset(arm.bones.size());
        an::applyNamedPose(arm, tmp, g_as.clipboard);
        g_as.pose = an::mirrorPose(arm, tmp);
        asPoseEdited();
    }
    if (ImGui::Button("Mirror Pose")) { g_as.pose = an::mirrorPose(arm, g_as.pose); asPoseEdited(); }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Animation (nuova Action)")) {
        g_as.actions.push_back(an::mirrorAction(asAction()));
        g_as.activeAction = (int)g_as.actions.size() - 1;
    }
    ImGui::Separator();
    for (size_t i = 0; i < g_as.poses.size(); ++i) {
        ImGui::PushID((int)i);
        if (ImGui::Button("Applica")) { an::applyNamedPose(arm, g_as.pose, g_as.poses[i]); asPoseEdited(); }
        ImGui::SameLine();
        if (ImGui::Button("50%")) { an::applyNamedPose(arm, g_as.pose, g_as.poses[i], 0.5f); asPoseEdited(); }
        ImGui::SameLine();
        ImGui::Text("%s  (%d ossa)", g_as.poses[i].name.c_str(), (int)g_as.poses[i].bones.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { g_as.poses.erase(g_as.poses.begin() + (long)i); ImGui::PopID(); break; }
        ImGui::PopID();
    }
}

static void asConstraintsTab() {
    an::Armature& arm = g_as.arm;
    if (!arm.valid(g_as.bone)) { ImGui::TextDisabled("Seleziona un osso"); return; }
    const std::string owner = arm.bones[(size_t)g_as.bone].name;
    static int addType = 0;
    const char* types[(int)an::ConstraintType::Count];
    for (int i = 0; i < (int)an::ConstraintType::Count; ++i) types[i] = an::constraintName((an::ConstraintType)i);
    ImGui::SetNextItemWidth(160);
    ImGui::Combo("##ct", &addType, types, (int)an::ConstraintType::Count);
    ImGui::SameLine();
    if (ImGui::Button(("Aggiungi a " + owner).c_str())) {
        an::BoneConstraint c;
        c.type = (an::ConstraintType)addType;
        c.owner = owner;
        if (c.type == an::ConstraintType::LimitRotation) { c.minV = glm::vec3(-90.f); c.maxV = glm::vec3(90.f); }
        if (c.type == an::ConstraintType::LimitScale) { c.minV = glm::vec3(0.5f); c.maxV = glm::vec3(2.f); }
        g_as.constraints.push_back(c);
    }
    ImGui::Separator();
    std::vector<an::Transform> g;
    arm.poseGlobals(g_as.pose, g);
    for (size_t i = 0; i < g_as.constraints.size(); ++i) {
        an::BoneConstraint& c = g_as.constraints[i];
        if (c.owner != owner) continue;
        ImGui::PushID((int)i);
        ImGui::Checkbox("##on", &c.enabled);
        ImGui::SameLine();
        const bool open = ImGui::TreeNodeEx("##c", ImGuiTreeNodeFlags_DefaultOpen, "%s", an::constraintName(c.type));
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { g_as.constraints.erase(g_as.constraints.begin() + (long)i); if (open) ImGui::TreePop(); ImGui::PopID(); break; }
        if (open) {
            const bool needsTarget = c.type != an::ConstraintType::LimitRotation && c.type != an::ConstraintType::LimitScale
                                  && c.type != an::ConstraintType::LimitLocation;
            if (needsTarget) {
                if (ImGui::BeginCombo("Bersaglio", c.target.empty() ? "(punto nel mondo)" : c.target.c_str())) {
                    if (ImGui::Selectable("(punto nel mondo)", c.target.empty())) c.target.clear();
                    for (const auto& b : arm.bones)
                        if (b.name != owner && ImGui::Selectable(b.name.c_str(), c.target == b.name)) c.target = b.name;
                    ImGui::EndCombo();
                }
                if (c.target.empty()) ImGui::DragFloat3("Punto", &c.targetPoint.x, 0.05f);
            }
            ImGui::SliderFloat("Influenza", &c.influence, 0.f, 1.f);
            switch (c.type) {
            case an::ConstraintType::CopyLocation:
            case an::ConstraintType::CopyRotation:
            case an::ConstraintType::CopyScale:
                ImGui::Checkbox("X", &c.axis[0]); ImGui::SameLine(); ImGui::Checkbox("Y", &c.axis[1]); ImGui::SameLine(); ImGui::Checkbox("Z", &c.axis[2]);
                if (c.type == an::ConstraintType::CopyLocation) ImGui::DragFloat3("Offset", &c.offset.x, 0.05f);
                break;
            case an::ConstraintType::LimitRotation:
            case an::ConstraintType::LimitScale:
            case an::ConstraintType::LimitLocation:
                ImGui::Checkbox("X", &c.axis[0]); ImGui::SameLine(); ImGui::Checkbox("Y", &c.axis[1]); ImGui::SameLine(); ImGui::Checkbox("Z", &c.axis[2]);
                ImGui::DragFloat3("Min", &c.minV.x, 0.5f);
                ImGui::DragFloat3("Max", &c.maxV.x, 0.5f);
                break;
            case an::ConstraintType::LookAt:
                ImGui::DragFloat3("Asse che guarda", &c.lookAxis.x, 0.05f, -1.f, 1.f);
                break;
            case an::ConstraintType::ChildOf:
                if (ImGui::Button("Set Inverse")) an::childOfSetInverse(arm, g, c);
                break;
            case an::ConstraintType::Ik:
                ImGui::SliderInt("Chain Length", &c.chainLength, 1, 8);
                if (ImGui::BeginCombo("Pole Target", c.pole.empty() ? "(nessuno)" : c.pole.c_str())) {
                    if (ImGui::Selectable("(nessuno)", c.pole.empty())) c.pole.clear();
                    for (const auto& b : arm.bones)
                        if (ImGui::Selectable(b.name.c_str(), c.pole == b.name)) c.pole = b.name;
                    ImGui::EndCombo();
                }
                ImGui::DragFloat("Twist", &c.twist, 0.5f, -180.f, 180.f);
                break;
            default:
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::TextDisabled("I vincoli si applicano in ordine, dopo l'animazione.");
}

static void asStateMachineTab() {
    if (ImGui::Button("Crea macchina Humanoid (Idle, Walk, Run, Jump, Fall, Climb, Swim, Attack)")) {
        auto byName = [](const char* n) -> const an::Action* {
            for (const auto& a : g_as.actions) if (a.name == n) return &a;
            return nullptr;
        };
        g_as.machine = an::humanoidStateMachine(byName("Idle"), byName("Walk"), byName("Run"), byName("Jump"), byName("Fall"),
                                                byName("Climb"), byName("Swim"), byName("Attack"));
        g_as.params = an::AnimParams{};
        g_as.params.flags["Grounded"] = true;
        g_as.status = "Macchina creata: le Action si collegano per nome (Idle, Walk, Run...)";
    }
    if (g_as.machine.states.empty()) {
        ImGui::TextDisabled("Rinomina le Action come gli stati (Idle, Walk, Run...) e crea la macchina.");
        return;
    }
    ImGui::Checkbox("Anteprima dal vivo", &g_as.machinePreview);
    ImGui::SameLine();
    ImGui::Text("Stato: %s%s", g_as.machine.currentName(), g_as.machine.fading() ? " (dissolvenza)" : "");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Speed", &g_as.params.numbers["Speed"], 0.f, 24.f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("VerticalSpeed", &g_as.params.numbers["VerticalSpeed"], -30.f, 30.f);
    ImGui::Checkbox("Grounded", &g_as.params.flags["Grounded"]);
    ImGui::SameLine();
    ImGui::Checkbox("Climbing", &g_as.params.flags["Climbing"]);
    ImGui::SameLine();
    ImGui::Checkbox("Swimming", &g_as.params.flags["Swimming"]);
    ImGui::SameLine();
    if (ImGui::Button("Attack!")) g_as.params.setTrigger("Attack");

    // Grafo: stati come riquadri, transizioni come frecce (trascinabili).
    ImGui::BeginChild("##smgraph", {0, std::max(140.f, ImGui::GetContentRegionAvail().y)}, ImGuiChildFlags_Borders);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImVec2 box{120, 34};
    auto boxAt = [&](int i) { const glm::vec2 p = g_as.machine.states[(size_t)i].editorPos; return ImVec2{o.x + 20 + p.x * 0.85f, o.y + 30 + p.y + (float)(i % 2) * 70.f}; };
    for (const auto& t : g_as.machine.transitions) {
        if (t.from < 0) continue;
        const ImVec2 a = boxAt(t.from), b = boxAt(t.to);
        const ImVec2 ca{a.x + box.x * 0.5f, a.y + box.y * 0.5f}, cb{b.x + box.x * 0.5f, b.y + box.y * 0.5f};
        dl->AddLine(ca, cb, IM_COL32(150, 160, 180, 160), 1.5f);
        const ImVec2 mid{(ca.x * 0.4f + cb.x * 0.6f), (ca.y * 0.4f + cb.y * 0.6f)};
        dl->AddCircleFilled(mid, 3.f, IM_COL32(200, 210, 230, 220));
    }
    for (int i = 0; i < (int)g_as.machine.states.size(); ++i) {
        const ImVec2 p = boxAt(i);
        const bool cur = i == g_as.machine.current;
        const bool hasAnim = g_as.machine.states[(size_t)i].action != nullptr;
        dl->AddRectFilled(p, {p.x + box.x, p.y + box.y}, cur ? IM_COL32(40, 120, 220, 255) : IM_COL32(52, 56, 66, 255), 6.f);
        dl->AddRect(p, {p.x + box.x, p.y + box.y}, hasAnim ? IM_COL32(120, 200, 120, 255) : IM_COL32(200, 120, 90, 255), 6.f);
        dl->AddText({p.x + 10, p.y + 9}, IM_COL32(240, 240, 245, 255), g_as.machine.states[(size_t)i].name.c_str());
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##st", box);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            g_as.machine.states[(size_t)i].editorPos.x += ImGui::GetIO().MouseDelta.x / 0.85f;
            g_as.machine.states[(size_t)i].editorPos.y += ImGui::GetIO().MouseDelta.y;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(hasAnim ? "Action collegata" : "Manca un'Action con questo nome");
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ---- Gizmo dell'osso selezionato ------------------------------------------------------------
// Anelli rosso/verde/blu = ruota attorno a X/Y/Z; frecce = sposta lungo X/Y/Z
// (assi dell'osso, o del mondo con "spazio globale"). Shift = scatti di 15
// gradi / 0.25 stud. Come i gizmo dell'Animation Editor di Roblox.

static void asBoneGizmo(const std::vector<an::Transform>& g) {
    static int dragAxis = -1;
    static float ang0 = 0.f;
    static glm::quat rot0{1, 0, 0, 0};
    static glm::vec3 pos0{0.f}, axisW{0.f};
    static ImVec2 m0{0.f, 0.f};
    if (g_as.gizmo == 2 || !g_as.arm.valid(g_as.bone) || g_as.mode == an::Mode::Edit || g_as.machinePreview
        || g_as.playing || (size_t)g_as.bone >= g.size()) {
        dragAxis = -1;
        return;
    }
    const int b = g_as.bone;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const glm::vec3 h = g[(size_t)b].pos;
    float hx, hy;
    if (!worldToScreen(h, hx, hy)) return;
    const float R = std::max(0.25f, glm::length(g_cam.pos - h) * 0.11f);
    glm::vec3 ax[3];
    for (int k = 0; k < 3; ++k) {
        glm::vec3 e(0.f);
        e[k] = 1.f;
        ax[k] = g_as.globalRotate ? e : glm::normalize(g[(size_t)b].rot * e);
    }
    static const ImU32 kCol[3] = {IM_COL32(235, 70, 70, 255), IM_COL32(80, 220, 90, 255), IM_COL32(80, 130, 255, 255)};
    const ImU32 kHot = IM_COL32(255, 215, 60, 255);
    auto segDist = [](ImVec2 p, ImVec2 a, ImVec2 c) {
        const ImVec2 ab{c.x - a.x, c.y - a.y};
        const float l2 = ab.x * ab.x + ab.y * ab.y;
        float t = l2 > 0.f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2 : 0.f;
        t = std::clamp(t, 0.f, 1.f);
        const float dx = p.x - (a.x + ab.x * t), dy = p.y - (a.y + ab.y * t);
        return std::sqrt(dx * dx + dy * dy);
    };

    // Disegno + quale maniglia e' sotto il mouse.
    int hover = -1;
    float best = 7.f;
    for (int k = 0; k < 3; ++k) {
        const ImU32 c = (dragAxis == k || (dragAxis < 0 && hover == k)) ? kHot : kCol[k];
        if (g_as.gizmo == 0) {
            const glm::vec3 ref = std::fabs(ax[k].y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            const glm::vec3 u = glm::normalize(glm::cross(ax[k], ref)), v = glm::cross(ax[k], u);
            ImVec2 pts[49];
            int n = 0;
            for (int s = 0; s <= 48; ++s) {
                const float t = (float)s / 48.f * 6.2831853f;
                float sx, sy;
                if (!worldToScreen(h + R * (std::cos(t) * u + std::sin(t) * v), sx, sy)) { n = 0; break; }
                pts[n++] = {sx, sy};
            }
            if (n < 2) continue;
            for (int s = 1; s < n; ++s) {
                const float d = segDist(io.MousePos, pts[s - 1], pts[s]);
                if (d < best) { best = d; hover = k; }
            }
            dl->AddPolyline(pts, n, (dragAxis == k || (dragAxis < 0 && hover == k)) ? kHot : c, 0, dragAxis == k ? 3.f : 2.f);
        } else {
            float ex, ey, sx, sy;
            if (!worldToScreen(h + ax[k] * R * 1.25f, ex, ey) || !worldToScreen(h + ax[k] * R * 0.3f, sx, sy)) continue;
            const float d = segDist(io.MousePos, {sx, sy}, {ex, ey});
            if (d < best) { best = d; hover = k; }
        }
    }
    if (g_as.gizmo == 1)   // frecce disegnate dopo, col colore giusto per l'hover
        for (int k = 0; k < 3; ++k) {
            float ex, ey, sx, sy;
            if (!worldToScreen(h + ax[k] * R * 1.25f, ex, ey) || !worldToScreen(h + ax[k] * R * 0.15f, sx, sy)) continue;
            const ImU32 c = (dragAxis == k || (dragAxis < 0 && hover == k)) ? kHot : kCol[k];
            dl->AddLine({sx, sy}, {ex, ey}, c, dragAxis == k ? 4.f : 3.f);
            const float dx = ex - sx, dy = ey - sy, l = std::max(1.f, std::sqrt(dx * dx + dy * dy));
            const ImVec2 dn{dx / l, dy / l}, nn{-dy / l, dx / l};
            dl->AddTriangleFilled({ex + dn.x * 10, ey + dn.y * 10}, {ex + nn.x * 6, ey + nn.y * 6}, {ex - nn.x * 6, ey - nn.y * 6}, c);
        }
    if (dragAxis >= 0) hover = dragAxis;

    // Una finestrina invisibile sotto il mouse: cosi' il clic va al gizmo e non
    // allo Studio (che selezionerebbe la part dietro).
    if (hover >= 0) {
        ImGui::SetNextWindowPos({io.MousePos.x - 20.f, io.MousePos.y - 20.f});
        ImGui::SetNextWindowSize({40.f, 40.f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("##asgizmo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings
                                               | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::InvisibleButton("##g", {40.f, 40.f});
        const bool pressed = ImGui::IsItemActivated();
        ImGui::End();
        ImGui::PopStyleVar();
        if (pressed && dragAxis < 0) {
            dragAxis = hover;
            axisW = ax[hover];
            rot0 = g[(size_t)b].rot;
            pos0 = h;
            m0 = io.MousePos;
            ang0 = std::atan2(io.MousePos.y - hy, io.MousePos.x - hx);
        }
    }
    if (dragAxis < 0) return;
    if (!ImGui::IsMouseDown(0)) { dragAxis = -1; return; }

    std::vector<an::Transform> gp;
    g_as.arm.poseGlobals(g_as.pose, gp);
    char info[64];
    if (g_as.gizmo == 0) {
        const float ang = std::atan2(io.MousePos.y - hy, io.MousePos.x - hx);
        const float sign = glm::dot(axisW, h - g_cam.pos) < 0.f ? -1.f : 1.f;   // asse verso chi guarda o no
        float delta = sign * (ang - ang0);
        while (delta > 3.14159265f) delta -= 6.2831853f;
        while (delta < -3.14159265f) delta += 6.2831853f;
        if (io.KeyShift) delta = glm::radians(std::round(glm::degrees(delta) / 15.f) * 15.f);
        if (std::fabs(delta) > 1e-4f) {
            asPoseEdited();
            g_as.arm.setGlobalRotation(g_as.pose, gp, b, glm::normalize(glm::angleAxis(delta, axisW) * rot0));
        }
        std::snprintf(info, sizeof(info), "%.1f°", glm::degrees(delta));
    } else {
        float sx, sy, ex, ey;
        float dist = 0.f;
        if (worldToScreen(pos0, sx, sy) && worldToScreen(pos0 + axisW * R, ex, ey)) {
            const ImVec2 sd{ex - sx, ey - sy};
            const float l2 = std::max(1.f, sd.x * sd.x + sd.y * sd.y);
            dist = ((io.MousePos.x - m0.x) * sd.x + (io.MousePos.y - m0.y) * sd.y) / l2 * R;
            if (io.KeyShift) dist = std::round(dist / 0.25f) * 0.25f;
        }
        if (std::fabs(dist) > 1e-4f) {
            asPoseEdited();
            g_as.arm.setGlobalPosition(g_as.pose, gp, b, pos0 + axisW * dist);
        }
        std::snprintf(info, sizeof(info), "%.2f stud", dist);
    }
    dl->AddText({io.MousePos.x + 16.f, io.MousePos.y + 8.f}, IM_COL32(255, 230, 120, 255), info);
}

// ---- Scheletro nel viewport ------------------------------------------------------------------

static void drawAnimStudioOverlay() {
    if (!g_showAnimStudio || !g_as.rigged || g_screen != AppScreen::Studio || g_studioTesting) return;
    const an::Pose p = g_as.mode == an::Mode::Edit ? an::Pose{} : g_as.pose;
    std::vector<an::Transform> g;
    g_as.arm.poseGlobals(p, g);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();

    // Onion skin: lo scheletro qualche frame prima (blu) e dopo (verde), sempre piu' tenue.
    if (g_as.onion && g_as.mode != an::Mode::Edit && !g_as.machinePreview) {
        const an::Action& act = asAction();
        for (int side = -1; side <= 1; side += 2) {
            for (int k = 1; k <= g_as.onionCount; ++k) {
                const float f = std::round(g_as.frame) + (float)(side * k * std::max(1, g_as.onionStep));
                if (f < act.frameStart || f > act.frameEnd) continue;
                an::Pose op = act.sample(g_as.arm, f);
                std::vector<an::Transform> og;
                g_as.arm.poseGlobals(op, og);
                an::applyConstraints(g_as.arm, op, og, g_as.constraints);
                g_as.arm.poseGlobals(op, og);
                const int alpha = std::max(40, 170 - 50 * (k - 1));
                const ImU32 oc = side < 0 ? IM_COL32(90, 150, 255, alpha) : IM_COL32(90, 230, 120, alpha);
                for (size_t i = 0; i < g_as.arm.bones.size(); ++i) {
                    float hx, hy, tx, ty;
                    if (!worldToScreen(og[i].pos, hx, hy) || !worldToScreen(g_as.arm.tail((int)i, og[i]), tx, ty)) continue;
                    dl->AddLine({hx, hy}, {tx, ty}, oc, 2.f);
                    dl->AddCircleFilled({hx, hy}, 2.5f, oc);
                }
            }
        }
    }

    static int dragBone = -1;
    static float dragAngle0 = 0.f;
    static glm::quat dragRot0{1, 0, 0, 0};
    for (size_t i = 0; i < g_as.arm.bones.size(); ++i) {
        const glm::vec3 h = g[i].pos, t = g_as.arm.tail((int)i, g[i]);
        float hx, hy, tx, ty;
        if (!worldToScreen(h, hx, hy) || !worldToScreen(t, tx, ty)) continue;
        const bool sel = (int)i == g_as.bone;
        const ImU32 col = sel ? IM_COL32(255, 190, 60, 255) : IM_COL32(120, 200, 255, 230);
        // Osso a "ottaedro" schiacciato come Blender: due linee larghe alla base.
        const ImVec2 dir{tx - hx, ty - hy};
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.f) {
            const ImVec2 n{-dir.y / len, dir.x / len};
            const float wdt = std::min(8.f, len * 0.12f);
            const ImVec2 b{hx + dir.x * 0.15f, hy + dir.y * 0.15f};
            dl->AddTriangleFilled({hx, hy}, {b.x + n.x * wdt, b.y + n.y * wdt}, {tx, ty}, (col & 0x00FFFFFF) | 0x50000000);
            dl->AddTriangleFilled({hx, hy}, {b.x - n.x * wdt, b.y - n.y * wdt}, {tx, ty}, (col & 0x00FFFFFF) | 0x50000000);
            dl->AddLine({hx, hy}, {b.x + n.x * wdt, b.y + n.y * wdt}, col, 1.5f);
            dl->AddLine({hx, hy}, {b.x - n.x * wdt, b.y - n.y * wdt}, col, 1.5f);
            dl->AddLine({b.x + n.x * wdt, b.y + n.y * wdt}, {tx, ty}, col, 1.5f);
            dl->AddLine({b.x - n.x * wdt, b.y - n.y * wdt}, {tx, ty}, col, 1.5f);
        }
        dl->AddCircleFilled({hx, hy}, sel ? 6.f : 4.5f, col);
        if (g_as.showNames) dl->AddText({hx + 7, hy - 7}, IM_COL32(230, 235, 245, 230), g_as.arm.bones[i].name.c_str());
        if (g_as.showAxes) {
            const glm::vec3 axes[3] = {g[i].rot * glm::vec3(0.4f, 0, 0), g[i].rot * glm::vec3(0, 0.4f, 0), g[i].rot * glm::vec3(0, 0, 0.4f)};
            const ImU32 ac[3] = {IM_COL32(240, 70, 70, 255), IM_COL32(70, 220, 70, 255), IM_COL32(70, 120, 255, 255)};
            for (int k = 0; k < 3; ++k) {
                float ax, ay;
                if (worldToScreen(h + axes[k], ax, ay)) dl->AddLine({hx, hy}, {ax, ay}, ac[k], 1.5f);
            }
        }

        // Giuntura cliccabile: una finestrina invisibile, cosi' Studio non seleziona le part sotto.
        char wid[24];
        std::snprintf(wid, sizeof(wid), "##asj%d", (int)i);
        ImGui::SetNextWindowPos({hx - 8, hy - 8});
        ImGui::SetNextWindowSize({16, 16});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin(wid, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::InvisibleButton("##j", {16, 16});
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_as.arm.bones[i].name.c_str());
        if (ImGui::IsItemClicked()) {
            g_as.bone = (int)i;
            if (g_as.mode != an::Mode::Edit) {
                dragBone = (int)i;
                dragAngle0 = std::atan2(io.MousePos.y - hy, io.MousePos.x - hx);
                if (g_as.pose.local.size() < g_as.arm.bones.size()) g_as.pose.local.resize(g_as.arm.bones.size());
                dragRot0 = g[i].rot;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // Trascinare una giuntura in Pose Mode ruota l'osso attorno all'asse della vista
    // (come R in Blender): l'angolo e' quello del mouse attorno alla giuntura.
    // Un clic senza trascinare seleziona soltanto (prima cambiava gia' la posa
    // e faceva sparire l'animazione).
    static int dragPrev = -1;
    static ImVec2 dragMouse0{0.f, 0.f};
    static bool dragMoved = false;
    if (dragBone != dragPrev) {
        dragPrev = dragBone;
        dragMouse0 = io.MousePos;
        dragMoved = false;
    }
    if (dragBone >= 0 && g_as.arm.valid(dragBone)) {
        if (!ImGui::IsMouseDown(0)) {
            dragBone = -1;
        } else {
            if (std::fabs(io.MousePos.x - dragMouse0.x) + std::fabs(io.MousePos.y - dragMouse0.y) > 3.f) dragMoved = true;
            float hx, hy;
            if (dragMoved && worldToScreen(g[(size_t)dragBone].pos, hx, hy)) {
                const float ang = std::atan2(io.MousePos.y - hy, io.MousePos.x - hx);
                const glm::mat4 inv = glm::inverse(g_lastView);
                const glm::vec3 viewAxis = glm::normalize(glm::vec3(inv[2]));  // verso chi guarda
                const glm::quat r = glm::angleAxis(-(ang - dragAngle0), viewAxis);
                asPoseEdited();
                std::vector<an::Transform> gp;
                g_as.arm.poseGlobals(g_as.pose, gp);
                g_as.arm.setGlobalRotation(g_as.pose, gp, dragBone, glm::normalize(r * dragRot0));
                dl->AddCircle({hx, hy}, 26.f, IM_COL32(255, 190, 60, 160), 32, 1.5f);
            }
        }
    }

    if (dragBone < 0) asBoneGizmo(g);
}

// ---- Finestra principale ------------------------------------------------------------------------

static std::string asAnimDir() {
    const std::string d = blockyDataDir() + "/animations";
    plat::makeDirs(d);
    return d;
}

static void drawAnimStudioWindow() {
    if (!g_showAnimStudio || g_screen != AppScreen::Studio) return;
    ImGui::SetNextWindowSize({980, 560}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animation Studio", &g_showAnimStudio, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Rig")) {
            if (ImGui::MenuItem("Inserisci Rig R6 (manichino del player)")) asInsertRig(false);
            if (ImGui::MenuItem("Inserisci Rig R15")) asInsertRig(true);
            if (ImGui::MenuItem("Auto Rig sulla selezione (R6 / R15)")) asAutoRig();
            if (ImGui::MenuItem("Rig vuoto (personalizzato)")) asEmptyRig();
            if (ImGui::MenuItem("Riposo (rimetti le parti)", nullptr, false, g_as.rigged)) { g_as.pose.reset(g_as.arm.bones.size()); asRestoreWorld(); }
            if (ImGui::MenuItem("Togli il rig", nullptr, false, g_as.rigged)) asResetRig();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("File")) {
            ImGui::SetNextItemWidth(180);
            ImGui::InputText("Nome", g_as.saveName, sizeof(g_as.saveName));
            const std::string path = asAnimDir() + "/" + g_as.saveName + ".blkanim";
            if (ImGui::MenuItem("Salva (.blkanim)", nullptr, false, g_as.rigged)) {
                g_as.status = an::saveFile(path, g_as.arm, g_as.actions, g_as.poses) ? "Salvato: " + path : "Salvataggio fallito";
            }
            if (ImGui::MenuItem("Apri (.blkanim)")) {
                an::Armature arm;
                std::vector<an::Action> acts;
                std::vector<an::NamedPose> poses;
                std::string err;
                if (an::loadFile(path, arm, acts, &poses, err)) {
                    // Il rig caricato usa le ossa del file; i legami alle parti si rifanno con Auto Rig o "Lega".
                    if (!g_as.rigged || g_as.arm.bones.size() != arm.bones.size()) { asResetRig(); g_as.arm = arm; g_as.rigged = true; }
                    g_as.actions = acts;
                    g_as.poses = poses;
                    g_as.pose.reset(g_as.arm.bones.size());
                    g_as.status = "Aperto: " + std::to_string(acts.size()) + " Action";
                } else {
                    g_as.status = "Apertura fallita: " + err;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Modifica", g_as.rigged)) {
            if (ImGui::MenuItem("Annulla", "Ctrl+Z", false, !g_asUndo.undo.empty())) asUndo();
            if (ImGui::MenuItem("Ripeti", "Ctrl+Y", false, !g_asUndo.redo.empty())) asRedo();
            ImGui::Separator();
            asKeyEditItems();
            ImGui::Separator();
            ImGui::MenuItem("Auto-key (registra)", "R", &g_as.autoKey);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Action", g_as.rigged)) {
            if (ImGui::MenuItem("Nuova Action")) {
                an::Action a;
                a.name = "Action " + std::to_string(g_as.actions.size() + 1);
                g_as.actions.push_back(a);
                g_as.activeAction = (int)g_as.actions.size() - 1;
            }
            if (ImGui::MenuItem("Duplica Action")) {
                an::Action copy = asAction();
                copy.name += " (copia)";
                g_as.actions.push_back(copy);
                g_as.activeAction = (int)g_as.actions.size() - 1;
            }
            if (ImGui::MenuItem("Elimina Action", nullptr, false, g_as.actions.size() > 1)) {
                g_as.actions.erase(g_as.actions.begin() + g_as.activeAction);
                g_as.activeAction = std::max(0, g_as.activeAction - 1);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Specchia in una nuova Action")) {
                g_as.actions.push_back(an::mirrorAction(asAction()));
                g_as.activeAction = (int)g_as.actions.size() - 1;
            }
            if (ImGui::BeginMenu("Bake vincoli e IK in chiavi")) {
                if (ImGui::MenuItem("Ogni frame")) asBakeAction(1);
                if (ImGui::MenuItem("Ogni 2 frame")) asBakeAction(2);
                if (ImGui::MenuItem("Ogni 5 frame")) asBakeAction(5);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("Loop", nullptr, &asAction().loop);
            if (ImGui::MenuItem("Fine = ultima chiave")) {
                const std::vector<float> fr = asAction().keyedFrames();
                if (!fr.empty()) asAction().frameEnd = std::max(asAction().frameStart + 1.f, fr.back());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Vista")) {
            ImGui::MenuItem("Nomi delle ossa", nullptr, &g_as.showNames);
            ImGui::MenuItem("Assi delle ossa", nullptr, &g_as.showAxes);
            ImGui::Separator();
            ImGui::MenuItem("Onion skin", "O", &g_as.onion);
            ImGui::SetNextItemWidth(120);
            ImGui::SliderInt("fantasmi per lato", &g_as.onionCount, 1, 4);
            ImGui::SetNextItemWidth(120);
            ImGui::SliderInt("ogni N frame", &g_as.onionStep, 1, 30);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Aiuto")) {
            ImGui::TextDisabled("Tasti (con la finestra a fuoco)");
            ImGui::BulletText("I chiave, Shift+I tutte le ossa, K menu chiave");
            ImGui::BulletText("Spazio play/pausa, frecce frame, Shift+frecce 10 frame");
            ImGui::BulletText(", e . chiave precedente/successiva");
            ImGui::BulletText("Ctrl+Z/Ctrl+Y annulla/ripeti, Ctrl+C/Ctrl+V copia/incolla chiavi");
            ImGui::BulletText("Ctrl+D duplica alla testina, Canc elimina, A tutto, Alt+A niente");
            ImGui::BulletText("R auto-key, O onion skin, tasto destro sul Dope Sheet = menu chiavi");
            ImGui::BulletText("Gizmo: anelli = ruota, frecce = sposta (Shift = a scatti). Clic su una part del rig = il suo osso");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (!g_as.rigged) {
        ImGui::TextWrapped("Per animare il personaggio:");
        ImGui::BulletText("1. Premi \"Inserisci Rig R6\": compare davanti a te un manichino con le misure del player, gia' riggato.");
        ImGui::BulletText("2. Clicca un pallino dello scheletro (es. la spalla) e trascina per ruotare.");
        ImGui::BulletText("3. Premi I per salvare la posa al frame attuale, sposta la linea rossa, rifai la posa, I.");
        ImGui::BulletText("4. Play per vedere l'animazione. File > Salva per tenerla.");
        ImGui::Spacing();
        if (ImGui::Button("Inserisci Rig R6", {170, 38})) asInsertRig(false);
        ImGui::SameLine();
        if (ImGui::Button("Inserisci Rig R15", {170, 38})) asInsertRig(true);
        ImGui::Spacing();
        ImGui::TextDisabled("Hai gia' un personaggio fatto di Part (Head, Torso, Left Arm...)? Selezionalo e:");
        if (ImGui::Button("Auto Rig sulla selezione", {170, 30})) asAutoRig();
        ImGui::SameLine();
        if (ImGui::Button("Rig vuoto", {170, 30})) asEmptyRig();
        if (!g_as.status.empty()) ImGui::TextColored({1.f, 0.75f, 0.35f, 1.f}, "%s", g_as.status.c_str());
        ImGui::End();
        return;
    }

    // Modalita' + Action
    const an::Mode modes[3] = {an::Mode::Edit, an::Mode::Pose, an::Mode::Animate};
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine();
        const bool on = g_as.mode == modes[i];
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.44f, 0.86f, 1.f});
        if (ImGui::Button(an::modeName(modes[i]))) { g_as.mode = modes[i]; g_as.playing = false; g_as.poseDirty = false; }
        if (on) ImGui::PopStyleColor();
    }
    // Gizmo dell'osso nel viewport (come Ruota / Sposta dell'Animation Editor di Roblox).
    ImGui::SameLine(0.f, 14.f);
    {
        static const char* kGz[3] = {"Ruota", "Sposta", "Nessun gizmo"};
        for (int i = 0; i < 3; ++i) {
            if (i) ImGui::SameLine();
            const bool on = g_as.gizmo == i;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.5f, 0.12f, 1.f});
            if (ImGui::Button(kGz[i])) g_as.gizmo = i;
            if (on) ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Mondo", &g_as.globalRotate);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Assi del gizmo: del mondo (acceso) o dell'osso (spento)");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("##act", asAction().name.c_str())) {
        for (int i = 0; i < (int)g_as.actions.size(); ++i)
            if (ImGui::Selectable(g_as.actions[(size_t)i].name.c_str(), i == g_as.activeAction)) g_as.activeAction = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Action")) {
        an::Action a;
        a.name = "Action " + std::to_string(g_as.actions.size() + 1);
        g_as.actions.push_back(a);
        g_as.activeAction = (int)g_as.actions.size() - 1;
    }
    ImGui::SameLine();
    {
        static char renameBuf[48];
        ImGui::SetNextItemWidth(110);
        std::snprintf(renameBuf, sizeof(renameBuf), "%s", asAction().name.c_str());
        if (ImGui::InputText("##arn", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue) && renameBuf[0])
            asAction().name = renameBuf;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d chiavi", asAction().keyCount());

    // Sinistra: ossa. Destra: timeline e schede.
    ImGui::BeginChild("##asleft", {290, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    asBonePanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##asright", {0, 0});
    asTransportBar();
    if (ImGui::BeginTabBar("##astabs")) {
        if (ImGui::BeginTabItem("Dope Sheet")) { asDopeSheet(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Curve Editor")) { asCurveEditor(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Eventi")) { asEventsTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Pose")) { asPosesTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Vincoli / IK")) { asConstraintsTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("State Machine")) { asStateMachineTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    asPollHotkeys(focused);
    asKeyMenu();
    asTrackUndo();
    if (!g_as.status.empty()) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 24.f);
        ImGui::TextDisabled("%s   |   I chiave  K menu  Spazio play  Canc elimina  Ctrl+D duplica  Ctrl+rotella zoom", g_as.status.c_str());
    }
    ImGui::End();
}
