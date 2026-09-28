// Sistema di manipolazione (gizmo): move/scale/rotate delle part, gizmo
// dell'accessorio nel catalogo avatar, ray-picking di part/modelli/scena.
// Non e' un header riusabile: usa direttamente le globali di main.cpp
// (g_scaleDragging, g_moveDragging, g_rotateDragging, g_catalog, ecc.).
// Compila solo se incluso da main.cpp esattamente in questo punto.

static bool rayPlane(const glm::vec3& o, const glm::vec3& d,
                     const glm::vec3& planeP, const glm::vec3& planeN, float& tOut) {
    float denom = glm::dot(planeN, d);
    if (std::fabs(denom) < 1e-6f) return false;
    float t = glm::dot(planeN, planeP - o) / denom;
    if (t < 0.f) return false;
    tOut = t;
    return true;
}

static float angleAroundAxis(const glm::vec3& center, const glm::vec3& axisN,
                             const glm::vec3& point) {
    glm::vec3 a = glm::normalize(axisN);
    glm::vec3 v = point - center;
    v -= a * glm::dot(v, a);
    if (glm::length(v) < 1e-5f) return 0.f;
    glm::vec3 ref = glm::abs(a.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 t = glm::normalize(glm::cross(a, ref));
    glm::vec3 b = glm::cross(a, t);
    return std::atan2(glm::dot(v, b), glm::dot(v, t));
}

static float rayDistToPoint(const glm::vec3& o, const glm::vec3& d, const glm::vec3& p) {
    glm::vec3 w = p - o;
    float tt = glm::dot(w, d);
    if (tt < 0.f) return 1e9f;
    return glm::length(w - d * tt);
}

static glm::vec3 partAxisDir(const WorkspacePart& p, int axis) {
    // partRotationMatrix compone R = Ry * Rx * Rz (Y piu' esterna, Z piu'
    // interna). L'asse "vero" attorno a cui gira ciascun componente Euler
    // e' quello ottenuto applicando SOLO le rotazioni piu' esterne di lui
    // nella composizione — non l'intera R, altrimenti appena un altro asse
    // e' gia' ruotato la maniglia punta (e trascina) nel verso sbagliato.
    if (axis == 1) return {0.f, 1.f, 0.f}; // Y: piu' esterna, sempre fissa
    if (axis == 0) { // X: solo Y le sta "fuori"
        glm::mat4 ry = glm::rotate(glm::mat4(1.f), glm::radians(p.rotation.y), {0, 1, 0});
        return glm::normalize(glm::vec3(ry * glm::vec4(1, 0, 0, 0)));
    }
    // Z: piu' interna — l'intera R va bene (Rz sul proprio asse locale e' identita').
    return glm::normalize(glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(0, 0, 1, 0)));
}

static float gizmoRingRadius(const WorkspacePart& p) {
    float m = std::max(p.size.x, std::max(p.size.y, p.size.z)) * 0.5f;
    return std::clamp(m + 2.4f, 3.2f, 14.f);
}

static float gizmoArrowLen(const WorkspacePart& p) {
    float m = std::max(p.size.x, std::max(p.size.y, p.size.z)) * 0.5f;
    return std::clamp(m + 2.8f, 3.5f, 12.f);
}

static glm::mat4 orientAlongZ(glm::vec3 origin, glm::vec3 dir, glm::vec3 scl) {
    dir = glm::normalize(dir);
    glm::vec3 up = (std::fabs(dir.y) < 0.92f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(up, dir));
    up = glm::normalize(glm::cross(dir, right));
    glm::mat4 m(1.f);
    m[0] = glm::vec4(right * scl.x, 0.f);
    m[1] = glm::vec4(up * scl.y, 0.f);
    m[2] = glm::vec4(dir * scl.z, 0.f);
    m[3] = glm::vec4(origin, 1.f);
    return m;
}

static glm::vec3 handleWorldPos(const WorkspacePart& p, int axis, int sign) {
    glm::vec3 local(0);
    local[axis] = sign * (p.size[axis] * 0.5f + kHandleSize * 0.65f);
    return p.position + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(local, 0.f));
}

static glm::vec3 moveHandleTip(const WorkspacePart& p, int axis) {
    return p.position + partAxisDir(p, axis) * gizmoArrowLen(p);
}


