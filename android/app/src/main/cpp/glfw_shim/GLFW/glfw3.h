#pragma once

/**
 * GLFW finto per Android — GLFW non esiste su Android (e' una libreria
 * desktop), ma tutto il motore lo usa: 28 funzioni e ~55 costanti sparse in
 * main.cpp, main_ui/, engine/.
 *
 * Invece di mettere #ifdef __ANDROID__ in decine di punti, questo file si
 * chiama esattamente come quello vero e viene messo PRIMO nell'include path
 * della sola build Android: il motore continua a scrivere
 * #include <GLFW/glfw3.h> e trova questo, senza cambiare una riga.
 *
 * Cosa fa davvero:
 *  - finestra/contesto/swap: no-op (ci pensa EGL + GameActivity)
 *  - tempo: orologio monotono vero
 *  - tastiera/mouse: stato finto pilotato dal touch (vedi blkShim* in fondo)
 *  - i SETTER di callback registrano le funzioni del motore, e il codice
 *    Android le richiama quando arriva un tocco: cosi' la logica di
 *    selezione/gizmo/camera gia' scritta funziona senza essere toccata.
 */

#include <chrono>
#include <cstring>

// ---- Tipi -------------------------------------------------------------
struct GLFWwindow;
struct GLFWmonitor;
struct GLFWcursor;

typedef struct GLFWimage { int width, height; unsigned char* pixels; } GLFWimage;

typedef void (*GLFWkeyfun)(GLFWwindow*, int, int, int, int);
typedef void (*GLFWcursorposfun)(GLFWwindow*, double, double);
typedef void (*GLFWmousebuttonfun)(GLFWwindow*, int, int, int);
typedef void (*GLFWscrollfun)(GLFWwindow*, double, double);
typedef void (*GLFWframebuffersizefun)(GLFWwindow*, int, int);
typedef void (*GLFWwindowclosefun)(GLFWwindow*);
typedef void (*GLFWerrorfun)(int, const char*);
typedef void (*(*GLFWglproc)(void))(void);

// ---- Costanti (valori veri di GLFW, cosi' i confronti restano identici)
#define GLFW_RELEASE 0
#define GLFW_PRESS   1
#define GLFW_REPEAT  2

#define GLFW_MOUSE_BUTTON_LEFT   0
#define GLFW_MOUSE_BUTTON_RIGHT  1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

#define GLFW_KEY_SPACE 32
#define GLFW_KEY_1 49
#define GLFW_KEY_2 50
#define GLFW_KEY_3 51
#define GLFW_KEY_4 52
#define GLFW_KEY_5 53
#define GLFW_KEY_6 54
#define GLFW_KEY_7 55
#define GLFW_KEY_8 56
#define GLFW_KEY_9 57
#define GLFW_KEY_A 65
#define GLFW_KEY_C 67
#define GLFW_KEY_D 68
#define GLFW_KEY_E 69
#define GLFW_KEY_F 70
#define GLFW_KEY_Q 81
#define GLFW_KEY_R 82
#define GLFW_KEY_S 83
#define GLFW_KEY_V 86
#define GLFW_KEY_W 87

#define GLFW_KEY_BACKSPACE 259
#define GLFW_KEY_DELETE    261
#define GLFW_KEY_ENTER     257
#define GLFW_KEY_TAB       258
#define GLFW_KEY_HOME      268
#define GLFW_KEY_END       269
#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_RIGHT  262
#define GLFW_KEY_LEFT   263
#define GLFW_KEY_DOWN   264
#define GLFW_KEY_UP     265

#define GLFW_KEY_F1 290
#define GLFW_KEY_F3 292
#define GLFW_KEY_F4 293
#define GLFW_KEY_F5 294

#define GLFW_KEY_KP_1 321
#define GLFW_KEY_KP_2 322
#define GLFW_KEY_KP_3 323
#define GLFW_KEY_KP_4 324
#define GLFW_KEY_KP_5 325
#define GLFW_KEY_KP_6 326
#define GLFW_KEY_KP_7 327
#define GLFW_KEY_KP_8 328
#define GLFW_KEY_KP_9 329

#define GLFW_KEY_LEFT_SHIFT    340
#define GLFW_KEY_LEFT_CONTROL  341
#define GLFW_KEY_RIGHT_SHIFT   344
#define GLFW_KEY_RIGHT_CONTROL 345
#define GLFW_KEY_LAST          348

