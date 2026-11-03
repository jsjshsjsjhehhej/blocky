/* Unica unita' di compilazione con il codice di miniaudio (vedi AudioMixer.h).
 * Encoder e generatori di onde non servono: il motore riproduce file, non
 * registra ne' sintetizza. */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
