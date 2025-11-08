#include "PatternStore.h"

#include <SD.h>
#include <algorithm>

#include "SDManager.h"

namespace {
constexpr const char* kPatternPath = "/light_patterns.json";
constexpr uint8_t kSchemaVersion = 1;

struct DefaultPattern {
    const char* id;
    const char* label;
    float colorCycleSec;
    float brightCycleSec;
    float fadeWidth;
    float minBrightness;
    float gradientSpeed;
    float centerX;
    float centerY;
    float radius;
    int   windowWidth;
    float radiusOsc;
    float xAmp;
    float yAmp;
    float xCycleSec;
    float yCycleSec;
};

constexpr DefaultPattern kDefaultPatterns[] = {
    {"pattern001", "Smooth Orbit", 18.f, 14.f, 9.f, 12.f, 0.6f, 0.f, 0.f, 22.f, 24, 4.f, 3.5f, 2.5f, 22.f, 20.f},
    {"pattern002", "Slow Breathing", 30.f, 28.f, 16.f, 8.f, 0.3f, 0.f, 0.f, 18.f, 28, 2.f, 2.f, 2.f, 32.f, 34.f},
    {"pattern003", "Rapid Sparks", 8.f, 6.f, 4.f, 20.f, 1.2f, 0.f, 0.f, 10.f, 14, 6.f, 5.f, 7.f, 9.f, 11.f},
    {"pattern004", "Wide Sweep", 20.f, 18.f, 11.f, 12.f, 0.6f, 6.f, -4.f, 40.f, 48, 3.f, 8.f, 5.f, 26.f, 24.f},
    {"pattern005", "Calm Center", 24.f, 18.f, 6.f, 4.f, 0.2f, 0.f, 0.f, 8.f, 12, 1.5f, 1.5f, 1.5f, 40.f, 36.f}
};

class SDBusyGuard {
public:
    SDBusyGuard() : owns_(!SDManager::isBusy()) {
        if (owns_) {
            SDManager::setBusy(true);
        }
    }
    ~SDBusyGuard() {
        if (owns_) {
            SDManager::setBusy(false);
        }
    }
    bool acquired() const { return owns_; }
private:
    bool owns_;
};

LightShowParams makeParams(const DefaultPattern& src) {
    LightShowParams p;
    p.RGB1 = CRGB::Black;
    p.RGB2 = CRGB::Black;
    p.colorCycleSec  = static_cast<uint8_t>(src.colorCycleSec);
    p.brightCycleSec = static_cast<uint8_t>(src.brightCycleSec);
    p.fadeWidth      = src.fadeWidth;
    p.minBrightness  = static_cast<uint8_t>(src.minBrightness);
    p.gradientSpeed  = src.gradientSpeed;
    p.centerX        = src.centerX;
    p.centerY        = src.centerY;
    p.radius         = src.radius;
    p.windowWidth    = src.windowWidth;
    p.radiusOsc      = src.radiusOsc;
    p.xAmp           = src.xAmp;
    p.yAmp           = src.yAmp;
    p.xCycleSec      = static_cast<uint8_t>(src.xCycleSec);
    p.yCycleSec      = static_cast<uint8_t>(src.yCycleSec);
    return p;
}

} // namespace

PatternStore& PatternStore::instance() {
    static PatternStore inst;
    return inst;
}

void PatternStore::begin() {
    if (ready_) {
        return;
    }
    patterns_.clear();
    bool loaded = loadFromSD();
    if (!loaded) {
        loadDefaults();
        saveToSD();
    }
    ensureDefaults();
    ready_ = true;
}