#define GLFW_CURSOR          0x00033001
#define GLFW_CURSOR_NORMAL   0x00034001
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_SAMPLES                0x0002100D
#define GLFW_CONTEXT_VERSION_MAJOR  0x00022002
#define GLFW_CONTEXT_VERSION_MINOR  0x00022003
#define GLFW_OPENGL_PROFILE         0x00022008
#define GLFW_OPENGL_CORE_PROFILE    0x00032001

// ---- Stato dello shim --------------------------------------------------
struct BlkShimState {
    bool keys[GLFW_KEY_LAST + 1] = {};
    bool mouse[8] = {};
    // Stato ESPOSTO dei tasti del mouse, tenuto premuto almeno un frame intero.
    // Un tocco veloce manda DOWN e UP nello stesso giro di eventi: leggendo
    // solo lo stato reale, chi guarda una volta per frame (ImGui) vedrebbe
    // sempre e solo "dito alzato" e il click non esisterebbe mai.
    bool mouseHeld[8] = {};
    bool mousePressedPending[8] = {};

    // Testo digitato sulla tastiera a schermo, in attesa di essere consegnato a
    // ImGui. E una coda e non uno stato perche un carattere e un evento: se lo
    // si leggesse una volta per frame come fosse uno stato, digitando in fretta
    // le lettere andrebbero perse.
    static constexpr int kMaxPendingChars = 64;
    unsigned int pendingChars[kMaxPendingChars] = {};
    int pendingCharCount = 0;

    // Tasti di modifica del testo (Backspace, Invio, frecce): stesso motivo.
    static constexpr int kMaxPendingKeys = 64;
    int pendingKeyCode[kMaxPendingKeys] = {};
    bool pendingKeyDown[kMaxPendingKeys] = {};
    int pendingKeyCount = 0;

    // Zone occupate dai comandi a schermo (joystick, salto), aggiornate ogni
    // frame dal motore. Servono a distinguere "due dita sul mondo" — che e un
    // pizzico per lo zoom — da "un pollice sul joystick e uno sul salto", che
    // non lo e affatto.
    static constexpr int kMaxUiRects = 4;
    float uiRect[kMaxUiRects][4] = {};
    int uiRectCount = 0;

    // Comandi a schermo, tenuti SEPARATI dal finto mouse.
    //
    // Prima joystick e salto passavano dal mouse come tutto il resto, e questo
    // causava due guai: il dito sul joystick faceva girare anche la telecamera,
    // e un secondo dito (il salto) sostituiva il primo facendo interrompere la
    // camminata. Avendo canali propri, ogni dito fa una cosa sola e le altre
    // continuano indisturbate.
    float stickX = 0.f, stickY = 0.f;
    bool  jumpDown = false;
    bool  jumpPressed = false;

    // Vero quando il tocco sta sopra all interfaccia. Lo dice il ponte ImGui a
    // ogni frame: serve al tocco prolungato, che deve valere come tasto destro
    // sui pannelli ma NON dentro il mondo, dove il tasto destro fa girare la
    // telecamera.
    bool  uiCaptured = false;
    double cursorX = 0.0, cursorY = 0.0;
    bool shouldClose = false;
    int fbWidth = 0, fbHeight = 0;
    // 1 = aspetta il refresh del telefono. Il motore lo imposta ogni frame
    // con glfwSwapInterval; senza questo campo la chiamata veniva ignorata
    // e i fotogrammi uscivano a mucchi.
    int swapInterval = 1;

    GLFWcursorposfun onCursorPos = nullptr;
    GLFWmousebuttonfun onMouseButton = nullptr;
    GLFWscrollfun onScroll = nullptr;
    GLFWframebuffersizefun onFramebufferSize = nullptr;
    GLFWwindowclosefun onWindowClose = nullptr;
};

inline BlkShimState& blkShim() {
    static BlkShimState s;
    return s;
}

/** Finestra finta: il motore la passa in giro ma non ci fa mai niente di
 *  specifico — basta che non sia nullptr. */
inline GLFWwindow* blkShimWindow() {
    return reinterpret_cast<GLFWwindow*>(&blkShim());
}

