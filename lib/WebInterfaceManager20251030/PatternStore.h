#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightManager.h"

class PatternStore {
public:
    static PatternStore& instance();

    void begin();
    bool isReady() const { return ready_; }

    String buildJson() const;

    bool select(const String& id, String& errorMessage);
    bool update(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool remove(JsonVariantConst body, String& affectedId, String& errorMessage);

    const String& activeId() const { return activePatternId_; }

    LightShowParams getActiveParams() const;
    bool parseParams(JsonVariantConst src, LightShowParams& out, String& errorMessage) const;

private:
    PatternStore() = default;

    struct PatternEntry {
        String id;
        String label;
        LightShowParams params;
    };

    void ensureDefaults();
    bool loadFromSD();
    void loadDefaults();
    bool saveToSD() const;

    PatternEntry* findEntry(const String& id);
    const PatternEntry* findEntry(const String& id) const;

    String generateId() const;

    std::vector<PatternEntry> patterns_;
    String activePatternId_;
    bool ready_{false};
};
