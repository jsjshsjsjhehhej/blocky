// Pipeline di rendering del mondo 3D (pass 4–10 di render/RenderPipeline.h).
// Non e' un header riusabile: usa le globali di main.cpp. Compila solo se
// incluso da main.cpp nel punto previsto.
//
// Ordine in renderWorld3D:
//   passPrepare → passShadows → passTerrain → passOpaqueParts →
//   passStudioGizmos → passWorkspaceModels → passAvatars → friends

#include "render/RenderCullLod.h"
#include "render/RenderPipeline.h"

static const glm::vec3 kGizmoRGB[3] = {
    {0.95f, 0.22f, 0.20f},
    {0.22f, 0.88f, 0.32f},
    {0.28f, 0.52f, 1.00f},
};

static void drawGizmoOverlayHud() {
    if (g_studioTesting || g_screen != AppScreen::Studio) return;
    if (!g_moveDragging && !g_rotateDragging && !g_scaleDragging) return;
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 140.f, 64.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##GizmoHud", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    if (g_rotateDragging) {
        const char* ax = (g_rotateAxis == 0) ? "X" : (g_rotateAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({1.f, 0.85f, 0.3f, 1.f}, "Rotate %s  %+0.1f°", ax, g_rotateLiveDelta);
        ImGui::TextDisabled("Snap 5° · Shift=1° · Ctrl=libero");
    } else if (g_moveDragging) {
        const char* ax = (g_moveAxis == 0) ? "X" : (g_moveAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.5f, 0.9f, 1.f, 1.f}, "Move %s  %+0.2f", ax, g_moveLiveDelta);
        ImGui::TextDisabled("Snap 0.25 · Shift=0.05 · Ctrl=libero");
    } else if (g_scaleDragging) {
        const char* ax = (g_scaleAxis == 0) ? "X" : (g_scaleAxis == 1) ? "Y" : "Z";
        ImGui::TextColored({0.6f, 1.f, 0.55f, 1.f}, "Scale %s  %.2f", ax, g_scaleLiveSize);
        ImGui::TextDisabled("Snap 0.25 · Shift=0.1 · Ctrl=libero");
    }
    ImGui::End();
}

static void drawScaleHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                             const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Scale || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    if (!g_scaleDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1, sg = 0;
        g_gizmoHoverAxis = pickScaleHandle((float)mx, (float)my, ax, sg) ? ax : -1;
    }
    float baseHs = scaleHandleDrawSize(p);
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            glm::vec3 hp = handleWorldPos(p, axis, s);
            bool hot = (g_scaleDragging && g_scaleAxis == axis && g_scaleSign == s)
                    || (g_gizmoHoverAxis == axis && !g_scaleDragging);
            float hs = hot ? baseHs * 1.35f : baseHs;
            glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.35f) : kGizmoRGB[axis];
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
            glm::mat4 m2 = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs * 1.18f));
            drawCube(vao, partP, m2, view, proj, col * 0.45f, sun, false, 0);
        }
    }
}

static void drawMoveHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                            const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Move || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float len = gizmoArrowLen(p);
    if (!g_moveDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickMoveHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
        else
            g_gizmoHoverAxis = ax;
    }
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.35f));
        drawCube(vao, partP, m, view, proj, glm::vec3(0.95f, 0.95f, 0.98f), sun, false, 0);
    }
    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_moveDragging && g_moveAxis == axis) || (g_gizmoHoverAxis == axis && !g_moveDragging);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.4f) : kGizmoRGB[axis];
        glm::vec3 dir = partAxisDir(p, axis);
        float stemLen = len * 0.78f;
        glm::vec3 stemMid = p.position + dir * (stemLen * 0.5f);
        float thick = hot ? 0.16f : 0.12f;
        glm::mat4 mStem = orientAlongZ(stemMid, dir, {thick, thick, stemLen});
        drawCube(vao, partP, mStem, view, proj, col * 0.92f, sun, false, 0);
        glm::vec3 tip = p.position + dir * len;
        float tipS = hot ? 0.55f : 0.42f;
        glm::mat4 mTip = orientAlongZ(tip - dir * (tipS * 0.35f), dir, {tipS, tipS, tipS * 1.35f});
        drawCube(vao, partP, mTip, view, proj, col, sun, false, 0);
    }
}

