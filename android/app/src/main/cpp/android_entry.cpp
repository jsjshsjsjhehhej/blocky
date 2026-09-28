/**
 * Punto d'ingresso Android del motore vero.
 *
 * Differenza chiave col desktop: qui il ciclo non e' nostro. GameActivity ci
 * chiama quando c'e' una superficie, quando arriva un tocco, quando l'app va
 * in background. Noi chiamiamo blockyAndroidFrame() una volta per giro — la
 * stessa funzione che su Windows gira dentro il while di Blocky_RunEngine.
 */

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <android/log.h>
#include <android/keycodes.h>
#include <unistd.h>
#include <thread>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <GLFW/glfw3.h>   // lo shim: ci serve per iniettare il touch

#include "android_bridge.h"
#include "asset_unpack.h"
#include "platform/Platform.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Blocky", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Blocky", __VA_ARGS__)

extern "C" int  blockyAndroidInit();
extern "C" void blockyAndroidFrame();
extern "C" void blockyAndroidShutdown();

namespace {

/**
 * Su Android printf/fprintf(stderr) finiscono nel vuoto: tutta la
 * diagnostica del motore (errori di compilazione shader, asset mancanti,
 * messaggi di Lua) sarebbe invisibile. Qui stdout/stderr vengono dirottati
 * dentro logcat, cosi' `adb logcat -s Blocky` mostra esattamente quello che
 * su Windows si vede nella console.
 */
void redirectStdioToLogcat() {
    static bool done = false;
    if (done) return;
    done = true;

    static int pfd[2];
    if (pipe(pfd) != 0) return;
    dup2(pfd[1], STDOUT_FILENO);
    dup2(pfd[1], STDERR_FILENO);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    std::thread([] {
        char buf[512];
        ssize_t n;
        while ((n = read(pfd[0], buf, sizeof(buf) - 1)) > 0) {
            if (buf[n - 1] == '\n') --n;
            buf[n] = '\0';
            __android_log_write(ANDROID_LOG_INFO, "BlockyOut", buf);
        }
    }).detach();
}

// Messaggio unico: lo stesso guasto puo emergere in due momenti diversi, e
// vederlo scritto in due modi diversi confonderebbe e basta.
constexpr const char* kGles2Message =
    "Attenzione: GLES 2 non supportato.\n\n"
    "Questo dispositivo offre solo OpenGL ES 2, mentre Blocky richiede "
    "OpenGL ES 3.0 o superiore.";

struct Ctx {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig  config{};
    bool engineReady = false;
    int width = 0, height = 0;
};

Ctx g_ctx;

/** Legge la dimensione vera della superficie e la comunica al motore. */
void syncSurfaceSize() {
    if (g_ctx.display == EGL_NO_DISPLAY || g_ctx.surface == EGL_NO_SURFACE) return;
    EGLint w = 0, h = 0;
    eglQuerySurface(g_ctx.display, g_ctx.surface, EGL_WIDTH, &w);
    eglQuerySurface(g_ctx.display, g_ctx.surface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;
    if (w == g_ctx.width && h == g_ctx.height) return;
    g_ctx.width = w;
    g_ctx.height = h;
    blkShimResize(w, h);
    LOGI("superficie: %dx%d", w, h);
}

/**
 * Prepara display, contesto e superficie.
 *
 * Display e contesto si creano UNA VOLTA SOLA e sopravvivono a tutto il resto;
 * la superficie invece nasce e muore ogni volta che Android ricrea la finestra
 * (rotazione dello schermo, app messa in secondo piano e ripresa). Vedi
 * releaseSurface() per il perche' questa distinzione e' essenziale.
 */
bool initEgl(android_app* app) {
    if (g_ctx.display == EGL_NO_DISPLAY) {
        constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };

        g_ctx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(g_ctx.display, nullptr, nullptr);

        EGLint num = 0;
        eglChooseConfig(g_ctx.display, attribs, nullptr, 0, &num);
        if (num <= 0) {
            // Nessuna configurazione OpenGL ES 3: il dispositivo arriva solo a
            // ES 2. Il motore usa shader ES 3 (uniform block, texture di
            // profondita, glDrawBuffers): non e una differenza che si aggira
            // con un ripiego, sarebbe un secondo renderer da scrivere.
            blk::android::showFatalMessage(kGles2Message);
            return false;
        }
        eglChooseConfig(g_ctx.display, attribs, &g_ctx.config, 1, &num);
    }

    if (g_ctx.context == EGL_NO_CONTEXT) {
        constexpr EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        g_ctx.context = eglCreateContext(g_ctx.display, g_ctx.config, nullptr, ctxAttribs);
        if (g_ctx.context == EGL_NO_CONTEXT) { LOGE("creazione contesto EGL fallita"); return false; }
    }

    if (g_ctx.surface == EGL_NO_SURFACE) {
        g_ctx.surface = eglCreateWindowSurface(g_ctx.display, g_ctx.config, app->window, nullptr);
        if (g_ctx.surface == EGL_NO_SURFACE) { LOGE("creazione superficie EGL fallita"); return false; }
    }

    if (!eglMakeCurrent(g_ctx.display, g_ctx.surface, g_ctx.surface, g_ctx.context)) {
        LOGE("eglMakeCurrent fallita");
        return false;
    }

    // Alcuni driver accettano la richiesta di un contesto ES 3 e ne consegnano
    // comunque uno piu vecchio: l unica risposta di cui fidarsi e quella del
    // contesto gia attivo.
    {
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        LOGI("OpenGL: %s", version ? version : "(sconosciuto)");
        if (!version || !std::strstr(version, "OpenGL ES 3")) {
            blk::android::showFatalMessage(kGles2Message);
            return false;
        }
    }

    syncSurfaceSize();
    LOGI("EGL pronto: %dx%d", g_ctx.width, g_ctx.height);
    return true;
}

/**
 * La finestra sparisce: si butta SOLO la superficie, mai il contesto.
 *
 * Distruggere il contesto EGL distrugge con se' ogni risorsa GPU che il motore
 * ha creato all'avvio — texture, shader, buffer — senza che il motore possa
 * accorgersene: al ritorno continuerebbe a disegnare con identificatori ormai
 * morti, cioe' schermo nero per sempre. Era esattamente quello che succedeva
 * ruotando il tablet. Tenendo vivo il contesto, basta riagganciarlo alla
 * superficie nuova e tutto riprende da dov'era.
 */
void releaseSurface() {
    if (g_ctx.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_ctx.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_ctx.surface != EGL_NO_SURFACE) {
        eglDestroySurface(g_ctx.display, g_ctx.surface);
        g_ctx.surface = EGL_NO_SURFACE;
    }
    g_ctx.width = g_ctx.height = 0;
}

/** Chiusura definitiva dell'app: qui si', si butta tutto. */
void termEgl() {
    releaseSurface();
    if (g_ctx.display == EGL_NO_DISPLAY) return;
    if (g_ctx.context != EGL_NO_CONTEXT) eglDestroyContext(g_ctx.display, g_ctx.context);
    eglTerminate(g_ctx.display);
    g_ctx = Ctx{};
}

void onCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (!initEgl(app)) return;
        if (!g_ctx.engineReady) {
            LOGI("avvio del motore...");
            const int r = blockyAndroidInit();
            g_ctx.engineReady = (r > 0);
            LOGI("motore %s (codice %d)", g_ctx.engineReady ? "pronto" : "NON avviato", r);
            // Ripetere la dimensione ADESSO non e' ridondante: la prima
            // blkShimResize e' arrivata prima che il motore registrasse la sua
            // callback di ridimensionamento, quindi era caduta nel vuoto e lui
            // era rimasto con la dimensione da finestra desktop — da cui la
            // scena disegnata su una porzione soltanto dello schermo.
            if (g_ctx.engineReady) blkShimResize(g_ctx.width, g_ctx.height);
        } else {
            // Ritorno da una rotazione o dal secondo piano: il motore e' gia'
            // vivo, gli si dice solo quanto e' grande adesso lo schermo.
            blkShimResize(g_ctx.width, g_ctx.height);
        }
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        syncSurfaceSize();
        break;
    case APP_CMD_TERM_WINDOW:
        releaseSurface();
        break;
    default:
        break;
    }
}

