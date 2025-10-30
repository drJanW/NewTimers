#pragma once

#include <Arduino.h>
#include "PlayFragment.h"

class AudioDirector {
public:
    static void plan();

    // Select the next fragment to play based on current SD index/voting data.
    static bool selectRandomFragment(AudioFragment& outFrag);
};
