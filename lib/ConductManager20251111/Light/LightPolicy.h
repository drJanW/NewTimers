#pragma once
#include <Arduino.h>

namespace LightPolicy {

    // Apply brightness rules (caps, floors)
    float applyBrightnessRules(float requested);

    // Placeholder: distance-driven light show adjustment
    bool distanceAnimationFor(float distanceMm,
                              uint32_t& frameIntervalMs,
                              float& intensity,
                              uint8_t& paletteId);

}
