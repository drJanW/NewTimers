#pragma once
#include <Arduino.h>
#include "PlayFragment.h"   // for AudioFragment

namespace SDPolicy {

    // Weighted random fragment selection
    bool getRandomFragment(AudioFragment& outFrag);

    // Delete a file (only if allowed by policy)
    bool deleteFile(uint8_t dirIndex, uint8_t fileIndex);

    // Diagnostics
    void showStatus();
}
