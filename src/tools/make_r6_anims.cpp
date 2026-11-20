// Animazioni R6 di Blocky: dalle pose esportate da Blender
// (tools/blender/blocky_r6_anims.py -> r6_pose_frames.json) al file .blkanim
// del motore, sullo STESSO rig che il motore costruisce per il player
// (Auto Rig R6 sulle parti di Humanoid::createR6Blocky).
//
//   make_r6_anims <r6_pose_frames.json> <uscita.blkanim>
//
// Nel JSON ogni parte ha, per fotogramma, la sua rotazione rispetto alla parte
// padre (quaternione w x y z, assi di Blocky). L'osso del motore che la muove
// non ha gli assi della parte (le ossa di braccia e gambe puntano in giu'):
// la posa locale dell'osso e' quella rotazione portata nei suoi assi,
//   posa = G0^-1 * delta * G0     (G0 = rotazione dell'osso a riposo nel mondo)
// Una chiave per fotogramma, lineare: la forma delle curve resta quella di Blender.

#include "engine/Humanoid.h"
#include "engine/MiniJson.h"
#include "engine/anim/AnimIO.h"
#include "engine/anim/AutoRig.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

using namespace blk::anim;

// Priorita' come Roblox: Idle 1, Movement 2, Action 3+ (vince sulle ossa che anima).
static float priorityOf(const std::string& n) {
    if (n == "Idle" || n == "Idle2") return 1.f;
    if (n == "Tool") return 3.f;
    if (n == "Wave" || n == "Dance") return 4.f;
    return 2.f;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "uso: make_r6_anims <r6_pose_frames.json> <uscita.blkanim>\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) { std::fprintf(stderr, "non trovo %s\n", argv[1]); return 1; }
    std::stringstream ss;
    ss << in.rdbuf();
    blk::json::Value root;
    if (!blk::json::parse(ss.str(), root) || !root.isObject()) { std::fprintf(stderr, "JSON non valido\n"); return 1; }
    const auto* anims = root.get("animations");
    if (!anims || anims->type != blk::json::Value::Type::Array) { std::fprintf(stderr, "manca animations\n"); return 1; }
    const float fps = root.get("fps") ? (float)root.get("fps")->num : 30.f;

    // Il rig del player, come lo costruisce LuaRuntime::setupCharacterRig.
    std::vector<PartBox> body;
    for (const auto& d : Humanoid::createR6Blocky().parts)
        if (d.name != "HumanoidRootPart") body.push_back({d.name, d.offset, d.size, glm::quat(1.f, 0.f, 0.f, 0.f)});
    AutoRigResult rig = autoRig(body);
    if (rig.kind != RigKind::R6) { std::fprintf(stderr, "rig R6 non costruito: %s\n", rig.error.c_str()); return 1; }
    std::vector<Transform> g0;
    rig.armature.restGlobals(g0);
    std::map<std::string, int> boneOfPart;
    for (const auto& b : rig.bindings) boneOfPart[b.part] = b.bone;
    const int rootBone = rig.armature.find("Root");

    std::vector<Action> actions;
    for (const auto& ja : anims->arr) {
        Action a;
        a.name = ja.get("name") ? ja.get("name")->str : "Anim";
        a.fps = fps;
        a.frameStart = 0.f;
        a.frameEnd = ja.get("length") ? (float)ja.get("length")->num : 30.f;
        a.loop = ja.get("loop") && ja.get("loop")->b;
        a.priority = priorityOf(a.name);
        const auto* frames = ja.get("frames");
        if (!frames || frames->type != blk::json::Value::Type::Array) continue;
        bool rootMoves = false;
        for (const auto& fr : frames->arr) {
            const auto* r = fr.get("root");
            if (r && r->arr.size() == 3)
                for (const auto& v : r->arr) rootMoves = rootMoves || std::fabs(v.num) > 1e-4;
        }
        for (size_t f = 0; f < frames->arr.size(); ++f) {
            const auto& fr = frames->arr[f];
            const auto* bones = fr.get("bones");
            if (!bones || !bones->isObject()) continue;
            for (const auto& kv : bones->obj) {
                // Tool: solo il braccio destro, il resto lo decidono idle/camminata.
                if (a.name == "Tool" && kv.first != "Right Arm") continue;
                const auto it = boneOfPart.find(kv.first);
                if (it == boneOfPart.end() || kv.second.arr.size() != 4) continue;
                const glm::quat delta = glm::normalize(glm::quat((float)kv.second.arr[0].num, (float)kv.second.arr[1].num,
                                                                 (float)kv.second.arr[2].num, (float)kv.second.arr[3].num));
                const glm::quat G = g0[(size_t)it->second].rot;
                Transform local;
                local.rot = glm::normalize(glm::conjugate(G) * delta * G);
                a.insertKeys(rig.armature.bones[(size_t)it->second].name, local, (float)f, Action::KeyRot, Interp::Linear);
            }
            if (rootMoves && rootBone >= 0) {
                const auto* r = fr.get("root");
                if (r && r->arr.size() == 3) {
                    Transform local;
                    local.pos = glm::conjugate(g0[(size_t)rootBone].rot)
                              * glm::vec3((float)r->arr[0].num, (float)r->arr[1].num, (float)r->arr[2].num);
                    a.insertKeys("Root", local, (float)f, Action::KeyLoc, Interp::Linear);
                }
            }
        }
        std::printf("  %-6s %3d fotogrammi  %s  priorita' %.0f  %zu curve\n", a.name.c_str(), (int)a.frameEnd,
                    a.loop ? "loop" : "una volta", a.priority, a.curves.size());
        actions.push_back(std::move(a));
    }
    if (actions.empty()) { std::fprintf(stderr, "nessuna animazione\n"); return 1; }
    if (!saveFile(argv[2], rig.armature, actions)) { std::fprintf(stderr, "non riesco a scrivere %s\n", argv[2]); return 1; }
    std::printf("scritto %s (%zu animazioni)\n", argv[2], actions.size());
    return 0;
}
