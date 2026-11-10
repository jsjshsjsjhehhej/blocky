// Pipeline di rendering del mondo 3D: overlay del gizmo, handle di scale/
// move/rotate, accessori avatar, shadow depth pass, frustum culling e
// renderWorld3D (il disegno principale della scena). Non e' un header
// riusabile: usa direttamente le globali di main.cpp (g_world, g_cam,
// g_lighting, ecc.). Compila solo se incluso da main.cpp esattamente in
// questo punto.

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
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            glm::vec3 hp = handleWorldPos(p, axis, s);
            bool hot = (g_scaleDragging && g_scaleAxis == axis && g_scaleSign == s);
            float hs = hot ? kHandleSize * 1.45f : kHandleSize;
            glm::vec3 col = hot ? glm::mix(kGizmoRGB[axis], glm::vec3(1.f), 0.35f) : kGizmoRGB[axis];
            glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs));
            drawCube(vao, partP, m, view, proj, col, sun, false, 0);
            glm::mat4 m2 = glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs * 1.2f));
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
    if (!g_lighting.globalShadows || g_engine.quality.shadowOccluders <= 0) return;
    if (!g_shadowMap.ready) return;

    glm::vec3 focus = g_renderCamPos;
    if (!g_shadowMap.begin(g_lighting.toSkyLight(), focus)) return;
    g_sunShadowOn = true;

    const float reach = g_shadowMap.extent + 24.f;
    const float reach2 = reach * reach;

    for (const auto& p : g_world.parts) {
        if (p.isTransparent()) continue;
        glm::vec3 d = p.position - focus;
        if (glm::dot(d, d) > reach2) continue;
        GLuint dv = vao;
        int vc = 36;
        if (p.shape == PartShape::Ball && g_partMeshes.ball.vao) {
            dv = g_partMeshes.ball.vao;
            vc = g_partMeshes.ball.vertexCount;
        } else if (p.shape == PartShape::Cylinder && g_partMeshes.cylinder.vao) {
            dv = g_partMeshes.cylinder.vao;
            vc = g_partMeshes.cylinder.vertexCount;
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

struct FrustumPlanes { glm::vec4 p[6]; };

static FrustumPlanes extractFrustum(const glm::mat4& viewProj) {
    FrustumPlanes f;
    glm::vec4 r0{viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]};
    glm::vec4 r1{viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]};
    glm::vec4 r2{viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]};
    glm::vec4 r3{viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]};
    f.p[0] = r3 + r0; f.p[1] = r3 - r0;
    f.p[2] = r3 + r1; f.p[3] = r3 - r1;
    f.p[4] = r3 + r2; f.p[5] = r3 - r2;
    for (auto& pl : f.p) {
        float len = glm::length(glm::vec3(pl));
        if (len > 1e-6f) pl /= len;
    }
    return f;
}

static bool sphereOutsideFrustum(const FrustumPlanes& f, glm::vec3 center, float radius) {
    for (const auto& pl : f.p)
        if (glm::dot(glm::vec3(pl), center) + pl.w < -radius) return true;
    return false;
}

