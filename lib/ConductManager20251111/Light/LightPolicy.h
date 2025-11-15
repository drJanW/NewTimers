#pragma once
#include <Arduino.h>

namespace LightPolicy {

    // Apply brightness rules (caps, floors, context)
    float applyBrightnessRules(float requested, bool quietHours);

    // Palette overrides (e.g. Christmas mode)
    uint32_t applyPaletteOverride(uint32_t baseColor, bool christmasMode);

    // Placeholder: distance-driven light show adjustment
    bool distanceAnimationFor(float distanceMm,
                              uint32_t& frameIntervalMs,
                              float& intensity,
                              uint8_t& paletteId);

}
