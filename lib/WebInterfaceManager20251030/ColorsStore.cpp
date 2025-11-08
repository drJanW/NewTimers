#include "ColorsStore.h"

#include <SD.h>
#include <algorithm>

#include "PatternStore.h"
#include "SDManager.h"

namespace {
constexpr const char* kColorPath = "/light_colors.json";
constexpr uint8_t kSchemaVersion = 1;

struct DefaultColor {
    const char* id;
    const char* label;
    uint32_t rgb1;
    uint32_t rgb2;
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

} // namespace

ColorsStore& ColorsStore::instance() {
    static ColorsStore inst;
    return inst;
}

void ColorsStore::begin() {
    if (ready_) {
        return;
    }
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    colors_.clear();
    bool colorsLoaded = loadColorsFromSD();
    if (!colorsLoaded) {
        loadDefaultColors();
        saveColorsToSD();
    }

    applyActiveToLights();
    ready_ = true;
}

bool ColorsStore::isReady() const {
    return ready_;
}

String ColorsStore::buildPatternsJson() const {
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    return patternStore.buildJson();
}

String ColorsStore::buildColorsJson() const {
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

String ColorsStore::getActivePatternId() const {
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    return patternStore.activeId();
}

bool ColorsStore::selectPattern(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        return false;
    }
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    if (!patternStore.select(id, errorMessage)) {
        return false;
    }
    applyActiveToLights();
    return true;
}

bool ColorsStore::selectColor(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("store not ready");
        PF("[ColorsStore] selectColor rejected: store not ready\n");
        return false;
    }
    if (id.isEmpty()) {
        activeColorId_ = String();
        applyActiveToLights();
        PF("[ColorsStore] Color cleared to defaults\n");
        return true;
    }
    ColorEntry* entry = findColor(id);
    if (!entry) {
        errorMessage = F("color not found");
        PF("[ColorsStore] selectColor unknown id='%s'\n", id.c_str());
        return false;
    }
    activeColorId_ = entry->id;
    PF("[ColorsStore] Color select %s\n", activeColorId_.c_str());
    applyActiveToLights();
    return true;
}

bool ColorsStore::updatePattern(JsonVariantConst body, String& affectedId, String& errorMessage) {
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    if (!patternStore.update(body, affectedId, errorMessage)) {
        return false;
    }
    applyActiveToLights();
    return true;
}

bool ColorsStore::deletePattern(JsonVariantConst body, String& affectedId, String& errorMessage) {
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    if (!patternStore.remove(body, affectedId, errorMessage)) {
        return false;
    }
    applyActiveToLights();
    return true;
}

bool ColorsStore::updateColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
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

bool ColorsStore::deleteColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
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

bool ColorsStore::preview(JsonVariantConst body, String& errorMessage) {
    String rawBody;
    serializeJson(body, rawBody);
    PF("[ColorsStore] preview entry raw=%s\n", rawBody.c_str());

    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }

    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        PF("[ColorsStore] preview reject: body not object\n");
        return false;
    }
    JsonVariantConst patternVariant = obj["pattern"];
    JsonVariantConst colorVariant = obj["color"];
    if (!patternVariant || !colorVariant) {
        errorMessage = F("pattern/color missing");
        PF("[ColorsStore] preview reject: missing sections (pattern=%d color=%d)\n",
           patternVariant.isNull() ? 0 : 1,
           colorVariant.isNull() ? 0 : 1);
        return false;
    }
    LightShowParams params;
    if (!patternStore.parseParams(patternVariant, params, errorMessage)) {
        PF("[ColorsStore] preview reject: pattern parse failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }
    CRGB primary;
    CRGB secondary;
    if (!parseColorPayload(colorVariant, primary, secondary, errorMessage)) {
        PF("[ColorsStore] preview reject: color parse failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }

    String patternJson;
    String colorJson;
    serializeJson(patternVariant, patternJson);
    serializeJson(colorVariant, colorJson);
    const char* patternId = obj["pattern_id"] | "";
    const char* colorId = obj["color_id"] | "";
    PF("[ColorsStore] preview request patternId='%s' colorId='%s' pattern=%s color=%s\n",
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
    PF("[ColorsStore] preview applied\n");
    return true;
}

void ColorsStore::ensureColorDefaults() {
    if (colors_.empty()) {
        loadDefaultColors();
    }
}

bool ColorsStore::loadColorsFromSD() {
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

void ColorsStore::loadDefaultColors() {
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

bool ColorsStore::saveColorsToSD() const {
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

const ColorsStore::ColorEntry* ColorsStore::findColor(const String& id) const {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](const ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

ColorsStore::ColorEntry* ColorsStore::findColor(const String& id) {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

bool ColorsStore::parseHexColor(const String& hex, CRGB& color) {
    if (hex.length() != 7 || hex.charAt(0) != '#') {
        return false;
    }
    long value = strtol(hex.c_str() + 1, nullptr, 16);
    color.r = static_cast<uint8_t>((value >> 16) & 0xFF);
    color.g = static_cast<uint8_t>((value >> 8) & 0xFF);
    color.b = static_cast<uint8_t>(value & 0xFF);
    return true;
}

bool ColorsStore::parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage) {
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

String ColorsStore::generateColorId() const {
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

void ColorsStore::applyActiveToLights() {
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady()) {
        patternStore.begin();
    }
    ensureColorDefaults();
    previewActive_ = false;

    LightShowParams params = patternStore.getActiveParams();
    String patternId = patternStore.activeId();
    if (patternId.isEmpty()) {
        patternId = F("<context>");
    }

    const ColorEntry* color = nullptr;
    if (!activeColorId_.isEmpty()) {
        color = findColor(activeColorId_);
    }
    if (!color && !colors_.empty()) {
        color = &colors_.front();
        activeColorId_ = color->id;
    }

    if (color) {
        params.RGB1 = color->primary;
        params.RGB2 = color->secondary;
    } else {
        params.RGB1 = toCRGB(kDefaultColors[0].rgb1);
        params.RGB2 = toCRGB(kDefaultColors[0].rgb2);
    }

    PF("[ColorsStore] Apply pattern=%s color=%s rgb1=%02X%02X%02X rgb2=%02X%02X%02X\n",
       patternId.c_str(),
       color ? color->id.c_str() : "<default>",
       params.RGB1.r, params.RGB1.g, params.RGB1.b,
       params.RGB2.r, params.RGB2.g, params.RGB2.b);
    PlayLightShow(params);
}

