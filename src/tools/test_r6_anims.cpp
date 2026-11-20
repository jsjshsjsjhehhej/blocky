// Animazioni R6 di base (assets/animations/blocky_r6.blkanim + DefaultR6Animate):
// le pose arrivano giuste sul rig del motore (avanti e' avanti, il braccio
// sinistro e' il sinistro), le curve non saltano, e "Animate" sceglie la
// traccia giusta per ogni stato del personaggio.

#include "engine/anim/DefaultR6Animate.h"

#include <cstdio>

using namespace blk::anim;

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

// Direzione "giu' lungo l'arto" della parte nella posa (spazio personaggio: avanti +Z, su +Y).
static glm::vec3 limbDir(const CharacterAnimator& a, const std::vector<Transform>& parts, const char* part) {
    for (size_t i = 0; i < a.binds.size(); ++i)
        if (a.binds[i].part == part) return parts[i].rot * glm::vec3(0.f, -1.f, 0.f);
    return glm::vec3(0.f);
}

// Angolo "in avanti" dell'arto in gradi (0 = giu', 90 = avanti orizzontale, 180 = in alto).
static float fwdDeg(const glm::vec3& d) { return glm::degrees(std::atan2(d.z, -d.y)); }

struct Sampler {
    CharacterAnimator a;
    std::map<std::string, const Action*> byName;
    std::vector<Transform> at(const std::string& name, float frame) {
        const Action* act = byName[name];
        const Pose p = act->sample(a.arm, frame);
        return a.partTransforms(p);
    }
};

