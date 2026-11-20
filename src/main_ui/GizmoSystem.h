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
    // Asse locale X/Y/Z portato in mondo con la stessa R del rendering (Ry*Rx*Rz).
    // Prima Y restava sempre (0,1,0): con rotazione X/Z la scala misurava sul
    // mondo sbagliato e tirando per ingrandire la Part si rimpiccioliva.
    glm::vec3 local(0.f);
    local[axis] = 1.f;
    glm::vec3 w = glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(local, 0.f));
    float len = glm::length(w);
    return len > 1e-6f ? w / len : local;
}

// Spazio del gizmo come Roblox Studio: Mondo (predefinito) o Locale (Ctrl+L).
// La scala resta sempre sugli assi della part.
static glm::vec3 gizmoAxisDir(const WorkspacePart& p, int axis) {
    if (g_studioLocalSpace) return partAxisDir(p, axis);
    glm::vec3 w(0.f);
    w[axis] = 1.f;
    return w;
}
// Asse della rotazione in corso, fissato quando parte il trascinamento.
static glm::vec3 g_rotateAxisW{0.f, 1.f, 0.f};

// Quanti stud sono "px" pixel alla distanza di quel punto: le maniglie restano
// della stessa grandezza sullo schermo, vicine o lontane (come in Roblox Studio).
static float gizmoPxToWorld(const glm::vec3& at, float px) {
    const float dist = std::max(0.02f, glm::length(g_cam.pos - at));
    return px * 2.f * dist * std::tan(glm::radians(g_cam.fov * 0.5f)) / (float)std::max(1, g_h);
}

/** Mezza misura della part (ruotata) lungo una direzione del mondo. */
static float partExtentAlong(const WorkspacePart& p, const glm::vec3& dir) {
    const glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    const glm::vec3 h = p.size * 0.5f;
    return std::fabs(glm::dot(R[0], dir)) * h.x + std::fabs(glm::dot(R[1], dir)) * h.y + std::fabs(glm::dot(R[2], dir)) * h.z;
}

static int g_gizmoHoverSign = 0;      // verso della maniglia sotto il mouse (+1 / -1)
static glm::vec3 g_scaleStartHit{0.f}; // punto preso sull'asse a inizio scala

/** Freccia di Sposta: parte dalla superficie della part e va fuori di 70 px. */
static void moveHandleSeg(const WorkspacePart& p, int axis, int sign, glm::vec3& a, glm::vec3& b) {
    const glm::vec3 dir = gizmoAxisDir(p, axis) * (float)sign;
    a = p.position + dir * (partExtentAlong(p, dir) + gizmoPxToWorld(p.position, 8.f));
    b = a + dir * gizmoPxToWorld(p.position, 70.f);
}

static bool partIndexValid(int i);                 // StudioFeaturesUi.h
static std::vector<int> studioActivePartIndices(); // StudioFeaturesUi.h
static bool gizmoOnModel();                       // piu' sotto

/** Due part (scatole ruotate) si compenetrano? Test degli assi separatori (SAT). */
static bool partsOverlap(const WorkspacePart& a, const WorkspacePart& b, float shrink = 0.02f) {
    const glm::mat3 Ra = glm::mat3(partRotationMatrix(a.rotation)), Rb = glm::mat3(partRotationMatrix(b.rotation));
    const glm::vec3 ea = glm::max(a.size * 0.5f - glm::vec3(shrink), glm::vec3(0.f));
    const glm::vec3 eb = glm::max(b.size * 0.5f - glm::vec3(shrink), glm::vec3(0.f));
    const glm::vec3 T = b.position - a.position;
    auto separated = [&](glm::vec3 L) {
        const float len = glm::length(L);
        if (len < 1e-5f) return false;   // assi paralleli: il test non dice niente
        L /= len;
        const float ra = std::fabs(glm::dot(Ra[0], L)) * ea.x + std::fabs(glm::dot(Ra[1], L)) * ea.y + std::fabs(glm::dot(Ra[2], L)) * ea.z;
        const float rb = std::fabs(glm::dot(Rb[0], L)) * eb.x + std::fabs(glm::dot(Rb[1], L)) * eb.y + std::fabs(glm::dot(Rb[2], L)) * eb.z;
        return std::fabs(glm::dot(T, L)) > ra + rb;
    };
    for (int i = 0; i < 3; ++i) {
        if (separated(Ra[i]) || separated(Rb[i])) return false;
        for (int j = 0; j < 3; ++j)
            if (separated(glm::cross(Ra[i], Rb[j]))) return false;
    }
    return true;
}

