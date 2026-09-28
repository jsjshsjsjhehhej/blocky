#pragma once

/**
 * Spostato in platform/AndroidAudio.h, dove gli altri backend di sistema.
 *
 * OpenSL ES e' un'API del sistema operativo, non del motore: stava in engine/
 * solo perche' e' nato risolvendo un bug dell'audio. Questo file resta come
 * rimando per i vecchi include (e perche' al momento della modifica il
 * filesystem non permetteva di cancellarlo: qualcosa tiene un handle sui file
 * del repo — e' anche l'origine dei file src/**\/*.tmp.* sparsi).
 */

#include "platform/AndroidAudio.h"