String PatternStore::buildJson() const {
    DynamicJsonDocument doc(8192);
    doc["schema"] = kSchemaVersion;
    doc["active_pattern"] = activePatternId_;
    JsonArray arr = doc.createNestedArray("patterns");
    for (const auto& entry : patterns_) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = entry.id;
        if (!entry.label.isEmpty()) {
            obj["label"] = entry.label;
        }
        JsonObject params = obj.createNestedObject("params");
        params["color_cycle_sec"]  = entry.params.colorCycleSec;
        params["bright_cycle_sec"] = entry.params.brightCycleSec;
        params["fade_width"]       = entry.params.fadeWidth;
        params["min_brightness"]   = entry.params.minBrightness;
        params["gradient_speed"]   = entry.params.gradientSpeed;
        params["center_x"]         = entry.params.centerX;
        params["center_y"]         = entry.params.centerY;
        params["radius"]           = entry.params.radius;
        params["window_width"]     = entry.params.windowWidth;
        params["radius_osc"]       = entry.params.radiusOsc;
        params["x_amp"]            = entry.params.xAmp;
        params["y_amp"]            = entry.params.yAmp;
        params["x_cycle_sec"]      = entry.params.xCycleSec;
        params["y_cycle_sec"]      = entry.params.yCycleSec;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool PatternStore::select(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        return false;
    }
    if (id.isEmpty()) {
        activePatternId_.clear();
        PF("[PatternStore] Cleared active pattern to context\n");
        return true;
    }
    PatternEntry* entry = findEntry(id);
    if (!entry) {
        errorMessage = F("pattern not found");
        return false;
    }
    activePatternId_ = entry->id;
    PF("[PatternStore] Selected %s\n", activePatternId_.c_str());
    return true;
}

bool PatternStore::update(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        return false;
    }
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }

    LightShowParams params;
    if (!parseParams(obj["params"], params, errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = F("params missing");
        }
        return false;
    }

    String label = obj["label"].as<String>();
    if (label.length() > 48) {
        label = label.substring(0, 48);
    }
    bool selectEntry = obj["select"].as<bool>();

    if (obj.containsKey("id")) {
        String id = obj["id"].as<String>();
        PatternEntry* existing = findEntry(id);
        if (!existing) {
            errorMessage = F("pattern not found");
            return false;
        }
        existing->params = params;
        existing->label = label;
        affectedId = existing->id;
        if (selectEntry) {
            activePatternId_ = existing->id;
        }
        PF("[PatternStore] Updated %s%s\n", affectedId.c_str(), selectEntry ? " (selected)" : "");
    } else {
        PatternEntry entry;
        entry.id = generateId();
        entry.label = label;
        entry.params = params;
        affectedId = entry.id;
        patterns_.push_back(entry);
        if (selectEntry || activePatternId_.isEmpty()) {
            activePatternId_ = entry.id;
        }
        PF("[PatternStore] Created %s%s\n", affectedId.c_str(), selectEntry ? " (selected)" : "");
    }

    if (!saveToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    return true;
}

bool PatternStore::remove(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        return false;
    }
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }
    String id = obj["id"].as<String>();
    if (id.isEmpty()) {
        errorMessage = F("id required");
        return false;
    }
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        errorMessage = F("pattern not found");
        return false;
    }
    bool wasActive = (activePatternId_ == it->id);
    patterns_.erase(it);
    if (patterns_.empty()) {
        loadDefaults();
        activePatternId_ = patterns_.front().id;
    } else if (wasActive) {
        activePatternId_ = patterns_.front().id;
    }
    if (!saveToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    affectedId = activePatternId_;
    PF("[PatternStore] Removed %s, fallback=%s\n", id.c_str(), affectedId.c_str());
    return true;
}

LightShowParams PatternStore::getActiveParams() const {
    const_cast<PatternStore*>(this)->ensureDefaults();
    const PatternEntry* entry = nullptr;
    if (!activePatternId_.isEmpty()) {
        entry = findEntry(activePatternId_);
    }
    if (!entry && !patterns_.empty()) {
        entry = &patterns_.front();
    }
    return entry ? entry->params : LightShowParams();
}

bool PatternStore::parseParams(JsonVariantConst src, LightShowParams& out, String& errorMessage) const {
    JsonObjectConst obj = src.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("params invalid");
        return false;
    }
    out.colorCycleSec  = obj["color_cycle_sec"].as<uint8_t>();
    out.brightCycleSec = obj["bright_cycle_sec"].as<uint8_t>();
    out.fadeWidth      = obj["fade_width"].as<float>();
    out.minBrightness  = obj["min_brightness"].as<uint8_t>();
    out.gradientSpeed  = obj["gradient_speed"].as<float>();
    out.centerX        = obj["center_x"].as<float>();
    out.centerY        = obj["center_y"].as<float>();
    out.radius         = obj["radius"].as<float>();
    out.windowWidth    = obj["window_width"].as<int>();
    out.radiusOsc      = obj["radius_osc"].as<float>();
    out.xAmp           = obj["x_amp"].as<float>();
    out.yAmp           = obj["y_amp"].as<float>();
    out.xCycleSec      = obj["x_cycle_sec"].as<uint8_t>();
    out.yCycleSec      = obj["y_cycle_sec"].as<uint8_t>();
    return true;
}