/** Spinge la part fuori da quelle che sta gia' attraversando (costruzione, spostamento, scala). */
static bool studioCollisionBlocked(const WorkspacePart& p);
static void studioPushOut(WorkspacePart& p) {
    if (!g_studioCollisions || gizmoOnModel() || !partIndexValid(g_selectedPart)) return;
    std::vector<int> skip = studioActivePartIndices();
    skip.push_back(g_selectedPart);
    for (int pass = 0; pass < 16; ++pass) {
        int other = -1;
        for (int i = 0; i < (int)g_world.parts.size(); ++i) {
            if (std::find(skip.begin(), skip.end(), i) != skip.end()) continue;
            const WorkspacePart& o = g_world.parts[(size_t)i];
            if (!o.canCollide || o.transparency >= 0.99f) continue;
            if (glm::length(o.position - p.position) > glm::length(p.size) + glm::length(o.size)) continue;
            if (partsOverlap(p, o, 0.004f)) { other = i; break; }
        }
        if (other < 0) return;
        const WorkspacePart& o = g_world.parts[(size_t)other];
        const glm::vec3 d = p.position - o.position;
        const bool flat = glm::length(p.rotation) < 0.5f && glm::length(o.rotation) < 0.5f;
        if (flat) {
            const glm::vec3 ha = p.size * 0.5f, hb = o.size * 0.5f;
            int axis = 1;
            float best = 1e9f;
            for (int a = 0; a < 3; ++a) {
                const float pen = (ha[a] + hb[a]) - std::fabs(d[a]);
                if (pen < best) { best = pen; axis = a; }
            }
            const float dir = (std::fabs(d[axis]) < 1e-4f) ? (axis == 1 ? 1.f : 1.f) : (d[axis] >= 0.f ? 1.f : -1.f);
            p.position[axis] += dir * (std::max(best, 0.01f) + 0.02f);
        } else {
            glm::vec3 n = d;
            if (glm::length(n) < 1e-3f) n = glm::vec3(0.f, 1.f, 0.f);
            p.position += glm::normalize(n) * 0.2f;
        }
    }
}

/** Con "Collisioni" acceso: la part selezionata entra dentro un'altra (non selezionata)? */
static bool studioCollisionBlocked(const WorkspacePart& p) {
    if (!g_studioCollisions || gizmoOnModel() || !partIndexValid(g_selectedPart)) return false;
    std::vector<int> skip = studioActivePartIndices();
    skip.push_back(g_selectedPart);
    const float rp = glm::length(p.size) * 0.5f;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        if (std::find(skip.begin(), skip.end(), i) != skip.end()) continue;
        const WorkspacePart& o = g_world.parts[(size_t)i];
        if (!o.canCollide || o.transparency >= 0.99f) continue;
        if (glm::length(o.position - p.position) > rp + glm::length(o.size) * 0.5f) continue;   // lontane
        if (partsOverlap(p, o)) return true;
    }
    return false;
}

/** Distanza in pixel del mouse da un segmento a schermo. */
static float gizmoSegDistPx(float mx, float my, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    float t = l2 > 0.f ? ((mx - ax) * dx + (my - ay) * dy) / l2 : 0.f;
    t = std::clamp(t, 0.f, 1.f);
    const float px = ax + dx * t - mx, py = ay + dy * t - my;
    return std::sqrt(px * px + py * py);
}

