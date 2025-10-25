#include "Globals.h"
#include "AudioManager.h"
#include "PlayFragment.h"
#include "PlaySentence.h"
#include "SDVoting.h"
#include "TimerManager.h"

AudioManager& AudioManager::instance() {
  static AudioManager inst;
  return inst;
}

AudioManager::AudioManager()
  : audioOutput()
{
}

static AudioOutputI2S_Metered* gMeterInstance = nullptr;

void audioMeterTick() {
  if (gMeterInstance) {
    gMeterInstance->publishLevel();
  }
}

bool AudioOutputI2S_Metered::begin()
{
  _acc = 0;
  _cnt = 0;
  _publishDue = false;
  setAudioLevelRaw(0);

  gMeterInstance = this;

  auto &tm = TimerManager::instance();
  tm.cancel(audioMeterTick);
  if (!tm.create(50, 0, audioMeterTick)) {
    PF("[AudioMeter] Failed to start meter timer\n");
  }

  return AudioOutputI2S::begin();
}

bool AudioOutputI2S_Metered::ConsumeSample(int16_t sample[2])
{
  int32_t v = sample[0];
  _acc += static_cast<int64_t>(v) * static_cast<int64_t>(v);
  _cnt++;
  _publishDue = true;
  return AudioOutputI2S::ConsumeSample(sample);
}

void AudioOutputI2S_Metered::publishLevel()
{
  if (!_publishDue || _cnt == 0) {
    return;
  }
  _publishDue = false;

  float mean = static_cast<float>(_acc) / static_cast<float>(_cnt);
  uint16_t rms = static_cast<uint16_t>(sqrtf(mean));
  setAudioLevelRaw(rms);
  _acc = 0;
  _cnt = 0;
}

void AudioManager::releaseDecoder()
{
  if (audioMp3Decoder) {
    audioMp3Decoder->stop();
    delete audioMp3Decoder;
    audioMp3Decoder = nullptr;
  }
}

void AudioManager::releaseSource()
{
  if (audioFile) {
    delete audioFile;
    audioFile = nullptr;
  }
}

void AudioManager::finalizePlayback()
{
  releaseDecoder();
  releaseSource();

  setAudioLevelRaw(0);
  audioOutput.SetGain(getBaseGain());
  setAudioBusy(false);
  setFragmentPlaying(false);
  setSentencePlaying(false);
}

void AudioManager::begin()
{
  audioOutput.SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  audioOutput.begin();
  audioOutput.SetGain(getBaseGain());
}

void AudioManager::stop()
{
  finalizePlayback();
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
      finalizePlayback();
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

bool AudioManager::startFragment(const AudioFragment& frag) {
  if (!PlayAudioFragment::start(frag)) {
    PF("[Audio] startFragment failed for %03u/%03u\n", frag.dirIndex, frag.fileIndex);
    return false;
  }
  return true;
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