static void renderWorld3D(GLuint vao, GLuint studP, GLuint partP, GLuint studId,
                          GLuint skinP, GLuint meshP, AnimatedModel* avatarGlb, bool useGlb,
                          const glm::mat4& view, const glm::mat4& proj, const glm::vec3& sun) {
    g_lastView = view;
    g_lastProj = proj;
    g_renderCamPos = (g_screen == AppScreen::Studio && g_mode == CamMode::Studio && !g_studioTesting)
        ? g_cam.pos : g_playerPos + glm::vec3(0, 5, 0);
    collectDynamicLights();
    collectShadowOccluders();
    renderShadowDepth(vao, meshP);
    {
        glm::vec3 origin = g_renderCamPos;
        float maxD = g_engine.quality.maxDrawDistance;
        int maxN = g_engine.quality.maxVisibleParts;
        float minPx = 0.f;
        bool studsOn = g_engine.quality.studTextures;
        if (antilag::engine().enabled()) {
            const antilag::FramePlan& ap = antilag::engine().plan();
            maxD = std::min(maxD, ap.maxDrawDistance);
            maxN = std::min(maxN, ap.maxVisibleParts);
            minPx = ap.minScreenPx;
            studsOn = studsOn && ap.studTextures;
        }
        const FrustumPlanes frustum = extractFrustum(proj * view);
        struct Cand { int i; float d2; bool transparent; };
        // static: riusato ogni frame invece di allocare/liberare da capo —
        // era l'unica allocazione per-frame rimasta nel percorso di
        // rendering principale. reserve() qui sotto costa solo se la
        // capacita' attuale non basta (in pratica: mai, dopo il primo frame).
        static std::vector<Cand> cands;
        cands.clear();
        cands.reserve(g_world.parts.size());
        if (antilag::engine().enabled())
            antilag::engine().beginPartDistances();
        for (int i = 0; i < (int)g_world.parts.size(); ++i) {
            const auto& part = g_world.parts[i];
            glm::vec3 d = part.position - origin;
            float d2 = glm::dot(d, d);
            if (antilag::engine().enabled())
                antilag::engine().notePartDistance(std::sqrt(d2));
            if (d2 > maxD * maxD) continue;
            float boundRadius = glm::length(part.size) * 0.5f;
            if (sphereOutsideFrustum(frustum, part.position, boundRadius)) continue;
            if (minPx > 0.f && !part.pointLight && !part.isNeon() && part.script.empty()) {
                float px = antilag::screenRadiusPx(
                    {origin.x, origin.y, origin.z}, glm::radians(g_cam.fov),
                    (float)std::max(g_renderH, 1),
                    {part.position.x, part.position.y, part.position.z}, boundRadius);
                if (px < minPx) continue;
            }
            cands.push_back({i, d2, part.isTransparent()});
        }
        if (antilag::engine().enabled() && antilag::engine().level() >= 4 && (int)cands.size() > 12) {
            antilag::Camera oc;
            oc.eye = {origin.x, origin.y, origin.z};
            oc.fovyRad = glm::radians(g_cam.fov);
            oc.viewportH = (float)std::max(g_renderH, 1);
            oc.setViewProj(antilag::fromColumnMajor(glm::value_ptr(proj * view)));
            static std::vector<antilag::OccludeIn> occItems;
            static std::vector<char> occHidden;
            occItems.resize(cands.size());
            occHidden.assign(cands.size(), 0);
            for (size_t n = 0; n < cands.size(); ++n) {
                const auto& part = g_world.parts[(size_t)cands[n].i];
                antilag::OccludeIn& it = occItems[n];
                it.x = part.position.x;
                it.y = part.position.y;
                it.z = part.position.z;
                it.radius = glm::length(part.size) * 0.5f;
                it.screenPx = antilag::screenRadiusPx(
                    oc.eye, oc.fovyRad, oc.viewportH, {it.x, it.y, it.z}, it.radius);
                it.opaque = !part.isTransparent();
                it.important = part.pointLight || part.isNeon() || !part.anchored || !part.script.empty();
                it.index = cands[n].i;
            }
            antilag::markOccluded(oc, occItems.data(), (int)occItems.size(),
                                  occHidden.data(), (int)occHidden.size());
            size_t kept = 0;
            for (size_t n = 0; n < cands.size(); ++n) {
                if (!occHidden[n]) cands[kept++] = cands[n];
            }
            cands.resize(kept);
        }
        if ((int)cands.size() > maxN) {
            std::nth_element(cands.begin(), cands.begin() + maxN, cands.end(),
                [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            cands.resize(maxN);
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            if (a.transparent != b.transparent) return !a.transparent && b.transparent;
            if (a.transparent) return a.d2 < b.d2;
            return a.d2 > b.d2;
        });
        for (const auto& c : cands) {
            auto& p = g_world.parts[c.i];
            glm::mat4 m = partModelMatrix(p);
            bool useStud = p.useStud && studsOn
                && !p.isNeon() && p.material == PartMaterial::Plastic
                && p.shape == PartShape::Block;
            GLuint drawVao = vao;
            int vertCount = 36;
            if (p.shape == PartShape::Ball && g_partMeshes.ball.vao) {
                drawVao = g_partMeshes.ball.vao;
                vertCount = g_partMeshes.ball.vertexCount;
            } else if (p.shape == PartShape::Cylinder && g_partMeshes.cylinder.vao) {
                drawVao = g_partMeshes.cylinder.vao;
                vertCount = g_partMeshes.cylinder.vertexCount;
            } else if (p.shape == PartShape::Wedge && g_partMeshes.wedge.vao) {
                drawVao = g_partMeshes.wedge.vao;
                vertCount = g_partMeshes.wedge.vertexCount;
            }
            drawCube(drawVao, useStud ? studP : partP, m, view, proj, p.color, sun, useStud, studId,
                     vertCount, p.effectiveEmissive(), p.material);
        }
    }
    drawSelectedPartBlueOutline(vao, partP, view, proj);
    drawScaleHandles(vao, partP, view, proj, sun);
    drawMoveHandles(vao, partP, view, proj, sun);
    drawRotateHandles(vao, partP, view, proj, sun);
    drawWorkspaceModels(meshP, skinP, view, proj, sun);

    bool drawGlb = useGlb && avatarGlb && avatarGlb->loaded
                && g_player.humanoid.jumpArmBlend < 0.15f
                && !g_walking
                && !(g_firstPerson && (g_screen == AppScreen::Playing
                    || (g_screen == AppScreen::Studio && g_studioTesting)));
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
    } else {
        GLuint playerVao = g_bevelCube.vao ? g_bevelCube.vao : vao;
        BlockDrawContext bctx{ playerVao, partP, view, proj, sun };
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
            g_player.draw(bctx, drawCubePlayer, g_playerPos, g_playerYaw, hideBody, armHold);
            drawAvatarAccessories(meshP, view, proj, sun,
                g_playerPos, g_playerYaw, g_player.humanoid, vao, partP);
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
