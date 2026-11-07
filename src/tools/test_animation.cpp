// Test del motore di animazione (engine/anim/): armature, curve, IK, vincoli,
// auto rig, blending, state machine, skinning, salvataggio. Niente grafica.

#include "engine/anim/AnimBlend.h"
#include "engine/anim/AnimCurve.h"
#include "engine/anim/AnimIO.h"
#include "engine/anim/AnimStateMachine.h"
#include "engine/anim/Armature.h"
#include "engine/anim/AutoRig.h"
#include "engine/anim/CharacterAnimator.h"
#include "engine/anim/BoneConstraints.h"
#include "engine/anim/Ik.h"
#include "engine/anim/Skinning.h"

#include <chrono>
#include <cstdio>

using namespace blk::anim;

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("[anim] %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++g_fail;
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
static bool near3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f) { return glm::length(a - b) <= eps; }

static Transform at(const glm::vec3& p) { Transform t; t.pos = p; return t; }

// Braccio: spalla (0,0,0) -> braccio lungo +Y 2 -> avambraccio 2.
static Armature arm2() {
    Armature a;
    const int up = a.addBone("Arm", -1, Transform{}, 2.f);
    a.addBone("Forearm", up, at({0, 2, 0}), 2.f);
    return a;
}

int main() {
    // ---- Armature ----
    {
        Armature a;
        const int root = a.addBone("Root", -1, Transform{}, 1.f);
        const int arm = a.addBone("Arm", root, at({0, 1, 0}), 1.f);
        const int hand = a.addBone("Hand", arm, at({0, 1, 0}), 0.5f);
        check(a.addBone("Arm", root, Transform{}) == 3 && a.bones[3].name == "Arm.001", "nomi unici come Blender (Arm.001)");
        check(!a.renameBone(3, "Hand") && a.renameBone(3, "Finger"), "rinomina rifiuta i doppioni");
        check(!a.setParent(root, hand), "niente cicli: la radice non va sotto la mano");

        std::vector<Transform> g;
        a.restGlobals(g);
        const glm::vec3 handWorld = g[(size_t)hand].pos;
        check(near3(handWorld, {0, 2, 0}), "posizione globale a riposo");
        check(a.setParent(hand, root, true), "riparenta tenendo la posizione nel mondo");
        a.restGlobals(g);
        check(near3(g[(size_t)hand].pos, handWorld), "dopo il cambio di padre la mano e' ferma");

        const int dup = a.duplicateBone(arm, false);
        check(dup >= 0 && a.bones[(size_t)dup].parent == root && a.bones[(size_t)dup].name == "Arm.001", "duplica osso (prende il primo nome libero)");
        a.setParent(hand, arm, true);
        const size_t before = a.bones.size();
        a.restGlobals(g);
        const glm::vec3 hw = g[(size_t)hand].pos;
        check(a.removeBone(arm) && a.bones.size() == before - 1, "elimina osso");
        const int hand2 = a.find("Hand");
        a.restGlobals(g);
        check(hand2 >= 0 && a.bones[(size_t)hand2].parent == root && near3(g[(size_t)hand2].pos, hw),
              "i figli dell'osso eliminato passano al nonno senza muoversi");
    }

    // ---- Pose ----
    {
        Armature a = arm2();
        Pose p;
        p.reset(a.bones.size());
        p.local[0].rot = glm::angleAxis(glm::radians(-90.f), glm::vec3(0, 0, 1)); // braccio verso +X
        std::vector<Transform> g;
        a.poseGlobals(p, g);
        check(near3(g[1].pos, {2, 0, 0}) && near3(a.tail(1, g[1]), {4, 0, 0}), "posa: ruotare la spalla muove tutto il braccio");
    }

    // ---- Curve ----
    {
        FCurve c;
        c.channel = Channel::LocY;
        c.insert(0, 0, Interp::Linear);
        c.insert(10, 10, Interp::Linear);
        check(near(c.eval(5), 5.f), "lineare a meta'");
        c.keys[0].interp = Interp::Constant;
        check(near(c.eval(9.9f), 0.f), "constant: scalino fino alla chiave dopo");

        FCurve b;
        b.channel = Channel::LocY;
        b.insert(0, 0);
        b.insert(10, 10);
        b.insert(20, 0);
        float maxV = -1e9f;
        for (float f = 0; f <= 20; f += 0.25f) maxV = std::max(maxV, b.eval(f));
        check(near(b.eval(0), 0) && near(b.eval(10), 10) && maxV <= 10.0001f, "bezier passa per le chiavi e non sfora il massimo");
        check(b.eval(5) > 2.f && b.eval(5) < 8.f, "bezier tra le chiavi e' morbida");

        FCurve r;
        r.channel = Channel::RotY;
        r.insert(0, 170.f);
        r.insert(10, -170.f); // srotolata a 190
        check(near(r.keys[1].value, 190.f), "rotazione srotolata: 170 -> -170 diventa 190 (20 gradi, non 340)");

        Armature a = arm2();
        Action act;
        Transform pose;
        pose.pos = {0.5f, 0, 0};
        pose.rot = glm::quat(glm::radians(glm::vec3(10, 20, 30)));
        act.insertKeys("Forearm", pose, 12.f);
        const Pose s = act.sample(a, 12.f);
        check(near3(s.get(1).pos, pose.pos) && std::fabs(glm::dot(s.get(1).rot, pose.rot)) > 0.9999f,
              "tasto I: chiavi della posa e lettura identica");
        check(act.keyCount() == 9, "9 canali per osso");

        // Dope sheet
        act.insertKeys("Forearm", Transform{}, 24.f);
        act.selectAll(false);
        for (auto& cv : act.curves) cv.keys[1].selected = true;
        act.moveSelected(6.f);
        check(act.curves[0].keys[1].frame == 30.f, "sposta chiavi selezionate");
        act.scaleSelected(0.5f, 12.f);
        check(near(act.curves[0].keys[1].frame, 21.f), "scala nel tempo attorno al pivot");
        check(act.duplicateSelected(10.f) == 9 && act.keyCount() == 27, "duplica chiavi");
        act.selectFrames(30.f, 32.f, {0, 1, 2, 3, 4, 5, 6, 7, 8}, false);
        check(act.deleteSelected() == 9 && act.keyCount() == 18, "box select + elimina");
        check(act.keyedFrames().size() == 2, "rombi del dope sheet: 2 frame con chiavi");
    }

    // ---- Easing stile Roblox, ease in-out, inverti, copia/incolla ----
    {
        FCurve e;
        e.channel = Channel::LocX;
        e.insert(0, 0, Interp::Bounce);
        e.insert(10, 10);
        check(near(e.eval(0), 0) && near(e.eval(10), 10), "bounce passa per le chiavi");
        float minB = 1e9f, maxB = -1e9f;
        for (float f = 0; f <= 10.f; f += 0.1f) { minB = std::min(minB, e.eval(f)); maxB = std::max(maxB, e.eval(f)); }
        check(minB >= -1e-3f && maxB <= 10.001f, "bounce resta dentro (rimbalza sotto l'arrivo)");
        e.keys[0].interp = Interp::Elastic;
        float maxE = -1e9f;
        for (float f = 0; f <= 10.f; f += 0.1f) maxE = std::max(maxE, e.eval(f));
        check(maxE > 10.1f, "elastic sfora l'arrivo e torna");
        e.keys[0].interp = Interp::Back;
        float maxK = -1e9f;
        for (float f = 0; f <= 10.f; f += 0.1f) maxK = std::max(maxK, e.eval(f));
        check(maxK > 10.05f && near(e.eval(10), 10), "back sfora un poco e arriva");

        Action a;
        FCurve& c = a.curve("Arm", Channel::LocY);
        c.insert(0, 0);
        c.insert(10, 4);
        c.insert(20, 0);
        a.selectAll(true);
        a.setEaseSelected(false);
        check(near(a.curves[0].keys[1].hIn.y, 4) && near(a.curves[0].keys[1].hOut.y, 4), "ease in-out: maniglie piatte");
        check(a.curves[0].eval(1) < 0.4f * 1.f && a.curves[0].eval(1) >= 0.f, "ease in-out: parte piano");
        a.selectAll(false);
        a.curves[0].keys[0].selected = a.curves[0].keys[1].selected = true;   // 0 e 10
        a.reverseSelected();
        check(near(a.curves[0].eval(0), 4) && near(a.curves[0].eval(10), 0), "inverti: 0->4 diventa 4->0");

        a.selectAll(false);
        a.selectAtFrame(20.f, false);
        const auto clip = a.copySelected();
        check(clip.size() == 1 && near(clip[0].key.frame, 0), "copia: frame relativo alla prima chiave");
        check(a.paste(clip, 40.f) == 1 && a.curves[0].findKey(40.f) >= 0 && a.curves[0].keys.size() == 4, "incolla alla testina");
        a.curve("Arm", Channel::RotX).insert(5, 30, Interp::Elastic);
        Armature arm;
        arm.addBone("Arm", -1, Transform{}, 1.f);
        std::string err;
        std::vector<Action> back;
        check(fromJson(toJson(arm, {a}), arm, back, nullptr, err) && back.size() == 1
                  && back[0].curves[(size_t)back[0].findCurve("Arm", Channel::RotX)].keys[0].interp == Interp::Elastic,
              "elastic salvato e riletto dal .blkanim");
    }

    // ---- IK ----
    {
        Armature a = arm2();
        a.addBone("Hand", 1, at({0, 2, 0}), 0.5f);
        Pose p;
        p.reset(a.bones.size());
        std::vector<Transform> g;
        a.poseGlobals(p, g);
        const glm::vec3 target(2.f, 2.f, 0.f);
        const glm::vec3 pole(0.f, 0.f, 5.f);
        check(ik::twoBone(a, p, g, 0, 1, 2, target, &pole), "IK 2 ossa risolve");
        check(near3(g[2].pos, target, 2e-3f), "la mano arriva sul bersaglio");
        check(near(glm::length(g[1].pos - g[0].pos), 2.f) && near(glm::length(g[2].pos - g[1].pos), 2.f), "le ossa non si allungano");
        check(g[1].pos.z > 0.5f, "il gomito si piega verso il pole target");

        Pose p2;
        p2.reset(a.bones.size());
        a.poseGlobals(p2, g);
        ik::twoBone(a, p2, g, 0, 1, 2, {10, 0, 0});
        check(near3(g[2].pos, {4, 0, 0}, 2e-3f), "bersaglio lontano: il braccio si stende verso di lui");

        Armature chain;
        int prev = -1;
        for (int i = 0; i < 4; ++i) prev = chain.addBone("Seg", prev, i == 0 ? Transform{} : at({0, 1, 0}), 1.f);
        Pose pc;
        pc.reset(chain.bones.size());
        std::vector<Transform> gc;
        chain.poseGlobals(pc, gc);
        const glm::vec3 t2(1.5f, 2.5f, 0.5f);
        ik::fabrik(chain, pc, gc, 3, 4, t2);
        check(near3(chain.tail(3, gc[3]), t2, 1e-2f), "FABRIK 4 ossa: la punta arriva al bersaglio");

        Pose pl;
        pl.reset(a.bones.size());
        a.poseGlobals(pl, g);
        ik::lookAt(a, pl, g, 0, {5, 0, 0});
        check(near3(g[0].rot * glm::vec3(0, 1, 0), {1, 0, 0}), "LookAt: l'asse +Y guarda il bersaglio");
        Pose ph;
        ph.reset(a.bones.size());
        a.poseGlobals(ph, g);
        ik::lookAt(a, ph, g, 0, {5, 0, 0}, {0, 1, 0}, 1.f, 30.f);
        const float ang = glm::degrees(std::acos(glm::dot(g[0].rot * glm::vec3(0, 1, 0), glm::vec3(0, 1, 0))));
        check(near(ang, 30.f, 0.5f), "Head Tracking con limite: si gira al massimo di 30 gradi");
    }

    // ---- Vincoli ----
    {
        Armature a = arm2();
        a.addBone("Target", -1, at({3, 1, 0}), 0.5f);
        Pose p;
        p.reset(a.bones.size());
        std::vector<Transform> g;
        a.poseGlobals(p, g);
        BoneConstraint cl;
        cl.type = ConstraintType::CopyLocation;
        cl.owner = "Arm";
        cl.target = "Target";
        cl.axis[1] = false;
        applyConstraints(a, p, g, {cl});
        check(near3(g[0].pos, {3, 0, 0}), "Copy Location solo X e Z");

        Pose p2;
        p2.reset(a.bones.size());
        p2.local[1].rot = glm::quat(glm::radians(glm::vec3(0, 0, 120)));
        a.poseGlobals(p2, g);
        BoneConstraint lr;
        lr.type = ConstraintType::LimitRotation;
        lr.owner = "Forearm";
        lr.minV = {-180, -180, -45};
        lr.maxV = {180, 180, 45};
        applyConstraints(a, p2, g, {lr});
        check(near(glm::degrees(glm::eulerAngles(p2.local[1].rot)).z, 45.f, 0.05f), "Limit Rotation: 120 gradi diventano 45");

        Pose p3;
        p3.reset(a.bones.size());
        a.poseGlobals(p3, g);
        BoneConstraint ikc;
        ikc.type = ConstraintType::Ik;
        ikc.owner = "Forearm";
        ikc.targetPoint = {2.5f, 2.5f, 0.f};
        ikc.chainLength = 2;
        applyConstraints(a, p3, g, {ikc});
        check(near3(a.tail(1, g[1]), ikc.targetPoint, 2e-3f), "vincolo IK: la coda dell'avambraccio sul bersaglio");

        Pose p4;
        p4.reset(a.bones.size());
        a.poseGlobals(p4, g);
        BoneConstraint co;
        co.type = ConstraintType::ChildOf;
        co.owner = "Arm";
        co.target = "Target";
        childOfSetInverse(a, g, co);
        p4.local[2].pos = {0, 0, 2}; // il bersaglio si sposta
        a.poseGlobals(p4, g);
        applyConstraints(a, p4, g, {co});
        check(near3(g[0].pos, {0, 0, 2}), "Child Of: l'osso segue il bersaglio spostato");

        BoneConstraint half = cl;
        half.influence = 0.5f;
        Pose p5;
        p5.reset(a.bones.size());
        a.poseGlobals(p5, g);
        applyConstraints(a, p5, g, {half});
        check(near3(g[0].pos, {1.5f, 0, 0}), "influenza 0.5 = a meta' strada");
    }

    // ---- Auto Rig ----
    {
        std::vector<PartBox> r6 = {
            {"Torso", {0, 3, 0}, {2, 2, 1}, {}},  {"Head", {0, 4.5f, 0}, {2, 1, 1}, {}},
            {"Left Arm", {-1.5f, 3, 0}, {1, 2, 1}, {}}, {"Right Arm", {1.5f, 3, 0}, {1, 2, 1}, {}},
            {"Left Leg", {-0.5f, 1, 0}, {1, 2, 1}, {}}, {"Right Leg", {0.5f, 1, 0}, {1, 2, 1}, {}},
        };
        AutoRigResult rr = autoRig(r6);
        const Armature& a = rr.armature;
        check(rr.kind == RigKind::R6 && a.find("Neck") >= 0 && a.find("Left Shoulder") >= 0 && a.find("Right Hip") >= 0
              && rr.bindings.size() == 6, "Auto Rig R6: Neck, Shoulder, Hip e 6 parti legate");
        Pose p;
        p.reset(a.bones.size());
        std::vector<Transform> w = partWorlds(a, p, rr.bindings);
        bool restOk = true;
        for (size_t i = 0; i < rr.bindings.size(); ++i)
            for (const auto& pb : r6)
                if (pb.name == rr.bindings[i].part && !near3(w[i].pos, pb.pos)) restOk = false;
        check(restOk, "a riposo le parti restano dove erano");
        std::vector<Transform> g;
        a.restGlobals(g);
        check(near3(g[(size_t)a.find("Left Shoulder")].pos, {-1.5f, 4, 0}), "la spalla nasce in cima al braccio");
        const int ls = a.find("Left Shoulder");
        p.local[(size_t)ls].rot = glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0));
        w = partWorlds(a, p, rr.bindings);
        glm::vec3 armPos{0}, legPos{0};
        for (size_t i = 0; i < rr.bindings.size(); ++i) {
            if (rr.bindings[i].part == "Left Arm") armPos = w[i].pos;
            if (rr.bindings[i].part == "Left Leg") legPos = w[i].pos;
        }
        check(near(armPos.y, 4.f, 1e-2f) && std::fabs(armPos.z) > 0.9f && near3(legPos, {-0.5f, 1, 0}),
              "ruotare la spalla alza il braccio e lascia ferma la gamba");

        std::vector<PartBox> r15 = {
            {"LowerTorso", {0, 2.6f, 0}, {2, 0.4f, 1}, {}}, {"UpperTorso", {0, 3.4f, 0}, {2, 1.2f, 1}, {}},
            {"Head", {0, 4.5f, 0}, {1.2f, 1.2f, 1.2f}, {}},
            {"LeftUpperArm", {-1.5f, 3.6f, 0}, {1, 0.8f, 1}, {}}, {"LeftLowerArm", {-1.5f, 2.9f, 0}, {1, 0.6f, 1}, {}},
            {"LeftHand", {-1.5f, 2.4f, 0}, {1, 0.4f, 1}, {}},
            {"LeftUpperLeg", {-0.5f, 2.0f, 0}, {1, 0.8f, 1}, {}}, {"LeftLowerLeg", {-0.5f, 1.2f, 0}, {1, 0.8f, 1}, {}},
            {"LeftFoot", {-0.5f, 0.6f, 0}, {1, 0.4f, 1}, {}},
        };
        AutoRigResult r2 = autoRig(r15);
        check(r2.kind == RigKind::R15 && r2.armature.find("LeftElbow") >= 0 && r2.armature.find("LeftKnee") >= 0
              && r2.armature.find("LeftAnkle") >= 0 && r2.armature.find("RightShoulder") < 0,
              "Auto Rig R15: gomiti, ginocchia, caviglie (e niente lato destro se mancano le parti)");
        const int elbow = r2.armature.find("LeftElbow");
        check(r2.armature.bones[(size_t)elbow].parent == r2.armature.find("LeftShoulder"), "R15: gomito figlio della spalla");
        check(autoRig({{"Cubo", {}, {1, 1, 1}, {}}}).kind == RigKind::None, "parti sconosciute: nessun rig (con motivo)");
    }

    // ---- Blending ----
    {
        Armature a = arm2();
        Action idle, run;
        Transform up;
        up.rot = glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0));
        idle.insertKeys("Arm", Transform{}, 0);
        run.insertKeys("Arm", up, 0);
        const Pose half = blend(idle.sample(a, 0), run.sample(a, 0), 0.5f);
        check(near(glm::degrees(glm::angle(half.get(0).rot)), 45.f, 0.1f), "blend 50%: 45 gradi");

        Pose base;
        base.reset(a.bones.size());
        base.local[0].pos = {1, 0, 0};
        Pose add;
        add.reset(a.bones.size());
        add.local[0].pos = {0, 2, 0};
        check(near3(addAdditive(base, add, 1.f).get(0).pos, {1, 2, 0}), "additiva: si somma sopra");

        std::vector<Layer> layers(2);
        layers[0].action = &idle;
        layers[1].action = &run;
        layers[1].mask = boneMask(a, {"Forearm"});
        const Pose lp = evaluateLayers(a, layers);
        check(lp.get(0).isIdentity() , "livello con maschera: il braccio non e' toccato dal livello Forearm");

        CrossFade cf;
        Pose from;
        from.reset(a.bones.size());
        cf.start(from, 0.2f);
        Pose to = run.sample(a, 0);
        Pose mid = cf.apply(to, 0.1f);
        const float midAng = glm::degrees(glm::angle(mid.get(0).rot));
        Pose end = cf.apply(to, 0.2f);
        check(midAng > 30.f && midAng < 60.f && near(glm::degrees(glm::angle(end.get(0).rot)), 90.f, 0.1f) && !cf.active,
              "cross fade: a meta' a meta' strada, poi arriva");

        BlendTree1D tree;
        tree.entries = {{0.f, &idle}, {10.f, &run}};
        const Pose tp = tree.sample(a, 5.f, 0.016f);
        check(near(glm::degrees(glm::angle(tp.get(0).rot)), 45.f, 0.1f), "blend tree 1D: velocita' 5 tra idle (0) e run (10)");
    }

    // ---- State machine ----
    {
        Armature a = arm2();
        Action idle, walk, run, jump, fall, attack;
        for (Action* x : {&idle, &walk, &run, &jump, &fall, &attack}) { x->insertKeys("Arm", Transform{}, 0); x->frameEnd = 30; }
        walk.events.push_back({15.f, "Footstep", "left"});
        attack.loop = false;
        AnimStateMachine m = humanoidStateMachine(&idle, &walk, &run, &jump, &fall, nullptr, nullptr, &attack);
        AnimParams pr;
        pr.flags["Grounded"] = true;
        std::vector<AnimEvent> ev;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Idle", "parte da Idle");
        pr.numbers["Speed"] = 6.f;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Walk", "Speed 6: Walk");
        ev.clear();
        for (int i = 0; i < 62; ++i) m.update(a, 1.f / 30.f, pr, &ev); // ~2 giri da 1 s
        int steps = 0;
        for (const auto& e : ev) if (e.name == "Footstep") ++steps;
        check(steps == 2, "evento Footstep una volta per giro di camminata");
        pr.numbers["Speed"] = 16.f;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Run", "Speed 16: Run");
        pr.flags["Grounded"] = false;
        pr.numbers["VerticalSpeed"] = 20.f;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Jump", "staccati da terra salendo: Jump");
        pr.numbers["VerticalSpeed"] = -5.f;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Fall", "scendendo: Fall");
        pr.flags["Grounded"] = true;
        pr.numbers["Speed"] = 0.f;
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Idle", "a terra: Idle");
        pr.setTrigger("Attack");
        m.update(a, 0.016f, pr, &ev);
        check(std::string(m.currentName()) == "Attack" && !pr.trigger("Attack"), "trigger Attack: entra e il trigger si spegne");
        for (int i = 0; i < 40; ++i) m.update(a, 1.f / 30.f, pr, &ev);
        check(std::string(m.currentName()) == "Idle", "a fine attacco torna a Idle (exit time)");
    }

    // ---- Skinning ----
    {
        Armature a = arm2();
        std::vector<glm::vec3> verts = {{0.1f, 0.5f, 0}, {0.1f, 3.5f, 0}, {0.1f, 2.0f, 0}};
        std::vector<VertexWeights> w = automaticWeights(a, verts, 4);
        check(w[0].of(0) > 0.95f && w[1].of(1) > 0.95f && near(w[2].of(0), 0.5f, 0.1f), "pesi automatici: ogni vertice segue l'osso vicino");
        std::vector<VertexWeights> w1 = automaticWeights(a, verts, 1);
        check(w1[2].count == 1 && near(w1[2].weight[0], 1.f), "1 influenza per vertice");
        Brush br;
        br.mode = BrushMode::Add;
        br.radius = 1.f;
        br.strength = 1.f;
        paint(w, verts, 1, verts[0], br);
        check(w[0].of(1) > 0.9f && near(w[0].of(0) + w[0].of(1), 1.f), "pennello Add: l'osso prende il vertice, il totale resta 1");
        br.mode = BrushMode::Subtract;
        paint(w, verts, 1, verts[0], br);
        check(w[0].of(1) < 0.1f, "pennello Subtract");
        br.mode = BrushMode::Smooth;
        br.radius = 2.f;
        br.strength = 1.f;
        const int touched = paint(w, verts, 0, verts[2], br);
        check(touched >= 2, "pennello Smooth tocca i vicini");
        check(weightColor(1.f) == glm::vec3(1, 0, 0) && weightColor(0.f) == glm::vec3(0, 0, 1), "colori: rosso alto, blu basso");

        Pose p;
        p.reset(a.bones.size());
        p.local[1].rot = glm::angleAxis(glm::radians(-90.f), glm::vec3(0, 0, 1));
        std::vector<VertexWeights> wk = automaticWeights(a, verts, 4);
        const auto moved = skinPositions(verts, wk, skinMatrices(a, p));
        check(near3(moved[0], verts[0], 1e-2f) && near(moved[1].x, 1.5f, 0.05f) && near(moved[1].y, 2.0f, 0.15f),
              "skinning: il vertice dell'avambraccio ruota col gomito, quello del braccio no");
        int bb[4];
        float ww[4];
        packTop4(w[2], bb, ww);
        check(near(ww[0] + ww[1] + ww[2] + ww[3], 1.f), "4 slot per la GPU, somma 1");
    }

    // ---- Salvataggio, specchio, libreria pose ----
    {
        std::vector<PartBox> r6 = {
            {"Torso", {0, 3, 0}, {2, 2, 1}, {}}, {"Head", {0, 4.5f, 0}, {2, 1, 1}, {}},
            {"Left Arm", {-1.5f, 3, 0}, {1, 2, 1}, {}}, {"Right Arm", {1.5f, 3, 0}, {1, 2, 1}, {}},
        };
        Armature a = autoRig(r6).armature;
        Action wave;
        wave.name = "Saluto";
        Transform up;
        up.rot = glm::quat(glm::radians(glm::vec3(0, 30, 150)));
        wave.insertKeys("Right Shoulder", Transform{}, 0);
        wave.insertKeys("Right Shoulder", up, 15);
        wave.events.push_back({15.f, "PlaySound", "ciao"});
        wave.markers.push_back({0.f, "Inizio"});
        Pose cur = wave.sample(a, 15);
        std::vector<NamedPose> lib = {capturePose(a, cur, "Mano alzata")};

        const std::string text = toJson(a, {wave}, lib);
        Armature a2;
        std::vector<Action> acts;
        std::vector<NamedPose> lib2;
        std::string err;
        check(fromJson(text, a2, acts, &lib2, err) && a2.bones.size() == a.bones.size() && acts.size() == 1
              && acts[0].events.size() == 1 && acts[0].markers.size() == 1 && lib2.size() == 1, "JSON: salva e rilegge tutto");
        const Pose again = acts[0].sample(a2, 7.5f), orig = wave.sample(a, 7.5f);
        const int rs = a.find("Right Shoulder");
        check(std::fabs(glm::dot(again.get(rs).rot, orig.get(rs).rot)) > 0.9999f, "dopo il salvataggio la curva da' gli stessi valori");
        check(!fromJson("{\"format\":\"altro\"}", a2, acts, nullptr, err) && !fromJson("{rotto", a2, acts, nullptr, err),
              "file sbagliati rifiutati");
        const std::string cyc = "{\"format\":\"blocky-anim\",\"version\":1,\"armature\":{\"name\":\"x\",\"bones\":"
                                "[{\"name\":\"a\",\"parent\":1},{\"name\":\"b\",\"parent\":0}]},\"actions\":[]}";
        check(!fromJson(cyc, a2, acts, nullptr, err), "ossa in cerchio rifiutate");

        check(mirrorName("Left Shoulder") == "Right Shoulder" && mirrorName("Arm.L") == "Arm.R" && mirrorName("Neck") == "Neck",
              "nomi specchiati");
        const Pose mp = mirrorPose(a, cur);
        const int ls = a.find("Left Shoulder");
        const glm::vec3 eR = glm::degrees(glm::eulerAngles(cur.get(rs).rot));
        const glm::vec3 eL = glm::degrees(glm::eulerAngles(mp.get(ls).rot));
        check(near(eL.x, eR.x, 0.1f) && near(eL.y, -eR.y, 0.1f) && near(eL.z, -eR.z, 0.1f), "Mirror Pose: la sinistra fa la destra allo specchio");
        const Action mw = mirrorAction(wave);
        check(mw.findCurve("Left Shoulder", Channel::RotZ) >= 0 && mw.findCurve("Right Shoulder", Channel::RotZ) < 0,
              "Mirror Animation: le curve passano all'altro lato");
        Pose blank;
        blank.reset(a.bones.size());
        applyNamedPose(a, blank, lib[0]);
        check(std::fabs(glm::dot(blank.get(rs).rot, cur.get(rs).rot)) > 0.9999f, "libreria pose: applica la posa salvata");
    }

    // ---- Animator del personaggio (AnimationTrack) ----
    {
        // Le parti del player (Humanoid::createR6Blocky) nello spazio del personaggio.
        const std::vector<PartBox> body = {
            {"Torso", {0, 3.0f, 0}, {2, 2, 1}, {}}, {"Head", {0, 4.7f, 0}, {1.25f, 1.25f, 1.25f}, {}},
            {"Left Arm", {-1.5f, 3.0f, 0}, {1, 2, 1}, {}}, {"Right Arm", {1.5f, 3.0f, 0}, {1, 2, 1}, {}},
            {"Left Leg", {-0.5f, 1.0f, 0}, {1, 2, 1}, {}}, {"Right Leg", {0.5f, 1.0f, 0}, {1, 2, 1}, {}},
        };
        CharacterAnimator ca;
        check(ca.setupFromParts(body) && ca.ready(), "animator: rig R6 del player");

        Action walk, wave;
        Transform legFwd, legBack, armUp;
        legFwd.rot = glm::angleAxis(glm::radians(30.f), glm::vec3(1, 0, 0));
        legBack.rot = glm::angleAxis(glm::radians(-30.f), glm::vec3(1, 0, 0));
        armUp.rot = glm::angleAxis(glm::radians(150.f), glm::vec3(0, 0, 1));
        walk.insertKeys("Left Hip", legFwd, 0); walk.insertKeys("Left Hip", legBack, 15); walk.insertKeys("Left Hip", legFwd, 30);
        walk.insertKeys("Right Shoulder", legBack, 0);
        walk.frameEnd = 30; walk.priority = 2; // Movement
        wave.insertKeys("Right Shoulder", armUp, 0); wave.insertKeys("Right Shoulder", armUp, 30);
        wave.frameEnd = 30; wave.loop = false; wave.priority = 3; // Action
        wave.markers.push_back({15.f, "Mano"});
        const int tw = ca.load(walk), tv = ca.load(wave);
        std::vector<TrackEvent> ev;
        ca.play(tw, 0.f);
        ca.play(tv, 0.2f);
        ca.update(0.1f, ev);
        check(ca.track(tv)->weight > 0.4f && ca.track(tv)->weight < 0.6f, "Play con fade 0.2 s: a 0.1 s peso a meta'");
        for (int i = 0; i < 9; ++i) ca.update(0.1f, ev); // 1 s: wave (non loop) finisce
        int markers = 0, stopped = 0, ended = 0, loops = 0;
        for (const auto& e : ev) {
            if (e.type == TrackEventType::MarkerReached && e.track == tv && e.name == "Mano") ++markers;
            if (e.type == TrackEventType::Stopped && e.track == tv) ++stopped;
            if (e.type == TrackEventType::Ended && e.track == tv) ++ended;
            if (e.type == TrackEventType::DidLoop && e.track == tw) ++loops;
        }
        check(markers == 1, "GetMarkerReachedSignal: il marker scatta una volta");
        check(loops == 1, "DidLoop della camminata dopo 1 s");
        ca.update(0.2f, ev);
        for (const auto& e : ev) {
            if (e.type == TrackEventType::Stopped && e.track == tv) ++stopped;
            if (e.type == TrackEventType::Ended && e.track == tv) ++ended;
        }
        check(stopped >= 1 && ended >= 1 && !ca.track(tv)->playing, "traccia non in loop: Stopped, poi Ended");

        // Priorita': Action sulla spalla destra, Movement sull'anca resta.
        ca.play(tv, 0.f);
        ca.track(tv)->time = 0.5f;
        std::vector<float> bw;
        const Pose p = ca.pose(&bw);
        const int rs = ca.arm.find("Right Shoulder"), lh = ca.arm.find("Left Hip"), nk = ca.arm.find("Neck");
        check(std::fabs(glm::dot(p.get(rs).rot, armUp.rot)) > 0.999f, "priorita': il saluto (Action) vince sulla spalla");
        check(!p.get(lh).isIdentity() && bw[(size_t)lh] > 0.99f, "le gambe restano alla camminata (Movement)");
        check(bw[(size_t)nk] == 0.f, "il collo non e' animato: resta al motore");

        // Conversione per BlockPlayer: ricostruita con la stessa formula deve dare la parte giusta.
        const std::vector<Transform> parts = ca.partTransforms(p);
        bool okConv = true;
        for (size_t i = 0; i < ca.binds.size(); ++i) {
            const glm::vec3 c = ca.partRest()[i].pos;
            const glm::vec3 pivot = (ca.binds[i].part == "Torso" || ca.binds[i].part == "Head") ? glm::vec3(0) : glm::vec3(0, 1, 0);
            glm::vec3 pos, eul;
            toBlockPartPose(parts[i], c, pivot, pos, eul);
            glm::mat4 m = glm::translate(glm::mat4(1.f), c + pivot + pos);
            m = glm::rotate(m, glm::radians(eul.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(eul.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(eul.z), glm::vec3(0, 0, 1));
            m = glm::translate(m, -pivot);
            const glm::vec3 center = glm::vec3(m[3]);
            const glm::quat q = glm::quat_cast(glm::mat3(m));
            if (!near3(center, parts[i].pos, 1e-3f) || std::fabs(glm::dot(q, parts[i].rot)) < 0.9999f) okConv = false;
        }
        check(okConv, "posa -> parti del corpo di BlockPlayer: centro e rotazione esatti");
        ca.stop(tw, 0.f);
        ca.stop(tv, 0.f);
        ev.clear();
        ca.update(0.016f, ev);
        check(ca.playingTracks().empty(), "Stop con fade 0: subito ferme");
    }

    // ---- Prestazioni: 120 ossa, oltre 1000 chiavi ----
    {
        Armature a;
        int prev = -1;
        for (int i = 0; i < 120; ++i) prev = a.addBone("B", i % 10 == 0 ? -1 : prev, at({0, 0.5f, 0}), 0.5f);
        Action act;
        for (int i = 0; i < 120; ++i)
            for (int f = 0; f <= 60; f += 6) {
                Transform t;
                t.rot = glm::quat(glm::radians(glm::vec3((float)f, (float)i, 0.f)));
                act.insertKeys(a.bones[(size_t)i].name, t, (float)f, Action::KeyRot);
            }
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<Transform> g;
        for (int f = 0; f < 600; ++f) {
            const Pose p = act.sample(a, (float)(f % 60));
            a.poseGlobals(p, g);
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 600.0;
        std::printf("[anim] 120 ossa, %d chiavi: %.3f ms per frame (campiona + globali)\n", act.keyCount(), ms);
        check(act.keyCount() > 1000 && ms < 4.0, "oltre 1000 chiavi e 120 ossa in tempo reale (< 4 ms a frame)");
    }

    std::printf(g_fail ? "[anim] %d FALLITI\n" : "[anim] TUTTO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
