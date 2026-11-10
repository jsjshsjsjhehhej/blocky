// Prova dell'anti-lag senza finestra: orologio, frustum, griglia,
// visibilita' e governor. Esce diverso da zero se un controllo fallisce.

#include "engine/antilag/AntiLag.h"
#include "engine/antilag/CasterRank.h"
#include "engine/antilag/InterestNet.h"
#include "engine/antilag/LodLadder.h"
#include "engine/antilag/ParticleRank.h"
#include "engine/antilag/Cause.h"
#include "engine/antilag/DetailBudget.h"
#include "engine/antilag/DistanceHist.h"
#include "engine/antilag/RenderLadder.h"
#include "engine/antilag/OcclusionBuffer.h"
#include "engine/antilag/Predictor.h"
#include "engine/antilag/Replay.h"
#include "engine/antilag/Report.h"
#include "engine/antilag/Score.h"
#include "engine/antilag/ScriptRoster.h"
#include "engine/antilag/SimBudget.h"
#include "engine/antilag/ZoneMemory.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static antilag::M4 cameraAt(antilag::V3 eye, antilag::V3 at, float fovyDeg, float aspect) {
    antilag::M4 view = antilag::lookAt(eye, at, antilag::v3(0, 1, 0));
    antilag::M4 proj = antilag::perspective(fovyDeg * 3.14159265f / 180.f, aspect, 0.1f, 400.f);
    return antilag::mul(proj, view);
}

static void testClock() {
    std::printf("\nOrologio:\n");
    antilag::FrameClock clock;
    for (int i = 0; i < 10; ++i) clock.push(0.016f, 16.7f);
    check(clock.count() == 10, "dieci campioni");
    check(std::fabs(clock.averageMs() - 16.f) < 0.2f, "media ~16 ms");
    check(antilag::FrameClock::classify(10.f, 16.7f) == antilag::HitchKind::None, "10 ms non e' uno scatto");
    check(antilag::FrameClock::classify(40.f, 16.7f) == antilag::HitchKind::Heavy, "40 ms e' uno scatto");
    check(antilag::FrameClock::classify(120.f, 16.7f) == antilag::HitchKind::Stall, "120 ms e' uno stallo");
    clock.push(0.100f, 16.7f);
    check(clock.heavies() + clock.stalls() >= 1, "lo scatto resta nella finestra");
    check(clock.percentile(1.f) >= clock.percentile(0.5f), "p100 >= p50");
    check(clock.badTail(30.f) >= 1, "la coda cattiva vede l'ultimo frame");
    for (int i = 0; i < 20; ++i) clock.push(0.010f, 16.7f);
    check(clock.goodTail(20.f) >= 20, "venti frame buoni in coda");
}

static void testFrustum() {
    std::printf("\nFrustum:\n");
    antilag::M4 clip = cameraAt(antilag::v3(0, 2, 10), antilag::v3(0, 2, 0), 60.f, 16.f / 9.f);
    antilag::Frustum f = antilag::frustumFromClip(clip);
    check(f.pointInside(0, 2, 0), "il bersaglio e' in vista");
    check(f.pointInside(0, 2, 8), "un punto davanti alla camera e' in vista");
    check(!f.pointInside(0, 2, 30), "un punto dietro la camera e' fuori");
    check(f.sphereOutside(0, 2, 40, 1.f), "sfera dietro e' fuori");
    check(!f.sphereOutside(0, 2, 0, 2.f), "sfera sul bersaglio non e' fuori");
    float px = antilag::screenRadiusPx(antilag::v3(0, 2, 10), 60.f * 3.14159265f / 180.f,
                                       720.f, antilag::v3(0, 2, 0), 2.f);
    float farPx = antilag::screenRadiusPx(antilag::v3(0, 2, 10), 60.f * 3.14159265f / 180.f,
                                          720.f, antilag::v3(0, 2, -200), 2.f);
    std::printf("  raggio schermo vicino %.1f px, lontano %.2f px\n", px, farPx);
    check(px > farPx * 4.f, "la stessa part lontana occupa meno pixel");
    check(px > 8.f, "a 10 stud una part da 2 stud si vede");
}