static float gizmoRingRadius(const WorkspacePart& p) {
    // Gli anelli abbracciano la part, ma non diventano mai minuscoli a schermo.
    const float around = glm::length(p.size) * 0.5f + gizmoPxToWorld(p.position, 14.f);
    return std::max(around, gizmoPxToWorld(p.position, 70.f));
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

static float scaleHandleDrawSize(const WorkspacePart& p) {
    float m = std::max(p.size.x, std::max(p.size.y, p.size.z));
    return std::clamp(m * 0.12f + 0.55f, 0.7f, 2.2f);
}

static glm::vec3 handleWorldPos(const WorkspacePart& p, int axis, int sign) {
    // Sfera di Ridimensiona: poco fuori dal centro della faccia (assi della part).
    glm::vec3 local(0);
    local[axis] = sign * (p.size[axis] * 0.5f + gizmoPxToWorld(p.position, 20.f));
    return p.position + glm::vec3(partRotationMatrix(p.rotation) * glm::vec4(local, 0.f));
}

static bool gizmoWorldToScreen(const glm::vec3& world, float& outX, float& outY) {
    glm::vec4 clip = g_lastProj * g_lastView * glm::vec4(world, 1.f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.f || ndc.z > 1.f) return false;
    outX = (ndc.x * 0.5f + 0.5f) * (float)g_w;
    outY = (1.f - (ndc.y * 0.5f + 0.5f)) * (float)g_h;
    return true;
}

static glm::vec3 moveHandleTip(const WorkspacePart& p, int axis) {
    glm::vec3 a, b;
    moveHandleSeg(p, axis, 1, a, b);
    return b;
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

// Vero quando l Animation Studio anima la part selezionata: il gizmo dello
// Studio la sposterebbe e l animazione la rimetterebbe subito a posto; li si
// usa il gizmo delle ossa (AnimationStudioUi.h).
static bool g_animStudioGizmoBlock = false;
// Vero mentre l Animation Studio mette le part del rig nella posa: lo Studio
// non deve trattarlo come uno spostamento fatto a mano (follow dei Model, Undo).
static bool g_animStudioDriving = false;

static WorkspacePart* gizmoPart() {
    if (g_animStudioGizmoBlock) return nullptr;
    if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size()) {
        if (g_world.parts[g_selectedPart].locked) return nullptr;
        return &g_world.parts[g_selectedPart];
    }
    if (!gizmoOnModel()) return nullptr;
    const auto& m = g_world.models[g_selectedModel];
    g_modelGizmoProxy.position = m.position;
    g_modelGizmoProxy.rotation = m.rotation;
    g_modelGizmoProxy.size = modelBaseExtent(g_selectedModel) * m.scale;
    return &g_modelGizmoProxy;
}

static float gizmoMinSize() { return kMinPartSize; }
/** Nessun tetto pratico: si puo' arrivare a migliaia di stud (anti NaN). */
static float gizmoMaxSize() { return gizmoOnModel() ? 1.0e5f : 1.0e5f; }
static bool gizmoSnapSize() { return !gizmoOnModel(); }

static void gizmoCommitToModel() {
    if (!gizmoOnModel()) return;
    auto& m = g_world.models[g_selectedModel];
    m.position = g_modelGizmoProxy.position;
    m.rotation = g_modelGizmoProxy.rotation;
    m.scale = glm::max(g_modelGizmoProxy.size / modelBaseExtent(g_selectedModel),
                       glm::vec3(kMinPartSize));
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
    const float pickR = 14.f;   // pixel attorno alla sfera
    float bestD2 = pickR * pickR;
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = -1; s <= 1; s += 2) {
            float sx = 0.f, sy = 0.f;
            if (!gizmoWorldToScreen(handleWorldPos(p, axis, s), sx, sy)) continue;
            const float d2 = (mx - sx) * (mx - sx) + (my - sy) * (my - sy);
            if (d2 < bestD2) { bestD2 = d2; outAxis = axis; outSign = s; hit = true; }
        }
    }
    return hit;
}

