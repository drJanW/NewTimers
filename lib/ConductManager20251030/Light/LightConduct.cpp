#include "LightConduct.h"

#include "Globals.h"
#include "LightPolicy.h"
#include "TimerManager.h"

namespace {

constexpr uint32_t kLightFallbackIntervalMs = 300;

uint32_t s_currentIntervalMs = 0;
float s_currentIntensity = 0.0f;
uint8_t s_currentPaletteId = 0;
bool s_timerActive = false;

void applyLightshowUpdate() {
    (void)s_currentIntensity;
    (void)s_currentPaletteId;
    // TODO: Implement distance-driven RGB lightshow update logic.
}

bool scheduleAnimation(uint32_t intervalMs) {
    TimerCallback cb = LightConduct::animationCallback;
    if (!TimerManager::instance().restart(intervalMs, 1, cb)) {
        PF("[LightConduct] Failed to schedule light animation (%lu ms)\n",
           static_cast<unsigned long>(intervalMs));
        s_timerActive = false;
        return false;
    }
    s_timerActive = true;
    s_currentIntervalMs = intervalMs;
    return true;
}

void stopAnimation() {
    if (s_timerActive) {
    TimerCallback cb = LightConduct::animationCallback;
        TimerManager::instance().cancel(cb);
        s_timerActive = false;
    }
    s_currentIntervalMs = 0;
    s_currentIntensity = 0.0f;
    s_currentPaletteId = 0;
}

} // namespace

void LightConduct::plan() {// TODO: route light intents and policies here
    stopAnimation();
    PL("[Conduct][Plan] TODO: route light intents and policies here");
}

void LightConduct::handleDistanceReading(float distanceMm) {
    uint32_t intervalMs = 0;
    float intensity = 0.0f;
    uint8_t paletteId = 0;

    if (!LightPolicy::distanceAnimationFor(distanceMm, intervalMs, intensity, paletteId)) {
        stopAnimation();
        return;
    }

    if (intervalMs == 0) {
        intervalMs = kLightFallbackIntervalMs;
    }

    s_currentIntervalMs = intervalMs;
    s_currentIntensity = intensity;
    s_currentPaletteId = paletteId;

    if (!s_timerActive) {
        scheduleAnimation(intervalMs);
    }
}

void LightConduct::animationCallback() {
    applyLightshowUpdate();
    s_timerActive = false;
}
