#pragma once

#include <Arduino.h>
#include <AudioOutputI2S.h>
#include <AudioFileSource.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "libhelix-mp3/mp3dec.h"
#include "Globals.h"
#include "SonarPingGenerator.h"

struct AudioFragment;

// Inlined audiometer: geen extra bestand
class AudioOutputI2S_Metered : public AudioOutputI2S {
public:
  using AudioOutputI2S::AudioOutputI2S;

  bool begin() override;
  bool ConsumeSample(int16_t sample[2]) override;

protected:
  void publishLevel();
  friend void audioMeterTick();
  uint64_t  _acc = 0;
  uint32_t  _cnt = 0;
  bool      _publishDue = false;
};

class AudioManager {
public:
  static AudioManager& instance();

  void begin();
  void stop();
  void update();

  void notifySonarDistance(float distanceMm);

  bool isBusy() const;
  bool isFragmentPlaying() const;
  bool isSentencePlaying() const;

  bool startFragment(const AudioFragment& frag);
  void startTTS(const String& phrase);
  void stopFragment();
  bool playSonarPing(const SonarPingProfile& profile);

  void setWebLevel(float value);
  float getWebLevel() const;
  void capVolume(float maxValue);
  void updateGain();

  AudioOutputI2S_Metered audioOutput;     // was AudioOutputI2S
  AudioFileSource*       audioFile = nullptr;
  AudioGeneratorMP3*     audioMp3Decoder = nullptr;
  SonarPingGenerator*    audioPingGenerator = nullptr;
  HMP3Decoder            helixDecoder = nullptr;

private:
  AudioManager();
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;

  void releaseDecoder();
  void releaseSource();
  void finalizePlayback();
  void finalizePing();
  void cancelSonarTimer();
  void scheduleSonar(uint32_t delayMs);
  void handleSonarTick();
  bool buildSonarProfile(float distanceMm, SonarPingProfile& profile, uint32_t& intervalMs);

  friend void sonarPingTick();

  struct SonarState {
    bool enabled = false;
    bool hasDistance = false;
    float lastDistance = 0.0f;
    uint32_t intervalMs = 0;
    SonarPingProfile profile;
  };

  SonarState sonarState_;
};