static bool pickMoveHandle(float mx, float my, int& outAxis) {
    if (g_tool != StudioTool::Move) return false;
    const WorkspacePart* pt = gizmoPart();
    if (!pt) return false;
    const auto& p = *pt;
    float best = 11.f;   // pixel dalla freccia
    bool hit = false;
    for (int axis = 0; axis < 3; ++axis)
        for (int s = -1; s <= 1; s += 2) {
            glm::vec3 a, b;
            moveHandleSeg(p, axis, s, a, b);
            float ax, ay, bx, by;
            if (!gizmoWorldToScreen(a, ax, ay) || !gizmoWorldToScreen(b, bx, by)) continue;
            const float d = gizmoSegDistPx(mx, my, ax, ay, bx, by);
            if (d < best) { best = d; outAxis = axis; g_gizmoHoverSign = s; hit = true; }
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
    const float band = gizmoPxToWorld(p.position, 9.f);
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisV = gizmoAxisDir(p, axis);
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

static void beginScaleDrag(int axis, int sign, float mx, float my) {
    WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    studioPushOut(*pt);
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
    g_scaleStartRotMat = partRotationMatrix(p.rotation);
    g_scaleStartFace = (float)sign * p.size[axis] * 0.5f;
    g_scaleStartMx = mx;
    g_scaleStartMy = my;
    g_scaleLiveSize = p.size[axis];
    glm::vec3 axisLocal(0.f);
    axisLocal[axis] = 1.f;
    const glm::vec3 axisV = glm::normalize(glm::vec3(g_scaleStartRotMat * glm::vec4(axisLocal, 0.f)));
    glm::vec3 side = glm::cross(axisV, g_cam.front());
    if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
    const glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    g_scaleStartHit = rayPlane(o, d, g_scaleStartPos, planeN, t) ? o + d * t : g_scaleStartPos;
}

static void updateScaleDrag(float mx, float my) {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (!pt) return;
    auto& p = *pt;
    const glm::vec3 prevSize = p.size, prevPos = p.position;
    // Come Roblox Studio: la faccia segue il mouse 1:1 lungo il suo asse.
    // Ctrl = si allargano tutti e due i lati; Shift = in proporzione su tutti gli assi.
    glm::vec3 axisLocal(0.f);
    axisLocal[g_scaleAxis] = 1.f;
    const glm::vec3 axisV = glm::normalize(glm::vec3(g_scaleStartRotMat * glm::vec4(axisLocal, 0.f)));
    glm::vec3 side = glm::cross(axisV, g_cam.front());
    if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
    const glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    float t = 0.f;
    if (!rayPlane(o, d, g_scaleStartPos, planeN, t)) return;
    float delta = glm::dot(o + d * t - g_scaleStartHit, axisV) * (float)g_scaleSign;   // + = verso fuori
    const float step = gizmoSnapSize() ? studioSnapMoveStep() : 0.f;   // Roblox usa il passo di Sposta
    if (step > 0.f) delta = snapValue(delta, step);
    const bool both = gizmoCtrl();
    const bool uniform = gizmoShift();
    const float minSize = gizmoMinSize(), maxSize = gizmoMaxSize();
    const float start = g_scaleStartSize[g_scaleAxis];
    const float newAxis = std::clamp(start + (both ? 2.f * delta : delta), minSize, maxSize);
    if (uniform) {
        const float k = newAxis / std::max(1e-4f, start);
        p.size = glm::clamp(g_scaleStartSize * k, glm::vec3(minSize), glm::vec3(maxSize));
    } else {
        p.size = g_scaleStartSize;
        p.size[g_scaleAxis] = newAxis;
    }
    // Con un lato solo la faccia opposta resta ferma: il centro si sposta di meta'.
    const float applied = p.size[g_scaleAxis] - start;
    glm::vec3 offsetLocal(0);
    if (!both) offsetLocal[g_scaleAxis] = (float)g_scaleSign * applied * 0.5f;
    p.position = g_scaleStartPos + glm::vec3(g_scaleStartRotMat * glm::vec4(offsetLocal, 0.f));
    if (studioCollisionBlocked(p)) {
        glm::vec3 loS = g_scaleStartSize, hiS = p.size;
        glm::vec3 loP = g_scaleStartPos, hiP = p.position;
        for (int k = 0; k < 10; ++k) {
            p.size = (loS + hiS) * 0.5f;
            p.position = (loP + hiP) * 0.5f;
            if (studioCollisionBlocked(p)) { hiS = p.size; hiP = p.position; }
            else { loS = p.size; loP = p.position; }
        }
        p.size = loS;
        p.position = loP;
        if (studioCollisionBlocked(p)) { p.size = prevSize; p.position = prevPos; return; }
    }
    g_scaleLiveSize = p.size[g_scaleAxis];
    gizmoCommitToModel();
}

static void endScaleDrag() {
    WorkspacePart* pt = g_scaleDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (gizmoSnapSize()) {
            float step = studioSnapMoveStep();
            if (step > 0.f) {
                if (gizmoShift()) {
                    for (int a = 0; a < 3; ++a)
                        p.size[a] = snapValue(p.size[a], step);
                } else {
                    p.size[g_scaleAxis] = snapValue(p.size[g_scaleAxis], step);
                }
            }
        }
        p.size = glm::clamp(p.size, glm::vec3(gizmoMinSize()), glm::vec3(gizmoMaxSize()));
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
        if (g_xformTrack)
            markPlaceDirty();
    }
    g_scaleDragging = false;
    g_xformTrack = false;
}

static void beginMoveDrag(int axis, float mx, float my) {
    WorkspacePart* pt = gizmoPart();
    if (!pt) return;
    studioPushOut(*pt);
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
        glm::vec3 axisV = gizmoAxisDir(p, axis);
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
        glm::vec3 axisV = gizmoAxisDir(p, g_moveAxis);
        glm::vec3 camF = g_cam.front();
        glm::vec3 side = glm::cross(axisV, camF);
        if (glm::length(side) < 1e-4f) side = glm::cross(axisV, glm::vec3(0, 1, 0));
        glm::vec3 planeN = glm::normalize(glm::cross(axisV, glm::normalize(side)));
        if (!rayPlane(o, d, g_moveStartPos, planeN, t)) return;
        glm::vec3 hit = o + d * t;
        float delta = glm::dot(hit - g_moveStartHit, axisV);
        if (!gizmoCtrl()) {
            float step = studioSnapMoveStep();
            if (step > 0.f) delta = snapValue(delta, step);
        }
        const glm::vec3 prevPos = p.position;
        const glm::vec3 want = g_moveStartPos + axisV * delta;
        p.position = want;
        if (studioCollisionBlocked(p)) {
            glm::vec3 lo = g_moveStartPos, hi = want;
            for (int k = 0; k < 12; ++k) {
                p.position = (lo + hi) * 0.5f;
                if (studioCollisionBlocked(p)) hi = p.position;
                else lo = p.position;
            }
            p.position = lo;
            if (studioCollisionBlocked(p)) { p.position = prevPos; return; }
        }
        g_moveLiveDelta = delta;
        gizmoCommitToModel();
    }
}

