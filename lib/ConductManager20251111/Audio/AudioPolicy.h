#pragma once
#include <Arduino.h>
#include "AudioManager.h"
#include "PlayFragment.h"

namespace AudioPolicy {

    // Arbitration checks
    bool canPlayFragment();
    bool canPlaySentence();

    // Rule application
    float applyVolumeRules(float requested, bool quietHours);

    // Distance-driven playback helpers
    bool distancePlaybackInterval(float distanceMm, uint32_t& intervalMs);
    float updateDistancePlaybackVolume(float distanceMm);

    // Optional: queueing strategy
    bool requestFragment(const AudioFragment& frag);
    void requestSentence(const String& phrase);

    // Calendar-driven theme box support
    void setThemeBox(const uint8_t* dirs, size_t count, const String& id);
    void clearThemeBox();
    bool themeBoxActive();
    const uint8_t* themeBoxDirs(size_t& count);
    const String& themeBoxId();
}
