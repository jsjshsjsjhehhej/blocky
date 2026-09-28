#include "android_bridge.h"

#include "jni_scope.h"

#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/log.h>
#include <game-text-input/gametextinput.h>

#include <GLFW/glfw3.h>   // lo shim: blkShimPushChar / blkShimPushKey

#include <string>
#include <vector>

#include "platform/Platform.h"
#include "platform/FilePick.h"

#define BRIDGE_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Blocky", __VA_ARGS__)
#define BRIDGE_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Blocky", __VA_ARGS__)

namespace blk {
namespace android {
namespace {

android_app* g_app = nullptr;

JavaVM* vm() { return g_app && g_app->activity ? g_app->activity->vm : nullptr; }
jobject activityObject() {
    return g_app && g_app->activity ? g_app->activity->javaGameActivity : nullptr;
}

constexpr jint kFlagActivityNewTask = 0x10000000;

/**
 * Apre un indirizzo nel browser di sistema.
 *
 * L'equivalente Android di ShellExecute e' un Intent ACTION_VIEW consegnato ad
 * Android, che sceglie lui l'app giusta. Si costruisce a mano via JNI perche'
 * non c'e' una API C che lo faccia. FLAG_ACTIVITY_NEW_TASK serve perche' il
 * browser deve aprirsi in un compito suo: senza, finirebbe impilato dentro
 * Blocky e chiudendolo si tornerebbe in un posto sbagliato.
 */
bool openUrl(const char* url) {
    if (!url || !url[0]) return false;

    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) {
        BRIDGE_LOGE("apertura link: nessun ambiente JNI");
        return false;
    }

    bool ok = false;
    jclass uriCls = env->FindClass("android/net/Uri");
    jclass intentCls = env->FindClass("android/content/Intent");
    jclass activityCls = env->GetObjectClass(activity);

    if (uriCls && intentCls && activityCls) {
        jmethodID parse = env->GetStaticMethodID(uriCls, "parse",
                                                 "(Ljava/lang/String;)Landroid/net/Uri;");
        jmethodID intentCtor = env->GetMethodID(intentCls, "<init>",
                                                "(Ljava/lang/String;Landroid/net/Uri;)V");
        jmethodID addFlags = env->GetMethodID(intentCls, "addFlags",
                                              "(I)Landroid/content/Intent;");
        jmethodID startActivity = env->GetMethodID(activityCls, "startActivity",
                                                   "(Landroid/content/Intent;)V");

        if (parse && intentCtor && addFlags && startActivity) {
            jstring jurl = env->NewStringUTF(url);
            jobject uri = env->CallStaticObjectMethod(uriCls, parse, jurl);
            jstring action = env->NewStringUTF("android.intent.action.VIEW");
            jobject intent = uri ? env->NewObject(intentCls, intentCtor, action, uri) : nullptr;

            if (intent) {
                jobject flagged = env->CallObjectMethod(intent, addFlags, kFlagActivityNewTask);
                if (flagged) env->DeleteLocalRef(flagged);
                env->CallVoidMethod(activity, startActivity, intent);
                ok = !env->ExceptionCheck();
                clearPendingException(env);
                env->DeleteLocalRef(intent);
            }
            if (uri) env->DeleteLocalRef(uri);
            env->DeleteLocalRef(action);
            env->DeleteLocalRef(jurl);
        }
    }
    clearPendingException(env);
    if (uriCls) env->DeleteLocalRef(uriCls);
    if (intentCls) env->DeleteLocalRef(intentCls);
    if (activityCls) env->DeleteLocalRef(activityCls);

