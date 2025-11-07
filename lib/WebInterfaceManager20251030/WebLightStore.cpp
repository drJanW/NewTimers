#include "WebLightStore.h"

#include <SD.h>
#include <algorithm>

#include "SDManager.h"

namespace {
constexpr const char* kPatternPath = "/light_patterns.json";
constexpr const char* kColorPath   = "/light_colors.json";
constexpr uint8_t kSchemaVersion   = 1;

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

struct DefaultColor {
    const char* id;
    const char* label;
    uint32_t rgb1;
    uint32_t rgb2;
};

constexpr DefaultPattern kDefaultPatterns[] = {
    {"pattern001", "Smooth Orbit", 18.f, 14.f, 9.f, 12.f, 0.6f, 0.f, 0.f, 22.f, 24, 4.f, 3.5f, 2.5f, 22.f, 20.f},
    {"pattern002", "Slow Breathing", 30.f, 28.f, 16.f, 8.f, 0.3f, 0.f, 0.f, 18.f, 28, 2.f, 2.f, 2.f, 32.f, 34.f},
    {"pattern003", "Rapid Sparks", 8.f, 6.f, 4.f, 20.f, 1.2f, 0.f, 0.f, 10.f, 14, 6.f, 5.f, 7.f, 9.f, 11.f},
    {"pattern004", "Wide Sweep", 20.f, 18.f, 11.f, 12.f, 0.6f, 6.f, -4.f, 40.f, 48, 3.f, 8.f, 5.f, 26.f, 24.f},
    {"pattern005", "Calm Center", 24.f, 18.f, 6.f, 4.f, 0.2f, 0.f, 0.f, 8.f, 12, 1.5f, 1.5f, 1.5f, 40.f, 36.f}
};

constexpr DefaultColor kDefaultColors[] = {
    {"color001", "Warm Sunset", 0xFF7F00, 0x552200},
    {"color002", "Arctic Ice",   0x00C6FF, 0x003F7F},
    {"color003", "Forest Dew",   0x3FAF4E, 0x0B3D17},
    {"color004", "Royal Magenta",0xA83279, 0x2B2E83},
    {"color005", "Golden Hour",  0xFFE066, 0xFF6B6B},
    {"color006", "Coral Reef",   0x1CD8D2, 0x93EDC7}
};

CRGB toCRGB(uint32_t value) {
    return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF));
}

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

LightShowParams makeParams(const DefaultPattern& src, const CRGB& primary, const CRGB& secondary) {
    LightShowParams p;
    p.RGB1 = primary;
    p.RGB2 = secondary;
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

WebLightStore& WebLightStore::instance() {
    static WebLightStore inst;
    return inst;
}

void WebLightStore::begin() {
    if (ready_) {
        return;
    }
    patterns_.clear();
    colors_.clear();

    bool patternsLoaded = loadPatternsFromSD();
    bool colorsLoaded   = loadColorsFromSD();

    if (!patternsLoaded) {
        loadDefaultPatterns();
        savePatternsToSD();
    }
    if (!colorsLoaded) {
        loadDefaultColors();
        saveColorsToSD();
    }

    applyActiveToLights();
    ready_ = true;
}

bool WebLightStore::isReady() const {
    return ready_;
}

String WebLightStore::buildPatternsJson() const {
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
        fillPatternParams(params, entry);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

String WebLightStore::buildColorsJson() const {
    DynamicJsonDocument doc(4096);
    doc["schema"] = kSchemaVersion;
    doc["active_color"] = activeColorId_;
    JsonArray arr = doc.createNestedArray("colors");
    for (const auto& entry : colors_) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = entry.id;
        if (!entry.label.isEmpty()) {
            obj["label"] = entry.label;
        }
        char buff[8];
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.primary.r, entry.primary.g, entry.primary.b);
        obj["rgb1_hex"] = buff;
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.secondary.r, entry.secondary.g, entry.secondary.b);
        obj["rgb2_hex"] = buff;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool WebLightStore::selectPattern(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        return false;
    }
    if (id.isEmpty()) {
        activePatternId_.clear();
        applyActiveToLights();
        PF("[WebLightStore] Pattern cleared to context\n");
        return true;
    }
    PatternEntry* entry = findPattern(id);
    if (!entry) {
        errorMessage = F("pattern not found");
        return false;
    }
    activePatternId_ = entry->id;
    PF("[WebLightStore] Pattern select %s\n", activePatternId_.c_str());
    applyActiveToLights();
    return true;
}