// ---- Ciclo di vita / finestra: no-op ----------------------------------
inline int  glfwInit(void) { return 1; }
inline void glfwTerminate(void) {}
inline void glfwWindowHint(int, int) {}
inline GLFWwindow* glfwCreateWindow(int w, int h, const char*, GLFWmonitor*, GLFWwindow*) {
    // Se la superficie vera e' gia' stata misurata (EGL lo fa prima che il
    // motore parta), la dimensione chiesta dal codice desktop va ignorata:
    // comanda lo schermo del dispositivo, non una costante da finestra PC.
    if (blkShim().fbWidth > 0 && blkShim().fbHeight > 0) return blkShimWindow();
    blkShim().fbWidth = w;
    blkShim().fbHeight = h;
    return blkShimWindow();
}
inline void glfwDestroyWindow(GLFWwindow*) {}
inline void glfwMakeContextCurrent(GLFWwindow*) {}
inline GLFWwindow* glfwGetCurrentContext(void) { return blkShimWindow(); }
inline void glfwSwapInterval(int interval) { blkShim().swapInterval = interval; }
inline int blkShimSwapInterval() { return blkShim().swapInterval; }
inline void glfwSwapBuffers(GLFWwindow*) {}   // il buffer lo scambia EGL
inline void glfwPollEvents(void) {}           // gli eventi arrivano da GameActivity
inline void glfwSetWindowTitle(GLFWwindow*, const char*) {}
inline void glfwSetWindowIcon(GLFWwindow*, int, const GLFWimage*) {}
inline void glfwSetInputMode(GLFWwindow*, int, int) {}
inline GLFWglproc glfwGetProcAddress(const char*) { return nullptr; } // GLES e' linkata diretta

inline int  glfwWindowShouldClose(GLFWwindow*) { return blkShim().shouldClose ? 1 : 0; }
inline void glfwSetWindowShouldClose(GLFWwindow*, int v) { blkShim().shouldClose = (v != 0); }

// ---- Tempo -------------------------------------------------------------
inline double glfwGetTime(void) {
    using clock = std::chrono::steady_clock;
    static const clock::time_point t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

// ---- Input: stato pilotato dal touch ----------------------------------
inline int glfwGetKey(GLFWwindow*, int key) {
    if (key < 0 || key > GLFW_KEY_LAST) return GLFW_RELEASE;
    return blkShim().keys[key] ? GLFW_PRESS : GLFW_RELEASE;
}
inline int glfwGetMouseButton(GLFWwindow*, int button) {
    if (button >= 0 && button < 8 && blkShim().mouseHeld[button]) return GLFW_PRESS;
    if (button < 0 || button >= 8) return GLFW_RELEASE;
    return blkShim().mouse[button] ? GLFW_PRESS : GLFW_RELEASE;
}
inline void glfwGetCursorPos(GLFWwindow*, double* x, double* y) {
    if (x) *x = blkShim().cursorX;
    if (y) *y = blkShim().cursorY;
}
inline const char* glfwGetKeyName(int, int) { return nullptr; }
inline int glfwGetKeyScancode(int key) { return key; }

// ---- Registrazione callback (il motore le mette, Android le chiama) ----
inline GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow*, GLFWcursorposfun f) {
    auto* old = blkShim().onCursorPos; blkShim().onCursorPos = f; return old;
}
inline GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow*, GLFWmousebuttonfun f) {
    auto* old = blkShim().onMouseButton; blkShim().onMouseButton = f; return old;
}
inline GLFWscrollfun glfwSetScrollCallback(GLFWwindow*, GLFWscrollfun f) {
    auto* old = blkShim().onScroll; blkShim().onScroll = f; return old;
}
inline GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow*, GLFWframebuffersizefun f) {
    auto* old = blkShim().onFramebufferSize; blkShim().onFramebufferSize = f; return old;
}
inline GLFWwindowclosefun glfwSetWindowCloseCallback(GLFWwindow*, GLFWwindowclosefun f) {
    auto* old = blkShim().onWindowClose; blkShim().onWindowClose = f; return old;
}
inline GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun) { return nullptr; }