    if (!ok) BRIDGE_LOGE("apertura link fallita: %s", url);
    return ok;
}


// ---- Testo dalla tastiera di sistema ----------------------------------
//
// La tastiera Android non manda una sequenza di tasti: modifica un BUFFER di
// testo condiviso (GameTextInputState) e avvisa che e' cambiato. ImGui invece
// ragiona per eventi ("e' arrivata la lettera a", "e' stato premuto
// Backspace"). Il ponte tra i due mondi e' confrontare il buffer di adesso con
// quello di prima e dedurre gli eventi mancanti — ed e' anche il motivo per cui
// prima non si scriveva niente: gli eventi tasto che cercavamo non esistevano.

/** Ultimo testo visto nel buffer dell'IME, come sequenza di codepoint. */
std::vector<unsigned int> g_imeText;

std::vector<unsigned int> decodeUtf8(const char* s, int len) {
    std::vector<unsigned int> out;
    if (!s || len <= 0) return out;
    for (int i = 0; i < len;) {
        const unsigned char c = (unsigned char)s[i];
        unsigned int cp = 0;
        int extra = 0;
        if (c < 0x80)        { cp = c;          extra = 0; }
        else if (c < 0xE0)   { cp = c & 0x1F;   extra = 1; }
        else if (c < 0xF0)   { cp = c & 0x0F;   extra = 2; }
        else                 { cp = c & 0x07;   extra = 3; }
        if (i + extra >= len) break;   // sequenza troncata: meglio fermarsi
        for (int k = 1; k <= extra; ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

void onImeStateChanged(void* /*ctx*/, const GameTextInputState* state) {
    if (!state) return;
    // Niente log del testo: finirebbe in logcat quello che l utente sta
    // scrivendo, password e indirizzi email compresi.
    if (state->text_length == 0 && g_imeText.empty()) return;
    const std::vector<unsigned int> now = decodeUtf8(state->text_UTF8, state->text_length);

    size_t common = 0;
    while (common < g_imeText.size() && common < now.size() && g_imeText[common] == now[common])
        ++common;

    // Quello che c'era e non c'e' piu' e' stato cancellato: un Backspace a
    // testa. Giu' e su insieme — ImGui distribuisce da solo gli eventi che
    // altrimenti si annullerebbero dentro lo stesso frame.
    for (size_t i = common; i < g_imeText.size(); ++i) {
        blkShimPushKey(GLFW_KEY_BACKSPACE, true);
        blkShimPushKey(GLFW_KEY_BACKSPACE, false);
    }
    for (size_t i = common; i < now.size(); ++i) blkShimPushChar(now[i]);

    g_imeText = now;
}

/**
 * Svuota il buffer dell'IME.
 *
 * Serve ogni volta che il cursore entra in un campo nuovo: senza, la tastiera
 * continuerebbe a modificare il testo del campo precedente e il confronto
 * produrrebbe una raffica di cancellazioni sbagliate.
 */
void resetImeBuffer() {
    if (!g_app || !g_app->activity) return;
    g_imeText.clear();
    GameTextInputState empty{};
    empty.text_UTF8 = "";
    empty.text_length = 0;
    empty.selection = {0, 0};
    empty.composingRegion = {-1, -1};
    GameActivity_setTextInputState(g_app->activity, &empty);
}


/**
 * Allinea il buffer della tastiera al contenuto di un campo.
 *
 * Due bug nascevano dal non farlo. Primo: passando da un campo all'altro la
 * tastiera continuava a lavorare sul testo del campo precedente, e il confronto
 * tra "prima" e "adesso" produceva una raffica di cancellazioni finite nel
 * campo sbagliato. Secondo: un campo che conteneva gia' del testo non si
 * poteva cancellare con Backspace, perche' per noi partiva vuoto e non c'era
 * niente da togliere.
 *
 * Passando qui il testo gia' presente, la tastiera parte esattamente da quello
 * che l'utente vede scritto.
 */
void syncTextInput(const char* text, int length) {
    g_imeText = decodeUtf8(text, length);
    if (!g_app || !g_app->activity) return;

    const std::string current(text ? text : "", (size_t)(length > 0 ? length : 0));
    GameTextInputState state{};
    state.text_UTF8 = current.c_str();
    state.text_length = (int32_t)current.size();
    state.selection = {(int32_t)g_imeText.size(), (int32_t)g_imeText.size()};
    state.composingRegion = {-1, -1};
    GameActivity_setTextInputState(g_app->activity, &state);
}

/**
 * Avvia o ferma il Foreground Service mentre si ospita una partita.
 *
 * Il perche' e' interamente commentato in Platform.h, accanto al gancio
 * (notifyHostingStateChanged): tiene vivo il PROCESSO, non rimette da solo
 * in moto net.poll() se GameActivity ha perso la superficie — quella parte
 * resta un lavoro a parte, dichiarato, non nascosto.
 */
void hostingStateChanged(bool hosting) {
    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID m = cls
        ? env->GetMethodID(cls, hosting ? "startHostForegroundService" : "stopHostForegroundService", "()V")
        : nullptr;
    if (m) env->CallVoidMethod(activity, m);
    clearPendingException(env);
    if (cls) env->DeleteLocalRef(cls);
}


/** Mostra o nasconde la tastiera di sistema. */
void softKeyboard(bool show) {
    if (!g_app || !g_app->activity) return;
    if (show) {
        resetImeBuffer();
        GameActivity_showSoftInput(g_app->activity, 0);
    }
    else GameActivity_hideSoftInput(g_app->activity, 0);
}


// ---- Ricezione dei broadcast ------------------------------------------

jobject g_multicastLock = nullptr;

/**
 * Chiede al Wi-Fi di consegnarci anche i pacchetti broadcast.
 *
 * Per risparmiare batteria Android li SCARTA di default prima ancora che
 * arrivino all'applicazione. Non e' un problema di permessi: la INTERNET c'e'
 * gia' e i pacchetti diretti passano. Riguarda solo broadcast e multicast, che
 * sono esattamente quelli con cui Blocky si fa trovare in rete locale — senza
 * questo blocco un tablet che ospita una partita resta invisibile agli altri.
 *
 * Il blocco resta preso per tutta la vita dell'app: prenderlo e rilasciarlo a
 * ogni partita complicherebbe il codice per un risparmio trascurabile.
 */
void acquireMulticastLock() {
    if (g_multicastLock) return;
    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) return;

    jclass ctxCls = env->FindClass("android/content/Context");
    jclass activityCls = env->GetObjectClass(activity);
    if (!ctxCls || !activityCls) { clearPendingException(env); return; }

    jmethodID getAppCtx = env->GetMethodID(activityCls, "getApplicationContext",
                                           "()Landroid/content/Context;");
    jmethodID getService = env->GetMethodID(ctxCls, "getSystemService",
                                            "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!getAppCtx || !getService) {
        clearPendingException(env);
        env->DeleteLocalRef(ctxCls);
        env->DeleteLocalRef(activityCls);
        return;
    }

    jobject appCtx = env->CallObjectMethod(activity, getAppCtx);
    jstring wifiName = env->NewStringUTF("wifi");
    jobject wifi = appCtx ? env->CallObjectMethod(appCtx, getService, wifiName) : nullptr;
    clearPendingException(env);

    if (wifi) {
        jclass wifiCls = env->GetObjectClass(wifi);
        jmethodID createLock = wifiCls
            ? env->GetMethodID(wifiCls, "createMulticastLock",
                               "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;")
            : nullptr;
        if (createLock) {
            jstring tag = env->NewStringUTF("blocky");
            jobject lock = env->CallObjectMethod(wifi, createLock, tag);
            if (lock && !env->ExceptionCheck()) {
                jclass lockCls = env->GetObjectClass(lock);
                jmethodID setRef = env->GetMethodID(lockCls, "setReferenceCounted", "(Z)V");
                jmethodID acquire = env->GetMethodID(lockCls, "acquire", "()V");
                if (setRef) env->CallVoidMethod(lock, setRef, JNI_FALSE);
                if (acquire) env->CallVoidMethod(lock, acquire);
                if (!env->ExceptionCheck()) {
                    // Riferimento globale: quello locale muore a fine funzione e
                    // con lui il blocco verrebbe rilasciato dal garbage collector.
                    g_multicastLock = env->NewGlobalRef(lock);
                    BRIDGE_LOGI("broadcast di rete abilitati (multicast lock)");
                }
                env->DeleteLocalRef(lockCls);
            }
            clearPendingException(env);
            if (lock) env->DeleteLocalRef(lock);
            env->DeleteLocalRef(tag);
        }
        clearPendingException(env);
        if (wifiCls) env->DeleteLocalRef(wifiCls);
        env->DeleteLocalRef(wifi);
    } else {
        BRIDGE_LOGE("WifiManager non disponibile: i broadcast resteranno filtrati");
    }

    env->DeleteLocalRef(wifiName);
    if (appCtx) env->DeleteLocalRef(appCtx);
    env->DeleteLocalRef(ctxCls);
    env->DeleteLocalRef(activityCls);
}

} // namespace

/**
 * Da chiamare una volta per frame: se la tastiera ha modificato il testo,
 * traduce la modifica in eventi per ImGui.
 *
 * Vale per qualunque campo di testo del motore, non per uno in particolare:
 * quello che decide se la tastiera serve e' io.WantTextInput di ImGui, che e'
 * vero per il campo che ha il cursore, qualunque esso sia.
 */
void resetTextInput(const char* text, int length) {
    syncTextInput(text, length);
}

void pumpTextInput(android_app* app) {
    if (!app || !app->activity) return;
    // Si interroga a ogni frame invece di fidarsi del flag textInputState: la
    // lettura e' una semplice copia in memoria, e cosi' il testo arriva anche
    // se il flag non viene alzato (per esempio mentre la tastiera e' in fase di
    // composizione, come fa Gboard con i suggerimenti).
    app->textInputState = 0;
    GameActivity_getTextInputState(app->activity, &onImeStateChanged, nullptr);
}


/**
 * Mostra un avviso di sistema e chiude l'app.
 *
 * Passa dal Kotlin perche' i casi in cui serve sono proprio quelli in cui il
 * nostro sistema di interfaccia non e' disponibile — per esempio quando non si
 * riesce nemmeno a creare un contesto grafico. Un messaggio in logcat non
 * basterebbe: chi usa il gioco non lo vede, e si troverebbe solo uno schermo
 * nero senza spiegazione.
 */

/**
 * Chiede al sistema di mostrare il suo selettore file.
 *
 * Non aspetta niente: la risposta arrivera' piu' tardi, dal lato Java, alla
 * funzione nativeFilePicked qui sotto. E' la differenza che rende questa
 * l'unica funzione del motore impossibile da "tradurre" da Windows: li'
 * GetOpenFileNameW restituisce un percorso subito, qui il sistema risponde
 * quando vuole lui, perche' nel frattempo l'utente sta usando un'altra app.
 */
void openFilePicker(const char* mimeType) {
    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) { plat::deliverPickedFile(""); return; }

    jclass cls = env->GetObjectClass(activity);
    jmethodID open = cls
        ? env->GetMethodID(cls, "openFilePicker", "(Ljava/lang/String;)V")
        : nullptr;
    if (open) {
        jstring m = env->NewStringUTF(mimeType ? mimeType : "*/*");
        env->CallVoidMethod(activity, open, m);
        env->DeleteLocalRef(m);
    } else {
        // Metodo assente: meglio rispondere "annullato" che lasciare chi
        // aspetta bloccato per sempre.
        plat::deliverPickedFile("");
    }
    clearPendingException(env);
    if (cls) env->DeleteLocalRef(cls);
}

void showFatalMessage(const char* message) {
    if (!message) return;
    BRIDGE_LOGE("%s", message);

    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID show = cls
        ? env->GetMethodID(cls, "showFatalMessage", "(Ljava/lang/String;)V")
        : nullptr;
    if (show) {
        jstring text = env->NewStringUTF(message);
        env->CallVoidMethod(activity, show, text);
        env->DeleteLocalRef(text);
    }
    clearPendingException(env);
    if (cls) env->DeleteLocalRef(cls);
}

unsigned int unicodeFromKey(int32_t keyCode, int32_t metaState) {
    JniScope jni(vm());
    JNIEnv* env = jni.env;
    if (!env) return 0;

    unsigned int cp = 0;
    jclass keyEventCls = env->FindClass("android/view/KeyEvent");
    if (keyEventCls) {
        jmethodID ctor = env->GetMethodID(keyEventCls, "<init>", "(II)V");
        jmethodID getUnicodeChar = env->GetMethodID(keyEventCls, "getUnicodeChar", "(I)I");
        if (ctor && getUnicodeChar) {
            // 0 = ACTION_DOWN: l'oggetto serve solo come tramite per la
            // conversione, l'azione in se' non ha effetto sul risultato.
            jobject ev = env->NewObject(keyEventCls, ctor, 0, (jint)keyCode);
            if (ev) {
                const jint c = env->CallIntMethod(ev, getUnicodeChar, (jint)metaState);
                if (!env->ExceptionCheck() && c > 0) cp = (unsigned int)c;
                env->DeleteLocalRef(ev);
            }
        }
        clearPendingException(env);
        env->DeleteLocalRef(keyEventCls);
    }
    return cp;
}


/**
 * Il lato Java consegna qui il file scelto (vuoto = annullato).
 *
 * Gira sul thread che ha fatto la copia, non su quello del gioco: si limita a
 * depositare il percorso, e sara' pumpFilePicks() a eseguire la richiamata
 * dentro il frame, dove il motore e' in uno stato coerente.
 */
extern "C" JNIEXPORT void JNICALL
Java_com_blocky_preview_MainActivity_nativeFilePicked(JNIEnv* env, jobject, jstring path) {
    std::string out;
    if (path) {
        const char* s = env->GetStringUTFChars(path, nullptr);
        if (s) { out = s; env->ReleaseStringUTFChars(path, s); }
    }
    BRIDGE_LOGI("selettore file: %s", out.empty() ? "annullato" : out.c_str());
    plat::deliverPickedFile(out);
}

/**
 * Legge la firma dell'APK installato, una volta all'avvio.
 *
 * Sincrona (non un gancio con richiamata come gli altri): PackageManager
 * risponde subito, dal disco locale, senza bisogno che l'utente faccia
 * niente — non c'e' un "dopo" da aspettare, a differenza del selettore file
 * o del login.
 *
 * Stringa vuota = non verificabile (eccezione, o firma assente). Va
 * trattata da chi chiama come "manomesso", MAI come "va bene cosi'": e'
 * la stessa regola di ogni altro controllo di integrita' di questo motore.
 */
std::string readSigningCertSha256() {
    JniScope jni(vm());
    JNIEnv* env = jni.env;
    jobject activity = activityObject();
    if (!env || !activity) return {};

    jclass cls = env->GetObjectClass(activity);
    jmethodID m = cls
        ? env->GetMethodID(cls, "signingCertSha256", "()Ljava/lang/String;")
        : nullptr;
    std::string out;
    if (m) {
        jstring js = (jstring)env->CallObjectMethod(activity, m);
        if (js) {
            const char* chars = env->GetStringUTFChars(js, nullptr);
            if (chars) { out = chars; env->ReleaseStringUTFChars(js, chars); }
            env->DeleteLocalRef(js);
        }
    }
    clearPendingException(env);
    if (cls) env->DeleteLocalRef(cls);
    return out;
}

void installPlatformHooks(android_app* app) {
    g_app = app;
    plat::setAndroidOpenUrlHook(&openUrl);
    plat::setAndroidSoftKeyboardHook(&softKeyboard);
    plat::setAndroidFilePickHook(&openFilePicker);
    plat::setAndroidHostingHook(&hostingStateChanged);
    acquireMulticastLock();
    BRIDGE_LOGI("ganci piattaforma installati (link, tastiera, selettore file)");
}

} // namespace android
} // namespace blk
