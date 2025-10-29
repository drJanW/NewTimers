#pragma once

#include "AudioManager.h"

namespace PlayPCM {

using PCM = AudioManager::PCMClipDesc;

// Loads a 16-bit mono PCM WAV into memory and caches it locally.
// Returns a pointer to the cached clip on success, nullptr on failure.
PCM* loadFromSD(const char* path);

// Plays the provided clip pointer using the supplied volume (0..1).
// Optional durationMs stops playback after the given milliseconds (0 = full clip).
bool play(const PCM* clip, float volume, uint16_t durationMs = 0);

}