static WorkspacePart g_modelGizmoProxy;

static bool gizmoOnModel() {
    return g_selectedPart < 0 && g_selectedModel >= 0
        && g_selectedModel < (int)g_world.models.size();
}

static glm::vec3 modelBaseExtent(int idx) {
    glm::vec3 ext{2.f};
    if (idx >= 0 && idx < (int)g_wsMeshes.size() && g_wsMeshes[idx].loaded)
        ext = glm::abs(g_wsMeshes[idx].boundsMax - g_wsMeshes[idx].boundsMin);
    return glm::max(ext, glm::vec3(0.05f));
}

static WorkspacePart* gizmoPart() {
    if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
        return &g_world.parts[g_selectedPart];
    if (!gizmoOnModel()) return nullptr;
    const auto& m = g_world.models[g_selectedModel];
    g_modelGizmoProxy.position = m.position;
    g_modelGizmoProxy.rotation = m.rotation;
    g_modelGizmoProxy.size = modelBaseExtent(g_selectedModel) * m.scale;
    return &g_modelGizmoProxy;
}

static float gizmoMinSize() { return gizmoOnModel() ? 0.02f : 0.5f; }
static bool gizmoSnapSize() { return !gizmoOnModel(); }

static void gizmoCommitToModel() {
    if (!gizmoOnModel()) return;
    auto& m = g_world.models[g_selectedModel];
    m.position = g_modelGizmoProxy.position;
    m.rotation = g_modelGizmoProxy.rotation;
    m.scale = glm::max(g_modelGizmoProxy.size / modelBaseExtent(g_selectedModel),
                       glm::vec3(0.01f));
    m.anim.reseedIfStatic(m.position, m.rotation, m.scale);
    markPlaceDirty();
}

static bool gizmoShift() {
    return glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
        || glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}
static bool gizmoCtrl() {
    return glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
        || glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
}

static float snapValue(float v, float step) {
    if (step <= 1e-6f) return v;
    return std::round(v / step) * step;
}

static void catalogAnimParams(float& bob, float& armL, float& armR, float& legL, float& legR) {
    const auto& hum = g_player.humanoid;
    bob = humanoidBob(hum);
    float armBase = humanoidArmSwing(hum);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(hum, jL, jR, jl, jr);
    const float jb = hum.jumpArmBlend;
    armL = armBase * (1.f - jb) + jL;
    armR = -armBase * (1.f - jb) + jR;
    legL = humanoidLegSwing(hum) * (1.f - jb) + jl;
    legR = -humanoidLegSwing(hum) * (1.f - jb) + jr;
}

static glm::mat4 catalogRootMat() {
    return glm::rotate(glm::translate(glm::mat4(1.f), glm::vec3(0)),
                       glm::radians(kCatGizmoYaw), glm::vec3(0, 1, 0));
}

static bool catalogSelectedAcc(AvatarAccessory*& out) {
    if (g_catalog.selected < 0 || g_catalog.selected >= (int)g_catalog.items.size())
        return false;
    out = &g_catalog.items[g_catalog.selected];
    return true;
}

static glm::vec3 catalogAccWorldOrigin(const AvatarAccessory& a) {
    float bob, armL, armR, legL, legR;
    catalogAnimParams(bob, armL, armR, legL, legR);
    glm::mat4 partW = catalogRootMat()
        * accessoryPartMatrix(a, g_player.humanoid, bob, armL, armR, legL, legR);
    return glm::vec3(partW * glm::vec4(a.offset, 1.f));
}

static float catalogGizmoSize() { return 2.8f; }

static bool pickCatalogMoveAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize();
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 tip = origin + axisV * len;
        float t = 0.f;
        if (rayHitAabb(o, d, tip, glm::vec3(0.45f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
        for (int k = 1; k <= 5; ++k) {
            glm::vec3 pt = origin + axisV * (len * (float)k / 6.f);
            if (rayHitAabb(o, d, pt, glm::vec3(0.22f), t) && t < bestT) {
                bestT = t; outAxis = axis; hit = true;
            }
        }
    }
    return hit;
}

static bool pickCatalogRotateAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float R = catalogGizmoSize() * 0.95f;
    float best = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
        glm::vec3 v = glm::normalize(glm::cross(axisV, u));
        float err = 1e9f;
        for (int i = 0; i < 64; ++i) {
            float a = (float)i / 64.f * 6.2831853f;
            glm::vec3 p = origin + (u * std::cos(a) + v * std::sin(a)) * R;
            err = std::min(err, rayDistToPoint(o, d, p));
        }
        if (err < 0.45f && err < best) { best = err; outAxis = axis; hit = true; }
    }
    return hit;
}