static void drawRotateHandles(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    if (g_tool != StudioTool::Rotate || g_studioTesting) return;
    if (g_screen != AppScreen::Studio) return;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    float R = gizmoRingRadius(p);
    const int segs = 64;

    if (!g_rotateDragging) {
        double mx = 0, my = 0;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int ax = -1;
        if (!pickRotateHandle((float)mx, (float)my, ax))
            g_gizmoHoverAxis = -1;
    }

    for (int axis = 0; axis < 3; ++axis) {
        bool hot = (g_rotateDragging && g_rotateAxis == axis)
                || (g_gizmoHoverAxis == axis);
        glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.45f) : kGizmoRGB[axis];
        float thick = hot ? 0.22f : 0.14f;
        glm::vec3 axisV = partAxisDir(p, axis);
        glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
        glm::vec3 v = glm::normalize(glm::cross(axisV, u));

        for (int i = 0; i < segs; ++i) {
            float a = (float)i / (float)segs * 6.2831853f;
            glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * R;
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(thick));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
        }

        for (int tick = 0; tick < 24; ++tick) {
            float a = (float)tick / 24.f * 6.2831853f;
            glm::vec3 radial = u * std::cos(a) + v * std::sin(a);
            glm::vec3 pos = p.position + radial * (R + 0.28f);
            float ts = (tick % 6 == 0) ? 0.20f : 0.11f;
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(ts));
            drawCube(vao, partP, m, view, proj, col * 0.75f, sun, false, 0);
        }

        if (g_rotateDragging && g_rotateAxis == axis && std::fabs(g_rotateLiveDelta) > 0.5f) {
            int nFill = std::clamp((int)std::fabs(g_rotateLiveDelta) / 3, 2, 40);
            float sign = (g_rotateLiveDelta >= 0.f) ? 1.f : -1.f;
            float startA = g_rotateStartAngle;
            for (int i = 0; i <= nFill; ++i) {
                float a = startA + sign * glm::radians(std::fabs(g_rotateLiveDelta)) * ((float)i / nFill);
                glm::vec3 pos = p.position + (u * std::cos(a) + v * std::sin(a)) * (R * 0.92f);
                glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(0.18f));
                drawCube(vao, partP, m, view, proj, glm::vec3(1.f, 0.92f, 0.35f), sun, false, 0);
            }
        }
    }
    glm::mat4 core = glm::scale(glm::translate(glm::mat4(1.f), p.position), glm::vec3(0.28f));
    drawCube(vao, partP, core, view, proj, glm::vec3(0.9f, 0.9f, 0.95f), sun, false, 0);
}

static void drawAvatarAccessories(GLuint meshP, const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& sun, glm::vec3 worldPos, float yawDeg,
                                  const Humanoid& hum, GLuint markerVao = 0, GLuint partP = 0,
                                  bool editorGhostMode = false) {
    if (g_catalog.items.empty()) return;
    float bob = humanoidBob(hum);
    float legSwing = humanoidLegSwing(hum);
    float armBase = humanoidArmSwing(hum);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(hum, jL, jR, jl, jr);
    const float jb = hum.jumpArmBlend;
    float armL = armBase * (1.f - jb) + jL;
    float armR = -armBase * (1.f - jb) + jR;
    float legL = legSwing * (1.f - jb) + jl;
    float legR = -legSwing * (1.f - jb) + jr;

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), worldPos),
                                 glm::radians(yawDeg), glm::vec3(0, 1, 0));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    auto drawOne = [&](int i, float alpha, bool depthWrite) {
        const auto& a = g_catalog.items[i];
        if (!a.mesh.loaded || a.mesh.vertexCount <= 0) return;
        glm::mat4 local = accessoryWorldMatrix(a, hum, bob, armL, armR, legL, legR);
        glm::mat4 model = root * local;

        glm::vec3 tint = a.tint;
        if (editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.88f, 0.35f), 0.22f);
        else if (!editorGhostMode && i == g_catalog.selected)
            tint = glm::mix(tint, glm::vec3(1.f, 0.85f, 0.2f), 0.25f);

        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
        if (alpha < 0.99f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        glUseProgram(meshP);
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(meshP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(meshP,"uSun"),1,glm::value_ptr(sun));
        glUniform3fv(glGetUniformLocation(meshP,"uCamPos"),1,glm::value_ptr(g_renderCamPos));
        uploadDynamicLights(meshP);
        a.mesh.drawTextured(meshP, tint, alpha);
        (void)markerVao;
        (void)partP;
    };

    if (editorGhostMode) {
        for (int i = 0; i < (int)g_catalog.items.size(); ++i) {
            if (i == g_catalog.selected) continue;
            drawOne(i, 0.38f, false);
        }
        if (g_catalog.selected >= 0 && g_catalog.selected < (int)g_catalog.items.size())
            drawOne(g_catalog.selected, 1.f, true);
        else {
            for (int i = 0; i < (int)g_catalog.items.size(); ++i)
                drawOne(i, 0.72f, true);
        }
    } else {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        for (int i = 0; i < (int)g_catalog.items.size(); ++i)
            drawOne(i, 1.f, true);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

static void drawCubeShadow(GLuint vao, GLuint prog, const glm::mat4& m, const glm::mat4&,
                           const glm::mat4&, const glm::vec3&, const glm::vec3&) {
    (void)prog;
    int vc = (g_bevelCube.vao && vao == g_bevelCube.vao) ? g_bevelCube.vertexCount : g_sharpCubeVerts;
    g_shadowMap.setModel(m);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vc);
}