static void testGrid() {
    std::printf("\nGriglia:\n");
    antilag::SpatialGrid grid;
    grid.reset(16.f);
    grid.insert(1, 0, 0, 0, 1.f);
    grid.insert(2, 4, 0, 0, 1.f);
    grid.insert(3, 80, 0, 0, 1.f);
    grid.insert(4, 0, 40, 0, 1.f);
    std::vector<int> hit;
    grid.queryCentersInSphere(0, 0, 0, 6.f, hit);
    bool has1 = false, has2 = false, has3 = false;
    for (int i : hit) {
        if (i == 1) has1 = true;
        if (i == 2) has2 = true;
        if (i == 3) has3 = true;
    }
    check(has1 && has2, "i vicini entrano nel raggio");
    check(!has3, "quello a 80 stud resta fuori");
    grid.querySphere(0, 0, 0, 0.5f, hit);
    check(!hit.empty(), "la sfera che tocca il volume della part la trova");

    antilag::M4 clip = cameraAt(antilag::v3(0, 5, 20), antilag::v3(0, 0, 0), 70.f, 1.f);
    antilag::Frustum f = antilag::frustumFromClip(clip);
    grid.queryFrustum(f, hit);
    bool sawFar = false, sawOrigin = false;
    for (int i : hit) {
        if (i == 1) sawOrigin = true;
        if (i == 3) sawFar = true;
    }
    check(sawOrigin, "la part all'origine e' nel frustum");
    std::printf("  frustum ha restituito %zu indici\n", hit.size());
    (void)sawFar;
}

static void testVisibility() {
    std::printf("\nVisibilita':\n");
    antilag::Camera cam;
    cam.eye = antilag::v3(0, 4, 18);
    cam.fovyRad = 60.f * 3.14159265f / 180.f;
    cam.viewportH = 720.f;
    cam.setViewProj(cameraAt(cam.eye, antilag::v3(0, 1, 0), 60.f, 16.f / 9.f));

    std::vector<antilag::PartIn> parts(6);
    parts[0] = {0, 1, 0, 4, 4, 4, false, false, false, true};
    parts[1] = {0, 1, -80, 4, 4, 4, false, false, false, true};
    parts[2] = {0, 1, 40, 4, 4, 4, false, false, false, true};
    parts[3] = {2.f, 2.f, 2.f, 2.f, 2.f, 2.f, true, false, false, true};
    parts[4] = {1.f, 1.f, 1.f, 2.f, 2.f, 2.f, false, true, false, true};
    parts[5] = {30.f, 1.f, 0.f, 2.f, 2.f, 2.f, false, false, true, true};

    antilag::Visibility vis;
    antilag::VisibilityLimits lim;
    lim.maxDistance = 50.f;
    lim.maxFull = 4;
    lim.maxCheap = 4;
    lim.minScreenPx = 0.f;
    lim.allowStuds = true;
    vis.rebuild(cam, parts.data(), (int)parts.size(), lim);
    const antilag::PartOut* nearP = vis.get(0);
    const antilag::PartOut* behind = vis.get(2);
    check(nearP && nearP->draw != antilag::DrawKind::Skip, "la part davanti si disegna");
    check(behind && behind->draw == antilag::DrawKind::Skip, "la part dietro non si disegna");
    check(vis.drawn() >= 1 && vis.drawn() <= 5, "il budget non disegna il mondo intero a caso");
    std::printf("  disegnate %d su %d\n", vis.drawn(), vis.considered());

    int lightScore = antilag::Visibility::scoreOf(parts[3], 5.f, 20.f, 50.f);
    int plainScore = antilag::Visibility::scoreOf(parts[0], 40.f, 2.f, 50.f);
    check(lightScore > plainScore, "una luce vicina pesa piu' di un blocco lontano");
}

