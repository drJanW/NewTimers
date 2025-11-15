#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightManager.h"

class ColorsStore {
public:
    static ColorsStore& instance();

    void begin();
    bool isReady() const;

    String buildPatternsJson() const;
    String buildColorsJson() const;

    bool selectPattern(const String& id, String& errorMessage);
    bool selectColor(const String& id, String& errorMessage);

    bool updatePattern(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool deletePattern(JsonVariantConst body, String& affectedId, String& errorMessage);

    bool updateColor(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool deleteColor(JsonVariantConst body, String& affectedId, String& errorMessage);

    bool preview(JsonVariantConst body, String& errorMessage);

    String getActivePatternId() const;
    const String& getActiveColorId() const { return activeColorId_; }

private:
    ColorsStore() = default;

    struct ColorEntry {
        String id;
        String label;
        CRGB primary;
        CRGB secondary;
    };

    void ensureColorDefaults();
    bool loadColorsFromSD();
    void loadDefaultColors();
    bool saveColorsToSD() const;

    const ColorEntry* findColor(const String& id) const;
    ColorEntry* findColor(const String& id);

    static bool parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage);

    static bool parseHexColor(const String& hex, CRGB& color);

    String generateColorId() const;

    void applyActiveToLights();

    std::vector<ColorEntry> colors_;
    String activeColorId_;
    bool ready_{false};
    bool previewActive_{false};
    LightShowParams previewBackupParams_;
    CRGB previewBackupColorA_;
    CRGB previewBackupColorB_;
};
