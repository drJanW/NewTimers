#pragma once

#include <Arduino.h>

#include "AudioManager.h"

class AudioConduct {
public:
    static const char* const kDistanceClipId;

    void plan();
    static void setDistanceClip(const AudioManager::PCMClipDesc* clip);
    static void startDistanceResponse();
    static void silenceDistance();
    static bool isDistancePlaybackScheduled();
};