/**
 * Tasti -> testo per ImGui.
 *
 * Vengono separati in due categorie perche' sono due cose diverse: Backspace o
 * una freccia sono COMANDI su un campo di testo, mentre una lettera e' un
 * CARATTERE, e quale carattere esattamente lo decide Android in base a Shift e
 * al layout di tastiera dell'utente — non si puo' indovinare dal codice tasto.
 */
// ---------------------------------------------------------------------------
//  Controller fisici (DualSense, Xbox, generici)
// ---------------------------------------------------------------------------
//
// Su Android i pad non passano da GLFW: Android li consegna come eventi tasto
// (AKEYCODE_BUTTON_*) e movimento (sorgente AINPUT_SOURCE_JOYSTICK). Finche'
// mancava questo ponte, lo shim rispondeva "nessun joystick collegato" e su
// Android non funzionava NESSUN controller.
//
// Il vantaggio e' che Android normalizza da solo: un DualSense, un pad Xbox e
// un pad cinese da 15 euro arrivano tutti sugli stessi codici. Non serve la
// tabella di mapping per GUID che su PC e' indispensabile.

/** L'evento viene da un controller (non da dita, mouse o stilo)? */
static bool isGamepadSource(int32_t source) {
    return (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD
        || (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK
        || (source & AINPUT_SOURCE_DPAD) == AINPUT_SOURCE_DPAD;
}

/**
 * Gli assi vanno chiesti esplicitamente, uno per uno.
 *
 * GameActivity NON riempie axisValues[] per gli assi che non sono stati
 * abilitati: senza queste chiamate gli stick leggono zero per sempre, senza
 * errori — un altro fallimento silenzioso della stessa famiglia.
 */
static void enablePadAxes() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int32_t axis : {AMOTION_EVENT_AXIS_X,  AMOTION_EVENT_AXIS_Y,
                         AMOTION_EVENT_AXIS_Z,  AMOTION_EVENT_AXIS_RZ,
                         AMOTION_EVENT_AXIS_RX, AMOTION_EVENT_AXIS_RY,
                         AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_RTRIGGER,
                         AMOTION_EVENT_AXIS_BRAKE,    AMOTION_EVENT_AXIS_GAS,
                         AMOTION_EVENT_AXIS_HAT_X,    AMOTION_EVENT_AXIS_HAT_Y}) {
        GameActivityPointerAxes_enableAxis(axis);
    }
}

