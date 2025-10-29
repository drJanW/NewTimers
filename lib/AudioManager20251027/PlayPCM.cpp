#include "PlayPCM.h"

#include "Globals.h"
#include "SDVoting.h"
#include "TimerManager.h"
#include <SD.h>
#include <cstring>
#include <new>

namespace PlayPCM {

namespace {

using PCM = AudioManager::PCMClipDesc;

PCM s_cachedClip = {};
std::unique_ptr<int16_t[]> s_cachedStorage;

inline uint16_t readLE16(const uint8_t* buf) {
  return static_cast<uint16_t>(buf[0] | (static_cast<uint16_t>(buf[1]) << 8));
}

inline uint32_t readLE32(const uint8_t* buf) {
  return static_cast<uint32_t>(buf[0]) |
         (static_cast<uint32_t>(buf[1]) << 8) |
         (static_cast<uint32_t>(buf[2]) << 16) |
         (static_cast<uint32_t>(buf[3]) << 24);
}

constexpr uint32_t kExpectedSampleRate = 22050;
constexpr uint16_t kExpectedChannels = 1;
constexpr uint16_t kExpectedBitsPerSample = 16;
// Policy: see AudioManager README §6 – ping.wav must stay fixed-format PCM.

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

bool isValidClip(const PCM* clip) {
  return clip && clip->samples && clip->sampleCount > 0 && clip->sampleRate > 0;
}

bool playInternal(const PCM* clip, float volume) {
  if (!isValidClip(clip)) {
    PF("[PlayPCM] playInternal: invalid clip pointer\n");
    return false;
  }

  const float clamped = clamp01(volume);
  const bool started = AudioManager::instance().playPCMClip(*clip, clamped);
  if (!started) {
    PF("[PlayPCM] playInternal failed (vol=%.2f samples=%lu sr=%lu)\n",
       static_cast<double>(clamped),
       static_cast<unsigned long>(clip->sampleCount),
       static_cast<unsigned long>(clip->sampleRate));
  }
  return started;
}

bool loadClip(const char* path, PCM& outClip, std::unique_ptr<int16_t[]>& storage) {
  outClip = {};
  storage.reset();

  if (!isSDReady()) {
    PF("[PlayPCM] SD not ready, skipping %s\n", path);
    return false;
  }
  if (!path) {
    PF("[PlayPCM] Invalid path (null)\n");
    return false;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) {
    PF("[PlayPCM] Failed to open %s\n", path);
    return false;
  }

  uint8_t header[44];
  if (file.read(header, sizeof(header)) != sizeof(header) ||
      std::memcmp(header + 0, "RIFF", 4) != 0 ||
      std::memcmp(header + 8, "WAVE", 4) != 0 ||
      std::memcmp(header + 12, "fmt ", 4) != 0 ||
      std::memcmp(header + 36, "data", 4) != 0) {
    PF("[PlayPCM] %s has unexpected WAV header\n", path);
    file.close();
    return false;
  }

  const uint16_t audioFormat = readLE16(header + 20);
  const uint16_t numChannels = readLE16(header + 22);
  const uint32_t sampleRate = readLE32(header + 24);
  const uint16_t bitsPerSample = readLE16(header + 34);
  const uint32_t dataSize = readLE32(header + 40);

  if (audioFormat != 1 ||
      numChannels != kExpectedChannels ||
      bitsPerSample != kExpectedBitsPerSample ||
      sampleRate != kExpectedSampleRate ||
      dataSize == 0) {
    PF("[PlayPCM] %s violates ping.wav policy (fmt=%u ch=%u bits=%u sr=%lu size=%lu)\n",
       path,
       static_cast<unsigned>(audioFormat),
       static_cast<unsigned>(numChannels),
       static_cast<unsigned>(bitsPerSample),
       static_cast<unsigned long>(sampleRate),
       static_cast<unsigned long>(dataSize));
    file.close();
    return false;
  }

  const uint32_t sampleCount = dataSize / (kExpectedBitsPerSample / 8U);
  if (sampleCount == 0) {
    PF("[PlayPCM] %s contains no samples\n", path);
    file.close();
    return false;
  }

  std::unique_ptr<int16_t[]> buffer(new (std::nothrow) int16_t[sampleCount]);
  if (!buffer) {
    PF("[PlayPCM] Out of memory loading %s\n", path);
    file.close();
    return false;
  }

  const size_t bytesToRead = static_cast<size_t>(dataSize);
  if (file.read(reinterpret_cast<uint8_t*>(buffer.get()), bytesToRead) != bytesToRead) {
    PF("[PlayPCM] Short read while loading %s\n", path);
    file.close();
    return false;
  }

  file.close();

  storage = std::move(buffer);
  outClip.samples = storage.get();
  outClip.sampleCount = sampleCount;
  outClip.sampleRate = sampleRate;
  outClip.durationMs = static_cast<uint32_t>((static_cast<uint64_t>(sampleCount) * 1000ULL + sampleRate / 2ULL) / sampleRate);

  PF("[PlayPCM] Loaded %s: %lu samples @ %lu Hz (%lu ms)\n",
     path,
     static_cast<unsigned long>(outClip.sampleCount),
     static_cast<unsigned long>(outClip.sampleRate),
     static_cast<unsigned long>(outClip.durationMs));
  return true;
}

TimerManager& timers() {
  return TimerManager::instance();
}

void cb_stopPCMPlayback() {
  AudioManager::instance().stopPCMClip();
}

void stopAfter(uint32_t durationMs) {
  auto& tm = timers();
  tm.cancel(cb_stopPCMPlayback);

  if (durationMs == 0U) {
    durationMs = 1U; // ensure timer fires even for immediate stops
  }

  if (!tm.create(durationMs, 1, cb_stopPCMPlayback)) {
    PF("[PlayPCM] Failed to arm stop timer (%lu ms)\n", static_cast<unsigned long>(durationMs));
    return;
  }
}

} // namespace

PCM* loadFromSD(const char* path) {
  PCM clip = {};
  std::unique_ptr<int16_t[]> storage;
  if (!loadClip(path, clip, storage)) {
    s_cachedClip = {};
    s_cachedStorage.reset();
    return nullptr;
  }

  s_cachedStorage = std::move(storage);
  s_cachedClip = clip;
  s_cachedClip.samples = s_cachedStorage.get();
  return s_cachedClip.samples ? &s_cachedClip : nullptr;
}

bool play(const PCM* clipPtr, float volume, uint16_t durationMs) {
  const bool started = playInternal(clipPtr, volume);
  if (!started) {
    stopAfter(0);
    return false;
  }

  uint32_t effectiveDuration = durationMs;
  if (effectiveDuration == 0U && clipPtr) {
    const uint32_t durationClipMs = clipPtr->durationMs;
    effectiveDuration = durationClipMs;
  }
  stopAfter(effectiveDuration);
  return true;
}

} // namespace PlayPCM
