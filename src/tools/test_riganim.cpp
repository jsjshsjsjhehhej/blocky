/**
 * Prove sull'animatore a keyframe del personaggio.
 *
 * Verifica il comportamento, non la sola compilazione: che una chiave sola
 * tenga la posa, che due chiavi interpolino a meta' strada, che scrivere e
 * rileggere da disco restituisca gli stessi numeri, e che una clip non tocchi
 * le parti di cui non ha tracce.
 */

#include "engine/RigAnim.h"

#include <cmath>
#include <cstdio>
#include <string>

static int g_fail = 0;

static void check(const char* what, bool ok) {
    std::printf("%-58s %s\n", what, ok ? "OK" : "FALLITO");
    if (!ok) ++g_fail;
}

static bool closeTo(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

static Humanoid makeRig() {
    Humanoid h;
    for (const auto& name : blk::riganim::partNames()) {
        HumanoidPartDef p;
        p.name = name;
        h.parts.push_back(p);
    }
    return h;
}

int main() {
    using namespace blk::riganim;

    // --- una chiave sola tiene la posa per tutta la clip ---
    {
        Clip c;
        c.id = "test_una_chiave";
        c.duration = 2.f;
        RigPose up;
        up.rot = glm::vec3(-90.f, 0.f, 0.f);
        c.setKey("Right Arm", 0.5f, up);

        Humanoid h = makeRig();
        applyToHumanoid(c, 0.f, h);
        float atStart = 0.f;
        for (const auto& p : h.parts) if (p.name == "Right Arm") atStart = p.anim.rot.x;

        applyToHumanoid(c, 2.f, h);
        float atEnd = 0.f;
        for (const auto& p : h.parts) if (p.name == "Right Arm") atEnd = p.anim.rot.x;

        check("una chiave sola tiene la posa prima e dopo",
              closeTo(atStart, -90.f) && closeTo(atEnd, -90.f));
    }

    // --- due chiavi: a meta' strada il valore e' a meta' ---
    {
        Clip c;
        c.id = "test_meta";
        c.duration = 1.f;
        RigPose a, b;
        a.rot = glm::vec3(0.f);
        b.rot = glm::vec3(90.f, 0.f, 0.f);
        b.scale = glm::vec3(2.f);
        c.setKey("Left Arm", 0.f, a);
        c.setKey("Left Arm", 1.f, b);

        Humanoid h = makeRig();
        applyToHumanoid(c, 0.5f, h);
        float rot = 0.f, scale = 0.f;
        for (const auto& p : h.parts)
            if (p.name == "Left Arm") { rot = p.anim.rot.x; scale = p.anim.scale.x; }

        check("a meta' fra due chiavi la rotazione e' a meta'", closeTo(rot, 45.f));
        check("a meta' fra due chiavi la scala e' a meta'", closeTo(scale, 1.5f));
    }

    // --- le parti senza traccia non vengono toccate ---
    {
        Clip c;
        c.id = "test_intatte";
        RigPose p;
        p.rot = glm::vec3(45.f, 0.f, 0.f);
        c.setKey("Head", 0.f, p);

        Humanoid h = makeRig();
        applyToHumanoid(c, 0.f, h);
        bool othersClean = true;
        for (const auto& part : h.parts)
            if (part.name != "Head" && !part.anim.isIdentity()) othersClean = false;

        check("una clip non tocca le parti di cui non ha tracce", othersClean);
    }

    // --- rimettere una chiave allo stesso istante la corregge, non la duplica ---
    {
        Clip c;
        RigPose a, b;
        a.rot = glm::vec3(10.f, 0.f, 0.f);
        b.rot = glm::vec3(70.f, 0.f, 0.f);
        c.setKey("Torso", 0.25f, a);
        c.setKey("Torso", 0.25f, b);
        check("rimettere una chiave la corregge invece di duplicarla",
              c.keyCount() == 1 && closeTo(c.find("Torso")->keys[0].pose.rot.x, 70.f));
    }

    // --- scrivere e rileggere da disco restituisce gli stessi numeri ---
    {
        Clip c;
        c.id = "test_giro_disco";
        c.name = "Saluto";
        c.duration = 1.75f;
        c.looped = false;
        RigPose p;
        p.pos   = glm::vec3(0.25f, -0.5f, 1.f);
        p.rot   = glm::vec3(-120.f, 33.f, 15.f);
        p.scale = glm::vec3(1.5f, 0.5f, 2.f);
        c.setKey("Right Arm", 0.6f, p);
        c.setKey("Head", 0.f, RigPose{});

        check("salvataggio riuscito", save(c));

        Clip back;
        const bool ok = load("test_giro_disco", back);
        const PartTrack* tr = back.find("Right Arm");
        const bool same = ok && tr && tr->keys.size() == 1
            && closeTo(tr->keys[0].t, 0.6f)
            && closeTo(tr->keys[0].pose.pos.x, 0.25f)
            && closeTo(tr->keys[0].pose.pos.z, 1.f)
            && closeTo(tr->keys[0].pose.rot.x, -120.f)
            && closeTo(tr->keys[0].pose.rot.y, 33.f)
            && closeTo(tr->keys[0].pose.scale.y, 0.5f);

        check("rileggendo da disco i numeri sono identici", same);
        check("nome e durata sopravvivono al salvataggio",
              back.name == "Saluto" && closeTo(back.duration, 1.75f) && !back.looped);
    }

    std::printf("\n%s\n", g_fail == 0 ? "TUTTO OK" : "CI SONO FALLIMENTI");
    return g_fail == 0 ? 0 : 1;
}