static bool pickCatalogRotateFree(float mx, float my) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float R = catalogGizmoSize() * 1.05f;
    return rayDistToPoint(o, d, origin) < R;
}

static bool pickCatalogScaleAxis(float mx, float my, int& outAxis) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return false;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize() * 0.75f;
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 hp = origin + axisV * len;
        float t = 0.f;
        if (rayHitAabb(o, d, hp, glm::vec3(0.4f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
    }
    return hit;
}

static void beginCatalogGizmo(int axis, float mx, float my) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    float bob, armL, armR, legL, legR;
    catalogAnimParams(bob, armL, armR, legL, legR);
    g_accGizmoPartWorld = catalogRootMat()
        * accessoryPartMatrix(*ap, g_player.humanoid, bob, armL, armR, legL, legR);
    g_accGizmoDrag = true;
    g_accGizmoAxis = axis;
    g_accGizmoStartOff = ap->offset;
    g_accGizmoStartRot = ap->rotation;
    g_accGizmoStartScl = ap->scale;
    g_accGizmoStartMx = mx;
    g_accGizmoStartMy = my;
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    if (g_catalog.tool == AccelTool::Rotate) {
        g_accGizmoStartAngle = 0.f;
    } else {
        glm::vec3 axisV(0); axisV[axis] = 1.f;
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (rayPlane(o, d, origin, planeN, t))
            g_accGizmoStartHit = o + d * t;
        else
            g_accGizmoStartHit = origin;
    }
}

static void updateCatalogGizmo(float mx, float my) {
    if (!g_accGizmoDrag) return;
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    glm::vec3 origin0 = glm::vec3(g_accGizmoPartWorld * glm::vec4(g_accGizmoStartOff, 1.f));
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV(0);
    if (g_accGizmoAxis >= 0 && g_accGizmoAxis < 3)
        axisV[g_accGizmoAxis] = 1.f;
    float t = 0.f;

    if (g_catalog.tool == AccelTool::Move) {
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, origin0, planeN, t)) return;
        float delta = glm::dot((o + d * t) - g_accGizmoStartHit, axisV);
        if (!gizmoCtrl()) delta = snapValue(delta, gizmoShift() ? 0.05f : 0.1f);
        glm::vec3 newWorld = origin0 + axisV * delta;
        glm::vec3 local = glm::vec3(glm::inverse(g_accGizmoPartWorld) * glm::vec4(newWorld, 1.f));
        ap->offset = local;
    } else if (g_catalog.tool == AccelTool::Rotate) {
        float dx = mx - g_accGizmoStartMx;
        float dy = my - g_accGizmoStartMy;
        float sens = gizmoShift() ? 0.18f : 0.45f;
        float snap = gizmoShift() ? 1.f : 5.f;
        ap->rotation = g_accGizmoStartRot;
        if (g_accGizmoAxis < 0) {
            float dYaw = dx * sens;
            float dPitch = dy * sens;
            if (!gizmoCtrl()) {
                dYaw = snapValue(dYaw, snap);
                dPitch = snapValue(dPitch, snap);
            }
            ap->rotation.y = g_accGizmoStartRot.y + dYaw;
            ap->rotation.x = g_accGizmoStartRot.x + dPitch;
        } else {
            float deltaDeg = (g_accGizmoAxis == 0 ? dy : dx) * sens;
            if (!gizmoCtrl()) deltaDeg = snapValue(deltaDeg, snap);
            ap->rotation[g_accGizmoAxis] = g_accGizmoStartRot[g_accGizmoAxis] + deltaDeg;
        }
    } else {
        glm::vec3 camF = glm::normalize(glm::vec3(glm::inverse(g_lastView)[2]));
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, origin0, planeN, t)) return;
        float delta = glm::dot((o + d * t) - g_accGizmoStartHit, axisV);
        float mul = 1.f + delta * 0.35f;
        mul = std::clamp(mul, 0.15f, 6.f);
        if (!gizmoCtrl()) mul = snapValue(mul, gizmoShift() ? 0.05f : 0.1f);
        ap->scale = g_accGizmoStartScl;
        ap->scale[g_accGizmoAxis] = std::clamp(g_accGizmoStartScl[g_accGizmoAxis] * mul, 0.05f, 8.f);
    }
}