/**
 * Lascia passare gli eventi dei controller.
 *
 * Il filtro predefinito di GameActivity scarta tutto cio' che non e' touch o
 * mouse: gli eventi del pad venivano buttati via prima ancora di arrivare al
 * nostro codice.
 */
static bool motionFilterAcceptAll(const GameActivityMotionEvent* ev) {
    if (!ev) return false;
    return true;
}
static bool keyFilterAcceptAll(const GameActivityKeyEvent* ev) {
    if (!ev) return false;
    return true;
}

/** Un pulsante del pad: da codice Android a numero pulsante GLFW. -1 = altro. */
static int padButtonFromKeyCode(int32_t keyCode) {
    switch (keyCode) {
    case AKEYCODE_BUTTON_A:      return GLFW_GAMEPAD_BUTTON_A;
    case AKEYCODE_BUTTON_B:      return GLFW_GAMEPAD_BUTTON_B;
    case AKEYCODE_BUTTON_X:      return GLFW_GAMEPAD_BUTTON_X;
    case AKEYCODE_BUTTON_Y:      return GLFW_GAMEPAD_BUTTON_Y;
    case AKEYCODE_BUTTON_L1:     return GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
    case AKEYCODE_BUTTON_R1:     return GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
    case AKEYCODE_BUTTON_SELECT: return GLFW_GAMEPAD_BUTTON_BACK;
    case AKEYCODE_BUTTON_START:  return GLFW_GAMEPAD_BUTTON_START;
    case AKEYCODE_BUTTON_MODE:   return GLFW_GAMEPAD_BUTTON_GUIDE;
    case AKEYCODE_BUTTON_THUMBL: return GLFW_GAMEPAD_BUTTON_LEFT_THUMB;
    case AKEYCODE_BUTTON_THUMBR: return GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;
    case AKEYCODE_DPAD_UP:       return GLFW_GAMEPAD_BUTTON_DPAD_UP;
    case AKEYCODE_DPAD_RIGHT:    return GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
    case AKEYCODE_DPAD_DOWN:     return GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
    case AKEYCODE_DPAD_LEFT:     return GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
    default:                     return -1;
    }
}

