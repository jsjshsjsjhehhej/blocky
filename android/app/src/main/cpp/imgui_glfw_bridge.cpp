/**
 * Le tre funzioni del backend ImGui-GLFW che il motore chiama, implementate
 * per Android.
 *
 * Non si compila imgui_impl_glfw.cpp vero perche' usa una cinquantina di
 * funzioni GLFW che il nostro shim non ha (clipboard, cursori di sistema,
 * monitor, IME). Il motore pero' ne chiama solo tre: Init, NewFrame,
 * Shutdown. Qui ci sono quelle, e basta.
 *
 * L'input arriva dallo STESSO stato dello shim GLFW che alimenta la logica di
 * gioco, quindi il dito muove ImGui e la camera insieme, senza due percorsi
 * separati che possono divergere.
 */

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

#include <cmath>

#include "android_bridge.h"

#include "platform/Platform.h"

bool ImGui_ImplGlfw_InitForOpenGL(GLFWwindow*, bool) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "blocky_android_shim";
    return true;
}


/** Traduce i pochi tasti non stampabili che servono a un campo di testo. */
static ImGuiKey imguiKeyFromShim(int glfwKey) {
    switch (glfwKey) {
    case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
    case GLFW_KEY_DELETE:    return ImGuiKey_Delete;
    case GLFW_KEY_ENTER:     return ImGuiKey_Enter;
    case GLFW_KEY_TAB:       return ImGuiKey_Tab;
    case GLFW_KEY_ESCAPE:    return ImGuiKey_Escape;
    case GLFW_KEY_LEFT:      return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT:     return ImGuiKey_RightArrow;
    case GLFW_KEY_UP:        return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN:      return ImGuiKey_DownArrow;
    case GLFW_KEY_HOME:      return ImGuiKey_Home;
    case GLFW_KEY_END:       return ImGuiKey_End;
    default:                 return ImGuiKey_None;
    }
}

/**
 * Svuota le code riempite dagli eventi Android e, se ImGui ha appena messo il
 * cursore dentro un campo di testo, fa comparire la tastiera di sistema.
 *
 * WantTextInput lo decide ImGui disegnando il frame precedente, quindi qui e'
 * gia' quello giusto: si reagisce solo al CAMBIO, altrimenti si chiederebbe di
 * aprire la tastiera sessanta volte al secondo.
 */
static void drainAndroidTextInput(ImGuiIO& io) {
    BlkShimState& s = blkShim();

    for (int i = 0; i < s.pendingCharCount; ++i) io.AddInputCharacter(s.pendingChars[i]);
    s.pendingCharCount = 0;

    for (int i = 0; i < s.pendingKeyCount; ++i) {
        const ImGuiKey k = imguiKeyFromShim(s.pendingKeyCode[i]);
        if (k != ImGuiKey_None) io.AddKeyEvent(k, s.pendingKeyDown[i]);
    }
    s.pendingKeyCount = 0;

    // Quando il cursore passa a un ALTRO campo, la tastiera va riallineata al
    // testo di quel campo: altrimenti continua a ragionare sul contenuto del
    // campo precedente e il confronto genera cancellazioni nel posto sbagliato.
    // Serve anche perche' un campo gia' pieno possa essere cancellato.
    static ImGuiID lastActiveId = 0;
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    const ImGuiID activeId = ctx ? ctx->ActiveId : 0;
    if (ctx && activeId != lastActiveId) {
        lastActiveId = activeId;
        if (activeId != 0 && ctx->InputTextState.ID == activeId) {
            const ImGuiInputTextState& ts = ctx->InputTextState;
            blk::android::resetTextInput(ts.TextA.Data ? ts.TextA.Data : "", ts.TextLen);
        } else {
            blk::android::resetTextInput("", 0);
        }
    }

    static bool keyboardShown = false;
    if (io.WantTextInput != keyboardShown) {
        keyboardShown = io.WantTextInput;
        if (plat::androidSoftKeyboardHook()) plat::androidSoftKeyboardHook()(keyboardShown);
    }
}

/**
 * Trascinare il dito per scorrere.
 *
 * ImGui fa scorrere le finestre con la ROTELLA del mouse, che su un tablet non
 * esiste: trascinare il dito era solo un movimento del cursore, e le liste
 * restavano immobili. Qui il trascinamento verticale viene tradotto in scatti
 * di rotella.
 *
 * Si scorre solo quando ImGui non ha gia' preso in carico il tocco per
 * qualcos'altro (ActiveId == 0): se il dito e' partito su un cursore, un campo
 * di testo o il joystick, quel gesto appartiene a loro e scorrere per sbaglio
 * sarebbe peggio del problema di partenza.
 */
