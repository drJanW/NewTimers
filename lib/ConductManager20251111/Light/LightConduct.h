#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdint>

class LightConduct {
public:
    void plan();

    static void handleDistanceReading(float distanceMm);

    static void animationCallback();

    // Time-of-day shift system
    static void shiftTimerCallback();
    static void intentApplyColorShifts(uint64_t statusBits);

    // Light pattern/color exports routed through conduct
    static bool patternSnapshot(String &payload, String &activePatternId);
    static bool colorSnapshot(String &payload, String &activeColorId);

    static bool selectPattern(const String &id, String &errorMessage);
    static bool updatePattern(JsonVariantConst body, String &affectedId, String &errorMessage);
    static bool deletePattern(JsonVariantConst body, String &affectedId, String &errorMessage);

    static bool selectColor(const String &id, String &errorMessage);
    static bool updateColor(JsonVariantConst body, String &affectedId, String &errorMessage);
    static bool deleteColor(JsonVariantConst body, String &affectedId, String &errorMessage);

    static bool previewPattern(JsonVariantConst body, String &errorMessage);
    static bool previewColor(JsonVariantConst body, String &errorMessage);
};
