#pragma once

// AudioMixer non disegna niente durante il gioco: le voci attive si vedono
// nel pannello Prestazioni (F4).

#include "systems/AudioMixer.h"

inline void drawAudioMixerHud(const AudioMixer&) {}
