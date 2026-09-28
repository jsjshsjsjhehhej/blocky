#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "AndroidOut.h"
#include "Renderer.h"

extern "C" {

static void handleCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        app->userData = new Renderer(app);
        break;
    case APP_CMD_TERM_WINDOW:
        if (app->userData) {
            delete reinterpret_cast<Renderer*>(app->userData);
            app->userData = nullptr;
        }
        break;
    default:
        break;
    }
}

void android_main(struct android_app* app) {
    aout << "Blocky Fase 1 — android_main avviato" << std::endl;
    app->onAppCmd = handleCmd;

    do {
        bool done = false;
        while (!done) {
            int events = 0;
            android_poll_source* source = nullptr;
            int result = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source));
            switch (result) {
            case ALOOPER_POLL_TIMEOUT:
            case ALOOPER_POLL_WAKE:
                done = true;
                break;
            case ALOOPER_EVENT_ERROR:
                aout << "ALooper_pollOnce: errore" << std::endl;
                break;
            default:
                if (source) source->process(app, source);
                break;
            }
        }
        if (app->userData) reinterpret_cast<Renderer*>(app->userData)->render();
    } while (!app->destroyRequested);
}

} // extern "C"