// ---- Gamepad: alimentato dagli eventi di GameActivity -----------------
//
// Su Android i controller NON passano da GLFW: arrivano come eventi tasto
// (AKEYCODE_BUTTON_*) e movimento (AINPUT_SOURCE_JOYSTICK) di GameActivity.
// Prima queste funzioni rispondevano tutte "nessun joystick collegato", e
// il risultato e' che su Android non funzionava NESSUN pad — ne' DualSense,
// ne' Xbox, ne' altri. Non era una scelta sul PS5: era l'assenza del canale.
//
// Ora android_entry.cpp riempie questo stato e GamepadInput.h continua a
// chiedere a GLFW come su PC, senza saperlo: stesso trucco dello shim che
// tiene in piedi tutto il resto del porting.
//
// Il pad si presenta come gia' mappato (glfwJoystickIsGamepad = vero):
// Android normalizza da solo i pulsanti sui codici AKEYCODE_BUTTON_*, quindi
// la tabella dei mapping GUID di GamepadInput.h qui non serve — sarebbe un
// secondo strato di traduzione sopra una traduzione gia' fatta.
#define GLFW_TRUE  1
#define GLFW_FALSE 0
#define GLFW_CONNECTED    0x00040001
#define GLFW_DISCONNECTED 0x00040002
#define GLFW_JOYSTICK_1    0
#define GLFW_JOYSTICK_LAST 15

#define GLFW_HAT_CENTERED 0
#define GLFW_HAT_UP       1
#define GLFW_HAT_RIGHT    2
#define GLFW_HAT_DOWN     4
#define GLFW_HAT_LEFT     8

#define GLFW_GAMEPAD_BUTTON_A            0
#define GLFW_GAMEPAD_BUTTON_B            1
#define GLFW_GAMEPAD_BUTTON_X            2
#define GLFW_GAMEPAD_BUTTON_Y            3
#define GLFW_GAMEPAD_BUTTON_LEFT_BUMPER  4
#define GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER 5
#define GLFW_GAMEPAD_BUTTON_BACK         6
#define GLFW_GAMEPAD_BUTTON_START        7
#define GLFW_GAMEPAD_BUTTON_GUIDE        8
#define GLFW_GAMEPAD_BUTTON_LEFT_THUMB   9
#define GLFW_GAMEPAD_BUTTON_RIGHT_THUMB  10
#define GLFW_GAMEPAD_BUTTON_DPAD_UP      11
#define GLFW_GAMEPAD_BUTTON_DPAD_RIGHT   12
#define GLFW_GAMEPAD_BUTTON_DPAD_DOWN    13
#define GLFW_GAMEPAD_BUTTON_DPAD_LEFT    14
#define GLFW_GAMEPAD_BUTTON_LAST         14

#define GLFW_GAMEPAD_AXIS_LEFT_X         0
#define GLFW_GAMEPAD_AXIS_LEFT_Y         1
#define GLFW_GAMEPAD_AXIS_RIGHT_X        2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y        3
#define GLFW_GAMEPAD_AXIS_LEFT_TRIGGER   4
#define GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER  5
#define GLFW_GAMEPAD_AXIS_LAST           5

typedef struct GLFWgamepadstate {
    unsigned char buttons[15];
    float axes[6];
} GLFWgamepadstate;

struct BlkPadState {
    bool  present = false;
    unsigned char buttons[15] = {};
    float axes[6] = {};
    char  name[64] = "Controller Android";
};

inline BlkPadState& blkShimPad() { static BlkPadState p; return p; }

