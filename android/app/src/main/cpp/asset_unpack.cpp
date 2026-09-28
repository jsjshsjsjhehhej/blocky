#include "asset_unpack.h"

#include <jni.h>

#include "jni_scope.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#define UNPACK_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Blocky", __VA_ARGS__)
#define UNPACK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Blocky", __VA_ARGS__)

namespace blk {
namespace android {
namespace {

namespace fs = std::filesystem;


/**
 * Elenca il contenuto di una cartella di asset.
 *
 * Si passa per l'AssetManager Java e non per AAssetDir perche' l'API nativa
 * restituisce soltanto i file: le sottocartelle sono invisibili, quindi con
 * quella non si potrebbe scendere ricorsivamente in assets/textures e simili.
 */
std::vector<std::string> listDir(JNIEnv* env, jobject assetManager, const std::string& path) {
    std::vector<std::string> out;
    jclass cls = env->GetObjectClass(assetManager);
    if (!cls) return out;
    jmethodID list = env->GetMethodID(cls, "list", "(Ljava/lang/String;)[Ljava/lang/String;");
    if (!list) {
        clearPendingException(env);
        env->DeleteLocalRef(cls);
        return out;
    }

    jstring jpath = env->NewStringUTF(path.c_str());
    jobject raw = env->CallObjectMethod(assetManager, list, jpath);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        raw = nullptr;
    }
    if (raw) {
        jobjectArray arr = static_cast<jobjectArray>(raw);
        const jsize n = env->GetArrayLength(arr);
        out.reserve(static_cast<size_t>(n));
        for (jsize i = 0; i < n; ++i) {
            jstring js = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
            if (!js) continue;
            const char* chars = env->GetStringUTFChars(js, nullptr);
            if (chars) {
                out.emplace_back(chars);
                env->ReleaseStringUTFChars(js, chars);
            }
            env->DeleteLocalRef(js);
        }
        env->DeleteLocalRef(raw);
    }
    env->DeleteLocalRef(jpath);
    env->DeleteLocalRef(cls);
    return out;
}

bool copyOne(AAssetManager* am, const std::string& rel, const fs::path& dest) {
    AAsset* asset = AAssetManager_open(am, rel.c_str(), AASSET_MODE_STREAMING);
    if (!asset) return false;

    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);

    FILE* out = std::fopen(dest.string().c_str(), "wb");
    if (!out) {
        AAsset_close(asset);
        UNPACK_LOGE("asset: non riesco a scrivere %s", dest.string().c_str());
        return false;
    }

    char buf[16 * 1024];
    int n;
    bool ok = true;
    while ((n = AAsset_read(asset, buf, sizeof(buf))) > 0) {
        if (std::fwrite(buf, 1, static_cast<size_t>(n), out) != static_cast<size_t>(n)) {
            ok = false;
            break;
        }
    }
    if (n < 0) ok = false;

    std::fclose(out);
    AAsset_close(asset);
    return ok;
}

/**
 * Una voce senza figli e' un file; una con figli e' una cartella. L'APK non
 * conserva le cartelle vuote, quindi non c'e' il caso ambiguo "cartella che
 * sembra un file".
 */
void walk(JNIEnv* env, jobject assetManager, AAssetManager* am,
          const std::string& rel, const fs::path& destRoot, int& written) {
    for (const std::string& name : listDir(env, assetManager, rel)) {
        if (name.empty()) continue;
        const std::string childRel = rel.empty() ? name : rel + "/" + name;
        const std::vector<std::string> kids = listDir(env, assetManager, childRel);
        if (!kids.empty()) {
            walk(env, assetManager, am, childRel, destRoot, written);
        } else if (copyOne(am, childRel, destRoot / childRel)) {
            ++written;
        }
    }
}

} // namespace

int unpackApkAssets(JavaVM* vm, jobject gameActivity, const std::string& destRoot) {
    JniScope jni(vm);
    if (!jni.env || !gameActivity) {
        UNPACK_LOGE("asset: nessun ambiente JNI, estrazione saltata");
        return 0;
    }
    JNIEnv* env = jni.env;

    jclass activityCls = env->GetObjectClass(gameActivity);
    jmethodID getAssets = activityCls
        ? env->GetMethodID(activityCls, "getAssets", "()Landroid/content/res/AssetManager;")
        : nullptr;
    if (!getAssets) {
        clearPendingException(env);
        if (activityCls) env->DeleteLocalRef(activityCls);
        UNPACK_LOGE("asset: getAssets() non trovato");
        return 0;
    }

    jobject assetManager = env->CallObjectMethod(gameActivity, getAssets);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        assetManager = nullptr;
    }
    env->DeleteLocalRef(activityCls);
    if (!assetManager) {
        UNPACK_LOGE("asset: AssetManager nullo");
        return 0;
    }

    AAssetManager* am = AAssetManager_fromJava(env, assetManager);
    if (!am) {
        env->DeleteLocalRef(assetManager);
        UNPACK_LOGE("asset: AAssetManager_fromJava fallita");
        return 0;
    }

    const fs::path root(destRoot);
    std::error_code ec;
    fs::create_directories(root, ec);

    int written = 0;
    walk(env, assetManager, am, std::string(), root, written);
    env->DeleteLocalRef(assetManager);

    UNPACK_LOGI("asset: %d file estratti in %s", written, destRoot.c_str());
    return written;
}

} // namespace android
} // namespace blk
