#include "Globals.h"
#include "AudioManager.h"
#include "PlayFragment.h"
#include "PlaySentence.h"
#include "SDVoting.h"

AudioManager& AudioManager::instance() {
  static AudioManager inst;
  return inst;
}

AudioManager::AudioManager()
  : audioOutput()
{
}

void AudioManager::begin()
{
  audioOutput.SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  audioOutput.begin();
  audioOutput.SetGain(getBaseGain());
}

void AudioManager::stop()
{
  if (audioMp3Decoder) {
    audioMp3Decoder->stop();
    delete audioMp3Decoder;
    audioMp3Decoder = nullptr;
  }
  if (audioFile) {
    delete audioFile;
    audioFile = nullptr;
  }
  setAudioLevelRaw(0);
  setAudioBusy(false);
  setFragmentPlaying(false);
  setSentencePlaying(false);
  audioOutput.SetGain(getBaseGain());
}

void AudioManager::update()
{
  // Zinnen laten doorrollen
  if (isSentencePlaying()) {
    PlaySentence::update();
  }

  // Decoder pompen en opruimen zodra klaar
  if (audioMp3Decoder) {
    bool keep = audioMp3Decoder->loop();
    if (!keep) {
      audioMp3Decoder->stop();
      delete audioMp3Decoder;
      audioMp3Decoder = nullptr;

      if (audioFile) {
        delete audioFile;
        audioFile = nullptr;
      }

      setAudioLevelRaw(0);
      audioOutput.SetGain(getBaseGain());
      setAudioBusy(false);
      setFragmentPlaying(false);
      setSentencePlaying(false);
    }
  }
}

bool AudioManager::isBusy() const {
  return isAudioBusy();
}

bool AudioManager::isFragmentPlaying() const {
  return ::isFragmentPlaying();
}

bool AudioManager::isSentencePlaying() const {
  return ::isSentencePlaying();
}

void AudioManager::startFragment(const AudioFragment& frag) {
  PlayAudioFragment::start(frag.dirIndex, frag.fileIndex, frag.startMs, frag.durationMs, frag.fadeMs);
}

void AudioManager::startTTS(const String& phrase) {
  PlaySentence::startTTS(phrase);
}

void AudioManager::stopFragment() {
  PlayAudioFragment::stop();
}

void AudioManager::setWebLevel(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  setWebAudioLevel(value);
  updateGain();
}

float AudioManager::getWebLevel() const {
  return getWebAudioLevel();
}

void AudioManager::capVolume(float maxValue) {
  if (maxValue < 0.0f) maxValue = 0.0f;
  if (maxValue > 1.0f) maxValue = 1.0f;
  float current = getWebAudioLevel();
  if (current > maxValue) {
    setWebAudioLevel(maxValue);
    updateGain();
  }
}

void AudioManager::updateGain() {
  PlayAudioFragment::updateGain();
  if (!isFragmentPlaying() && !isSentencePlaying()) {
    audioOutput.SetGain(getBaseGain() * getWebAudioLevel());
  }
}