int main() {
    std::printf("=== Animazioni R6 di base ===\n");
    std::string err;
    const char* paths[] = {"assets/animations/blocky_r6.blkanim", "../assets/animations/blocky_r6.blkanim",
                           "../../assets/animations/blocky_r6.blkanim"};
    bool loaded = false;
    for (const char* p : paths) loaded = loaded || loadR6DefaultActions(p, err);
    check(loaded, "il file delle animazioni si carica");
    if (!loaded) { std::printf("  (%s)\n", err.c_str()); return 1; }

    Sampler s;
    std::vector<PartBox> body;
    for (const auto& d : Humanoid::createR6Blocky().parts)
        if (d.name != "HumanoidRootPart") body.push_back({d.name, d.offset, d.size, glm::quat(1.f, 0.f, 0.f, 0.f)});
    s.a.setupFromParts(body);
    for (const auto& act : *r6DefaultActions()) s.byName[act.name] = &act;
    const char* names[] = {"Idle", "Idle2", "Walk", "Jump", "Fall", "Land", "Climb", "Sit", "Tool", "Wave", "Dance"};
    bool all = true;
    for (const char* n : names) all = all && s.byName.count(n);
    check(all && !s.byName.count("Run"), "ci sono le 11 animazioni (e non Run)");

    std::printf("\nPose sul rig del motore:\n");
    {
        auto w0 = s.at("Walk", 0.f);
        const float la = fwdDeg(limbDir(s.a, w0, "Left Arm")), ll = fwdDeg(limbDir(s.a, w0, "Left Leg"));
        const float ra = fwdDeg(limbDir(s.a, w0, "Right Arm")), rl = fwdDeg(limbDir(s.a, w0, "Right Leg"));
        std::printf("      camminata f0: braccio sx %.0f, gamba sx %.0f, braccio dx %.0f, gamba dx %.0f\n", la, ll, ra, rl);
        check(la > 45 && ll < -45 && ra < -35 && rl > 35, "camminata: braccio sinistro avanti, gamba sinistra indietro (e l'opposto a destra)");
    }
    {
        auto j = s.at("Jump", 11.f);
        const float la = fwdDeg(limbDir(s.a, j, "Left Arm")), ra = fwdDeg(limbDir(s.a, j, "Right Arm"));
        std::printf("      salto f11: braccia %.0f / %.0f\n", la, ra);
        check(la > 90 && ra > 110, "salto: braccia avanti e in su oltre l'orizzontale");
    }
    {
        auto st = s.at("Sit", 0.f);
        const float ll = fwdDeg(limbDir(s.a, st, "Left Leg")), ra = fwdDeg(limbDir(s.a, st, "Right Arm"));
        float torsoY = 0.f;
        for (size_t i = 0; i < s.a.binds.size(); ++i) if (s.a.binds[i].part == "Torso") torsoY = st[i].pos.y;
        std::printf("      seduto: gamba %.0f, braccio %.0f, torso a y %.2f (in piedi 3.00)\n", ll, ra, torsoY);
        check(std::fabs(ll - 90) < 3 && std::fabs(ra - 90) < 3 && torsoY < 2.2f, "seduto: gambe e braccia a 90 gradi, corpo abbassato");
    }
    {
        auto c = s.at("Climb", 0.f);
        glm::vec3 headUp(0.f);
        for (size_t i = 0; i < s.a.binds.size(); ++i)
            if (s.a.binds[i].part == "Head") headUp = c[i].rot * glm::vec3(0.f, 0.f, 1.f);
        std::printf("      scalata: la faccia guarda in su di %.0f gradi\n", glm::degrees(std::asin(glm::clamp(headUp.y, -1.f, 1.f))));
        check(headUp.y > 0.35f, "scalata: la testa guarda in su");
    }
    {
        const Action* tool = s.byName["Tool"];
        bool onlyRight = !tool->curves.empty();
        for (const auto& c : tool->curves) onlyRight = onlyRight && c.bone == "Right Shoulder";
        auto t = s.at("Tool", 0.f);
        check(onlyRight && std::fabs(fwdDeg(limbDir(s.a, t, "Right Arm")) - 90) < 3, "oggetto in mano: solo il braccio destro, teso a 90 gradi");
    }

    std::printf("\nCurve senza salti (mezzo fotogramma alla volta):\n");
    {
        float worst = 0.f;
        std::string where;
        for (const auto& act : *r6DefaultActions()) {
            for (float f = act.frameStart; f < act.frameEnd; f += 0.5f) {
                auto p0 = s.at(act.name, f), p1 = s.at(act.name, f + 0.5f);
                for (size_t i = 0; i < p0.size(); ++i) {
                    const float d = glm::degrees(2.f * std::acos(glm::clamp(std::fabs(glm::dot(p0[i].rot, p1[i].rot)), 0.f, 1.f)));
                    if (d > worst) { worst = d; where = act.name + " / " + s.a.binds[i].part; }
                }
            }
        }
        std::printf("      passo piu' grande: %.1f gradi (%s)\n", worst, where.c_str());
        check(worst < 25.f, "nessuna parte gira di colpo tra un fotogramma e l'altro");
    }

    std::printf("\nAnimate (stato -> traccia):\n");
    {
        DefaultR6Animate an;
        check(an.setup(), "si prepara sul rig del player");
        R6AnimInput in;
        const float dt = 1.f / 60.f;
        for (int i = 0; i < 30; ++i) an.update(in, dt);
        check(an.state() == "Idle" && an.isPlaying("Idle"), "fermo: Idle");
        in.walking = true;
        for (int i = 0; i < 30; ++i) an.update(in, dt);
        check(an.state() == "Walk" && an.isPlaying("Walk") && !an.isPlaying("Idle"), "cammina: Walk (e Idle si spegne)");
        in.grounded = false;
        in.velocityY = 25.f;
        an.update(in, dt);
        check(an.state() == "Jump", "stacca da terra salendo: Jump");
        in.velocityY = -20.f;
        for (int i = 0; i < 40; ++i) an.update(in, dt);
        check(an.state() == "Fall", "poi scende: Fall");
        in.grounded = true;
        in.walking = false;
        in.velocityY = 0.f;
        for (int i = 0; i < 20; ++i) an.update(in, dt);
        check(an.state() == "Idle", "atterra fermo: Idle");
        in.toolEquipped = true;
        for (int i = 0; i < 20; ++i) an.update(in, dt);
        Humanoid h = Humanoid::createR6Blocky();
        an.applyTo(h);
        float rightArmX = 0.f, leftArmW = 0.f;
        for (const auto& p : h.parts) {
            if (p.name == "Right Arm") rightArmX = p.anim.rot.x;
            if (p.name == "Left Arm") leftArmW = p.animWeight;
        }
        std::printf("      braccio destro con l'oggetto: %.0f gradi (BlockPlayer: -90 = avanti)\n", rightArmX);
        check(an.isPlaying("Tool") && std::fabs(rightArmX + 90.f) < 4.f && leftArmW > 0.99f && h.defaultAnimActive,
              "oggetto in mano: braccio destro avanti, il resto resta animato");
        in.toolEquipped = false;
        in.emote = "dance";
        for (int i = 0; i < 20; ++i) an.update(in, dt);
        check(an.isPlaying("Dance"), "emote dance");
        in.walking = true;
        for (int i = 0; i < 20; ++i) an.update(in, dt);
        check(!an.isPlaying("Dance") && an.state() == "Walk", "muovendosi la danza si ferma");
        in.walking = false;
        in.emote = "";
        in.seated = true;
        for (int i = 0; i < 20; ++i) an.update(in, dt);
        check(an.state() == "Sit", "seduto (auto): Sit");
        DefaultR6Animate::clearFrom(h);
        bool cleared = !h.defaultAnimActive;
        for (const auto& p : h.parts) cleared = cleared && p.animWeight == 0.f && !p.animFromDefault;
        check(cleared, "clearFrom toglie la posa delle animazioni di base");
    }

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
