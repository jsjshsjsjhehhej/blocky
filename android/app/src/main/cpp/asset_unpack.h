#pragma once

#include <jni.h>

#include <string>

namespace blk {
namespace android {

/**
 * Copia tutti gli asset dell'APK dentro destRoot, ricreando le sottocartelle.
 *
 * Gli asset di un APK non sono file veri: vivono compressi dentro l'archivio e
 * si aprono solo con AAssetManager. Il motore pero' li legge con ifstream,
 * stb_image e Assimp, che vogliono percorsi normali — estrarli una volta e'
 * l'unico modo per non riscrivere ogni caricamento del motore.
 *
 * Ritorna il numero di file scritti (0 = nulla estratto, qualcosa non va).
 */
int unpackApkAssets(JavaVM* vm, jobject gameActivity, const std::string& destRoot);

} // namespace android
} // namespace blk