bool WebLightStore::selectColor(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        PF("[WebLightStore] selectColor rejected: store not ready\n");
        return false;
    }
    if (id.isEmpty()) {
        activeColorId_ = String();
        applyActiveToLights();
        PF("[WebLightStore] Color cleared to defaults\n");
        return true;
    }
    ColorEntry* entry = findColor(id);
    if (!entry) {
        errorMessage = F("color not found");
        PF("[WebLightStore] selectColor unknown id='%s'\n", id.c_str());
        return false;
    }
    activeColorId_ = entry->id;
    PF("[WebLightStore] Color select %s\n", activeColorId_.c_str());
    applyActiveToLights();
    return true;
}

bool WebLightStore::updatePattern(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!body.is<JsonObject>()) {
        errorMessage = F("invalid payload");
        return false;
    }
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }
    LightShowParams params;
    JsonVariantConst paramVariant = obj["params"];
    if (!paramVariant || !parsePatternParams(paramVariant, params, errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = F("params missing");
        }
        return false;
    }

    String label = obj["label"].as<String>();
    bool select = obj["select"].as<bool>();

    if (label.length() > 48) {
        label = label.substring(0, 48);
    }

    if (obj.containsKey("id")) {
        String id = obj["id"].as<String>();
        PatternEntry* existing = findPattern(id);
        if (!existing) {
            errorMessage = F("pattern not found");
            return false;
        }
        existing->params = params;
        existing->label = label;
        affectedId = existing->id;
        if (select) {
            activePatternId_ = existing->id;
        }
    } else {
        PatternEntry entry;
        entry.params = params;
        entry.label = label;
        entry.id = generatePatternId();
        affectedId = entry.id;
        patterns_.push_back(entry);
        if (select || activePatternId_.isEmpty()) {
            activePatternId_ = entry.id;
        }
    }

    if (!savePatternsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    applyActiveToLights();
    return true;
}

bool WebLightStore::deletePattern(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!body.is<JsonObject>()) {
        errorMessage = F("invalid payload");
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
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& p){ return p.id == id; });
    if (it == patterns_.end()) {
        errorMessage = F("pattern not found");
        return false;
    }
    bool wasActive = (activePatternId_ == it->id);
    patterns_.erase(it);
    if (patterns_.empty()) {
        loadDefaultPatterns();
        activePatternId_ = patterns_.front().id;
    } else if (wasActive) {
        activePatternId_ = patterns_.front().id;
    }
    if (!savePatternsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    applyActiveToLights();
    affectedId = activePatternId_;
    return true;
}

bool WebLightStore::updateColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!body.is<JsonObject>()) {
        errorMessage = F("invalid payload");
        return false;
    }
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }
    CRGB primary, secondary;
    JsonVariantConst colorVariant = obj;
    if (!parseColorPayload(colorVariant, primary, secondary, errorMessage)) {
        return false;
    }
    String label = obj["label"].as<String>();
    if (label.length() > 48) {
        label = label.substring(0, 48);
    }
    bool select = obj["select"].as<bool>();

    if (obj.containsKey("id")) {
        String id = obj["id"].as<String>();
        ColorEntry* existing = findColor(id);
        if (!existing) {
            errorMessage = F("color not found");
            return false;
        }
        existing->primary = primary;
        existing->secondary = secondary;
        existing->label = label;
        affectedId = existing->id;
        if (select) {
            activeColorId_ = existing->id;
        }
    } else {
        ColorEntry entry;
        entry.id = generateColorId();
        entry.label = label;
        entry.primary = primary;
        entry.secondary = secondary;
        colors_.push_back(entry);
        affectedId = entry.id;
        if (select || activeColorId_.isEmpty()) {
            activeColorId_ = entry.id;
        }
    }

    if (!saveColorsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    applyActiveToLights();
    return true;
}

bool WebLightStore::deleteColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!body.is<JsonObject>()) {
        errorMessage = F("invalid payload");
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
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](const ColorEntry& c){ return c.id == id; });
    if (it == colors_.end()) {
        errorMessage = F("color not found");
        return false;
    }
    bool wasActive = (activeColorId_ == it->id);
    colors_.erase(it);
    if (colors_.empty()) {
        loadDefaultColors();
        activeColorId_ = colors_.front().id;
    } else if (wasActive) {
        activeColorId_ = colors_.front().id;
    }
    if (!saveColorsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    applyActiveToLights();
    affectedId = activeColorId_;
    return true;
}