static void renderShadowDepth(GLuint vao, GLuint meshP) {
    g_sunShadowOn = false;
    bumpSceneUniforms();   // uSunShadow / uLightVP cambiano qui
    // quality.shadows era impostato dai preset ma mai letto — ora conta.
    if (!pipelineSunShadowsEnabled(g_engine.quality, g_lighting)) return;
    if (!g_shadowMap.ready) return;

    glm::vec3 focus = g_renderCamPos;
    if (!g_shadowMap.begin(g_lighting.toSkyLight(), focus)) return;
    g_sunShadowOn = true;

    const float reach = g_shadowMap.extent + 24.f;
    const float reach2 = reach * reach;
    const float shapeLow2Shadow =
        lodDistancesFor(g_engine.quality).shapeLow * lodDistancesFor(g_engine.quality).shapeLow;

    for (const auto& p : g_world.parts) {
        if (p.isTransparent()) continue;
        glm::vec3 d = p.position - focus;
        float d2 = glm::dot(d, d);
        if (d2 > reach2) continue;
        GLuint dv = vao;
        int vc = 36;
        PartMeshLod shadowLod = (d2 > shapeLow2Shadow) ? PartMeshLod::Low : PartMeshLod::High;
        if (ShapeMesh* sm = g_partMeshes.meshForLod(p.shape, shadowLod)) {
            dv = sm->vao;
            vc = sm->vertexCount;
        } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
            dv = g_partMeshes.wedge.vao;
            vc = g_partMeshes.wedge.vertexCount;
        }
        g_shadowMap.setModel(partModelMatrix(p));
        glBindVertexArray(dv);
        glDrawArrays(GL_TRIANGLES, 0, vc);
    }
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (i >= (int)g_wsMeshes.size() || !g_wsMeshes[i].loaded) continue;
        const auto& m = g_world.models[i];
        glm::vec3 d = m.position - focus;
        if (glm::dot(d, d) > reach2) continue;
        g_shadowMap.setModel(glm::translate(glm::mat4(1.f), m.position)
                             * partRotationMatrix(m.rotation)
                             * glm::scale(glm::mat4(1.f), m.scale));
        g_wsMeshes[i].drawTextured(g_shadowMap.depthProg, {1.f, 1.f, 1.f}, 1.f);
    }

    const glm::mat4 idm(1.f);
    const glm::vec3 zero(0.f);
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext sctx{ playerVao, g_shadowMap.depthProg, idm, idm, zero };
    const bool inWorld = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio);
    if (inWorld && !g_ragdoll.active) {
        bool hideBody[(int)AvatarSlot::Count] = {};
        fillBodyHideMask(g_catalog, hideBody);
        g_player.draw(sctx, drawCubeShadow, g_playerPos, g_playerYaw, hideBody);
        drawAvatarAccessories(g_shadowMap.depthProg, idm, idm, zero,
                              g_playerPos, g_playerYaw, g_player.humanoid);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUseProgram(g_shadowMap.depthProg);
    }
    for (size_t i = 0; i < g_net.remotes.size() && i < g_remoteBodies.size(); ++i)
        g_remoteBodies[i].draw(sctx, drawCubeShadow,
                               g_net.remotes[i].displayPos, g_net.remotes[i].displayYaw);

    // Il viewport va riportato alla risoluzione della SCENA, non a quella
    // della finestra: la scena puo essere piu piccola (Render scale).
    g_shadowMap.end(g_renderW, g_renderH, g_bloom.ready ? g_bloom.sceneFbo : 0);
}

