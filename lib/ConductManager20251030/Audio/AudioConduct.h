#pragma once

#include <Arduino.h>

#include "AudioManager.h"
#include "TimerManager.h"

class AudioConduct {
public:
    static const char* const kDistanceClipId;

    void plan();
    static void startDistanceResponse(bool playImmediately = false);
    static void cb_playPCM();
};

void setDistanceClipPointer(const AudioManager::PCMClipDesc* clip);
const AudioManager::PCMClipDesc* getDistanceClipPointer();