bool WebLightStore::preview(JsonVariantConst body, String& errorMessage) {
    String rawBody;
    serializeJson(body, rawBody);
    PF("[WebLightStore] preview entry raw=%s\n", rawBody.c_str());

    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        PF("[WebLightStore] preview reject: body not object\n");
        return false;
    }
    JsonVariantConst patternVariant = obj["pattern"];
    JsonVariantConst colorVariant = obj["color"];
    if (!patternVariant || !colorVariant) {
        errorMessage = F("pattern/color missing");
        PF("[WebLightStore] preview reject: missing sections (pattern=%d color=%d)\n",
           patternVariant.isNull() ? 0 : 1,
           colorVariant.isNull() ? 0 : 1);
        return false;
    }
    LightShowParams params;
    if (!parsePatternParams(patternVariant, params, errorMessage)) {
        PF("[WebLightStore] preview reject: pattern parse failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }
    CRGB primary;
    CRGB secondary;
    if (!parseColorPayload(colorVariant, primary, secondary, errorMessage)) {
        PF("[WebLightStore] preview reject: color parse failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }

    String patternJson;
    String colorJson;
    serializeJson(patternVariant, patternJson);
    serializeJson(colorVariant, colorJson);
    const char* patternId = obj["pattern_id"] | "";
    const char* colorId = obj["color_id"] | "";
    PF("[WebLightStore] preview request patternId='%s' colorId='%s' pattern=%s color=%s\n",
       patternId,
        colorId,
       patternJson.c_str(),
       colorJson.c_str());

    previewBackupParams_ = params;
    previewBackupColorA_ = primary;
    previewBackupColorB_ = secondary;
    params.RGB1 = primary;
    params.RGB2 = secondary;
    PlayLightShow(params);
    previewActive_ = true;
    PF("[WebLightStore] preview applied\n");
    return true;
}

void WebLightStore::ensureDefaults() {
    if (patterns_.empty()) {
        loadDefaultPatterns();
    }
    if (colors_.empty()) {
        loadDefaultColors();
    }
}

bool WebLightStore::loadPatternsFromSD() {
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
        if (!parsePatternParams(item["params"], params, errMsg)) {
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

bool WebLightStore::loadColorsFromSD() {
    if (!SDManager::isReady() || !SD.exists(kColorPath)) {
        return false;
    }
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    File file = SD.open(kColorPath, FILE_READ);
    if (!file) {
        return false;
    }
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        return false;
    }
    colors_.clear();
    activeColorId_ = doc["active_color"].as<String>();
    JsonArray arr = doc["colors"].as<JsonArray>();
    if (arr.isNull()) {
        return false;
    }
    for (JsonObject item : arr) {
        CRGB primary, secondary;
        String errMsg;
        if (!parseColorPayload(item, primary, secondary, errMsg)) {
            continue;
        }
        ColorEntry entry;
        entry.id = item["id"].as<String>();
        entry.label = item["label"].as<String>();
        entry.primary = primary;
        entry.secondary = secondary;
        if (entry.id.isEmpty()) {
            continue;
        }
        colors_.push_back(entry);
    }
    return !colors_.empty();
}

void WebLightStore::loadDefaultPatterns() {
    patterns_.clear();
    const size_t count = sizeof(kDefaultPatterns) / sizeof(kDefaultPatterns[0]);
    patterns_.reserve(count);
    CRGB defaultPrimary = toCRGB(kDefaultColors[0].rgb1);
    CRGB defaultSecondary = toCRGB(kDefaultColors[0].rgb2);
    for (const auto& src : kDefaultPatterns) {
        PatternEntry entry;
        entry.id = src.id;
        entry.label = src.label;
        entry.params = makeParams(src, defaultPrimary, defaultSecondary);
        patterns_.push_back(entry);
    }
}

void WebLightStore::loadDefaultColors() {
    colors_.clear();
    const size_t count = sizeof(kDefaultColors) / sizeof(kDefaultColors[0]);
    colors_.reserve(count);
    for (const auto& src : kDefaultColors) {
        ColorEntry entry;
        entry.id = src.id;
        entry.label = src.label;
        entry.primary = toCRGB(src.rgb1);
        entry.secondary = toCRGB(src.rgb2);
        colors_.push_back(entry);
    }
}

bool WebLightStore::savePatternsToSD() const {
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
        fillPatternParams(params, entry);
    }
    const size_t written = serializeJson(doc, file);
    file.close();
    return written > 0;
}

bool WebLightStore::saveColorsToSD() const {
    if (!SDManager::isReady()) {
        return false;
    }
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    SD.remove(kColorPath);
    File file = SD.open(kColorPath, FILE_WRITE);
    if (!file) {
        return false;
    }
    DynamicJsonDocument doc(4096);
    doc["schema"] = kSchemaVersion;
    doc["active_color"] = activeColorId_;
    JsonArray arr = doc.createNestedArray("colors");
    for (const auto& entry : colors_) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = entry.id;
        if (!entry.label.isEmpty()) {
            obj["label"] = entry.label;
        }
        char buff[8];
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.primary.r, entry.primary.g, entry.primary.b);
        obj["rgb1_hex"] = buff;
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.secondary.r, entry.secondary.g, entry.secondary.b);
        obj["rgb2_hex"] = buff;
    }
    const size_t written = serializeJson(doc, file);
    file.close();
    return written > 0;
}

