#include "AudioState.h"
#include "HWconfig.h"

#include <atomic>

namespace {
std::atomic<float> g_audioLevel{0.0f};
std::atomic<float> g_baseGain{MAX_AUDIO_VOLUME};
std::atomic<float> g_webAudioLevel{1.0f};
std::atomic<float> g_timeAudioLevel{1.0f};
std::atomic<int16_t> g_audioLevelRaw{0};
std::atomic<bool> g_audioBusy{false};
std::atomic<uint8_t> g_currentDir{0};
std::atomic<uint8_t> g_currentFile{0};
std::atomic<bool> g_currentValid{false};
std::atomic<bool> g_fragmentPlaying{false};
std::atomic<bool> g_sentencePlaying{false};
std::atomic<bool> g_ttsActive{false};
std::atomic<bool> g_wordPlaying{false};
std::atomic<int32_t> g_currentWordId{0};
} // namespace

float getAudioLevel() {
    return g_audioLevel.load(std::memory_order_relaxed);
}

void setAudioLevel(float value) {
    g_audioLevel.store(value, std::memory_order_relaxed);
}

float getWebAudioLevel() {
    return g_webAudioLevel.load(std::memory_order_relaxed);
}

void setWebAudioLevel(float value) {
    g_webAudioLevel.store(value, std::memory_order_relaxed);
}

float getTimeAudioLevel() {
    return g_timeAudioLevel.load(std::memory_order_relaxed);
}

void setTimeAudioLevel(float value) {
    g_timeAudioLevel.store(value, std::memory_order_relaxed);
}

void setAudioLevelRaw(int16_t value) {
    g_audioLevelRaw.store(value, std::memory_order_relaxed);
}

int16_t getAudioLevelRaw() {
    return g_audioLevelRaw.load(std::memory_order_relaxed);
}

float getBaseGain() {
    return g_baseGain.load(std::memory_order_relaxed);
}

void setBaseGain(float value) {
    g_baseGain.store(value, std::memory_order_relaxed);
}

float getEffectiveAudioLevel() {
    const float level = g_audioLevel.load(std::memory_order_relaxed);
    const float web = g_webAudioLevel.load(std::memory_order_relaxed);
    const float ttl = g_timeAudioLevel.load(std::memory_order_relaxed);
    return level * web * ttl;
}

bool isAudioBusy() {
    return g_audioBusy.load(std::memory_order_relaxed);
}

void setAudioBusy(bool value) {
    g_audioBusy.store(value, std::memory_order_relaxed);
}

bool getCurrentDirFile(uint8_t& dir, uint8_t& file) {
    if (!g_currentValid.load(std::memory_order_relaxed)) {
        return false;
    }
    dir = g_currentDir.load(std::memory_order_relaxed);
    file = g_currentFile.load(std::memory_order_relaxed);
    return true;
}

void setCurrentDirFile(uint8_t dir, uint8_t file) {
    g_currentDir.store(dir, std::memory_order_relaxed);
    g_currentFile.store(file, std::memory_order_relaxed);
    g_currentValid.store(true, std::memory_order_relaxed);
}

bool isFragmentPlaying() {
    return g_fragmentPlaying.load(std::memory_order_relaxed);
}

void setFragmentPlaying(bool value) {
    g_fragmentPlaying.store(value, std::memory_order_relaxed);
}

bool isSentencePlaying() {
    return g_sentencePlaying.load(std::memory_order_relaxed);
}

void setSentencePlaying(bool value) {
    g_sentencePlaying.store(value, std::memory_order_relaxed);
}

bool isTtsActive() {
    return g_ttsActive.load(std::memory_order_relaxed);
}

void setTtsActive(bool value) {
    g_ttsActive.store(value, std::memory_order_relaxed);
}

bool isWordPlaying() {
    return g_wordPlaying.load(std::memory_order_relaxed);
}

void setWordPlaying(bool value) {
    g_wordPlaying.store(value, std::memory_order_relaxed);
}

int32_t getCurrentWordId() {
    return g_currentWordId.load(std::memory_order_relaxed);
}

void setCurrentWordId(int32_t value) {
    g_currentWordId.store(value, std::memory_order_relaxed);
}