static void testGovernor() {
    std::printf("\nGovernor:\n");
    antilag::Governor gov;
    gov.setMode(antilag::Mode::Auto);
    gov.setTargetFps(60.f);
    antilag::BaseQuality base;
    base.maxDrawDistance = 800.f;
    base.maxVisibleParts = 8000;
    base.physicsHz = 72.f;
    base.luaBudgetUs = 5000;
    base.maxParticles = 1024;
    base.shadowOccluders = 8;

    antilag::FrameClock calm;
    for (int i = 0; i < 20; ++i) calm.push(0.012f, gov.targetMs());
    antilag::FramePlan easy = gov.evaluate(calm, base);
    check(easy.level == 0, "frame da 12 ms: livello 0");
    check(std::fabs(easy.renderScale - 1.f) < 0.01f, "scala piena quando non serve");
    check(easy.physicsHz > 60.f, "la fisica resta quella di base");

    antilag::Governor stressed;
    stressed.setMode(antilag::Mode::Auto);
    stressed.setTargetFps(60.f);
    antilag::FrameClock bad;
    antilag::FramePlan hard;
    for (int i = 0; i < 40; ++i) {
        bad.push(0.040f, stressed.targetMs());
        hard = stressed.evaluate(bad, base);
    }
    std::printf("  dopo 40 frame da 40 ms: livello %d, scala %.2f, parti %d, fisica %.0f\n",
                hard.level, hard.renderScale, hard.maxVisibleParts, hard.physicsHz);
    check(hard.level >= 4, "il lag sostenuto sale di diversi livelli");
    check(hard.renderScale < 1.f, "la risoluzione interna scende");
    check(hard.maxVisibleParts < base.maxVisibleParts, "meno part disegnate");
    check(hard.physicsHz < base.physicsHz, "la fisica rallenta");
    check(hard.luaBudgetUs < base.luaBudgetUs, "il budget script si stringe");
    check(hard.maxDrawDistance < base.maxDrawDistance, "la vista si accorcia");

    antilag::FrameClock stall;
    antilag::Governor spike;
    antilag::FramePlan spiked;
    for (int i = 0; i < 8; ++i) {
        stall.push(0.016f, spike.targetMs());
        spiked = spike.evaluate(stall, base);
    }
    stall.push(0.150f, spike.targetMs());
    spiked = spike.evaluate(stall, base);
    check(spiked.level >= 2, "uno stallo da 150 ms sale subito di due gradini");
    int afterStall = spiked.level;

    antilag::FramePlan recovered;
    for (int i = 0; i < 80; ++i) {
        stall.push(0.010f, spike.targetMs());
        recovered = spike.evaluate(stall, base);
    }
    std::printf("  dopo il recupero: livello %d (%s)\n", recovered.level, antilag::reasonName(recovered.reason));
    check(recovered.level < afterStall, "i frame buoni fanno scendere il livello");
    check(recovered.level > 0, "non torna a zero in un secondo e mezzo");

    antilag::Governor off;
    off.setMode(antilag::Mode::Off);
    antilag::FramePlan stopped = off.evaluate(bad, base);
    check(stopped.level == 0 && stopped.renderScale == 1.f, "Off non tocca la qualita'");

    antilag::Governor strong;
    strong.setMode(antilag::Mode::Strong);
    antilag::FrameClock warm;
    for (int i = 0; i < 20; ++i) warm.push(0.016f, strong.targetMs());
    antilag::FramePlan splan = strong.evaluate(warm, base);
    check(splan.level >= 2, "Strong non aspetta il primo scatto");
}

static void testEngineLoop() {
    std::printf("\nMotore (observe + cull):\n");
    antilag::Engine eng;
    eng.setMode(antilag::Mode::Auto);
    eng.setTargetFps(60.f);
    antilag::BaseQuality base;
    base.maxDrawDistance = 200.f;
    base.maxVisibleParts = 100;
    base.physicsHz = 60.f;
    base.luaBudgetUs = 2000;
    base.maxParticles = 200;
    eng.setBase(base);

    for (int i = 0; i < 30; ++i) eng.observe(0.045f);
    check(eng.level() >= 1, "observe fa salire il livello");
    check(eng.plan().heartbeatStride >= 1, "lo stride degli script e' almeno 1");
    check(eng.plan().maxParticles < base.maxParticles || eng.level() == 0, "le particelle hanno un tetto");

    antilag::Camera cam;
    cam.eye = antilag::v3(0, 6, 24);
    cam.fovyRad = 1.0f;
    cam.viewportH = 720.f;
    cam.velocity = antilag::v3(0, 0, -12);
    cam.setViewProj(cameraAt(cam.eye, antilag::v3(0, 1, 0), 57.f, 1.6f));
    eng.setCamera(cam);

    std::vector<antilag::PartIn> world;
    world.reserve(500);
    for (int z = -20; z < 20; ++z) {
        for (int x = -5; x <= 5; ++x) {
            antilag::PartIn p;
            p.x = (float)x * 4.f;
            p.y = 1.f;
            p.z = (float)z * 6.f;
            p.hx = p.hy = p.hz = 4.f;
            p.anchored = true;
            world.push_back(p);
        }
    }
    eng.rebuildVisibility(world.data(), (int)world.size());
    eng.rebuildGrid(world.data(), (int)world.size(), 16.f);
    int drawn = eng.visibility().drawn();
    std::printf("  mondo %zu part, disegnate %d, celle %d, %s\n",
                world.size(), drawn, eng.grid().cellCount(), eng.summary().c_str());
    check(drawn > 0, "qualcosa davanti si vede");
    check(drawn < (int)world.size(), "il mondo dietro e lontano non si disegna tutto");

    std::vector<int> near;
    eng.grid().queryCentersInSphere(0, 1, 0, 10.f, near);
    check(!near.empty(), "la griglia trova le part attorno all'origine");
    bool farInNear = false;
    for (int i : near) {
        if (i >= 0 && i < (int)world.size() && std::fabs(world[(size_t)i].z) > 30.f)
            farInNear = true;
    }
    check(!farInNear, "la query da 10 stud non tira dentro il fondo mappa");
}

