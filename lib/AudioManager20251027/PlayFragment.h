// lib/AudioManager20251022/PlayFragment.h
#pragma once
#include <Arduino.h>

struct AudioFragment {
  uint8_t  dirIndex;
  uint8_t  fileIndex;
  uint32_t startMs;
  uint32_t durationMs;
  uint16_t fadeMs;
};

namespace PlayAudioFragment {
  bool start(const AudioFragment& fragment);
  void stop(uint16_t fadeOutMs = 0xFFFF); // default uses current fade; <=40ms treated as immediate
  void abortImmediate();
  void updateGain();
}