// ---- Terrain: una VBO per chunk, una draw call per (chunk, materiale) ----
// Vale per qualunque place che usa workspace.Terrain: le facce esposte
// arrivano gia' pronte da Terrain::ChunkMesh (greedy 2D), qui si caricano in
// GPU solo quando il chunk cambia (version) e si scartano i chunk fuori
// frustum / oltre la distanza di disegno. Prima era un drawCube per box.
struct TerrainGpuChunk {
    GLuint vao = 0, vbo = 0;
    uint32_t version = 0;
    int vertCount = 0;
    std::vector<Terrain::MeshRange> ranges;
    glm::vec3 bmin{0}, bmax{0};
    bool seen = false;
};
static std::unordered_map<Terrain::Coord, TerrainGpuChunk, Terrain::CoordHash> g_terrainGpu;
static int g_terrainDrawCalls = 0;
static int g_terrainChunksDrawn = 0;

static void terrainGpuFree(TerrainGpuChunk& g) {
    if (g.vbo) glDeleteBuffers(1, &g.vbo);
    if (g.vao) glDeleteVertexArrays(1, &g.vao);
    g.vbo = g.vao = 0;
    g.vertCount = 0;
}

static void terrainGpuUpload(TerrainGpuChunk& g, const Terrain::ChunkMesh& m) {
    g.version = m.version;
    g.ranges = m.ranges;
    g.bmin = m.bmin;
    g.bmax = m.bmax;
    g.vertCount = (int)(m.verts.size() / 6);
    if (g.vertCount == 0) {
        terrainGpuFree(g);
        g.version = m.version;
        return;
    }
    if (!g.vao) {
        glGenVertexArrays(1, &g.vao);
        glGenBuffers(1, &g.vbo);
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    } else {
        glBindVertexArray(g.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    }
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m.verts.size() * sizeof(float)),
                 m.verts.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
}

// Materiale Part usato per ombreggiare/texturare un voxel: cosi' il terreno
// riusa lo stesso shader delle Part (ombre, foschia, riflessi, texture).
static PartMaterial terrainPartMaterial(uint8_t id) {
    switch (id) {
    case Terrain::Grass: case Terrain::Leaves: case Terrain::Moss:
    case Terrain::Cactus: case Terrain::Sapling: case Terrain::Reed:
    case Terrain::Wheat: case Terrain::Flower: case Terrain::Mushroom:
        return PartMaterial::Grass;
    case Terrain::Wood: case Terrain::Planks: case Terrain::Door:
    case Terrain::DoorOpen: case Terrain::Ladder: case Terrain::ChestBlk:
    case Terrain::Bed:
        return PartMaterial::Wood;
    case Terrain::Brick:
        return PartMaterial::Brick;
    case Terrain::Glass:
        return PartMaterial::Glass;
    case Terrain::Water:
        return PartMaterial::Ice;        // alpha 0.55: si vede il fondale
    case Terrain::IronBlock: case Terrain::GoldBlock: case Terrain::DiamondBlock:
        return PartMaterial::Metal;
    case Terrain::Torch: case Terrain::Lava:
        return PartMaterial::Neon;
    case Terrain::Snow: case Terrain::Wool: case Terrain::Sponge:
        return PartMaterial::SmoothPlastic;
    case Terrain::Stone: case Terrain::Cobble: case Terrain::Bedrock:
    case Terrain::Coal: case Terrain::Iron: case Terrain::Gold:
    case Terrain::Diamond: case Terrain::Gravel: case Terrain::Obsidian:
    case Terrain::Dirt: case Terrain::Sand: case Terrain::Clay:
    case Terrain::Farmland:
        return PartMaterial::Concrete;
    default:
        return PartMaterial::Plastic;
    }
}