inline void blkShimPadSetPresent(bool on, const char* name) {
    BlkPadState& p = blkShimPad();
    if (p.present != on) {
        // Cambio di stato: azzera tutto, cosi' un pad scollegato non lascia
        // uno stick "premuto" che fa camminare il personaggio da solo.
        std::memset(p.buttons, 0, sizeof(p.buttons));
        std::memset(p.axes, 0, sizeof(p.axes));
    }
    p.present = on;
    if (name && name[0]) {
        std::strncpy(p.name, name, sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = 0;
    }
}

inline void blkShimPadButton(int glfwButton, bool down) {
    if (glfwButton < 0 || glfwButton > GLFW_GAMEPAD_BUTTON_LAST) return;
    blkShimPad().buttons[glfwButton] = down ? 1 : 0;
}

inline void blkShimPadAxis(int glfwAxis, float value) {
    if (glfwAxis < 0 || glfwAxis > GLFW_GAMEPAD_AXIS_LAST) return;
    blkShimPad().axes[glfwAxis] = value;
}

typedef void (*GLFWjoystickfun)(int, int);

inline int glfwJoystickPresent(int jid) {
    return (jid == GLFW_JOYSTICK_1 && blkShimPad().present) ? GLFW_TRUE : GLFW_FALSE;
}
inline int glfwJoystickIsGamepad(int jid) { return glfwJoystickPresent(jid); }

inline const char* glfwGetJoystickName(int jid) {
    return glfwJoystickPresent(jid) ? blkShimPad().name : nullptr;
}
inline const char* glfwGetGamepadName(int jid) { return glfwGetJoystickName(jid); }

// GUID fittizio ma stabile: GamepadInput.h lo usa per riconoscere la marca,
// e Android non lo espone. Il nome del dispositivo, che invece arriva, basta
// per distinguere un DualSense da un pad Xbox.
inline const char* glfwGetJoystickGUID(int jid) {
    return glfwJoystickPresent(jid) ? "androidgameactivity000000000000" : nullptr;
}

inline const float* glfwGetJoystickAxes(int jid, int* count) {
    if (!glfwJoystickPresent(jid)) { if (count) *count = 0; return nullptr; }
    if (count) *count = 6;
    return blkShimPad().axes;
}
inline const unsigned char* glfwGetJoystickButtons(int jid, int* count) {
    if (!glfwJoystickPresent(jid)) { if (count) *count = 0; return nullptr; }
    if (count) *count = 15;
    return blkShimPad().buttons;
}
inline const unsigned char* glfwGetJoystickHats(int jid, int* count) {
    // La croce direzionale e' gia' fra i pulsanti (DPAD_UP..DPAD_LEFT):
    // esporla anche come "hat" la farebbe contare due volte.
    (void)jid;
    if (count) *count = 0;
    return nullptr;
}

inline int glfwGetGamepadState(int jid, GLFWgamepadstate* st) {
    if (!st) return GLFW_FALSE;
    if (!glfwJoystickPresent(jid)) { std::memset(st, 0, sizeof(*st)); return GLFW_FALSE; }
    const BlkPadState& p = blkShimPad();
    std::memcpy(st->buttons, p.buttons, sizeof(st->buttons));
    std::memcpy(st->axes, p.axes, sizeof(st->axes));
    return GLFW_TRUE;
}

// Android normalizza gia' i pulsanti: accettiamo la tabella e la ignoriamo,
// invece di rispondere "fallito" e far credere a un errore.
inline int glfwUpdateGamepadMappings(const char*) { return GLFW_TRUE; }
inline GLFWjoystickfun glfwSetJoystickCallback(GLFWjoystickfun) { return nullptr; }

// ---- Ponte dal lato Android (chiamato da Renderer.cpp / main.cpp) -----

/** Un dito si muove: diventa "il mouse si e' mosso". */
inline void blkShimPointerMove(double x, double y) {
    blkShim().cursorX = x;
    blkShim().cursorY = y;
    if (blkShim().onCursorPos) blkShim().onCursorPos(blkShimWindow(), x, y);
}

/** Dito giu'/su: diventa tasto sinistro del mouse. */
inline void blkShimPointerButton(bool down, double x, double y) {
    blkShim().cursorX = x;
    blkShim().cursorY = y;
    blkShim().mouse[GLFW_MOUSE_BUTTON_LEFT] = down;
    if (down) {
        blkShim().mouseHeld[GLFW_MOUSE_BUTTON_LEFT] = true;
        blkShim().mousePressedPending[GLFW_MOUSE_BUTTON_LEFT] = true;
    }
    if (blkShim().onMouseButton)
        blkShim().onMouseButton(blkShimWindow(), GLFW_MOUSE_BUTTON_LEFT,
                                down ? GLFW_PRESS : GLFW_RELEASE, 0);
}

/** Pizzico a due dita: diventa rotella (zoom camera). */
inline void blkShimScroll(double dy) {
    if (blkShim().onScroll) blkShim().onScroll(blkShimWindow(), 0.0, dy);
}

/** Dichiara le zone dei comandi a schermo per questo frame. */
inline void blkShimClearUiRects() { blkShim().uiRectCount = 0; }

inline void blkShimSetStick(float x, float y) {
    blkShim().stickX = x;
    blkShim().stickY = y;
}

inline void blkShimSetUiCaptured(bool on) { blkShim().uiCaptured = on; }
inline bool blkShimUiCaptured() { return blkShim().uiCaptured; }

/** Tocco prolungato = tasto destro: e il gesto equivalente su uno schermo. */
inline void blkShimSetRightButton(bool down, double x, double y) {
    auto& s = blkShim();
    s.cursorX = x;
    s.cursorY = y;
    s.mouse[GLFW_MOUSE_BUTTON_RIGHT] = down;
    if (down) {
        s.mouseHeld[GLFW_MOUSE_BUTTON_RIGHT] = true;
        s.mousePressedPending[GLFW_MOUSE_BUTTON_RIGHT] = true;
    }
    if (s.onMouseButton)
        s.onMouseButton(blkShimWindow(), GLFW_MOUSE_BUTTON_RIGHT,
                        down ? GLFW_PRESS : GLFW_RELEASE, 0);
}

inline void blkShimSetJump(bool down) {
    auto& s = blkShim();
    if (down && !s.jumpDown) s.jumpPressed = true;
    s.jumpDown = down;
}

inline float blkShimStickX() { return blkShim().stickX; }
inline float blkShimStickY() { return blkShim().stickY; }
inline bool  blkShimJumpDown() { return blkShim().jumpDown; }

/** "Appena premuto" vale un frame solo: si consuma leggendolo. */
inline bool blkShimTakeJumpPressed() {
    auto& s = blkShim();
    const bool v = s.jumpPressed;
    s.jumpPressed = false;
    return v;
}

inline void blkShimAddUiRect(float x0, float y0, float x1, float y1) {
    auto& s = blkShim();
    if (s.uiRectCount >= BlkShimState::kMaxUiRects) return;
    float* r = s.uiRect[s.uiRectCount++];
    r[0] = x0; r[1] = y0; r[2] = x1; r[3] = y1;
}

/** Un punto e dentro la zona numero i? 0 = joystick, 1 = salto. */
inline bool blkShimPointInUiRect(int i, float x, float y) {
    const auto& s = blkShim();
    if (i < 0 || i >= s.uiRectCount) return false;
    const float* r = s.uiRect[i];
    return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3];
}