static void endCatalogGizmo() {
    if (g_accGizmoDrag)
        saveEquippedAvatar(g_catalog);
    g_accGizmoDrag = false;
}

static void drawCatalogGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun) {
    AvatarAccessory* ap = nullptr;
    if (!catalogSelectedAcc(ap)) return;
    glm::vec3 origin = catalogAccWorldOrigin(*ap);
    float len = catalogGizmoSize();
    const glm::vec3 cols[3] = {
        {0.95f, 0.22f, 0.20f}, {0.22f, 0.88f, 0.32f}, {0.28f, 0.52f, 1.00f}
    };

    drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), origin), glm::vec3(0.28f)),
             view, proj, glm::vec3(0.95f), sun, false, 0);

    if (g_catalog.tool == AccelTool::Move) {
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && g_accGizmoAxis == axis;
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), 0.4f) : cols[axis];
            glm::vec3 dir(0); dir[axis] = 1.f;
            float stemLen = len * 0.78f;
            glm::vec3 mid = origin + dir * (stemLen * 0.5f);
            float thick = hot ? 0.14f : 0.10f;
            drawCube(vao, partP, orientAlongZ(mid, dir, {thick, thick, stemLen}),
                     view, proj, col * 0.92f, sun, false, 0);
            glm::vec3 tip = origin + dir * len;
            float tipS = hot ? 0.48f : 0.38f;
            drawCube(vao, partP, orientAlongZ(tip - dir * (tipS * 0.3f), dir, {tipS, tipS, tipS * 1.3f}),
                     view, proj, col, sun, false, 0);
        }
    } else if (g_catalog.tool == AccelTool::Rotate) {
        float R = len * 0.95f;
        const int segs = 48;
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && (g_accGizmoAxis == axis || g_accGizmoAxis < 0);
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), g_accGizmoAxis < 0 ? 0.25f : 0.45f)
                                : cols[axis];
            float thick = (hot && g_accGizmoAxis == axis) ? 0.18f : 0.12f;
            glm::vec3 axisV(0); axisV[axis] = 1.f;
            glm::vec3 ref = (std::fabs(axisV.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::vec3 u = glm::normalize(glm::cross(axisV, ref));
            glm::vec3 v = glm::normalize(glm::cross(axisV, u));
            for (int i = 0; i < segs; ++i) {
                float a = (float)i / (float)segs * 6.2831853f;
                glm::vec3 pos = origin + (u * std::cos(a) + v * std::sin(a)) * R;
                drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(thick)),
                         view, proj, col, sun, false, 0);
            }
            for (int tick = 0; tick < 24; ++tick) {
                float a = (float)tick / 24.f * 6.2831853f;
                glm::vec3 pos = origin + (u * std::cos(a) + v * std::sin(a)) * (R + 0.22f);
                float ts = (tick % 6 == 0) ? 0.16f : 0.09f;
                drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), pos), glm::vec3(ts)),
                         view, proj, col * 0.75f, sun, false, 0);
            }
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) {
            bool hot = g_accGizmoDrag && g_accGizmoAxis == axis;
            glm::vec3 col = hot ? glm::mix(cols[axis], glm::vec3(1.f), 0.4f) : cols[axis];
            glm::vec3 dir(0); dir[axis] = 1.f;
            glm::vec3 hp = origin + dir * (len * 0.75f);
            float hs = hot ? 0.48f : 0.36f;
            drawCube(vao, partP, glm::scale(glm::translate(glm::mat4(1.f), hp), glm::vec3(hs)),
                     view, proj, col, sun, false, 0);
            glm::vec3 mid = origin + dir * (len * 0.38f);
            drawCube(vao, partP, orientAlongZ(mid, dir, {0.08f, 0.08f, len * 0.7f}),
                     view, proj, col * 0.7f, sun, false, 0);
        }
    }
}