static bool terrainMaterialTransparent(uint8_t id) {
    return id == Terrain::Water || id == Terrain::Glass;
}

static void drawTerrainVoxels(GLuint /*vao*/, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    g_terrainDrawCalls = 0;
    g_terrainChunksDrawn = 0;
    if (g_terrain.empty()) {
        if (!g_terrainGpu.empty()) {
            for (auto& kv : g_terrainGpu) terrainGpuFree(kv.second);
            g_terrainGpu.clear();
        }
        return;
    }

    // 1) sincronizza GPU <-> chunk (upload solo se la version e' cambiata)
    for (auto& kv : g_terrainGpu) kv.second.seen = false;
    g_terrain.forEachChunkMesh([&](const Terrain::Coord& c, const Terrain::ChunkMesh& m) {
        TerrainGpuChunk& g = g_terrainGpu[c];
        g.seen = true;
        if (g.version != m.version) terrainGpuUpload(g, m);
    });
    for (auto it = g_terrainGpu.begin(); it != g_terrainGpu.end();) {
        if (!it->second.seen) {
            terrainGpuFree(it->second);
            it = g_terrainGpu.erase(it);
        } else {
            ++it;
        }
    }

    // 2) culling per chunk — distanza terrain dedicata (su mobile il
    // maxDrawDistance da Part e' troppo alto: un mondo 50x50 resta tutto
    // in vista e il telefono resta a ~30 fps).
    const FrustumPlanes frustum = extractFrustum(proj * view);
    const glm::vec3 origin = g_renderCamPos;
    float maxD = g_engine.quality.maxDrawDistance;
    if (g_engine.quality.mobile)
        maxD = std::min(maxD, 56.f);
    if (!g_engine.quality.studTextures)
        maxD = std::min(maxD, 52.f);
    maxD = std::max(40.f, maxD);
    const float waterLim = g_engine.quality.mobile ? 42.f : 120.f;
    const float waterMax2 = waterLim * waterLim;
    struct Vis { TerrainGpuChunk* g; float d2; };
    static std::vector<Vis> vis;
    vis.clear();
    for (auto& kv : g_terrainGpu) {
        TerrainGpuChunk& g = kv.second;
        if (!g.vao || g.vertCount == 0) continue;
        const glm::vec3 center = (g.bmin + g.bmax) * 0.5f;
        const float radius = glm::length(g.bmax - g.bmin) * 0.5f;
        const glm::vec3 d = center - origin;
        const float d2 = glm::dot(d, d);
        const float lim = maxD + radius;
        if (d2 > lim * lim) continue;
        if (sphereOutsideFrustum(frustum, center, radius)) continue;
        vis.push_back({&g, d2});
    }
    std::sort(vis.begin(), vis.end(), [](const Vis& a, const Vis& b) { return a.d2 < b.d2; });

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Lite solo con preset Low (stud off): colori flat. Su mobile Medium+ usa
    // le texture materiale sui voxel come su PC.
    const bool lite = !g_engine.quality.studTextures;
    TerrainDrawPass tp = beginTerrainPass(partP, view, proj, sun, lite);

    auto emitOf = [](uint8_t id) -> float {
        if (id == Terrain::Torch || id == Terrain::Lava) return 1.f;
        return 0.f;
    };

    // 3) opachi
    for (const Vis& v : vis) {
        bool any = false;
        for (const Terrain::MeshRange& r : v.g->ranges) {
            if (terrainMaterialTransparent(r.id)) continue;
            PartMaterial mat = lite ? PartMaterial::Plastic : terrainPartMaterial(r.id);
            drawTerrainRange(tp, v.g->vao, Terrain::colorOf(r.id), mat,
                             emitOf(r.id), r.first, r.count);
            ++g_terrainDrawCalls;
            any = true;
        }
        if (any) ++g_terrainChunksDrawn;
    }
    // 4) trasparenti (acqua, vetro), dal piu' lontano — acqua lontana saltata su mobile
    for (auto it = vis.rbegin(); it != vis.rend(); ++it) {
        if (g_engine.quality.mobile && it->d2 > waterMax2) continue;
        for (const Terrain::MeshRange& r : it->g->ranges) {
            if (!terrainMaterialTransparent(r.id)) continue;
            PartMaterial mat = (r.id == Terrain::Water) ? PartMaterial::Ice
                               : (lite ? PartMaterial::Plastic : PartMaterial::Glass);
            if (lite && r.id == Terrain::Water) mat = PartMaterial::Ice; // serve l'alpha
            drawTerrainRange(tp, it->g->vao, Terrain::colorOf(r.id), mat,
                             0.f, r.first, r.count);
            ++g_terrainDrawCalls;
        }
    }
}