static void endMoveDrag() {
    WorkspacePart* pt = g_moveDragging ? gizmoPart() : nullptr;
    if (pt) {
        auto& p = *pt;
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
        if (g_xformTrack)
            markPlaceDirty();
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
    glm::vec3 axisV = gizmoAxisDir(p, axis);
    g_rotateAxisW = axisV;
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
    const glm::vec3 axisV = g_rotateAxisW;
    float t = 0.f;
    if (!rayPlane(o, d, p.position, axisV, t)) return;
    float ang = angleAroundAxis(p.position, axisV, o + d * t);
    float deltaDeg = glm::degrees(ang - g_rotateStartAngle);
    if (!gizmoCtrl()) {
        float step = studioSnapRotateStep();
        if (step > 0.f) deltaDeg = snapValue(deltaDeg, step);
    }
    // Rotazione vera attorno all asse (mondo o part): prima si sommavano i gradi a
    // un angolo di Eulero, e con una part gia ruotata girava attorno all asse sbagliato.
    const glm::vec3 prevRot = p.rotation;
    p.rotation = partgroups::toEuler(glm::angleAxis(glm::radians(deltaDeg), axisV) * partgroups::toQuat(g_rotateStartRot));
    if (studioCollisionBlocked(p)) { p.rotation = prevRot; return; }
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
            // lo scatto e gia sul giro fatto (updateRotateDrag), non sui singoli angoli
        }
        if (g_xformTrack && g_selectedPart >= 0)
            g_undo.recordTransform(g_selectedPart, g_xformBefore, p);
        gizmoCommitToModel();
        if (g_xformTrack)
            markPlaceDirty();
    }
    g_rotateDragging = false;
    g_rotateLiveDelta = 0.f;
    g_xformTrack = false;
}

/** Chiude move/scale/rotate aperti — va chiamato prima dello snapshot del Test. */
static void commitActiveStudioGizmo() {
    endScaleDrag();
    endMoveDrag();
    endRotateDrag();
}

static bool rayHitPart(const glm::vec3& o, const glm::vec3& d, const WorkspacePart& p, float& tOut, float padWorld = 0.f) {
    glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    glm::mat3 Rt = glm::transpose(R);
    glm::vec3 lo = Rt * (o - p.position);
    glm::vec3 ld = Rt * d;
    const glm::vec3 size = glm::max(p.size, glm::vec3(std::max(0.f, padWorld)));
    return rayHitAabb(lo, ld, glm::vec3(0), size, tOut);
}


