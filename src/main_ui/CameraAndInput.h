// Modalita' camera (terza persona, prima persona, orbit di Studio), shake
// della camera, mouse-look, callback GLFW di scroll/movimento mouse,
// lock del cursore in Play, e il crosshair in prima persona. Non e' un
// header riusabile: usa direttamente le globali di main.cpp (g_cam,
// g_playerYaw, g_firstPerson, ecc.). Compila solo se incluso da main.cpp
// esattamente in questo punto.

static glm::vec3 playFocusPos() {
    if (g_ragdoll.active)
        return g_ragdoll.focusPos();
    return g_playerPos;
}

static glm::vec3 playerFwd(float yaw) {
    float r = glm::radians(yaw);
    return glm::normalize(glm::vec3(sinf(r), 0.f, cosf(r)));
}

static float yawFromDir(const glm::vec3& dir) {
    return glm::degrees(atan2f(dir.x, dir.z));
}

static glm::vec3 camFlatForward() {
    float y = glm::radians(g_orbitYaw);
    return glm::normalize(glm::vec3(-sinf(y), 0.f, -cosf(y)));
}

static glm::vec3 camFlatRight() {
    return glm::normalize(glm::cross(camFlatForward(), glm::vec3(0, 1, 0)));
}

// Se il raggio camera-target incontra una part collidibile prima della
// distanza desiderata, avvicina la camera (tirandola davanti all'ostacolo)
// invece di lasciarla attraversare i muri (noclip).
static glm::vec3 clampCameraOffset(const glm::vec3& target, const glm::vec3& offset) {
    float offLen = glm::length(offset);
    if (offLen < 1e-4f) return offset;
    glm::vec3 dirUnit = offset / offLen;
    float tHit = 0.f;
    if (cameraRayHitParts(target, dirUnit, offLen, tHit)) {
        const float kCamCollisionMargin = 0.4f;
        return dirUnit * std::max(0.5f, tHit - kCamCollisionMargin);
    }
    return offset;
}

static glm::mat4 thirdPersonView() {
    float pitchR = glm::radians(g_orbitPitch);
    float cp = cosf(pitchR), sp = sinf(pitchR);
    glm::vec3 fwd = camFlatForward();
    glm::vec3 right = camFlatRight();

    if (g_shiftLock) {
        float dist = glm::clamp(g_orbitDist, 7.f, 12.f);
        glm::vec3 target = playFocusPos() + glm::vec3(0.f, 3.55f, 0.f) + right * 0.45f;
        glm::vec3 offset = - fwd * (dist * cp)
            + glm::vec3(0.f, 1.f, 0.f) * (dist * (-sp))
            + right * 1.85f;
        glm::vec3 eye = target + clampCameraOffset(target, offset);
        return glm::lookAt(eye, target + fwd * 2.5f, glm::vec3(0, 1, 0));
    }

    float yawR = glm::radians(g_orbitYaw);
    glm::vec3 target = playFocusPos() + glm::vec3(0.f, 3.2f, 0.f);
    glm::vec3 offset(
        g_orbitDist * cp * sinf(yawR),
        g_orbitDist * (-sp),
        g_orbitDist * cp * cosf(yawR));
    glm::vec3 eye = target + clampCameraOffset(target, offset);
    return glm::lookAt(eye, target, glm::vec3(0, 1, 0));
}

static void onResize(GLFWwindow*, int w, int h) {
    g_w = std::max(w, 1);
    g_h = std::max(h, 1);
    glViewport(0, 0, g_w, g_h);

    // Le misure dell interfaccia dipendono dallo schermo: su Android la
    // rotazione cambia il lato corto, e con esso quanto deve essere grande un
    // bersaglio da toccare. Il tema va riapplicato perche e lui a scrivere lo
    // stile, e blk::ui::applyToStyle ci si appoggia sopra.
    const float prevScale = blk::ui::uiScale();
    blk::ui::update(g_w, g_h);
    if (blk::ui::uiScale() != prevScale) {
        blk::ui::applyToStyle(ImGui::GetStyle());
        if (ImGui::GetCurrentContext())
            ImGui::GetIO().FontGlobalScale = blk::ui::uiScale();
    }
}