// ---- Pass nominati (stesso look di prima, ordine leggibile) ----------------

static void passPrepare(const glm::mat4& view, const glm::mat4& proj) {
    g_lastView = view;
    g_lastProj = proj;
    g_renderCamPos = (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting)
        ? g_cam.pos : g_playerPos + glm::vec3(0, 5, 0);
    collectDynamicLights();
    collectShadowOccluders();
}

static void passShadows(GLuint vao, GLuint meshP) {
    renderShadowDepth(vao, meshP);
}

static void passTerrain(GLuint vao, GLuint partP, const glm::mat4& view,
                        const glm::mat4& proj, const glm::vec3& sun) {
    drawTerrainVoxels(vao, partP, view, proj, sun);
}

static void passOpaqueParts(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& sun) {
    g_renderCullStats.reset();
    g_renderCullStats.partsTotal = (int)g_world.parts.size();

    glm::vec3 origin = g_renderCamPos;
    const float maxD = g_engine.quality.maxDrawDistance;
    const int maxN = g_engine.quality.maxVisibleParts;
    const RenderLodDistances lodD = lodDistancesFor(g_engine.quality);
    const float studOff2 = lodD.studOff * lodD.studOff;
    const float shapeLow2 = lodD.shapeLow * lodD.shapeLow;
    const float smallCull2 = lodD.smallCull * lodD.smallCull;

    const bool useFrustum = g_engine.quality.autoLod || g_engine.quality.frustumCull;
    const bool useOcclusion = g_engine.quality.autoLod || g_engine.quality.occlusionCull;
    const FrustumPlanes frustum = extractFrustum(proj * view);
    const glm::mat4 viewProj = proj * view;
    struct Cand { int i; float d2; bool transparent; };
    static std::vector<Cand> cands;
    cands.clear();
    cands.reserve(g_world.parts.size());
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const auto& part = g_world.parts[i];
        if (part.transparency >= 0.99f) continue;
        if (g_viewIsolate && g_isolatePart >= 0
            && i != g_isolatePart && i != g_selectedPart)
            continue;
        glm::vec3 d = part.position - origin;
        float d2 = glm::dot(d, d);
        if (d2 > maxD * maxD) {
            ++g_renderCullStats.partsDistCulled;
            continue;
        }
        if (lodD.smallMaxSize > 0.f) {
            float maxS = std::max(part.size.x, std::max(part.size.y, part.size.z));
            if (maxS <= lodD.smallMaxSize && d2 > smallCull2) {
                ++g_renderCullStats.partsSmallCulled;
                continue;
            }
        }
        float boundRadius = partBoundRadius(part.size);
        if (useFrustum && sphereOutsideFrustum(frustum, part.position, boundRadius)) {
            ++g_renderCullStats.partsFrustumCulled;
            continue;
        }
        cands.push_back({i, d2, part.isTransparent() || part.transparency > 0.05f});
    }
    if (useOcclusion && !cands.empty()) {
        static std::vector<uint8_t> occMask;
        const float minOcc = g_engine.quality.mobile ? 2.5f : 2.f;
        const int maxOcc = g_engine.quality.mobile ? 48 : 96;
        buildAndTestOcclusion(g_world.parts, cands, viewProj, minOcc, maxOcc, occMask);
        int w = 0;
        for (int ci = 0; ci < (int)cands.size(); ++ci) {
            const bool keepSelected = (g_screen == AppScreen::Studio && !g_studioTesting
                && cands[ci].i == g_selectedPart);
            if (occMask[ci] && !keepSelected) {
                ++g_renderCullStats.partsOcclusionCulled;
                continue;
            }
            cands[w++] = cands[ci];
        }
        cands.resize(w);
    }
    const int afterCull = (int)cands.size();
    if (afterCull > maxN) {
        if (g_engine.quality.priorityPartCull) {
            struct Scored { int ci; float score; };
            static std::vector<Scored> scored;
            scored.clear();
            scored.reserve(cands.size());
            for (int ci = 0; ci < (int)cands.size(); ++ci) {
                const auto& c = cands[ci];
                float score = partKeepScore(c.d2, g_world.parts[c.i].size);
                if (g_screen == AppScreen::Studio && !g_studioTesting
                    && c.i == g_selectedPart)
                    score *= 1.0e6f;
                scored.push_back({ci, score});
            }
            std::nth_element(scored.begin(), scored.begin() + maxN, scored.end(),
                [](const Scored& a, const Scored& b) { return a.score > b.score; });
            static std::vector<Cand> kept;
            kept.clear();
            kept.reserve(maxN);
            for (int k = 0; k < maxN; ++k)
                kept.push_back(cands[scored[k].ci]);
            cands.swap(kept);
        } else {
            std::nth_element(cands.begin(), cands.begin() + maxN, cands.end(),
                [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            cands.resize(maxN);
        }
        g_renderCullStats.partsBudgetCulled = afterCull - maxN;
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        if (a.transparent != b.transparent) return !a.transparent && b.transparent;
        if (a.transparent) return a.d2 < b.d2;
        return a.d2 > b.d2;
    });
    for (const auto& c : cands) {
        auto& p = g_world.parts[c.i];
        glm::mat4 m = partModelMatrix(p);
        const float dist2 = c.d2;
        bool useStud = p.useStud && g_engine.quality.studTextures
            && !p.isNeon() && p.material == PartMaterial::Plastic
            && p.shape == PartShape::Block;
        if (useStud && dist2 > studOff2) {
            useStud = false;
            ++g_renderCullStats.partsStudLod;
        }
        PartMeshLod meshLod = (dist2 > shapeLow2) ? PartMeshLod::Low : PartMeshLod::High;
        GLuint drawVao = vao;
        int vertCount = 36;
        if (ShapeMesh* sm = g_partMeshes.meshForLod(p.shape, meshLod)) {
            if (meshLod == PartMeshLod::Low)
                ++g_renderCullStats.partsShapeLod;
            drawVao = sm->vao;
            vertCount = sm->vertexCount;
        } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
            drawVao = g_partMeshes.wedge.vao;
            vertCount = g_partMeshes.wedge.vertexCount;
        }
        PartMaterial drawMat = p.material;
        if (p.transparency > 0.05f && drawMat != PartMaterial::ForceField
            && drawMat != PartMaterial::Neon)
            drawMat = PartMaterial::Glass;
        glm::vec3 drawCol = p.color;
        if (p.transparency > 0.05f)
            drawCol = glm::mix(drawCol, glm::vec3(0.85f, 0.9f, 1.f), p.transparency * 0.35f);
        if (g_viewCollision && !p.canCollide)
            drawCol = glm::mix(drawCol, glm::vec3(1.f, 0.2f, 0.2f), 0.55f);
        if (g_viewWireframe)
            drawCol *= 0.55f;
        drawCube(drawVao, useStud ? studP : partP, m, view, proj, drawCol, sun, useStud, studId,
                 vertCount, p.effectiveEmissive(), drawMat);
        ++g_renderCullStats.partsDrawn;
    }
}

