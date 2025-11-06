#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightManager.h"

class WebLightStore {
public:
    static WebLightStore& instance();

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

    const String& getActivePatternId() const { return activePatternId_; }
    const String& getActiveColorId() const { return activeColorId_; }

private:
    WebLightStore() = default;

    struct PatternEntry {
        String id;
        String label;
        LightShowParams params;
    };

    struct ColorEntry {
        String id;
        String label;
        CRGB primary;
        CRGB secondary;
    };

    void ensureDefaults();
    bool loadPatternsFromSD();
    bool loadColorsFromSD();
    void loadDefaultPatterns();
    void loadDefaultColors();
    bool savePatternsToSD() const;
    bool saveColorsToSD() const;

    const PatternEntry* findPattern(const String& id) const;
    PatternEntry* findPattern(const String& id);
    const ColorEntry* findColor(const String& id) const;
    ColorEntry* findColor(const String& id);

    static bool parsePatternParams(JsonVariantConst src, LightShowParams& out, String& errorMessage);
    static bool parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage);

    static bool parseHexColor(const String& hex, CRGB& color);
    static void fillPatternParams(JsonObject dest, const PatternEntry& entry);

    String generatePatternId() const;
    String generateColorId() const;

    void applyActiveToLights();

    std::vector<PatternEntry> patterns_;
    std::vector<ColorEntry> colors_;
    String activePatternId_;
    String activeColorId_;
    bool ready_{false};
    bool previewActive_{false};
    LightShowParams previewBackupParams_;
    CRGB previewBackupColorA_;
    CRGB previewBackupColorB_;
};