static void testCutsMonotone() {
    std::printf("\nGradini:\n");
    antilag::BaseQuality base;
    base.maxVisibleParts = 1000;
    base.maxDrawDistance = 500.f;
    base.physicsHz = 72.f;
    int prevParts = 100000;
    float prevDist = 100000.f;
    bool mono = true;
    for (int level = 0; level <= 7; ++level) {
        antilag::FramePlan p = antilag::planFrom(base, level, antilag::StepReason::None, 16.7f);
        if (p.maxVisibleParts > prevParts || p.maxDrawDistance > prevDist + 0.1f) mono = false;
        prevParts = p.maxVisibleParts;
        prevDist = p.maxDrawDistance;
        check(p.physicsHz >= 20.f, "la fisica non scende sotto 20 Hz");
        check(p.renderScale >= 0.5f && p.renderScale <= 1.f, "la scala resta nel range del renderer");
    }
    check(mono, "salire di livello non aumenta mai parti o distanza");
    check(antilag::modeFromName("Off") == antilag::Mode::Off, "nome Off");
    check(antilag::modeFromName("strong") == antilag::Mode::Strong, "nome Strong");
    check(antilag::modeFromName("auto") == antilag::Mode::Auto, "nome Auto");
}

static bool containsId(const int* ids, int n, int id) {
    for (int i = 0; i < n; ++i) if (ids[i] == id) return true;
    return false;
}

static void testRoster() {
    std::printf("\nScript:\n");
    antilag::ScriptRoster roster;
    roster.beginSweep();
    roster.consider(1, 10, 1.f);
    roster.consider(2, 10, 2.f);
    roster.consider(3, 1, 50.f);
    roster.consider(4, 200, 80.f);
    roster.endSweep();
    roster.noteCost(1, 100.f);
    roster.noteCost(2, 100.f);
    roster.noteCost(3, 5000.f);
    roster.noteCost(4, 100.f);
    int ids[8];
    int n = roster.select(1, 250, ids, 8);
    check(containsId(ids, n, 4), "lo script critico entra anche se e' lontano");
    check(containsId(ids, n, 1), "il vicino economico entra");
    check(!containsId(ids, n, 3), "il caro lontano aspetta");
    check(n <= 3, "il budget da 250 us non li fa girare tutti");

    antilag::ScriptRoster turn;
    turn.beginSweep();
    for (int id = 0; id < 4; ++id) turn.consider(id, 10, 0.f);
    turn.endSweep();
    for (int id = 0; id < 4; ++id) turn.noteCost(id, 40.f);
    int a[4], b[4];
    int na = turn.select(2, 1000, a, 4);
    int nb = turn.select(2, 1000, b, 4);
    check(na == 2 && nb == 2, "lo stride 2 ne esegue meta' per frame");
    bool overlap = false;
    for (int i = 0; i < na; ++i)
        if (containsId(b, nb, a[i])) overlap = true;
    check(!overlap, "nel frame dopo girano gli altri");

    turn.beginSweep();
    turn.consider(0, 10, 0.f);
    turn.endSweep();
    check(turn.aliveCount() == 1, "chi non c'e' piu' esce dalla lista");
}

static void testRanks() {
    std::printf("\nRanghi:\n");
    antilag::Camera cam;
    cam.eye = antilag::v3(0, 2, 10);
    cam.fovyRad = 1.0f;
    cam.viewportH = 720.f;
    cam.setViewProj(cameraAt(cam.eye, antilag::v3(0, 2, 0), 57.f, 1.6f));

    antilag::PuffIn puffs[3];
    puffs[0] = {0, 2, 0, 0.8f, 1.f, 1.2f, 10};
    puffs[1] = {0, 2, -80, 0.1f, 1.f, 0.1f, 11};
    puffs[2] = {0, 2, 0, 0.f, 1.f, 2.f, 12};
    int keep[4];
    int nk = antilag::rankParticles(puffs, 3, cam, 1, keep, 4);
    check(nk == 1 && keep[0] == 10, "si tiene il fumo grande vicino");

    antilag::CasterIn casters[3];
    casters[0] = {0, 2, 1, 2, 2, 2, 2.f, 1, true, false};
    casters[1] = {0, 2, -40, 8, 8, 8, 0.2f, 2, true, false};
    casters[2] = {0, 2, 2, 6, 6, 6, 1.f, 3, false, false};
    int lights[4];
    int nl = antilag::rankCasters(casters, 3, cam, true, 1, lights, 4);
    check(nl == 1 && lights[0] == 1, "la torcia vicina batte il neon lontano");
    int shadows[4];
    int ns = antilag::rankCasters(casters, 3, cam, false, 2, shadows, 4);
    check(ns == 1 && shadows[0] == 3, "per le ombre si prende il muro, non la luce");

    check(antilag::lodFor(30.f, true, false) == antilag::Lod::Full, "30 px con stud: dettaglio pieno");
    check(antilag::lodFor(1.f, true, false) == antilag::Lod::Skip, "un pixel si salta");
    check(antilag::lodFor(1.f, true, true) != antilag::Lod::Skip, "una luce piccola non si spegne");
    check(antilag::lodDrawsStuds(antilag::Lod::Flat) == false, "flat non disegna gli stud");
}