// ---- Trascinare una part col mouse (il "dragger" di Roblox Studio) ---------------------------
// Clic su una part con Seleziona o Sposta e trascina: la part scivola sopra le
// altre part e sul terreno (y = 0), con lo snap della griglia. Le altre part
// selezionate la seguono (follow dei Model, StudioGroupsUi.h).
static bool partIndexValid(int i);                 // StudioFeaturesUi.h
static std::vector<int> studioActivePartIndices(); // StudioFeaturesUi.h
static bool g_freeDragArmed = false;
static bool g_freeDragging = false;
static float g_freeDragX0 = 0.f, g_freeDragY0 = 0.f;
static WorkspacePart g_freeDragBefore;

static void armFreeDrag(float mx, float my) {
    g_freeDragArmed = false;
    if (g_animStudioGizmoBlock || !partIndexValid(g_selectedPart) || g_world.parts[(size_t)g_selectedPart].locked) return;
    g_freeDragArmed = true;
    g_freeDragX0 = mx;
    g_freeDragY0 = my;
}

/** Il punto sotto il mouse sulle part (escluse quelle trascinate) o sul terreno, con la normale. */
static bool freeDragSurface(float mx, float my, const std::vector<int>& skip, glm::vec3& hitP, glm::vec3& hitN) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    d = glm::normalize(d);
    float best = 1e9f;
    int bestPart = -1;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        if (std::find(skip.begin(), skip.end(), i) != skip.end()) continue;
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.transparency >= 0.99f) continue;
        float t = 0.f;
        if (rayHitPart(o, d, p, t) && t > 0.f && t < best) { best = t; bestPart = i; }
    }
    if (bestPart >= 0) {
        const WorkspacePart& p = g_world.parts[(size_t)bestPart];
        hitP = o + d * best;
        // Faccia colpita: l'asse locale dove il punto sta sul bordo della scatola.
        const glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
        const glm::vec3 l = glm::transpose(R) * (hitP - p.position);
        int ax = 0;
        float bestR = -1.f;
        for (int k = 0; k < 3; ++k) {
            const float r = std::fabs(l[k]) / std::max(1e-3f, p.size[k] * 0.5f);
            if (r > bestR) { bestR = r; ax = k; }
        }
        glm::vec3 n(0.f);
        n[ax] = l[ax] >= 0.f ? 1.f : -1.f;
        hitN = glm::normalize(R * n);
        return true;
    }
    if (d.y < -1e-4f) {   // terreno
        const float t = -o.y / d.y;
        hitP = o + d * t;
        hitN = {0.f, 1.f, 0.f};
        return true;
    }
    return false;
}

/** La part sotto il mouse, il punto sulla sua superficie e la normale (niente terreno). */
static bool raycastPartSurface(float mx, float my, int& part, glm::vec3& hitP, glm::vec3& hitN) {
    glm::vec3 o, d;
    screenToRay(mx, my, g_lastView, g_lastProj, o, d);
    d = glm::normalize(d);
    float best = 1e9f;
    part = -1;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        if (rayHitPart(o, d, g_world.parts[(size_t)i], t) && t > 0.f && t < best) { best = t; part = i; }
    }
    if (part < 0) return false;
    const WorkspacePart& p = g_world.parts[(size_t)part];
    hitP = o + d * best;
    const glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    const glm::vec3 l = glm::transpose(R) * (hitP - p.position);
    int ax = 0;
    float bestR = -1.f;
    for (int k = 0; k < 3; ++k) {
        const float r = std::fabs(l[k]) / std::max(1e-3f, p.size[k] * 0.5f);
        if (r > bestR) { bestR = r; ax = k; }
    }
    glm::vec3 n(0.f);
    n[ax] = l[ax] >= 0.f ? 1.f : -1.f;
    hitN = glm::normalize(R * n);
    return true;
}

/** Un clic con lo strumento vincoli acceso: primo punto, poi secondo = vincolo creato. */
static void constraintToolClick(float mx, float my) {
    int part = -1;
    glm::vec3 hit, n;
    if (!raycastPartSurface(mx, my, part, hit, n)) { g_exportMsg = "Clicca su una part"; return; }
    if (g_ctPart0 < 0) {
        g_ctPart0 = part;
        g_ctPoint0 = hit;
        g_ctNormal0 = n;
        g_exportMsg = std::string("Ora clicca un punto sulla seconda part (Esc annulla)");
        return;
    }
    if (part == g_ctPart0) { g_exportMsg = "Serve un'altra part"; return; }
    const std::vector<WeldConstraint> before = g_world.welds;
    const int idx = partconstraints::createAt(g_world, (ConstraintKind)g_constraintToolKind, g_ctPart0, part, g_ctPoint0, hit, g_ctNormal0);
    if (idx >= 0) {
        const std::vector<WeldConstraint> after = g_world.welds;
        g_undo.recordCustom([before, after](bool undoing) { g_world.welds = undoing ? before : after; markPlaceDirty(); });
        markPlaceDirty();
        g_exportMsg = std::string(constraintKindName((ConstraintKind)g_constraintToolKind)) + " creato fra \""
                    + g_world.parts[(size_t)g_ctPart0].name + "\" e \"" + g_world.parts[(size_t)part].name + "\"";
    }
    g_ctPart0 = -1;   // lo strumento resta acceso per il prossimo (come Roblox)
}

