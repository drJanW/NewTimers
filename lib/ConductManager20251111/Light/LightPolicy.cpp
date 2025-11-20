#include <Arduino.h>

#include "LightPolicy.h"
#include "Globals.h" // for MAX_BRIGHTNESS

namespace LightPolicy {

float applyBrightnessRules(float requested) {
    float v = clamp(requested, 0.0f, static_cast<float>(MAX_BRIGHTNESS));
    return v;
}

bool distanceAnimationFor(float distanceMm,
                          uint32_t& frameIntervalMs,
                          float& intensity,
                          uint8_t& paletteId) {
    (void)distanceMm;
    (void)frameIntervalMs;
    (void)intensity;
    (void)paletteId;
    // TODO: Implement distance-driven RGB lightshow modulation.
    frameIntervalMs = 0;
    intensity = 0.0f;
    paletteId = 0;
    return false;
}

}