/** Segnala che un pad si e' fatto vivo, una volta sola per sessione. */
static void notePadSeen(int32_t deviceId) {
    static int32_t known = -1;
    if (known == deviceId) return;
    known = deviceId;
    blkShimPadSetPresent(true, "Controller");
    __android_log_print(ANDROID_LOG_INFO, "Blocky",
                        "pad: controller collegato (device %d)", deviceId);
}

/** Assi di un evento di movimento del pad. */
static void pumpPadMotion(const GameActivityMotionEvent& ev) {
    if (ev.pointerCount == 0) return;
    const GameActivityPointerAxes& p = ev.pointers[0];

    notePadSeen(ev.deviceId);

    auto ax = [&](int32_t a) { return GameActivityPointerAxes_getAxisValue(&p, a); };

    blkShimPadAxis(GLFW_GAMEPAD_AXIS_LEFT_X,  ax(AMOTION_EVENT_AXIS_X));
    blkShimPadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y,  ax(AMOTION_EVENT_AXIS_Y));
    blkShimPadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, ax(AMOTION_EVENT_AXIS_Z));
    blkShimPadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, ax(AMOTION_EVENT_AXIS_RZ));

    // I trigger: Android li da da 0 a 1, GLFW da -1 a +1 (e GamepadInput.h fa
    // (v+1)/2 per tornare a 0..1). Senza questa conversione un trigger a
    // riposo risulterebbe premuto a meta'.
    float lt = ax(AMOTION_EVENT_AXIS_LTRIGGER);
    float rt = ax(AMOTION_EVENT_AXIS_RTRIGGER);
    if (lt == 0.f) lt = ax(AMOTION_EVENT_AXIS_BRAKE);   // alcuni pad usano questi
    if (rt == 0.f) rt = ax(AMOTION_EVENT_AXIS_GAS);
    blkShimPadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,  lt * 2.f - 1.f);
    blkShimPadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, rt * 2.f - 1.f);

    // Croce direzionale analogica: alcuni pad la mandano come assi invece che
    // come tasti. Va tradotta in pulsanti, o la si perde.
    const float hx = ax(AMOTION_EVENT_AXIS_HAT_X);
    const float hy = ax(AMOTION_EVENT_AXIS_HAT_Y);
    blkShimPadButton(GLFW_GAMEPAD_BUTTON_DPAD_LEFT,  hx < -0.5f);
    blkShimPadButton(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, hx >  0.5f);
    blkShimPadButton(GLFW_GAMEPAD_BUTTON_DPAD_UP,    hy < -0.5f);
    blkShimPadButton(GLFW_GAMEPAD_BUTTON_DPAD_DOWN,  hy >  0.5f);
}