static void passStudioGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                             const glm::mat4& proj, const glm::vec3& sun) {
    drawSelectedPartBlueOutline(vao, partP, view, proj);
    drawScaleHandles(vao, partP, view, proj, sun);
    drawMoveHandles(vao, partP, view, proj, sun);
    drawRotateHandles(vao, partP, view, proj, sun);
}

static void passWorkspaceModels(GLuint meshP, GLuint skinP, const glm::mat4& view,
                                const glm::mat4& proj, const glm::vec3& sun) {
    drawWorkspaceModels(meshP, skinP, view, proj, sun);
}

static void passAvatars(GLuint vao, GLuint partP, GLuint meshP, GLuint skinP,
                        AnimatedModel* avatarGlb, bool useGlb,
                        const glm::mat4& view, const glm::mat4& proj,
                        const glm::vec3& sun) {
    bool drawGlb = useGlb && avatarGlb && avatarGlb->loaded
                && g_player.humanoid.jumpArmBlend < 0.15f
                && !g_walking
                && !(g_firstPerson && (g_screen == AppScreen::Playing
                    || (g_screen == AppScreen::Studio && g_studioTesting)));
    GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
    BlockDrawContext bctx{ playerVao, partP, view, proj, sun };

    if (drawGlb) {
        avatarGlb->update(0.f);
        glm::mat4 fm = avatarGlb->placement(5.5f, g_playerPos.y,
            { g_playerPos.x, 0.f, g_playerPos.z });
        fm = glm::translate(glm::mat4(1.f), g_playerPos)
           * glm::rotate(glm::mat4(1.f), glm::radians(g_playerYaw + 90.f), {0,1,0})
           * glm::translate(glm::mat4(1.f), -g_playerPos)
           * fm;
        glUseProgram(skinP);
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uModel"),1,GL_FALSE,glm::value_ptr(fm));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skinP,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(skinP,"uSun"),1,glm::value_ptr(sun));
        avatarGlb->draw(skinP);
        // Remoti anche con avatar GLB locale (prima sparivano).
        if (g_remoteBodies.size() < g_net.remotes.size())
            g_remoteBodies.resize(g_net.remotes.size());
        for (size_t i = 0; i < g_net.remotes.size(); ++i) {
            const auto& r = g_net.remotes[i];
            if (i < g_remoteBodies.size())
                g_remoteBodies[i].draw(bctx, drawCubePlayer, r.displayPos, r.displayYaw);
        }
    } else {
        const bool hideLocalFp = g_firstPerson
            && (g_screen == AppScreen::Playing
                || (g_screen == AppScreen::Studio && g_studioTesting))
            && !g_ragdoll.active;
        if (g_ragdoll.active) {
            g_ragdoll.draw(drawCubePlayer, playerVao, partP, view, proj, sun);
        } else if (!hideLocalFp) {
            bool hideBody[(int)AvatarSlot::Count] = {};
            fillBodyHideMask(g_catalog, hideBody);
            float armHold = (effectiveHeldSlot() >= 0) ? kEquippedArmHoldDeg : 0.f;
            const BackpackItem* heldIt = nullptr;
            {
                const int hs = effectiveHeldSlot();
                if (hs >= 0 && hs < (int)g_world.backpack.size())
                    heldIt = &g_world.backpack[hs];
            }
            const std::vector<HoldPartPose>* holds =
                (heldIt && !heldIt->holdPose.empty()) ? &heldIt->holdPose : nullptr;
            if (holds && findHoldPose(*holds, "Right Arm"))
                armHold = 0.f;
            g_player.draw(bctx, drawCubePlayer, g_playerPos, g_playerYaw, hideBody, armHold, holds);
            drawAvatarAccessories(meshP, view, proj, sun,
                g_playerPos, g_playerYaw, g_player.humanoid, vao, partP);
            drawEquippedSword(vao, partP, view, proj, sun);
            drawEquippedTorch(vao, partP, view, proj, sun);
            drawEquippedItemMesh(vao, partP, meshP, view, proj, sun);
        } else {
            drawEquippedSword(vao, partP, view, proj, sun);
            drawEquippedTorch(vao, partP, view, proj, sun);
            drawEquippedItemMesh(vao, partP, meshP, view, proj, sun);
        }
        if (g_remoteBodies.size() < g_net.remotes.size())
            g_remoteBodies.resize(g_net.remotes.size());
        for (size_t i = 0; i < g_net.remotes.size(); ++i) {
            const auto& r = g_net.remotes[i];
            if (i < g_remoteBodies.size())
                g_remoteBodies[i].draw(bctx, drawCubePlayer, r.displayPos, r.displayYaw);
        }
    }
    drawFriendPlayers(vao, partP, view, proj, sun);
}

static void renderWorld3D(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                          GLuint skinP, GLuint meshP, AnimatedModel* avatarGlb, bool useGlb,
                          const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    passPrepare(view, proj);
    passShadows(vao, meshP);
    passTerrain(vao, partP, view, proj, sun);
    passOpaqueParts(vao, studP, partP, studId, view, proj, sun);
    passStudioGizmos(vao, partP, view, proj, sun);
    passWorkspaceModels(meshP, skinP, view, proj, sun);
    passAvatars(vao, partP, meshP, skinP, avatarGlb, useGlb, view, proj, sun);
}
