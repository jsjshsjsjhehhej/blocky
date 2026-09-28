#pragma once

#include <cstdint>
#include <string>

struct android_app;

namespace blk {
namespace android {

/**
 * Installa i ganci che il motore usa per le cose che su Android si fanno solo
 * da Java: aprire un link nel browser e far comparire la tastiera di sistema.
 * Da chiamare una volta all'avvio, prima che il motore parta.
 */
void installPlatformHooks(android_app* app);

/** Impronta SHA-256 della firma dell'APK installato, o vuota se non leggibile. */
std::string readSigningCertSha256();

/**
 * Carattere Unicode corrispondente a un tasto premuto, 0 se quel tasto non
 * produce testo (frecce, Invio, tasti di sistema).
 *
 * Il codice del tasto da solo non basta: la stessa posizione fisica produce
 * "a" o "A" a seconda dello Shift, e le lettere accentate dipendono dal layout
 * scelto dall'utente. La conversione giusta la sa solo KeyEvent di Android.
 */
unsigned int unicodeFromKey(int32_t keyCode, int32_t metaState);

/**
 * Consegna a ImGui il testo scritto sulla tastiera di sistema, se e cambiato.
 * Da chiamare una volta per frame.
 */
/**
 * Da chiamare quando il cursore entra in un campo di testo, passando il
 * contenuto attuale del campo: allinea la tastiera di sistema a quel testo.
 */
void resetTextInput(const char* text, int length);

void pumpTextInput(android_app* app);

/**
 * Mostra un avviso di sistema e chiude l app. Da usare quando il motore non
 * puo partire affatto e quindi non puo nemmeno disegnare l avviso da solo.
 */
void showFatalMessage(const char* message);

} // namespace android
} // namespace blk
