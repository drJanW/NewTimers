#pragma once

#include <Arduino.h>
#include <AudioOutputI2S.h>
#include <AudioFileSource.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "libhelix-mp3/mp3dec.h"
#include "Globals.h"

struct AudioFragment;

// Inlined audiometer: geen extra bestand
class AudioOutputI2S_Metered : public AudioOutputI2S {
public:
  using AudioOutputI2S::AudioOutputI2S;

  bool begin() override {
    _acc = 0; _cnt = 0; _t0 = millis();
    setAudioLevelRaw(0);
    return AudioOutputI2S::begin();
  }

  // Library levert stereo buffer; project speelt mono.
  // Meet sample[0] en publiceer elke ~50 ms.
  bool ConsumeSample(int16_t sample[2]) override {
    int32_t v = sample[0];
    _acc += (int64_t)v * v;
    _cnt++;

    uint32_t now = millis();
    if ((now - _t0) >= 50 && _cnt) {
      float mean = (float)_acc / (float)_cnt;          // mean square
      uint16_t rms = (uint16_t)sqrtf(mean);            // 0..32767
      setAudioLevelRaw(rms);                            // gebruikt door LightManager
      _acc = 0; _cnt = 0; _t0 = now;
    }
    return AudioOutputI2S::ConsumeSample(sample);
  }

private:
  uint64_t  _acc = 0;
  uint32_t  _cnt = 0;
  uint32_t  _t0  = 0;
};

class AudioManager {
public:
  static AudioManager& instance();

  void begin();
  void stop();
  void update();

  bool isBusy() const;
  bool isFragmentPlaying() const;
  bool isSentencePlaying() const;

  void startFragment(const AudioFragment& frag);
  void startTTS(const String& phrase);
  void stopFragment();

  void setWebLevel(float value);
  float getWebLevel() const;
  void capVolume(float maxValue);
  void updateGain();

  AudioOutputI2S_Metered audioOutput;     // was AudioOutputI2S
  AudioFileSource*       audioFile = nullptr;
  AudioGeneratorMP3*     audioMp3Decoder = nullptr;
  HMP3Decoder            helixDecoder = nullptr;

private:
  AudioManager();
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;
};