bool PatternStore::getParamsForId(const String& id, LightShowParams& out) const {
    if (id.isEmpty()) {
        return false;
    }
    const PatternEntry* entry = findEntry(id);
    if (!entry) {
        return false;
    }
    out = entry->params;
    return true;
}

void PatternStore::ensureDefaults() {
    if (!ready_ && patterns_.empty()) {
        loadDefaults();
    }
    if (patterns_.empty()) {
        loadDefaults();
    }
    if (activePatternId_.isEmpty() && !patterns_.empty()) {
        activePatternId_ = patterns_.front().id;
    }
}

bool PatternStore::loadFromSD() {
    if (!SDManager::isReady() || !SD.exists(kPatternPath)) {
        return false;
    }
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    File file = SD.open(kPatternPath, FILE_READ);
    if (!file) {
        return false;
    }
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        return false;
    }
    patterns_.clear();
    activePatternId_ = doc["active_pattern"].as<String>();
    JsonArray arr = doc["patterns"].as<JsonArray>();
    if (arr.isNull()) {
        return false;
    }
    for (JsonObject item : arr) {
        LightShowParams params;
        String errMsg;
        if (!parseParams(item["params"], params, errMsg)) {
            continue;
        }
        PatternEntry entry;
        entry.id = item["id"].as<String>();
        entry.label = item["label"].as<String>();
        entry.params = params;
        if (entry.id.isEmpty()) {
            continue;
        }
        patterns_.push_back(entry);
    }
    return !patterns_.empty();
}

void PatternStore::loadDefaults() {
    patterns_.clear();
    const size_t count = sizeof(kDefaultPatterns) / sizeof(kDefaultPatterns[0]);
    patterns_.reserve(count);
    for (const auto& src : kDefaultPatterns) {
        PatternEntry entry;
        entry.id = src.id;
        entry.label = src.label;
        entry.params = makeParams(src);
        patterns_.push_back(entry);
    }
    if (!patterns_.empty()) {
        activePatternId_ = patterns_.front().id;
    }
}

bool PatternStore::saveToSD() const {
    if (!SDManager::isReady()) {
        return false;
    }
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    SD.remove(kPatternPath);
    File file = SD.open(kPatternPath, FILE_WRITE);
    if (!file) {
        return false;
    }
    DynamicJsonDocument doc(8192);
    doc["schema"] = kSchemaVersion;
    doc["active_pattern"] = activePatternId_;
    JsonArray arr = doc.createNestedArray("patterns");
    for (const auto& entry : patterns_) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = entry.id;
        if (!entry.label.isEmpty()) {
            obj["label"] = entry.label;
        }
        JsonObject params = obj.createNestedObject("params");
        params["color_cycle_sec"]  = entry.params.colorCycleSec;
        params["bright_cycle_sec"] = entry.params.brightCycleSec;
        params["fade_width"]       = entry.params.fadeWidth;
        params["min_brightness"]   = entry.params.minBrightness;
        params["gradient_speed"]   = entry.params.gradientSpeed;
        params["center_x"]         = entry.params.centerX;
        params["center_y"]         = entry.params.centerY;
        params["radius"]           = entry.params.radius;
        params["window_width"]     = entry.params.windowWidth;
        params["radius_osc"]       = entry.params.radiusOsc;
        params["x_amp"]            = entry.params.xAmp;
        params["y_amp"]            = entry.params.yAmp;
        params["x_cycle_sec"]      = entry.params.xCycleSec;
        params["y_cycle_sec"]      = entry.params.yCycleSec;
    }
    const size_t written = serializeJson(doc, file);
    file.close();
    return written > 0;
}

PatternStore::PatternEntry* PatternStore::findEntry(const String& id) {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

const PatternStore::PatternEntry* PatternStore::findEntry(const String& id) const {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

String PatternStore::generateId() const {
    int maxIndex = 0;
    for (const auto& entry : patterns_) {
        if (entry.id.startsWith("pattern")) {
            int idx = entry.id.substring(7).toInt();
            if (idx > maxIndex) {
                maxIndex = idx;
            }
        }
    }
    char buff[12];
    snprintf(buff, sizeof(buff), "pattern%03d", maxIndex + 1);
    return String(buff);
}
