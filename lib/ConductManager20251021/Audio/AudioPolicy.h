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

    // Optional: queueing strategy
    void requestFragment(const AudioFragment& frag);
    void requestSentence(const String& phrase);
}
