#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>
#include <atomic>

#include "LightManager.h"

class ColorsStore {
public:
    static ColorsStore& instance();

    void begin();
    bool isReady() const;

    String buildPatternsJson() const;
    String buildColorsJson() const;

    // Shift mux accessors (percent values from -100..+100)
    void setColorAShiftBrightness(float percent) { setMux(percent, &colorShifts_.colorA_brightness); }
    float getColorAShiftBrightness() const { return getMux(&colorShifts_.colorA_brightness); }
    void setColorAShiftHue(float percent) { setMux(percent, &colorShifts_.colorA_hue); }
    float getColorAShiftHue() const { return getMux(&colorShifts_.colorA_hue); }
    void setColorAShiftSaturation(float percent) { setMux(percent, &colorShifts_.colorA_saturation); }
    float getColorAShiftSaturation() const { return getMux(&colorShifts_.colorA_saturation); }
    void setColorAShiftValue(float percent) { setMux(percent, &colorShifts_.colorA_value); }
    float getColorAShiftValue() const { return getMux(&colorShifts_.colorA_value); }

    void setColorBShiftBrightness(float percent) { setMux(percent, &colorShifts_.colorB_brightness); }
    float getColorBShiftBrightness() const { return getMux(&colorShifts_.colorB_brightness); }
    void setColorBShiftHue(float percent) { setMux(percent, &colorShifts_.colorB_hue); }
    float getColorBShiftHue() const { return getMux(&colorShifts_.colorB_hue); }
    void setColorBShiftSaturation(float percent) { setMux(percent, &colorShifts_.colorB_saturation); }
    float getColorBShiftSaturation() const { return getMux(&colorShifts_.colorB_saturation); }
    void setColorBShiftValue(float percent) { setMux(percent, &colorShifts_.colorB_value); }
    float getColorBShiftValue() const { return getMux(&colorShifts_.colorB_value); }

    bool selectPattern(const String& id, String& errorMessage);
    bool selectColor(const String& id, String& errorMessage);

    bool updatePattern(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool deletePattern(JsonVariantConst body, String& affectedId, String& errorMessage);

    bool updateColor(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool deleteColor(JsonVariantConst body, String& affectedId, String& errorMessage);

    bool preview(JsonVariantConst body, String& errorMessage);
    bool previewColors(JsonVariantConst body, String& errorMessage);

    String getActivePatternId() const;
    const String& getActiveColorId() const { return activeColorId_; }

    // Re-apply current colors with shifts (call when shifts change)
    void reapplyWithShifts();

private:
    ColorsStore() = default;

    struct ColorEntry {
        String id;
        String label;
        CRGB colorA;
        CRGB colorB;
    };

    void ensureColorDefaults();
    bool loadColorsFromSD();
    void loadDefaultColors();
    bool saveColorsToSD() const;

    const ColorEntry* findColor(const String& id) const;
    ColorEntry* findColor(const String& id);

    static bool parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage);

    static bool parseHexColor(const String& hex, CRGB& color);
    static void sanitizeLabel(String& label);
    static void ensureLabelForId(const String& id, String& label);
    static String lookupDefaultLabel(const String& id);

    String generateColorId() const;

    void applyActiveToLights();

    std::vector<ColorEntry> colors_;
    String activeColorId_;
    bool ready_{false};
    bool previewActive_{false};
    LightShowParams previewBackupParams_;
    CRGB previewBackupColorA_;
    CRGB previewBackupColorB_;

    struct ColorShiftState {
        std::atomic<float> colorA_brightness{0.0f};
        std::atomic<float> colorA_hue{0.0f};
        std::atomic<float> colorA_saturation{0.0f};
        std::atomic<float> colorA_value{0.0f};
        std::atomic<float> colorB_brightness{0.0f};
        std::atomic<float> colorB_hue{0.0f};
        std::atomic<float> colorB_saturation{0.0f};
        std::atomic<float> colorB_value{0.0f};
    } colorShifts_;
};