static void testInterestAndSplit() {
    std::printf("\nRete e taglio di fase:\n");
    antilag::PeerIn peers[2];
    peers[0] = {1, 2, 0, 0, 1.f};
    peers[1] = {2, 200, 0, 0, 0.1f};
    antilag::PeerOut plan[2];
    antilag::planPeers(peers, 2, antilag::v3(0, 0, 0), 6, 0.f, plan, 2);
    check(!plan[0].drop && plan[0].interval < 0.05f, "chi e' addosso resta frequente");
    check(plan[1].drop, "un lontano poco importante si tace al livello alto");

    antilag::BaseQuality base;
    base.maxDrawDistance = 400.f;
    base.maxVisibleParts = 2000;
    base.physicsHz = 60.f;
    base.luaBudgetUs = 4000;
    base.maxParticles = 64;
    base.maxLights = 8;
    base.shadowOccluders = 8;
    base.postFx = true;
    antilag::FramePlan gov = antilag::planFrom(base, 3, antilag::StepReason::Sustained, 16.7f);

    antilag::SplitInput gpu;
    gpu.plan = gov;
    gpu.blame = antilag::Phase::Present;
    gpu.frameMs = 28.f;
    gpu.targetMs = 16.7f;
    gpu.heat = 0.f;
    antilag::FramePlan gput = antilag::splitBudget(gpu);
    check(gput.renderScale < gov.renderScale, "se la GPU e' piena la scala scende ancora");
    check(gput.luaBudgetUs == gov.luaBudgetUs, "un collo GPU non taglia gli script");
    check(!gput.postFx, "l'alone si spegne quando il frame non ci sta");

    antilag::SplitInput scripts = gpu;
    scripts.blame = antilag::Phase::Scripts;
    antilag::FramePlan st = antilag::splitBudget(scripts);
    check(st.luaBudgetUs < gov.luaBudgetUs, "se la colpa e' lo script si stringe il suo budget");
    check(std::fabs(st.renderScale - gov.renderScale) < 0.001f, "uno script lento non abbassa la risoluzione");

    float ext = antilag::popExtend(100.f, antilag::v3(0, 0, -30.f), antilag::v3(0, 0, -1.f), 0.f);
    check(ext > 100.f && ext < 120.f, "correre allunga la vista di pochi stud");
    float back = antilag::popExtend(100.f, antilag::v3(0, 0, 30.f), antilag::v3(0, 0, -1.f), 0.f);
    check(std::fabs(back - 100.f) < 0.01f, "andare indietro non allunga la vista davanti");
    float capped = antilag::capExtend(ext, 100.f, 104.f);
    check(capped <= 104.f, "l'allungo non supera la distanza scelta dal giocatore");
}

static void testReplay() {
    std::printf("\nTracce:\n");
    antilag::BaseQuality base;
    base.maxDrawDistance = 600.f;
    base.maxVisibleParts = 4000;
    base.physicsHz = 72.f;
    base.luaBudgetUs = 5000;
    base.maxParticles = 64;
    base.maxLights = 8;
    base.shadowOccluders = 8;
    base.postFx = true;
    base.renderScale = 1.f;

    antilag::TraceFrame gpu[36];
    for (int i = 0; i < 36; ++i) gpu[i] = antilag::gpuFrame(0.034f, 22.f);
    antilag::Engine gpuEng;
    gpuEng.setBase(base);
    gpuEng.setTargetFps(60.f);
    antilag::TraceTotals gt;
    antilag::playTrace(gpuEng, gpu, 36, gt);
    std::printf("  GPU: livello max %d, scala min %.2f, lua min %d, post spenti %d\n",
                gt.maxLevel, gt.minScale, gt.minLuaUs, gt.postOffFrames);
    check(gt.maxLevel >= 3, "trenta frame GPU lenti alzano il livello");
    check(gt.minScale < 0.9f, "la scala interna scende sulla traccia GPU");
    check(gt.postOffFrames > 0, "il post si spegne sulla traccia GPU");
    check(gt.journalled > 0, "gli scatti finiscono nel diario");

    antilag::TraceFrame calm[20];
    for (int i = 0; i < 20; ++i) calm[i] = antilag::calmFrame();
    antilag::Engine calmEng;
    calmEng.setBase(base);
    antilag::TraceTotals ct;
    antilag::playTrace(calmEng, calm, 20, ct);
    check(ct.maxLevel == 0, "frame da 12 ms: il livello resta zero");
    check(ct.minScale > 0.99f, "niente taglio se il frame ci sta");

    antilag::Engine hot;
    hot.setBase(base);
    antilag::Camera cam;
    cam.eye = antilag::v3(10, 2, 4);
    hot.setCamera(cam);
    for (int i = 0; i < 6; ++i) hot.observe(0.050f);
    check(hot.heat().at(10, 2, 4) > 0.2f, "gli scatti scaldano la cella dove eri");
    check(hot.heat().at(400, 2, 4) == 0.f, "il resto della mappa non si scalda");
    check(hot.journal().size() >= 6, "ogni scatto e' nel diario");
    check(hot.journal().worstMs() >= 49.f, "il diario tiene il millisecondo vero");
}