void pumpKeys(android_input_buffer* ib) {
    for (uint64_t i = 0; i < ib->keyEventsCount; ++i) {
        const GameActivityKeyEvent& ev = ib->keyEvents[i];
        const bool down = (ev.action == AKEY_EVENT_ACTION_DOWN);

        // Un tasto del pad va al pad, non alla tastiera. Le frecce sono il
        // caso che conta: la croce direzionale di un controller e i tasti
        // freccia sono lo STESSO codice Android, e distinguerli si puo' solo
        // guardando da dove arriva l'evento.
        if (isGamepadSource(ev.source)) {
            const int padBtn = padButtonFromKeyCode(ev.keyCode);
            if (padBtn >= 0) {
                notePadSeen(ev.deviceId);
                blkShimPadButton(padBtn, down);
                continue;
            }
        }

        int mapped = 0;
        switch (ev.keyCode) {
        case AKEYCODE_DEL:          mapped = GLFW_KEY_BACKSPACE; break;
        case AKEYCODE_FORWARD_DEL:  mapped = GLFW_KEY_DELETE;    break;
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER: mapped = GLFW_KEY_ENTER;     break;
        case AKEYCODE_TAB:          mapped = GLFW_KEY_TAB;       break;
        case AKEYCODE_ESCAPE:       mapped = GLFW_KEY_ESCAPE;    break;
        case AKEYCODE_DPAD_LEFT:    mapped = GLFW_KEY_LEFT;      break;
        case AKEYCODE_DPAD_RIGHT:   mapped = GLFW_KEY_RIGHT;     break;
        case AKEYCODE_DPAD_UP:      mapped = GLFW_KEY_UP;        break;
        case AKEYCODE_DPAD_DOWN:    mapped = GLFW_KEY_DOWN;      break;
        case AKEYCODE_MOVE_HOME:    mapped = GLFW_KEY_HOME;      break;
        case AKEYCODE_MOVE_END:     mapped = GLFW_KEY_END;       break;
        default: break;
        }

        if (mapped) {
            blkShimPushKey(mapped, down);
            continue;
        }
        if (!down) continue;

        const unsigned int cp = blk::android::unicodeFromKey(ev.keyCode, ev.metaState);
        if (cp >= 32) blkShimPushChar(cp);
    }
}

/** Tocchi -> stato del mouse finto: da li' entrano nella logica di selezione
 *  e camera gia' esistente, senza toccarla. */
/**
 * Dita -> comandi, uno per uno.
 *
 * La regola e' una sola: quando un dito si appoggia gli viene assegnato UN
 * compito, e quel compito resta suo finche' non lo si stacca. Il dito sul
 * joystick muove solo il joystick, quello sul salto salta e basta, quello sul
 * mondo guarda intorno.
 *
 * Prima si seguiva un dito solo e lo si trasformava in un finto mouse: da li'
 * venivano entrambi i difetti visti sul tablet — il pollice sul joystick
 * faceva girare anche la telecamera, e appoggiando il secondo dito per saltare
 * il primo veniva dimenticato e la camminata si interrompeva.
 */
