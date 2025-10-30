#include "Arduino.h"
#include "Globals.h"

#include <esp_system.h>   // esp_random()

void bootRandomSeed() {
    uint32_t seed = esp_random() ^ micros();
    randomSeed(seed);
}
// --- Flag ---