static void applyTouchScroll(ImGuiIO& io) {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return;

    static bool tracking = false;    // c'e' un dito appoggiato sull'interfaccia
    static bool scrolling = false;   // e si e' deciso che sta scorrendo, non toccando
    static float startX = 0.f, startY = 0.f;
    static float lastX = 0.f, lastY = 0.f;

    const bool down = blkShim().mouseHeld[GLFW_MOUSE_BUTTON_LEFT];
    const float x = (float)blkShim().cursorX;
    const float y = (float)blkShim().cursorY;

    if (!down || !io.WantCaptureMouse) {
        tracking = false;
        scrolling = false;
        lastX = x;
        lastY = y;
        return;
    }

    if (!tracking) {
        tracking = true;
        scrolling = false;
        startX = lastX = x;
        startY = lastY = y;
        return;
    }

    // Tocco o trascinamento?
    //
    // Su un telefono la stessa gestualita' puo' voler dire due cose opposte, e
    // si distinguono solo dalla DISTANZA percorsa: un dito fermo che si stacca
    // e' un tocco, un dito che si muove e' uno scorrimento. Serviva davvero,
    // perche' in un elenco come lo Shop ogni dito si appoggia sopra a un
    // pulsante: aspettando che ImGui non avesse nulla in mano — come faceva la
    // versione precedente — non si sarebbe scorso mai.
    if (!scrolling) {
        const float movedY = std::fabs(y - startY);
        const float movedX = std::fabs(x - startX);
        if (movedY < 12.f && movedX < 12.f) return;   // ancora indeciso: e' un tocco
        scrolling = true;
        // Il pulsante sotto al dito non deve piu' credere di essere premuto,
        // altrimenti al rilascio scatterebbe come se lo avessi toccato.
        ImGui::ClearActiveID();
    }

    // Uno "scatto di rotella" vale circa 40 pixel di dito, cosi' l'elenco
    // segue il pollice invece di schizzare via.
    const float dx = (x - lastX) / 40.f;
    const float dy = (y - lastY) / 40.f;
    if (dx != 0.f || dy != 0.f) io.AddMouseWheelEvent(dx, dy);
    lastX = x;
    lastY = y;
}

void ImGui_ImplGlfw_NewFrame() {
    ImGuiIO& io = ImGui::GetIO();

    const int w = blkShim().fbWidth > 0 ? blkShim().fbWidth : 1;
    const int h = blkShim().fbHeight > 0 ? blkShim().fbHeight : 1;
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

    static double last = 0.0;
    const double now = glfwGetTime();
    io.DeltaTime = (last > 0.0) ? (float)(now - last) : (1.f / 60.f);
    if (io.DeltaTime <= 0.f) io.DeltaTime = 1.f / 60.f;
    last = now;

    // mouseHeld, non mouse: lo stato reale di un tocco veloce e' gia' tornato
    // a false quando ImGui arriva a leggerlo una volta per frame.
    // Solo quando CAMBIA qualcosa.
    //
    // Prima venivano rimandati a ogni frame, e questo schiacciava il cursore
    // da controller: il motore lo muove DOPO questa funzione, ma al frame
    // successivo il ponte riscriveva posizione e pulsante del dito (fermo,
    // non premuto) e il click del pad spariva. Due sorgenti che scrivono lo
    // stesso stato, quella silenziosa vince — ed e' il motivo per cui ✕
    // sembrava non funzionare.
    //
    // Tacendo quando il dito non fa nulla, il tocco e il controller possono
    // convivere: comanda l'ultimo che si e' davvero mosso.
    static float lastX = -1.f, lastY = -1.f;
    static bool lastL = false, lastR = false;

    const float cx = (float)blkShim().cursorX;
    const float cy = (float)blkShim().cursorY;
    const bool  l  = blkShim().mouseHeld[GLFW_MOUSE_BUTTON_LEFT];
    const bool  r  = blkShim().mouseHeld[GLFW_MOUSE_BUTTON_RIGHT];

    if (cx != lastX || cy != lastY) {
        io.AddMousePosEvent(cx, cy);
        lastX = cx;
        lastY = cy;
    }
    if (l != lastL) { io.AddMouseButtonEvent(0, l); lastL = l; }
    if (r != lastR) { io.AddMouseButtonEvent(1, r); lastR = r; }

    drainAndroidTextInput(io);
    applyTouchScroll(io);
    // Lo strato Android ne ha bisogno per sapere se un tocco prolungato deve
    // valere come tasto destro (sopra i pannelli) o no (dentro il mondo).
    blkShimSetUiCaptured(io.WantCaptureMouse);
}

void ImGui_ImplGlfw_Shutdown() {}