void pumpTouch(android_app* app) {
    auto* ib = android_app_swap_input_buffers(app);
    if (!ib) return;

    // A cosa serve ogni dito, per identificatore. -1 = nessun dito assegnato.
    enum class Duty { None, Stick, Jump, Look };
    static int32_t stickId = -1, jumpId = -1, lookId = -1;
    static float stickOriginX = 0.f, stickOriginY = 0.f;
    static float pinchStart = 0.f;
    // Tocco prolungato: dito fermo per un po = tasto destro.
    static double lookDownTime = 0.0;
    static float  lookDownX = 0.f, lookDownY = 0.f;
    static bool   longPressFired = false;

    auto dutyOf = [&](int32_t id) {
        if (id == stickId) return Duty::Stick;
        if (id == jumpId)  return Duty::Jump;
        if (id == lookId)  return Duty::Look;
        return Duty::None;
    };

    // Il rilascio deve avvenire DOVE sta il dito: passando 0,0 il cursore
    // saltava in alto a sinistra un istante prima del rilascio, e ImGui vedeva
    // il clic finire fuori dal pulsante — cioe nessun clic.
    auto release = [&](int32_t id, float x, float y) {
        if (id == stickId) { stickId = -1; blkShimSetStick(0.f, 0.f); }
        if (id == jumpId)  { jumpId = -1;  blkShimSetJump(false); }
        if (id == lookId) {
            lookId = -1;
            // Se era diventato un tocco prolungato, si rilascia il tasto destro
            // invece del sinistro: il clic sinistro non e mai avvenuto.
            if (longPressFired) blkShimSetRightButton(false, x, y);
            else blkShimPointerButton(false, x, y);
            longPressFired = false;
        }
    };

    for (uint64_t i = 0; i < ib->motionEventsCount; ++i) {
        const GameActivityMotionEvent& ev = ib->motionEvents[i];
        if (ev.pointerCount == 0) continue;

        // Gli stick di un controller arrivano come eventi di movimento, sulla
        // stessa coda delle dita. Vanno riconosciuti e tolti di mezzo prima
        // della logica del tocco, o uno stick verrebbe letto come un dito che
        // striscia sullo schermo.
        if (isGamepadSource(ev.source)) {
            pumpPadMotion(ev);
            continue;
        }

        const int action = ev.action & AMOTION_EVENT_ACTION_MASK;
        const int index = (ev.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                          >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // --- un dito si appoggia ---
        if (action == AMOTION_EVENT_ACTION_DOWN ||
            action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            if (index >= (int)ev.pointerCount) continue;
            const int32_t id = ev.pointers[index].id;
            const float x = GameActivityPointerAxes_getX(&ev.pointers[index]);
            const float y = GameActivityPointerAxes_getY(&ev.pointers[index]);

            if (blkShimPointInUiRect(0, x, y) && stickId < 0) {
                stickId = id;
                stickOriginX = x;
                stickOriginY = y;
            } else if (blkShimPointInUiRect(1, x, y) && jumpId < 0) {
                jumpId = id;
                blkShimSetJump(true);
            } else if (lookId < 0) {
                lookDownTime = glfwGetTime();
                lookDownX = x;
                lookDownY = y;
                longPressFired = false;
                lookId = id;
                blkShimPointerButton(true, x, y);
            }
            continue;
        }

        // --- un dito si stacca ---
        if (action == AMOTION_EVENT_ACTION_UP ||
            action == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (index < (int)ev.pointerCount) {
                release(ev.pointers[index].id,
                        GameActivityPointerAxes_getX(&ev.pointers[index]),
                        GameActivityPointerAxes_getY(&ev.pointers[index]));
            }
            pinchStart = 0.f;
            continue;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            const float cx = (float)blkShim().cursorX;
            const float cy = (float)blkShim().cursorY;
            release(stickId, cx, cy);
            release(jumpId, cx, cy);
            release(lookId, cx, cy);
            pinchStart = 0.f;
            continue;
        }

        // --- le dita si muovono: un evento riporta TUTTE le posizioni ---
        if (action != AMOTION_EVENT_ACTION_MOVE) continue;

        // I punti "sul mondo", raccolti in ordine: il primo comanda la
        // telecamera, il secondo serve solo a misurare il pizzico.
        //
        // Vanno raccolti in un elenco e non in due variabili sciolte. Prima il
        // dito della telecamera riempiva il primo punto e un dito senza compito
        // riempiva il secondo: se il dito della telecamera si staccava mentre
        // l'altro restava appoggiato, il primo punto era rimasto a (0,0) e ci
        // si muoveva il cursore — la visuale schizzava nell'angolo. Da qui il
        // "la visuale si resetta".
        float worldX[2] = {0.f, 0.f};
        float worldY[2] = {0.f, 0.f};
        int worldCount = 0;

        for (uint32_t p = 0; p < ev.pointerCount; ++p) {
            const int32_t id = ev.pointers[p].id;
            const float x = GameActivityPointerAxes_getX(&ev.pointers[p]);
            const float y = GameActivityPointerAxes_getY(&ev.pointers[p]);
            const Duty duty = dutyOf(id);

            if (duty == Duty::Stick) {
                // Lo scostamento si misura dal punto in cui il pollice si e'
                // appoggiato, non dal centro disegnato: cosi' il joystick
                // "nasce" sotto al dito e non serve centrarlo con precisione.
                const float limit = blkShimStickRadius();
                float dx = (x - stickOriginX) / limit;
                float dy = (y - stickOriginY) / limit;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1.f) { dx /= len; dy /= len; }
                // Lo schermo cresce verso il basso, il mondo verso l'avanti.
                blkShimSetStick(dx, -dy);
                continue;
            }
            if (duty == Duty::Jump) continue;

            // Telecamera per primo, cosi' e' sempre lui a guidare; un dito
            // senza compito vale solo come secondo punto per il pizzico, e solo
            // se non sta sopra ai comandi.
            const bool onControls = blkShimPointInUiRect(0, x, y) || blkShimPointInUiRect(1, x, y);
            if (duty != Duty::Look && onControls) continue;

            if (duty == Duty::Look && worldCount > 0) {
                // Sposta in seconda posizione chi c'era, la telecamera va prima.
                worldX[1] = worldX[0];
                worldY[1] = worldY[0];
                worldX[0] = x;
                worldY[0] = y;
                if (worldCount < 2) ++worldCount;
            } else if (worldCount < 2) {
                worldX[worldCount] = x;
                worldY[worldCount] = y;
                ++worldCount;
            }
        }

        // Un dito solo sul mondo muove la visuale — ma solo se e' DAVVERO quello
        // della telecamera: un dito rimasto senza compito non deve comandarla.
        if (worldCount == 1) {
            if (lookId >= 0) blkShimPointerMove(worldX[0], worldY[0]);
            pinchStart = 0.f;
        } else if (worldCount >= 2) {
            // Due dita sul mondo = pizzico = rotella, cioe' zoom e prima persona.
            const float dx = worldX[0] - worldX[1];
            const float dy = worldY[0] - worldY[1];
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (pinchStart <= 0.f) {
                pinchStart = dist;
            } else if (std::fabs(dist - pinchStart) > 6.f) {
                blkShimScroll((dist - pinchStart) / 60.0);
                pinchStart = dist;
            }
        }
    }

    // Tocco prolungato = tasto destro.
    //
    // Su uno schermo il tasto destro non esiste, e tenere premuto e il gesto
    // che tutti si aspettano per "altre opzioni". Vale solo sopra
    // all interfaccia: nel mondo il tasto destro fa girare la telecamera, e
    // trasformare ogni pressione un po lunga in una rotazione sarebbe assurdo.
    if (lookId >= 0 && !longPressFired && blkShimUiCaptured()) {
        const double heldFor = glfwGetTime() - lookDownTime;
        const float dx = (float)blkShim().cursorX - lookDownX;
        const float dy = (float)blkShim().cursorY - lookDownY;
        const bool stillThere = (dx * dx + dy * dy) < (18.f * 18.f);
        if (heldFor > 0.5 && stillThere) {
            longPressFired = true;
            // Il clic sinistro va annullato: il dito non lo ha "cliccato",
            // lo ha tenuto premuto, e sono due intenzioni diverse.
            blkShimPointerButton(false, blkShim().cursorX, blkShim().cursorY);
            blkShimSetRightButton(true, blkShim().cursorX, blkShim().cursorY);
        }
    }

    pumpKeys(ib);
    android_app_clear_motion_events(ib);
    android_app_clear_key_events(ib);
}

} // namespace