static void updateFreeDrag(float mx, float my) {
    if (!g_freeDragArmed && !g_freeDragging) return;
    if (!partIndexValid(g_selectedPart)) { g_freeDragArmed = g_freeDragging = false; return; }
    if (!g_freeDragging) {
        if (std::fabs(mx - g_freeDragX0) + std::fabs(my - g_freeDragY0) < 5.f) return;
        g_freeDragging = true;
        g_freeDragBefore = g_world.parts[(size_t)g_selectedPart];
    }
    WorkspacePart& p = g_world.parts[(size_t)g_selectedPart];
    // Come Roblox: mentre trascini, R gira di 90 gradi attorno a Y, T inclina attorno a X.
    if (ImGui::IsKeyPressed(ImGuiKey_R, false))
        p.rotation = partgroups::toEuler(glm::angleAxis(glm::radians(90.f), glm::vec3(0, 1, 0)) * partgroups::toQuat(p.rotation));
    if (ImGui::IsKeyPressed(ImGuiKey_T, false))
        p.rotation = partgroups::toEuler(glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0)) * partgroups::toQuat(p.rotation));
    std::vector<int> skip = studioActivePartIndices();
    if (std::find(skip.begin(), skip.end(), g_selectedPart) == skip.end()) skip.push_back(g_selectedPart);
    glm::vec3 hit, n;
    if (!freeDragSurface(mx, my, skip, hit, n)) return;
    // Mezza misura della part (ruotata) lungo la normale: cosi' si appoggia, non affonda.
    const glm::mat3 R = glm::mat3(partRotationMatrix(p.rotation));
    const glm::vec3 half = p.size * 0.5f;
    const float push = std::fabs(glm::dot(R[0], n)) * half.x + std::fabs(glm::dot(R[1], n)) * half.y
                     + std::fabs(glm::dot(R[2], n)) * half.z;
    glm::vec3 pos = hit + n * push;
    const float step = studioSnapMoveStep();
    if (step > 0.f && !gizmoCtrl())
        for (int k = 0; k < 3; ++k)
            if (std::fabs(n[k]) < 0.99f) pos[k] = snapValue(pos[k], step);   // snap solo lungo la superficie
    const glm::vec3 prevPos = p.position;
    p.position = pos;
    if (studioCollisionBlocked(p)) p.position = prevPos;
}

static void endFreeDrag() {
    if (g_freeDragging && partIndexValid(g_selectedPart)) {
        g_undo.recordTransform(g_selectedPart, g_freeDragBefore, g_world.parts[(size_t)g_selectedPart]);
        markPlaceDirty();
    }
    g_freeDragArmed = false;
    g_freeDragging = false;
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
    const bool editing = g_screen == AppScreen::Studio && !g_studioTesting;
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        float t = 0.f;
        const float pad = editing ? gizmoPxToWorld(g_world.parts[i].position, 8.f) : 0.f;
        if (rayHitPart(o, d, g_world.parts[i], t, pad) && t < bestT) {
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

// StudioGroupsUi.h: clic su una part di un Model seleziona il Model, Ctrl
// aggiunge/toglie, Alt prende solo la part.
static void studioPickPart(int partIndex);

static void applyScenePick(const ScenePick& hit) {
    switch (hit.kind) {
    case PickKind::Part: studioPickPart(hit.index); break;
    case PickKind::Model: selectModel(hit.index); break;
    case PickKind::Label: selectLabel(hit.index); break;
    case PickKind::Spawn: selectSpawn(); break;
    case PickKind::PlayerRoot: selectPlayer(-1); break;
    case PickKind::PlayerPart: selectPlayer(hit.index); break;
    default: clearAllSelection(); break;
    }
}

