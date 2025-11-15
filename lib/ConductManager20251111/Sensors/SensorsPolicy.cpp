#include "SensorsPolicy.h"
#include "Globals.h"
#include "SensorManager.h"

#include <cmath>

namespace {

#ifndef SENSORS_POLICY_DEBUG
#define SENSORS_POLICY_DEBUG 0
#endif

#if SENSORS_POLICY_DEBUG
#define SP_LOG(...) PF(__VA_ARGS__)
#else
#define SP_LOG(...) do {} while (0)
#endif

constexpr float kMinValidMm = 40.0f;
constexpr float kMaxValidMm = 3600.0f;
constexpr uint32_t kFreshWindowMs = 1500; // Distance sample considered fresh

bool s_haveDistance = false;
float s_lastDistanceMm = 0.0f;
uint32_t s_lastTsMs = 0;

} // namespace

namespace SensorsPolicy {

void configure() {
    s_haveDistance = true;
    s_lastDistanceMm = 0.0f;
    s_lastTsMs = millis();
    SensorManager::setDistanceMillimeters(s_lastDistanceMm);
    SP_LOG("[SensorsPolicy] Reset distance filter state\n");
}

bool normaliseDistance(float rawMm, uint32_t sampleTsMs, float& filteredOut) {
    if (!std::isfinite(rawMm)) {
        return false;
    }
    if (rawMm < kMinValidMm || rawMm > kMaxValidMm) {
        return false;
    }

    const float filtered = rawMm;

    s_lastDistanceMm = filtered;
    s_lastTsMs = sampleTsMs;
    s_haveDistance = true;
    SensorManager::setDistanceMillimeters(filtered);
    filteredOut = filtered;

    SP_LOG("[SensorsPolicy] raw=%.1f filtered=%.1f ts=%lu\n",
           rawMm,
           filtered,
           static_cast<unsigned long>(sampleTsMs));

    return true;
}

bool latestDistance(float& distanceMm, uint32_t& sampleTsMs) {
    if (!s_haveDistance) {
        return false;
    }
    distanceMm = s_lastDistanceMm;
    sampleTsMs = s_lastTsMs;
    return true;
}

bool latestFreshDistance(float& distanceMm) {
    if (!s_haveDistance) {
        return false;
    }

    const uint32_t now = millis();
    if (!isFresh(now, 0)) {
        return false;
    }

    distanceMm = s_lastDistanceMm;
    return true;
}

float currentDistance() {
    return s_lastDistanceMm;
}

bool isFresh(uint32_t nowMs, uint32_t maxAgeMs) {
    if (!s_haveDistance) {
        return false;
    }
    const uint32_t window = maxAgeMs ? maxAgeMs : kFreshWindowMs;
    const uint32_t age = nowMs - s_lastTsMs;
    return age <= window;
}

uint32_t freshnessWindowMs() {
    return kFreshWindowMs;
}

} // namespace SensorsPolicy
