#include "LightPolicy.h"
#include "Globals.h" // for MAX_BRIGHTNESS

namespace LightPolicy {

float applyBrightnessRules(float requested, bool quietHours) {
    float v = requested;
    if (v < 0.0f) v = 0.0f;
    if (v > MAX_BRIGHTNESS) v = MAX_BRIGHTNESS;
    if (quietHours && v > 50) v = 50; // cap brightness at night
    return v;
}

uint32_t applyPaletteOverride(uint32_t baseColor, bool christmasMode) {
    if (christmasMode) {
        // Example: force red/green palette
        // In practice, you’d map baseColor to festive variant
        return 0x00FF00; // green as placeholder
    }
    return baseColor;
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