static void testOcclusion() {
    std::printf("\nOcclusione:\n");
    antilag::Camera cam;
    cam.eye = antilag::v3(0, 2, 20);
    cam.fovyRad = 60.f * 3.14159265f / 180.f;
    cam.viewportH = 720.f;
    cam.setViewProj(cameraAt(cam.eye, antilag::v3(0, 2, 0), 60.f, 16.f / 9.f));

    antilag::OcclusionBuffer buf;
    buf.clear();
    buf.drawOccluder(cam, antilag::v3(0, 2, 8), 5.f, 20.f);
    check(buf.drawn() == 1, "il muro grande viene scritto");
    float behind = buf.coverage(cam, antilag::v3(0, 2, 0), 1.f);
    float side = buf.coverage(cam, antilag::v3(14, 2, 8), 1.f);
    std::printf("  copertura dietro %.2f, di lato %.2f\n", behind, side);
    check(behind > 0.8f, "la part dietro il muro e' coperta");
    check(side < 0.5f, "la part di lato non e' coperta");

    antilag::OccludeIn items[3];
    items[0] = {0, 2, 8, 5.f, 200.f, true, false, 0};
    items[1] = {0, 2, 0, 1.f, 30.f, true, false, 1};
    items[2] = {0, 2, 0, 1.f, 30.f, true, true, 2};
    char hidden[3] = {};
    int hid = antilag::markOccluded(cam, items, 3, hidden, 3);
    check(hid >= 1 && hidden[1], "il batch nasconde chi sta dietro");
    check(!hidden[2], "una luce dietro il muro resta");
    check(!hidden[0], "il muro non nasconde se stesso");
}

static void testPredictZonesDetail() {
    std::printf("\nPrevisione, zone, dettaglio, sim:\n");
    antilag::Predictor calm;
    for (int i = 0; i < 12; ++i) calm.push(12.f);
    check(calm.earlySteps(16.7f) == 0, "una serie piatta non anticipa");
    check(calm.forecast() < 16.f, "la previsione resta sui 12 ms");

    antilag::Predictor rising;
    for (int i = 0; i < 8; ++i) rising.push(10.f + (float)i * 4.f);
    std::printf("  forecast %.1f trend %.2f early %d\n",
                rising.forecast(), rising.trend(), rising.earlySteps(16.7f));
    check(rising.forecast() > 20.f, "la serie che sale viene prevista in salita");
    check(rising.earlySteps(16.7f) >= 1, "si anticipa almeno un gradino");

    antilag::ZoneMemory zones;
    zones.remember(0, 0, 0, 4);
    check(zones.recall(0, 0, 0) == 4, "la cella ricorda il livello");
    check(zones.recall(300, 0, 0) == 0, "una cella mai vista non ha pavimento");
    for (int i = 0; i < 55; ++i) zones.tick(0.4f);
    check(zones.recall(0, 0, 0) == 0, "dopo venti secondi la memoria se ne va");

    antilag::Governor gov;
    gov.setTargetFps(60.f);
    gov.setFloor(3);
    antilag::FrameClock clock;
    antilag::BaseQuality base;
    antilag::FramePlan pinned;
    for (int i = 0; i < 8; ++i) {
        clock.push(0.012f, gov.targetMs());
        pinned = gov.evaluate(clock, base);
    }
    check(pinned.level >= 3, "il pavimento di zona tiene il livello anche con frame buoni");
    gov.setMode(antilag::Mode::Off);
    gov.setFloor(5);
    antilag::FramePlan off = gov.evaluate(clock, base);
    check(off.level == 0, "Off ignora il pavimento");

    antilag::DetailIn details[6];
    for (int i = 0; i < 6; ++i) {
        details[i].index = i;
        details[i].screenPx = 40.f - (float)i * 7.f;
    }
    details[5].screenPx = 1.f;
    details[5].important = true;
    antilag::DetailOut outs[6];
    int points = antilag::detailPointsForLevel(4, 400);
    int n = antilag::assignDetail(details, 6, points, true, outs, 6);
    check(n == 6, "ogni part riceve una banda");
    check(antilag::spentPoints(outs, n) <= points + 2, "il punteggio non esplode");
    bool tinyKept = false;
    bool bigDrawn = false;
    for (int i = 0; i < n; ++i) {
        if (outs[i].index == 5) tinyKept = outs[i].lod != antilag::Lod::Skip;
        if (outs[i].index == 0) bigDrawn = outs[i].lod != antilag::Lod::Skip;
    }
    check(bigDrawn, "la part grande a schermo si disegna");
    check(tinyKept, "l'importante piccolo non viene saltato");
    check(antilag::detailPointsForLevel(7, 1000) < antilag::detailPointsForLevel(0, 1000),
          "a livello alto il budget dettaglio e' piu' stretto");

    antilag::FramePlan low = antilag::planFrom(base, 0, antilag::StepReason::None, 16.7f);
    antilag::FramePlan high = antilag::planFrom(base, 7, antilag::StepReason::Stall, 16.7f);
    check(antilag::stepsForFrame(low, 0.016f) <= 2, "a 60 Hz un frame normale fa uno o due passi");
    check(antilag::stepsForFrame(high, 0.05f) <= 2, "sotto stallo non si recuperano otto passi");
    check(antilag::particleEmitsFor(high, 16, 60) <= antilag::particleEmitsFor(low, 16, 0),
          "a livello alto nascono meno particelle");
    check(!antilag::voiceAudible(high, 80.f, 100.f), "un suono lontano si tace quando il frame non ci sta");
    check(antilag::voiceAudible(low, 80.f, 100.f), "a livello zero lo stesso suono si sente");
    float net = antilag::netIntervalFor(high, 12, 0.8f);
    check(net >= 0.1f && net <= 0.25f, "tanti peer in una zona calda rallentano l'invio");
    antilag::SimCaps caps = antilag::capsFor(high, 0.04f, 40, 12, 0.8f);
    check(caps.physicsSteps <= 2 && caps.voices <= 2 && caps.navRepairs == 0,
          "la simulazione a livello 7 sta in un frame corto");
}