/** Raggio del joystick, ricavato dalla zona che il motore ha dichiarato:
 *  cosi non ci sono due numeri da tenere allineati a mano. */
inline float blkShimStickRadius() {
    const auto& s = blkShim();
    if (s.uiRectCount < 1) return 96.f;
    const float w = (s.uiRect[0][2] - s.uiRect[0][0]) * 0.5f;
    return w > 1.f ? w : 96.f;
}

inline bool blkShimPointInUi(float x, float y) {
    const auto& s = blkShim();
    for (int i = 0; i < s.uiRectCount; ++i) {
        const float* r = s.uiRect[i];
        if (x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]) return true;
    }
    return false;
}

/** Un carattere digitato sulla tastiera di sistema. */
inline void blkShimPushChar(unsigned int codepoint) {
    if (codepoint == 0) return;
    auto& s = blkShim();
    if (s.pendingCharCount >= BlkShimState::kMaxPendingChars) return;
    s.pendingChars[s.pendingCharCount++] = codepoint;
}

/** Un tasto non stampabile (Backspace, Invio, frecce). */
inline void blkShimPushKey(int key, bool down) {
    if (key < 0 || key > GLFW_KEY_LAST) return;
    auto& s = blkShim();
    s.keys[key] = down;
    if (s.pendingKeyCount >= BlkShimState::kMaxPendingKeys) return;
    s.pendingKeyCode[s.pendingKeyCount] = key;
    s.pendingKeyDown[s.pendingKeyCount] = down;
    ++s.pendingKeyCount;
}

/** Tasto virtuale (pulsanti a schermo: W/S/Spazio...). */
inline void blkShimSetKey(int key, bool down) {
    if (key < 0 || key > GLFW_KEY_LAST) return;
    blkShim().keys[key] = down;
}

/**
 * Da chiamare dopo aver disegnato il frame.
 *
 * Applica qui i rilasci del dito, non nel momento in cui arrivano: cosi' un
 * tocco che va giu' e su tra due frame resta comunque "premuto" per un frame
 * intero, abbastanza perche' ImGui — che legge lo stato una volta per frame —
 * lo veda davvero e il click sui pulsanti funzioni.
 */
inline void blkShimEndFrame() {
    for (int b = 0; b < 8; ++b) {
        if (blkShim().mousePressedPending[b]) { blkShim().mousePressedPending[b] = false; continue; }
        if (!blkShim().mouse[b]) blkShim().mouseHeld[b] = false;
    }
}

/** La superficie ha cambiato dimensione. */
inline void blkShimResize(int w, int h) {
    blkShim().fbWidth = w;
    blkShim().fbHeight = h;
    if (blkShim().onFramebufferSize) blkShim().onFramebufferSize(blkShimWindow(), w, h);
}