static bool pickScaleHandle(float mx, float my, int& outAxis, int& outSign) {
    if (g_tool != StudioTool::Scale) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestT = 1e9f;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            float t = 0.f;
            glm::vec3 hp = handleWorldPos(p, axis, s);
            if (rayHitAabb(o, d, hp, glm::vec3(kHandleSize * 1.15f), t) && t < bestT) {
                bestT = t;
                outAxis = axis;
                outSign = s;
                hit = true;
            }
        }
    }
    return hit;
}

static bool pickMoveHandle(float mx, float my, int& outAxis) {
    if (g_tool != StudioTool::Move) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestT = 1e9f;
    bool hit = false;
    float len = gizmoArrowLen(p);
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 tip = moveHandleTip(p, axis);
        float t = 0.f;
        if (rayHitAabb(o, d, tip, glm::vec3(0.55f), t) && t < bestT) {
            bestT = t; outAxis = axis; hit = true;
        }
        glm::vec3 dir = partAxisDir(p, axis);
        for (int k = 1; k <= 6; ++k) {
            glm::vec3 pt = p.position + dir * (len * (float)k / 7.f);
            if (rayHitAabb(o, d, pt, glm::vec3(0.28f), t) && t < bestT) {
                bestT = t; outAxis = axis; hit = true;
            }
        }
    }
    return hit;
}

static bool pickRotateHandle(float mx, float my, int& outAxis) {
    if (g_tool != StudioTool::Rotate) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float bestScore = 1e9f;
    bool hit = false;
    float R = gizmoRingRadius(p);
    const float band = 0.55f;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV = partAxisDir(p, axis);
        float t = 0.f;
        if (!rayPlane(o, d, p.position, axisV, t)) continue;
        glm::vec3 hitP = o + d * t;
        glm::vec3 radial = hitP - p.position;
        radial -= axisV * glm::dot(radial, axisV);
        float dist = glm::length(radial);
        float err = std::fabs(dist - R);
        if (err < band && err < bestScore) {
            bestScore = err;
            outAxis = axis;
            hit = true;
        }
    }
    g_gizmoHoverAxis = hit ? outAxis : -1;
    return hit;
}

static void beginScaleDrag(int axis, int sign) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_scaleDragging = true;
    g_moveDragging = false;
    g_rotateDragging = false;
    g_scaleAxis = axis;
    g_scaleSign = sign;
    g_scaleStartPos = p.position;
    g_scaleStartSize = p.size;
    g_scaleStartFace = p.position[axis] + sign * p.size[axis] * 0.5f;
    g_scaleLiveSize = p.size[axis];
}

static void updateScaleDrag(float mx, float my) {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);

    glm::vec3 axisV = partAxisDir(p, g_scaleAxis);
    glm::vec3 camF = g_cam.front();
    glm::vec3 side = glm::cross(axisV, camF);
    if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
    side = glm::normalize(side);
    glm::vec3 planeN = glm::normalize(glm::cross(axisV, side));

    glm::vec3 faceLocal(0);
    faceLocal[g_scaleAxis] = (float)g_scaleSign * g_scaleStartSize[g_scaleAxis] * 0.5f;
    glm::vec3 planeP = g_scaleStartPos + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(faceLocal, 0.f));
    float t = 0.f;
    if (!rayPlane(o, d, planeP, planeN, t)) return;
    glm::vec3 hit = o + d * t;
    float newFace = glm::dot(hit - g_scaleStartPos, axisV);
    float startFace = (float)g_scaleSign * g_scaleStartSize[g_scaleAxis] * 0.5f;
    float expand = (newFace - startFace) * (float)g_scaleSign;
    const float minSize = gizmoMinSize();
    float newSize = std::clamp(g_scaleStartSize[g_scaleAxis] + expand, minSize, 80.f);
    if (!gizmoCtrl() && gizmoSnapSize()) {
        float step = gizmoShift() ? 0.1f : 0.25f;
        newSize = std::clamp(snapValue(newSize, step), minSize, 80.f);
    }
    float applied = newSize - g_scaleStartSize[g_scaleAxis];
    p.size = g_scaleStartSize;
    p.size[g_scaleAxis] = newSize;
    glm::vec3 offsetLocal(0);
    offsetLocal[g_scaleAxis] = (float)g_scaleSign * applied * 0.5f;
    p.position = g_scaleStartPos + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(offsetLocal, 0.f));
    g_scaleLiveSize = newSize;
    gizmoCommitToModel();
}