static glm::mat4 firstPersonView() {
    float pitchR = glm::radians(g_orbitPitch);
    float yawR = glm::radians(g_orbitYaw);
    glm::vec3 eye = (g_ragdoll.active ? playFocusPos() : g_playerPos)
        + glm::vec3(0.f, g_ragdoll.active ? 1.2f : 4.55f, 0.f);
    glm::vec3 front(
        -sinf(yawR) * cosf(pitchR),
         sinf(pitchR),
        -cosf(yawR) * cosf(pitchR));
    if (glm::dot(front, front) < 1e-8f)
        front = glm::vec3(0.f, 0.f, -1.f);
    else
        front = glm::normalize(front);
    return glm::lookAt(eye, eye + front, glm::vec3(0.f, 1.f, 0.f));
}

static float g_camShakeMagnitude = 0.f;
static float g_camShakeDuration  = 0.f;
static float g_camShakeRemaining = 0.f;
static float g_camShakeSeed = 0.f;

static void triggerCameraShake(float magnitudeDeg, float durationSec) {
    if (magnitudeDeg <= 0.f || durationSec <= 0.f) return;
    const float currentPeak = (g_camShakeDuration > 0.f)
        ? g_camShakeMagnitude * (g_camShakeRemaining / g_camShakeDuration) : 0.f;
    if (magnitudeDeg < currentPeak) return;

    g_camShakeMagnitude = magnitudeDeg;
    g_camShakeDuration = durationSec;
    g_camShakeRemaining = durationSec;
    g_camShakeSeed = (float)glfwGetTime() * 17.f;
}

static void tickCameraShake(float dt) {
    if (g_camShakeRemaining > 0.f) g_camShakeRemaining = std::max(0.f, g_camShakeRemaining - dt);
}

static glm::mat4 applyCameraShake(const glm::mat4& view) {
    if (g_camShakeRemaining <= 0.f) return view;

    const float t = (float)glfwGetTime() * 24.f;
    const float decay = g_camShakeRemaining / g_camShakeDuration;
    const float amt = glm::radians(g_camShakeMagnitude) * decay * decay;

    const float rx = std::sin(t * 1.7f + g_camShakeSeed) * amt;
    const float ry = std::sin(t * 2.3f + g_camShakeSeed * 1.3f) * amt * 0.6f;
    const float rz = std::sin(t * 1.3f + g_camShakeSeed * 0.7f) * amt * 0.4f;

    const glm::mat4 jitter = glm::rotate(glm::mat4(1.f), rx, glm::vec3(1, 0, 0))
                            * glm::rotate(glm::mat4(1.f), ry, glm::vec3(0, 1, 0))
                            * glm::rotate(glm::mat4(1.f), rz, glm::vec3(0, 0, 1));
    return jitter * view;
}

static glm::mat4 playCameraView() {
    glm::mat4 view = g_firstPerson ? firstPersonView() : thirdPersonView();
    return applyCameraShake(view);
}

static void syncPlayerYawToCamera() {
    g_playerYaw = yawFromDir(camFlatForward());
}

static void applyMouseLook(float dx, float dy) {
    bool fp = g_firstPerson;
    float sens = (g_shiftLock || fp) ? 0.20f : 0.16f;
    float pitchSens = (g_shiftLock || fp) ? 0.15f : 0.13f;
    float pitchMin = fp ? -85.f : -75.f;
    float pitchMax = fp ? 80.f : 55.f;
    if (g_screen == AppScreen::Playing ||
        (g_screen == AppScreen::Studio && g_mode == CamMode::Play)) {
        g_orbitYaw   -= dx * sens;
        g_orbitPitch  = glm::clamp(g_orbitPitch + dy * pitchSens, pitchMin, pitchMax);
        if ((g_shiftLock || fp) && (g_screen == AppScreen::Playing || g_studioTesting))
            syncPlayerYawToCamera();
    } else if (g_screen == AppScreen::Studio && g_mode == CamMode::Studio) {
        g_cam.yaw -= dx * 0.12f;
        g_cam.pitch = glm::clamp(g_cam.pitch + dy * 0.12f, -89.f, 89.f);
    } else if (g_screen == AppScreen::AvatarPublisher) {
        g_catalog.camYaw -= dx * 0.25f;
        g_catalog.camPitch = glm::clamp(g_catalog.camPitch + dy * 0.2f, -80.f, 60.f);
    }
}

