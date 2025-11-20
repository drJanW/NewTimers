#include "LightConduct.h"

#include "Globals.h"
#include "LightPolicy.h"
#include "TimerManager.h"
#include "ColorsStore.h"

namespace {

constexpr uint32_t kLightFallbackIntervalMs = 300;

uint32_t s_currentIntervalMs = 0;
float s_currentIntensity = 0.0f;
uint8_t s_currentPaletteId = 0;
bool s_timerActive = false;

ColorsStore &ensureColorsStore() {
    ColorsStore &store = ColorsStore::instance();
    if (!store.isReady()) {
        store.begin();
    }
    return store;
}

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

bool LightConduct::patternSnapshot(String &payload, String &activePatternId) {
    ColorsStore &store = ensureColorsStore();
    payload = store.buildPatternsJson();
    if (payload.isEmpty()) {
        return false;
    }
    activePatternId = store.getActivePatternId();
    return true;
}

bool LightConduct::colorSnapshot(String &payload, String &activeColorId) {
    ColorsStore &store = ensureColorsStore();
    payload = store.buildColorsJson();
    if (payload.isEmpty()) {
        return false;
    }
    activeColorId = store.getActiveColorId();
    return true;
}

bool LightConduct::selectPattern(const String &id, String &errorMessage) {
    return ensureColorsStore().selectPattern(id, errorMessage);
}

bool LightConduct::updatePattern(JsonVariantConst body, String &affectedId, String &errorMessage) {
    return ensureColorsStore().updatePattern(body, affectedId, errorMessage);
}

bool LightConduct::deletePattern(JsonVariantConst body, String &affectedId, String &errorMessage) {
    return ensureColorsStore().deletePattern(body, affectedId, errorMessage);
}

bool LightConduct::selectColor(const String &id, String &errorMessage) {
    return ensureColorsStore().selectColor(id, errorMessage);
}

bool LightConduct::updateColor(JsonVariantConst body, String &affectedId, String &errorMessage) {
    return ensureColorsStore().updateColor(body, affectedId, errorMessage);
}

bool LightConduct::deleteColor(JsonVariantConst body, String &affectedId, String &errorMessage) {
    return ensureColorsStore().deleteColor(body, affectedId, errorMessage);
}

bool LightConduct::previewPattern(JsonVariantConst body, String &errorMessage) {
    return ensureColorsStore().preview(body, errorMessage);
}

bool LightConduct::previewColor(JsonVariantConst body, String &errorMessage) {
    return ensureColorsStore().previewColors(body, errorMessage);
}
