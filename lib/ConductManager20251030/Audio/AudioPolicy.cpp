#include "AudioPolicy.h"
#include "Globals.h" // only for constants like MAX_AUDIO_VOLUME

namespace {

constexpr float kDistanceMinMm = 40.0f;
constexpr float kDistanceMaxMm = 1000.0f;
constexpr uint32_t kIntervalMinMs = 160;
constexpr uint32_t kIntervalMaxMs = 2400;
constexpr float kIntervalMinMsF = static_cast<float>(kIntervalMinMs);
constexpr float kIntervalMaxMsF = static_cast<float>(kIntervalMaxMs);

constexpr size_t kMaxThemeDirs = 16;

constexpr float kDistanceVolumeNear = 1.0f;   // multiplier when visitor is right in front of the dome
constexpr float kDistanceVolumeFar  = 0.35f;  // multiplier when the visitor is at the edge of sensing range

float s_distanceVolume = kDistanceVolumeNear;

bool    s_themeActive = false;
uint8_t s_themeDirs[kMaxThemeDirs];
size_t  s_themeDirCount = 0;
String  s_themeId;

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

void clearThemeBox() {
    if (!s_themeActive && s_themeDirCount == 0 && s_themeId.isEmpty()) {
        return;
    }
    s_themeActive = false;
    s_themeDirCount = 0;
    s_themeId = "";
    PF("[AudioPolicy] Theme box cleared\n");
}

void setThemeBox(const uint8_t* dirs, size_t count, const String& id) {
    if (!dirs || count == 0) {
        clearThemeBox();
        return;
    }

    const size_t limit = count > kMaxThemeDirs ? kMaxThemeDirs : count;
    for (size_t i = 0; i < limit; ++i) {
        s_themeDirs[i] = dirs[i];
    }
    s_themeDirCount = limit;
    s_themeActive = s_themeDirCount > 0;
    s_themeId = id;

    PF("[AudioPolicy] Theme box %s set with %u directories\n",
       s_themeId.c_str(), static_cast<unsigned>(s_themeDirCount));
}

bool themeBoxActive() {
    return s_themeActive && s_themeDirCount > 0;
}

const uint8_t* themeBoxDirs(size_t& count) {
    count = s_themeDirCount;
    return themeBoxActive() ? s_themeDirs : nullptr;
}

const String& themeBoxId() {
    return s_themeId;
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

float updateDistancePlaybackVolume(float distanceMm) {
    if (distanceMm <= 0.0f) {
        return s_distanceVolume;
    }

    const float clampedDistance = clamp(distanceMm, kDistanceMinMm, kDistanceMaxMm);
    const float mapped = map(clampedDistance,
                             kDistanceMinMm,
                             kDistanceMaxMm,
                             kDistanceVolumeNear,
                             kDistanceVolumeFar);

    s_distanceVolume = clamp(mapped, kDistanceVolumeFar, kDistanceVolumeNear);
    return s_distanceVolume;
}

}