static bool g_accGizmoDrag = false;
static int g_accGizmoAxis = -1;
static glm::vec3 g_accGizmoStartOff{0}, g_accGizmoStartRot{0}, g_accGizmoStartScl{1};
static glm::vec3 g_accGizmoStartHit{0};
static float g_accGizmoStartAngle = 0.f;
static float g_accGizmoStartMx = 0.f, g_accGizmoStartMy = 0.f;
static glm::mat4 g_accGizmoPartWorld{1.f};
static constexpr float kCatGizmoYaw = 180.f;

static bool pickCatalogMoveAxis(float mx, float my, int& outAxis);
static bool pickCatalogRotateAxis(float mx, float my, int& outAxis);
static bool pickCatalogScaleAxis(float mx, float my, int& outAxis);
static void beginCatalogGizmo(int axis, float mx, float my);
static void updateCatalogGizmo(float mx, float my);
static void endCatalogGizmo();
static void drawCatalogGizmos(GLuint vao, GLuint partP, const glm::mat4& view,
                              const glm::mat4& proj, const glm::vec3& sun);

static bool playCameraActive() {
    return ((g_screen == AppScreen::Playing) && !g_playMenuOpen)
        || (g_screen == AppScreen::Studio && g_studioTesting && !g_playMenuOpen);
}

static void syncPlayCursor(GLFWwindow* win);

static void onScroll(GLFWwindow* win, double , double yoff) {
    const bool inPlay = playCameraActive()
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (!playCameraActive() && ImGui::GetIO().WantCaptureMouse) return;
    if (inPlay && win
        && (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
        if (yoff > 0.0) g_world.cycleBackpack(-1);
        else if (yoff < 0.0) g_world.cycleBackpack(+1);
        return;
    }
    if (g_screen == AppScreen::AvatarPublisher) {
        g_catalog.camDist = glm::clamp(g_catalog.camDist - (float)yoff * 0.9f, 4.f, 40.f);
    } else if (g_screen == AppScreen::Playing ||
               (g_screen == AppScreen::Studio && g_mode == CamMode::Play)) {
        if (g_firstPerson) {
            if (yoff < 0.0) {
                g_firstPerson = false;
                g_orbitDist = glm::clamp(kFpExitDist - (float)yoff * 0.8f, kFpEnterDist, 40.f);
                syncPlayCursor(win);
            }
        } else {
            float next = g_orbitDist - (float)yoff * 0.8f;
            if (yoff > 0.0 && next <= kFpEnterDist) {
                g_firstPerson = true;
                g_shiftLock = false;
                g_playerYaw = yawFromDir(camFlatForward());
                g_orbitDist = kFpEnterDist;
                syncPlayCursor(win);
            } else {
                g_orbitDist = glm::clamp(next, kFpEnterDist, 40.f);
            }
        }
    }
}

// --- Regolare la presa trascinando nel viewport -----------------------------
// Prima la presa si regolava solo scrivendo numeri in tre caselle: nessuno sa
// in anticipo che "ruota Y di 90" e' l'orientamento giusto per un fucile, lo si
// scopre provando. Qui l'oggetto lo si muove con il mouse come una Part
// qualsiasi, guardandolo in mano mentre cambia.
//
// Non e' il gizmo delle Part (GizmoSystem.h) e non poteva esserlo: quello
// lavora su gizmoPart(), che ha una posizione nel mondo dove piantare le
// frecce. L'oggetto in mano una posizione nel mondo non ce l'ha — segue la
// mano del personaggio — quindi le frecce non avrebbero dove stare.
static bool gripDragActive() {
    return g_gripEditMode != 0
        && g_selectedBackpack >= 0
        && g_selectedBackpack < (int)g_world.backpack.size();
}

static bool g_gripDragging = false;
static float g_gripDragLastX = 0.f, g_gripDragLastY = 0.f;

static bool handleGripDrag(double x, double y) {
    if (!gripDragActive() || !ImGui::GetCurrentContext()) return false;

    ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.WantCaptureMouse) {
        g_gripDragging = false;
        return false;
    }
    if (!g_gripDragging) {
        // Primo frame del trascinamento: si fissa l'origine, non si muove
        // ancora nulla — altrimenti l'oggetto scatterebbe al primo click.
        g_gripDragging = true;
        g_gripDragLastX = (float)x;
        g_gripDragLastY = (float)y;
        return true;
    }

    const float dx = (float)x - g_gripDragLastX;
    const float dy = (float)y - g_gripDragLastY;
    g_gripDragLastX = (float)x;
    g_gripDragLastY = (float)y;

    auto& b = g_world.backpack[g_selectedBackpack];
    if (g_gripEditMode == 1) {
        const float k = 0.01f;                    // pixel -> unita' di mondo
        if (io.KeyShift) b.gripOffset.z += dy * k;  // Shift: avanti / indietro
        else {
            b.gripOffset.x += dx * k;
            b.gripOffset.y -= dy * k;               // schermo in giu' = mondo in giu'
        }
    } else {
        const float k = 0.5f;                     // pixel -> gradi
        if (io.KeyShift) b.gripRotation.z += dx * k;
        else {
            b.gripRotation.y += dx * k;
            b.gripRotation.x += dy * k;
        }
    }
    return true;
}