static void endScaleDrag() {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.1f : 0.25f;
            if (gizmoSnapSize())
                p.size = glm::vec3(snapValue(p.size.x, step), snapValue(p.size.y, step),
                                   snapValue(p.size.z, step));
            p.position = glm::vec3(snapValue(p.position.x, step), snapValue(p.position.y, step),
                                   snapValue(p.position.z, step));
        }
        p.size = glm::clamp(p.size, glm::vec3(gizmoMinSize()), glm::vec3(80.f));
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_scaleDragging = false;
    g_xformTrack = false;
}

static void beginMoveDrag(int axis, float mx, float my) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_moveDragging = true;
    g_scaleDragging = false;
    g_rotateDragging = false;
    g_moveAxis = axis;
    g_moveStartPos = p.position;
    g_moveLiveDelta = 0.f;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    glm::vec3 planeN = (axis == 1) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    if (axis >= 0) {
        glm::vec3 axisV = partAxisDir(p, axis);
        glm::vec3 camF = g_cam.front();
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
    }
    if (rayPlane(o, d, p.position, planeN, t))
        g_moveStartHit = o + d * t;
    else
        g_moveStartHit = p.position;
}

static void updateMoveDrag(float mx, float my) {
    WorkspacePart* pt = g_moveDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;

    if (g_moveAxis >= 0 && g_moveAxis <= 2) {
        glm::vec3 axisV = partAxisDir(p, g_moveAxis);
        glm::vec3 camF = g_cam.front();
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, g_moveStartPos, planeN, t)) return;
        glm::vec3 hit = o + d * t;
        float delta = glm::dot(hit - g_moveStartHit, axisV);
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.05f : 0.25f;
            delta = snapValue(delta, step);
        }
        p.position = g_moveStartPos + axisV * delta;
        g_moveLiveDelta = delta;
        gizmoCommitToModel();
    }
}

static void endMoveDrag() {
    WorkspacePart* pt = g_moveDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (!gizmoCtrl()) {
            float step = gizmoShift() ? 0.05f : 0.25f;
            p.position = glm::vec3(snapValue(p.position.x, step),
                                   snapValue(p.position.y, step),
                                   snapValue(p.position.z, step));
        }
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_moveDragging = false;
    g_moveLiveDelta = 0.f;
    g_xformTrack = false;
}

static void beginRotateDrag(int axis, float mx, float my) {
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    const auto& p = *pt;
    g_xformBefore = p;
    g_xformTrack = true;
    g_rotateDragging = true;
    g_scaleDragging = false;
    g_moveDragging = false;
    g_rotateAxis = axis;
    g_rotateStartRot = p.rotation;
    g_rotateLiveDelta = 0.f;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV = partAxisDir(p, axis);
    float t = 0.f;
    if (rayPlane(o, d, p.position, axisV, t))
        g_rotateStartAngle = angleAroundAxis(p.position, axisV, o + d * t);
    else
        g_rotateStartAngle = 0.f;
}

static void updateRotateDrag(float mx, float my) {
    WorkspacePart* pt = g_rotateDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    glm::vec3 axisV = partAxisDir(p, g_rotateAxis);
    float t = 0.f;
    if (!rayPlane(o, d, p.position, axisV, t)) return;
    float ang = angleAroundAxis(p.position, axisV, o + d * t);
    float deltaDeg = glm::degrees(ang - g_rotateStartAngle);
    if (!gizmoCtrl()) {
        float step = gizmoShift() ? 1.f : 5.f;
        deltaDeg = snapValue(deltaDeg, step);
    }
    p.rotation = g_rotateStartRot;
    p.rotation[g_rotateAxis] = g_rotateStartRot[g_rotateAxis] + deltaDeg;
    g_rotateLiveDelta = deltaDeg;
    gizmoCommitToModel();
}

static void endRotateDrag() {
    WorkspacePart* pt = g_rotateDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        for (int i = 0; i < 3; ++i) {
            while (p.rotation[i] > 180.f) p.rotation[i] -= 360.f;
            while (p.rotation[i] < -180.f) p.rotation[i] += 360.f;
            if (!gizmoCtrl()) {
                float step = gizmoShift() ? 1.f : 5.f;
                p.rotation[i] = snapValue(p.rotation[i], step);
            }
        }
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
    }
    g_rotateDragging = false;
    g_rotateLiveDelta = 0.f;
    g_xformTrack = false;
}

