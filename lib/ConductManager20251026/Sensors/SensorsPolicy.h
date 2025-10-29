#pragma once

#include <Arduino.h>

namespace SensorsPolicy {

void configure();

// Normalise raw VL53 distance; returns true when value accepted.
bool normaliseDistance(float rawMm, uint32_t sampleTsMs, float& filteredOut);

// Retrieve latest accepted distance (if any).
bool latestDistance(float& distanceMm, uint32_t& sampleTsMs);

// Retrieve the latest distance when it is still within the freshness window.
bool latestFreshDistance(float& distanceMm);

// Lightweight accessor for the most recent distance value.
float currentDistance();

// Check whether the cached distance is fresh enough for downstream use.
bool isFresh(uint32_t nowMs, uint32_t maxAgeMs = 0);

// Default freshness window applied when maxAgeMs==0.
uint32_t freshnessWindowMs();

}