static void onMouse(GLFWwindow*, double x, double y) {
    // Prima di tutto il resto: mentre si regola la presa il trascinamento
    // appartiene all'oggetto, non alla telecamera.
    if (handleGripDrag(x, y)) return;

    const bool fpLook = playCameraActive() && g_firstPerson && !g_playMenuOpen && !g_showChat;
#ifdef __ANDROID__
    // Su un tablet il tasto destro non esiste: guardarsi intorno e semplicemente
    // trascinare il dito sul mondo. Vale solo se ImGui non ha gia preso quel
    // tocco, cosi il pollice sul joystick non fa girare anche la telecamera.

    const bool touchLook = playCameraActive()
        && glfwGetMouseButton(nullptr, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
        && !ImGui::GetIO().WantCaptureMouse;
    // Il punto di partenza dello sguardo lo azzera onMouseBtn quando il dito si
    // appoggia: quello e il momento giusto, perche questo gestore non viene
    // chiamato affatto quando il dito si stacca.



#else
    const bool touchLook = false;
#endif
    const bool looking = fpLook || g_rmbLook || touchLook;
    if (!looking && ImGui::GetIO().WantCaptureMouse) return;
    if (!looking) return;
    if (g_firstMouse) { g_lastMx = (float)x; g_lastMy = (float)y; g_firstMouse = false; return; }
    float dx = (float)x - g_lastMx;
    float dy = g_lastMy - (float)y;
    g_lastMx = (float)x;
    g_lastMy = (float)y;
    applyMouseLook(dx, dy);
}

static void syncPlayCursor(GLFWwindow* win) {
    if (!win) return;
    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    bool lock = false;
    if (inPlay && !g_playMenuOpen && !g_showChat) {
        if (g_firstPerson) lock = true;
        else if (g_rmbLook) lock = true;
    }
    if (lock == g_playCursorLocked) return;
    g_playCursorLocked = lock;
    g_firstMouse = true;
    glfwSetInputMode(win, GLFW_CURSOR,
        lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (lock && ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

static void setPlayCursor(GLFWwindow* win, bool playing) {
    if (!playing) {
        g_playCursorLocked = true;
        g_rmbLook = false;
        g_playCursorLocked = false;
        g_firstMouse = true;
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return;
    }
    g_playCursorLocked = !g_playCursorLocked;
    syncPlayCursor(win);
}

static void drawFpCrosshair() {
    if (!g_firstPerson) return;
    if (!(g_screen == AppScreen::Playing
          || (g_screen == AppScreen::Studio && g_studioTesting)))
        return;
    if (g_playMenuOpen) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float cx = (float)g_w * 0.5f;
    const float cy = (float)g_h * 0.5f;
    dl->AddCircleFilled({cx, cy}, 3.2f, IM_COL32(255, 255, 255, 230), 14);
    dl->AddCircle({cx, cy}, 3.2f, IM_COL32(0, 0, 0, 180), 14, 1.4f);
    dl->AddCircleFilled({cx, cy}, 1.2f, IM_COL32(0, 0, 0, 200), 10);
}
