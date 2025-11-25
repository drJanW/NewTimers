#include "LightConduct.h"

#include "Globals.h"
#include "LightPolicy.h"
#include "TimerManager.h"
#include "ColorsStore.h"
#include "PatternStore.h"
#include "ShiftStore.h"
#include "TimeOfDay.h"

namespace {

constexpr uint32_t kLightFallbackIntervalMs = 300;
constexpr uint32_t kShiftCheckIntervalMs = 60000; // 60 seconds

uint32_t s_currentIntervalMs = 0;
float s_currentIntensity = 0.0f;
uint8_t s_currentPaletteId = 0;
bool s_timerActive = false;
bool s_shiftTimerActive = false;

ShiftStore& s_shiftStore = ShiftStore::instance();

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

bool scheduleShiftTimer() {
    TimerCallback cb = LightConduct::shiftTimerCallback;
    if (!TimerManager::instance().restart(kShiftCheckIntervalMs, 1, cb)) {
        PF("[LightConduct] Failed to schedule shift timer (%lu ms)\n",
           static_cast<unsigned long>(kShiftCheckIntervalMs));
        s_shiftTimerActive = false;
        return false;
    }
    s_shiftTimerActive = true;
    return true;
}

} // namespace

void LightConduct::plan() {// TODO: route light intents and policies here
    stopAnimation();
    
    // Load color shifts CSV
    s_shiftStore.begin();
    
    // Start the shift timer for periodic checks
    scheduleShiftTimer();
    
    // Apply shifts immediately on startup
    uint64_t statusBits = TimeOfDay::getActiveStatusBits();
    intentApplyColorShifts(statusBits);
    
    PL("[Conduct][Plan] Light shift system initialized");
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

void LightConduct::shiftTimerCallback() {
    s_shiftTimerActive = false;
    
    // Get current time-of-day status bits
    uint64_t statusBits = TimeOfDay::getActiveStatusBits();
    
    // Apply color shifts based on active statuses
    intentApplyColorShifts(statusBits);
    
    // Reschedule the timer
    scheduleShiftTimer();
}

void LightConduct::intentApplyColorShifts(uint64_t statusBits) {
    // Compute combined multipliers for all color parameters
    float colorMults[COLOR_PARAM_COUNT];
    s_shiftStore.computeColorMultipliers(statusBits, colorMults);

    // Apply to ColorsStore via mux setters
    // Multipliers are applied as percentages: (mult - 1.0) * 100
    // e.g. mult=0.5 → -50%, mult=2.0 → +100%
    ColorsStore &store = ensureColorsStore();
    
    store.setColorAShiftHue((colorMults[COLOR_A_HUE] - 1.0f) * 100.0f);
    store.setColorAShiftSaturation((colorMults[COLOR_A_SAT] - 1.0f) * 100.0f);
    store.setColorAShiftBrightness((colorMults[COLOR_A_BRIGHT] - 1.0f) * 100.0f);
    store.setColorAShiftValue((colorMults[COLOR_A_VALUE] - 1.0f) * 100.0f);
    
    store.setColorBShiftHue((colorMults[COLOR_B_HUE] - 1.0f) * 100.0f);
    store.setColorBShiftSaturation((colorMults[COLOR_B_SAT] - 1.0f) * 100.0f);
    store.setColorBShiftBrightness((colorMults[COLOR_B_BRIGHT] - 1.0f) * 100.0f);
    store.setColorBShiftValue((colorMults[COLOR_B_VALUE] - 1.0f) * 100.0f);

    // Compute and apply pattern shifts
    float patMults[PAT_PARAM_COUNT];
    s_shiftStore.computePatternMultipliers(statusBits, patMults);
    
    PatternStore &patStore = PatternStore::instance();
    for (int i = 0; i < PAT_PARAM_COUNT; i++) {
        patStore.setShift(static_cast<PatternParam>(i), (patMults[i] - 1.0f) * 100.0f);
    }

    // Re-render lights with new shift values
    store.reapplyWithShifts();

    PF("[LightConduct] Applied color+pattern shifts (status=0x%llX)\n", statusBits);
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
