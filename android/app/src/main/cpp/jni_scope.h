#pragma once

#include <jni.h>

namespace blk {
namespace android {

/**
 * Aggancia il thread corrente alla VM Java per la durata dello scope.
 *
 * android_main gira su un thread suo, che non e' detto sia gia' agganciato.
 * Lo scope lo aggancia se serve e lo sgancia all'uscita: sganciare un thread
 * che NON abbiamo agganciato noi farebbe crashare il runtime, per questo il
 * flag invece di una DetachCurrentThread incondizionata.
 */
struct JniScope {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;

    explicit JniScope(JavaVM* v) : vm(v) {
        if (!vm) return;
        const jint st = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (st == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        } else if (st != JNI_OK) {
            env = nullptr;
        }
    }
    ~JniScope() {
        if (attached && vm) vm->DetachCurrentThread();
    }
    JniScope(const JniScope&) = delete;
    JniScope& operator=(const JniScope&) = delete;
};

/** Scarta un'eccezione Java in sospeso: lasciarla pendente fa abortire la VM
 *  alla prossima chiamata JNI. */
inline void clearPendingException(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

} // namespace android
} // namespace blk