extern "C" void android_main(android_app* app) {
    redirectStdioToLogcat();
    LOGI("android_main: Blocky");

    // Prima di qualunque cosa: ricostruire il layout dei file che il motore si
    // aspetta. selfDir() diventa la memoria privata dell app, e dentro ci
    // finisce una copia vera della cartella assets estratta dall APK — da quel
    // momento texture, script Lua e theme.json si caricano con lo stesso
    // codice che gira su Windows.
    if (app->activity && app->activity->internalDataPath) {
        const std::string base = app->activity->internalDataPath;
        plat::setAndroidBaseDir(base);
        blk::android::installPlatformHooks(app);

        // Letta qui, PRIMA che il motore parta: intgBoot() (chiamato da
        // Blocky_RunEngine) la legge una volta sola all'avvio, quindi deve
        // gia' essere pronta. Vedi platform/Platform.h e Integrity.h per il
        // resto della catena.
        const std::string sig = blk::android::readSigningCertSha256();
        plat::setAndroidSigningCertSha256(sig);
        LOGI("firma APK: %s", sig.empty() ? "NON LEGGIBILE" : sig.c_str());

        LOGI("radice dati: %s", base.c_str());
        blk::android::unpackApkAssets(app->activity->vm, app->activity->javaGameActivity,
                                      base + "/assets");
    } else {
        LOGE("nessun internalDataPath: gli asset non saranno caricabili");
    }
    app->onAppCmd = onCmd;

    // I controller: senza queste tre righe gli eventi del pad non arrivano
    // affatto. Il filtro predefinito di GameActivity tiene solo touch e mouse,
    // e gli assi degli stick restano a zero finche' non li si chiede uno per
    // uno. Entrambe le cose falliscono in silenzio — il pad semplicemente
    // "non funziona", senza un errore da nessuna parte.
    android_app_set_motion_event_filter(app, motionFilterAcceptAll);
    android_app_set_key_event_filter(app, keyFilterAcceptAll);
    enablePadAxes();

    do {
        bool done = false;
        while (!done) {
            int events = 0;
            android_poll_source* source = nullptr;
            const int result = ALooper_pollOnce(0, nullptr, &events,
                                                reinterpret_cast<void**>(&source));
            switch (result) {
            case ALOOPER_POLL_TIMEOUT:
            case ALOOPER_POLL_WAKE:
                done = true;
                break;
            case ALOOPER_EVENT_ERROR:
                LOGE("ALooper_pollOnce: errore");
                done = true;
                break;
            default:
                if (source) source->process(app, source);
                break;
            }
        }

        if (g_ctx.engineReady && g_ctx.surface != EGL_NO_SURFACE) {
            // L'ordine qui conta, e sbagliarlo rompe tutto.
            //
            // pumpTouch legge le zone dichiarate dal frame PRECEDENTE: e' il
            // dato piu' fresco che esista quando il tocco arriva. Solo dopo si
            // azzerano, e sara' draw() — piu' in basso, dentro
            // blockyAndroidFrame — a ridichiararle se si sta giocando.
            //
            // Azzerandole prima di pumpTouch, come era scritto un momento fa,
            // il tocco non trovava mai nessuna zona e joystick e salto non
            // rispondevano affatto. Non azzerandole per niente, invece,
            // restavano appese anche nei menu e si mangiavano i pulsanti in
            // basso a sinistra.
            pumpTouch(app);
            // NB: qui NON si traduce il pad in tocco. Il motore ha gia' il suo
            // cursore da controller (src/ui/MenuPadCursor.h), che funziona su
            // ogni piattaforma: farlo anche qui significava due sistemi che
            // muovono lo stesso cursore nello stesso frame, e vinceva quello
            // sbagliato. Da qui il pad esce solo come stato di joystick, che
            // il motore legge con le stesse funzioni GLFW che usa su PC.
            blkShimClearUiRects();
            blk::android::pumpTextInput(app);
            blockyAndroidFrame();
            blkShimEndFrame();
            eglSwapBuffers(g_ctx.display, g_ctx.surface);

            // "Esci" dal menu del gioco.
            //
            // Il motore lo comunica come fa su desktop, chiedendo alla finestra
            // di chiudersi. Su Android quella finestra e' finta e nessuno
            // guardava quel segnale: il pulsante non faceva assolutamente
            // nulla. Qui lo si traduce nel gesto equivalente vero, cioe'
            // chiedere ad Android di terminare l'activity — dopodiche' il
            // sistema alza destroyRequested e il ciclo esce da solo, passando
            // dallo spegnimento ordinato del motore.
            if (glfwWindowShouldClose(blkShimWindow()) && app->activity) {
                LOGI("uscita richiesta dal gioco");
                GameActivity_finish(app->activity);
            }
        }
    } while (!app->destroyRequested);

    if (g_ctx.engineReady) blockyAndroidShutdown();
    termEgl();
}