static bool rayHitPart(const glm::vec3& o, const glm::vec3& d, const WorkspacePart& p, float& tOut) {
    glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    glm::mat3 Rt = glm::transpose(R);
    glm::vec3 lo = Rt * (o - p.position);
    glm::vec3 ld = Rt * d;
    return rayHitAabb(lo, ld, glm::vec3(0), p.size, tOut);
}

// Usata dalla camera in terza persona per non attraversare i muri (vedi
// thirdPersonView in CameraAndInput.h) — dirUnit deve essere normalizzato
// cosi' tOut e' una distanza vera in stud, non un parametro 0..1.
static bool cameraRayHitParts(const glm::vec3& o, const glm::vec3& dirUnit, float maxDist, float& tOut) {
    float bestT = maxDist;
    bool hit = false;
    for (const auto& p : g_world.parts) {
        if (!p.canCollide) continue;
        float t = 0.f;
        if (rayHitPart(o, dirUnit, p, t) && t >= 0.f && t < bestT) {
            bestT = t;
            hit = true;
        }
    }
    tOut = bestT;
    return hit;
}

static int pickPartAtMouse(float mx, float my) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    int best = -1;
    float bestT = 1e9f;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        if (rayHitPart(o, d, g_world.parts[i], t) && t < bestT) {
            bestT = t;
            best = i;
        }
    }
    return best;
}

enum class PickKind { None, Part, Model, Label, Spawn, PlayerRoot, PlayerPart };

struct ScenePick {
    PickKind kind = PickKind::None;
    int index = -1;
    float t = 1e9f;
};

static bool rayHitAabbWorld(const glm::vec3& o, const glm::vec3& d,
                            const glm::vec3& center, const glm::vec3& size, float& tOut) {
    return rayHitAabb(o, d, center, size, tOut);
}

static ScenePick pickSceneAtMouse(float mx, float my) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    ScenePick best;

    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        if (rayHitPart(o, d, g_world.parts[i], t) && t < best.t) {
            best = {PickKind::Part, i, t};
        }
    }

    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        const auto& m = g_world.models[i];
        glm::vec3 sz = glm::max(m.scale * 2.f, glm::vec3(1.f));
        float t = 0.f;
        if (rayHitAabbWorld(o, d, m.position, sz, t) && t < best.t)
            best = {PickKind::Model, i, t};
    }

    {
        float t = 0.f;
        glm::vec3 spawnPos = g_world.spawn.position + glm::vec3(0, 1.5f, 0);
        if (rayHitAabbWorld(o, d, spawnPos, {2.f, 3.f, 2.f}, t) && t < best.t)
            best = {PickKind::Spawn, -1, t};
    }

    {
        glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), g_playerPos),
                                     glm::radians(g_playerYaw), {0, 1, 0});
        float tRoot = 0.f;
        if (rayHitAabbWorld(o, d, g_playerPos + glm::vec3(0, 3.f, 0), {2.2f, 6.5f, 2.2f}, tRoot)
            && tRoot < best.t) {
            best = {PickKind::PlayerRoot, -1, tRoot};
            for (int i = 0; i < (int)g_player.humanoid.parts.size(); ++i) {
                const auto& bp = g_player.humanoid.parts[i];
                if (bp.name == "HumanoidRootPart") continue;
                glm::vec3 world = glm::vec3(root * glm::vec4(bp.offset, 1.f));
                float t = 0.f;
                if (rayHitAabbWorld(o, d, world, bp.size * 1.15f, t) && t < best.t)
                    best = {PickKind::PlayerPart, i, t};
            }
        }
    }

    return best;
}

static void applyScenePick(const ScenePick& hit) {
    switch (hit.kind) {
    case PickKind::Part: selectPart(hit.index); break;
    case PickKind::Model: selectModel(hit.index); break;
    case PickKind::Label: selectLabel(hit.index); break;
    case PickKind::Spawn: selectSpawn(); break;
    case PickKind::PlayerRoot: selectPlayer(-1); break;
    case PickKind::PlayerPart: selectPlayer(hit.index); break;
    default: clearAllSelection(); break;
    }
}