static void testDistanceLadderCause() {
    std::printf("\nDistanze, scala, causa:\n");
    antilag::DistanceHist hist;
    for (int i = 0; i < 100; ++i) hist.add(10.f);
    for (int i = 0; i < 100; ++i) hist.add(200.f);
    float nearReach = hist.reach(40);
    float farReach = hist.reach(150);
    std::printf("  reach 40 -> %.1f, reach 150 -> %.1f\n", nearReach, farReach);
    check(nearReach < 30.f, "i primi 40 stanno nel secchio vicino");
    check(farReach > 80.f, "per prenderne 150 si arriva ai lontani");
    check(hist.beyond(100.f) >= 90, "i lontani restano oltre 100 stud");
    float capped = antilag::capDistanceToContent(400.f, hist, 40);
    check(capped < 80.f && capped >= 24.f, "la vista si chiude dove sta il contenuto");
    antilag::DistanceHist empty;
    check(antilag::capDistanceToContent(400.f, empty, 40) == 400.f, "senza dati la vista non si tocca");

    antilag::RenderLadder ladder;
    float down = ladder.follow(0.5f);
    float held = ladder.follow(1.f);
    std::printf("  scala dopo il taglio %.2f, tenuta %.2f\n", down, held);
    check(down <= 0.51f, "un piano a 0.5 non resta sopra 0.5");
    check(held < 0.9f, "un solo frame buono non riporta la risoluzione piena");

    antilag::FrameClock clock;
    antilag::PhaseMeter meter;
    for (int i = 0; i < 8; ++i) clock.push(0.016f, 16.7f);
    clock.push(0.150f, 16.7f);
    for (int i = 0; i < 6; ++i) clock.push(0.012f, 16.7f);
    antilag::CauseReport once = antilag::classify(clock, meter, 12.f, 16.7f);
    check(once.cause == antilag::Cause::OneShot, "uno stallo isolato non e' un problema di mappa");
    check(!once.pinZone, "uno stallo isolato non inchioda la zona");

    antilag::FrameClock gpuClock;
    antilag::PhaseMeter gpuMeter;
    for (int i = 0; i < 10; ++i) {
        gpuClock.push(0.030f, 16.7f);
        gpuMeter.begin();
        gpuMeter.add(antilag::Phase::Present, 18.f);
        gpuMeter.add(antilag::Phase::World, 4.f);
        gpuMeter.add(antilag::Phase::Scripts, 0.4f);
        gpuMeter.commit();
    }
    antilag::CauseReport gpu = antilag::classify(gpuClock, gpuMeter, 30.f, 16.7f);
    std::printf("  causa gpu: %s conf %.2f\n", antilag::causeName(gpu.cause), gpu.confidence);
    check(gpu.cause == antilag::Cause::Gpu, "se il tempo e' nello swap, la causa e' la GPU");
    check(gpu.pinZone, "una GPU sempre piena inchioda la zona");
    antilag::FramePlan plan = antilag::planFrom(antilag::BaseQuality{}, 2, antilag::StepReason::Sustained, 16.7f);
    float luaBefore = (float)plan.luaBudgetUs;
    antilag::applyCause(plan, gpu);
    check(plan.renderScale < 0.85f, "la causa GPU stringe ancora la scala");
    check(plan.luaBudgetUs == (int)luaBefore, "la causa GPU non tocca gli script");

    antilag::PhaseMeter scriptMeter;
    scriptMeter.begin();
    scriptMeter.add(antilag::Phase::Scripts, 14.f);
    scriptMeter.add(antilag::Phase::Present, 1.f);
    scriptMeter.commit();
    antilag::FrameClock scriptClock;
    for (int i = 0; i < 8; ++i) scriptClock.push(0.028f, 16.7f);
    antilag::CauseReport sc = antilag::classify(scriptClock, scriptMeter, 28.f, 16.7f);
    check(sc.cause == antilag::Cause::Script, "se il tempo e' negli script, la causa sono gli script");

    antilag::BaseQuality base;
    base.maxVisibleParts = 8000;
    base.maxDrawDistance = 600.f;
    base.physicsHz = 72.f;
    base.luaBudgetUs = 5000;
    base.maxParticles = 64;
    antilag::SceneLoad plate = antilag::sceneBaseplate();
    antilag::SceneLoad city = antilag::sceneCity();
    antilag::SceneLoad scripts = antilag::sceneScripts();
    int plateLevel = antilag::cheapestLevel(base, plate, 16.7f);
    int cityLevel = antilag::cheapestLevel(base, city, 16.7f);
    int scriptLevel = antilag::cheapestLevel(base, scripts, 16.7f);
    std::printf("  livello stimato baseplate %d, citta' %d, script %d\n",
                plateLevel, cityLevel, scriptLevel);
    check(plateLevel == 0, "una baseplate non ha bisogno di tagli");
    check(cityLevel > plateLevel, "una citta' pesante sale di livello");
    antilag::FramePlan cityPlan = antilag::planFrom(base, cityLevel, antilag::StepReason::None, 16.7f);
    antilag::Score cityScore = antilag::scorePlan(base, cityPlan, city);
    check(cityScore.playable, "anche il taglio resta giocabile");
    check(cityScore.savedMs > 1.f, "il taglio sulla citta' recupera millisecondi");
    check(antilag::worthIt(cityScore), "il recupero vale la perdita di immagine");
    float gpuSave = antilag::savedOnCause(base, cityPlan, city, antilag::Cause::Gpu);
    float scriptSave = antilag::savedOnCause(base, antilag::planFrom(base, scriptLevel, antilag::StepReason::None, 16.7f),
                                            scripts, antilag::Cause::Script);
    check(gpuSave > 0.5f, "sulla citta' si risparmia sul disegno");
    check(scriptSave > 0.5f, "sugli script pesanti si risparmia sugli script");

    antilag::PlanTrace trace;
    antilag::FramePlan highPlan = antilag::planFrom(base, 0, antilag::StepReason::None, 16.7f);
    antilag::FramePlan lowPlan = antilag::planFrom(base, 4, antilag::StepReason::Sustained, 16.7f);
    for (int i = 0; i < 3; ++i) {
        trace.push(highPlan);
        trace.push(lowPlan);
    }
    check(trace.reversals() >= 1, "su e giu' della scala viene contato");
    check(trace.oscillating(), "tre inversioni di fila sono un lampeggio");

    antilag::Engine eng;
    eng.setBase(base);
    eng.setMode(antilag::Mode::Auto);
    for (int i = 0; i < 4; ++i) eng.observe(0.012f);
    std::string text = antilag::report(eng);
    check(text.find("Anti-lag Auto") != std::string::npos, "il rapporto dice la modalita'");
    check(text.find("L0") != std::string::npos, "il rapporto elenca i livelli");
    check((int)eng.trace().size() == 4, "ogni observe lascia un piano nella traccia");
}

int main() {
    std::printf("=== Blocky anti-lag ===\n");
    testClock();
    testFrustum();
    testGrid();
    testVisibility();
    testGovernor();
    testEngineLoop();
    testCutsMonotone();
    testRoster();
    testRanks();
    testInterestAndSplit();
    testReplay();
    testOcclusion();
    testPredictZonesDetail();
    testDistanceLadderCause();
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
