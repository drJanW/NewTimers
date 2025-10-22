// lib/AudioManager20251005/PlayFragment.h
#pragma once
#include <Arduino.h>

// Houd header macro-vrij: geen referentie naar MAX_FADE_STEPS hier.

struct AudioFragment {
  uint8_t  dirIndex;
  uint8_t  fileIndex;
  uint32_t startMs;
  uint32_t durationMs;
  uint16_t fadeMs;
};

namespace PlayAudioFragment {
  void prepareFadeArray(uint8_t steps);
  void start(uint8_t dirIndex, uint8_t fileIndex, uint32_t startMs, uint32_t durationMs, uint16_t fadeMs);
  void stop();
  void updateGain(); // compat
}

// Buiten namespace voor bestaande call-sites
bool getRandomFragment(AudioFragment& outFrag);