const WebLightStore::PatternEntry* WebLightStore::findPattern(const String& id) const {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& e){ return e.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

WebLightStore::PatternEntry* WebLightStore::findPattern(const String& id) {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](PatternEntry& e){ return e.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

const WebLightStore::ColorEntry* WebLightStore::findColor(const String& id) const {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](const ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

WebLightStore::ColorEntry* WebLightStore::findColor(const String& id) {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

bool WebLightStore::parsePatternParams(JsonVariantConst src, LightShowParams& out, String& errorMessage) {
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

bool WebLightStore::parseHexColor(const String& hex, CRGB& color) {
    if (hex.length() != 7 || hex.charAt(0) != '#') {
        return false;
    }
    long value = strtol(hex.c_str() + 1, nullptr, 16);
    color.r = static_cast<uint8_t>((value >> 16) & 0xFF);
    color.g = static_cast<uint8_t>((value >> 8) & 0xFF);
    color.b = static_cast<uint8_t>(value & 0xFF);
    return true;
}

bool WebLightStore::parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage) {
    JsonObjectConst obj = src.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("color invalid");
        return false;
    }

    String rgb1 = obj["rgb1_hex"].as<String>();
    String rgb2 = obj["rgb2_hex"].as<String>();

    if (rgb1.isEmpty() && obj.containsKey("primary")) {
        rgb1 = obj["primary"].as<String>();
    }
    if (rgb2.isEmpty() && obj.containsKey("secondary")) {
        rgb2 = obj["secondary"].as<String>();
    }

    if (!parseHexColor(rgb1, a) || !parseHexColor(rgb2, b)) {
        errorMessage = F("bad color");
        return false;
    }
    return true;
}

void WebLightStore::fillPatternParams(JsonObject dest, const PatternEntry& entry) {
    dest["color_cycle_sec"]  = entry.params.colorCycleSec;
    dest["bright_cycle_sec"] = entry.params.brightCycleSec;
    dest["fade_width"]       = entry.params.fadeWidth;
    dest["min_brightness"]   = entry.params.minBrightness;
    dest["gradient_speed"]   = entry.params.gradientSpeed;
    dest["center_x"]         = entry.params.centerX;
    dest["center_y"]         = entry.params.centerY;
    dest["radius"]           = entry.params.radius;
    dest["window_width"]     = entry.params.windowWidth;
    dest["radius_osc"]       = entry.params.radiusOsc;
    dest["x_amp"]            = entry.params.xAmp;
    dest["y_amp"]            = entry.params.yAmp;
    dest["x_cycle_sec"]      = entry.params.xCycleSec;
    dest["y_cycle_sec"]      = entry.params.yCycleSec;
}

String WebLightStore::generatePatternId() const {
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

String WebLightStore::generateColorId() const {
    int maxIndex = 0;
    for (const auto& entry : colors_) {
        if (entry.id.startsWith("color")) {
            int idx = entry.id.substring(5).toInt();
            if (idx > maxIndex) {
                maxIndex = idx;
            }
        }
    }
    char buff[12];
    snprintf(buff, sizeof(buff), "color%03d", maxIndex + 1);
    return String(buff);
}

void WebLightStore::applyActiveToLights() {
    ensureDefaults();
    previewActive_ = false;
    const PatternEntry* pattern = nullptr;
    if (!activePatternId_.isEmpty()) {
        pattern = findPattern(activePatternId_);
        if (!pattern && !patterns_.empty()) {
            pattern = &patterns_.front();
            activePatternId_ = pattern->id;
        }
    } else if (!patterns_.empty()) {
        pattern = &patterns_.front();
    }
    const ColorEntry* color = nullptr;
    if (!activeColorId_.isEmpty()) {
        color = findColor(activeColorId_);
    }
    if (!color && !colors_.empty()) {
        if (!activeColorId_.isEmpty()) {
            color = &colors_.front();
            activeColorId_ = color->id;
        } else {
            color = &colors_.front();
        }
    }
    LightShowParams params = pattern ? pattern->params : makeParams(kDefaultPatterns[0], toCRGB(kDefaultColors[0].rgb1), toCRGB(kDefaultColors[0].rgb2));
    if (color) {
        params.RGB1 = color->primary;
        params.RGB2 = color->secondary;
    }
    PF("[WebLightStore] Apply pattern=%s color=%s rgb1=%02X%02X%02X rgb2=%02X%02X%02X\n",
       pattern ? pattern->id.c_str() : "<none>",
       color ? color->id.c_str() : "<default>",
       params.RGB1.r, params.RGB1.g, params.RGB1.b,
       params.RGB2.r, params.RGB2.g, params.RGB2.b);
    PlayLightShow(params);
}
