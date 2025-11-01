#include <Arduino.h>
#include "LightPolicy.h"
#include "Globals.h" // for MAX_BRIGHTNESS
#include "../../ContextManager20251030/Calendar.h"
#include "../../LightManager20251030/LightManager.h"

namespace LightPolicy {

namespace {

constexpr uint8_t kDefaultCycleSec = 10;
constexpr float  kDefaultFadeWidth = 4.0f;
constexpr float  kDefaultGradientSpeed = 1.0f;

struct CalendarShowState {
    bool   active{false};
    String showId;
    String colorsId;
};

CalendarShowState s_calendarState;

CRGB toCRGB(uint32_t rgb) {
    return CRGB(
        static_cast<uint8_t>((rgb >> 16) & 0xFFU),
        static_cast<uint8_t>((rgb >> 8) & 0xFFU),
        static_cast<uint8_t>(rgb & 0xFFU));
}

uint8_t guardCycle(uint8_t value) {
    return value > 0U ? value : kDefaultCycleSec;
}

float guardFadeWidth(float value) {
    return value > 0.05f ? value : kDefaultFadeWidth;
}

float guardGradientSpeed(float value) {
    return (value > 0.0f) ? value : kDefaultGradientSpeed;
}

void applyShowParams(const LightShowParams& params,
                     const CalendarLightShow& show,
                     bool colorsOverridden,
                     const String& colorsId) {
    PlayLightShow(params);
    s_calendarState.active  = true;
    s_calendarState.showId  = show.id;
    s_calendarState.colorsId = colorsOverridden ? colorsId : String();

    const unsigned long rgb1 = (static_cast<unsigned long>(params.RGB1.r) << 16) |
                               (static_cast<unsigned long>(params.RGB1.g) << 8)  |
                               static_cast<unsigned long>(params.RGB1.b);
    const unsigned long rgb2 = (static_cast<unsigned long>(params.RGB2.r) << 16) |
                               (static_cast<unsigned long>(params.RGB2.g) << 8)  |
                               static_cast<unsigned long>(params.RGB2.b);

    PF("[LightPolicy] Calendar show %s applied (palette #%06lX -> #%06lX, window %d)\n",
       show.id.c_str(), rgb1 & 0x00FFFFFFUL, rgb2 & 0x00FFFFFFUL,
       params.windowWidth);
}

} // namespace

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

void applyCalendarLightshow(const CalendarLightShow& show,
                            const CalendarColorRange& colors) {
    LightShowParams params;

    uint32_t primary   = show.rgb1;
    uint32_t secondary = show.rgb2;
    bool colorsOverridden = false;
    if (colors.valid) {
        primary = colors.startColor;
        secondary = colors.endColor;
        colorsOverridden = true;
    }

    params.RGB1 = toCRGB(primary);
    params.RGB2 = toCRGB(secondary);
    params.colorCycleSec  = guardCycle(show.colorCycleSec);
    params.brightCycleSec = guardCycle(show.brightCycleSec);
    params.fadeWidth      = guardFadeWidth(show.fadeWidth);
    params.minBrightness  = show.minBrightness > MAX_BRIGHTNESS
                                ? MAX_BRIGHTNESS
                                : show.minBrightness;
    params.gradientSpeed  = guardGradientSpeed(show.gradientSpeed);
    params.centerX        = show.centerX;
    params.centerY        = show.centerY;
    params.radius         = show.radius;
    params.windowWidth    = show.windowWidth > 0 ? show.windowWidth : 16;
    params.radiusOsc      = show.radiusOsc;
    params.xAmp           = show.xAmp;
    params.yAmp           = show.yAmp;
    params.xCycleSec      = guardCycle(show.xCycleSec);
    params.yCycleSec      = guardCycle(show.yCycleSec);

    const String colorsId = colors.valid ? colors.id : String();
    applyShowParams(params, show, colorsOverridden, colorsId);
}

void clearCalendarLightshow() {
    if (!s_calendarState.active) {
        return;
    }
    s_calendarState = CalendarShowState{};
    PF("[LightPolicy] Calendar show cleared\n");
}

}
