#include "AudioPolicy.h"
#include "Globals.h" // only for constants like MAX_AUDIO_VOLUME

namespace {

constexpr float kDistanceMinMm = 120.0f;
constexpr float kDistanceMaxMm = 1100.0f;
constexpr uint32_t kIntervalMinMs = 200;
constexpr uint32_t kIntervalMaxMs = 3000;
constexpr float kIntervalMinMsF = static_cast<float>(kIntervalMinMs);
constexpr float kIntervalMaxMsF = static_cast<float>(kIntervalMaxMs);

} // namespace

namespace AudioPolicy {

bool canPlayFragment() {
    auto& audio = AudioManager::instance();

    // Rule: fragments only if audio not busy and no TTS active
    return !audio.isBusy() && !audio.isSentencePlaying();
}

bool canPlaySentence() {
    // Rule: TTS always wins; can interrupt fragment
    return true;
}

float applyVolumeRules(float requested, bool quietHours) {
   // float v = requested;
   // if (v < 0.0f) v = 0.0f;
   // if (v > 1.0f) v = 1.0f;
   // if (quietHours && v > 0.3f) v = 0.3f; // cap volume at night
    return clamp(requested, 0.0f, 1.0f);
}

bool requestFragment(const AudioFragment& frag) {
    auto& audio = AudioManager::instance();
    if (audio.isPCMClipActive()) {
        audio.stopPCMClip();
    }
    if (!canPlayFragment()) {
        return false;
    }
    return audio.startFragment(frag);
}

void requestSentence(const String& phrase) {
    if (canPlaySentence()) {
        AudioManager::instance().startTTS(phrase);
    }
}

bool distancePlaybackInterval(float distanceMm, uint32_t& intervalMs) {
    if (distanceMm <= 0.0f || distanceMm > kDistanceMaxMm) {
        return false;
    }

    const float clamped      = clamp(distanceMm, kDistanceMinMm, kDistanceMaxMm);
    const float mapped       = map(clamped,
                                   kDistanceMinMm,
                                   kDistanceMaxMm,
                                   kIntervalMinMsF,
                                   kIntervalMaxMsF);
    const float bounded      = clamp(mapped, kIntervalMinMsF, kIntervalMaxMsF);

    intervalMs = static_cast<uint32_t>(bounded + 0.5f);
    return true;
}

void updateDistancePlaybackVolume(float distanceMm) {
    (void)distanceMm;
    // TODO: Implement distance-proportional volume shaping.
}

}
