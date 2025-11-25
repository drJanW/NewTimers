#include "ColorsStore.h"

#include <SD.h>
#include <algorithm>
#include <ctype.h>

#include "PatternStore.h"
#include "SDManager.h"
#include "CsvUtils.h"
#include "SDBusyGuard.h"

namespace {
constexpr const char* kColorPath = "/light_colors.csv";
constexpr const char* kActiveColorPrefix = "# active_color=";
constexpr size_t kActiveColorPrefixLen = sizeof("# active_color=") - 1;
constexpr uint8_t kSchemaVersion = 1;

struct DefaultColor {
    const char* id;
    const char* label;
    uint32_t rgb1;
    uint32_t rgb2;
};

constexpr DefaultColor kDefaultColors[] = {
    {"1", "Warm Sunset", 0xFF7F00, 0x552200},
    {"2", "Arctic Ice",   0x00C6FF, 0x003F7F},
    {"3", "Forest Dew",   0x3FAF4E, 0x0B3D17},
    {"4", "Royal Magenta",0xA83279, 0x2B2E83},
    {"5", "Golden Hour",  0xFFE066, 0xFF6B6B},
    {"6", "Coral Reef",   0x1CD8D2, 0x93EDC7}
};

CRGB toCRGB(uint32_t value) {
    return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF));
}

bool isNumericId(const String& id) {
    if (id.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < id.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(id[i]))) {
            return false;
        }
    }
    return true;
}

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
    // 40+ color entries need generous heap; reserve 12 KB to avoid truncation/empty JSON
    DynamicJsonDocument doc(12288);
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
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.colorA.r, entry.colorA.g, entry.colorA.b);
        obj["colorA_hex"] = buff;
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.colorB.r, entry.colorB.g, entry.colorB.b);
        obj["colorB_hex"] = buff;
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
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        const bool isObject = body.is<JsonObject>();
        PF("[ColorsStore] updateColor reject: JsonObjectConst null (isObject=%d isNull=%d)\n",
           isObject ? 1 : 0,
           body.isNull() ? 1 : 0);
        return false;
    }
    String rawBody;
    serializeJson(body, rawBody);
    PF("[ColorsStore] updateColor payload=%s\n", rawBody.c_str());
    CRGB colorA;
    CRGB colorB;
    JsonVariantConst colorVariant = obj["color"];
    if (colorVariant.isNull()) {
        colorVariant = obj;
    }
    if (!parseColorPayload(colorVariant, colorA, colorB, errorMessage)) {
        return false;
    }
    JsonObjectConst colorObj = colorVariant.as<JsonObjectConst>();
    String label = obj["label"].as<String>();
    if (label.isEmpty() && !colorObj.isNull() && colorObj.containsKey("label")) {
        label = colorObj["label"].as<String>();
    }
    const bool labelKeyPresent = obj.containsKey("label") || (!colorObj.isNull() && colorObj.containsKey("label"));
    if (label.length() > 48) {
        label = label.substring(0, 48);
    }
    sanitizeLabel(label);
    // Label guard disabled: allow updates without label (legacy clients)
    // if (!labelKeyPresent || label.isEmpty()) {
    //     errorMessage = F("label required");
    //     const String logId = obj["id"].as<String>();
    //     PF("[ColorsStore] updateColor reject: missing label for id=%s\n", logId.c_str());
    //     return false;
    // }
    bool select = obj["select"].as<bool>();

    auto resolveId = [&]() -> String {
        if (obj.containsKey("id")) {
            return obj["id"].as<String>();
        }
        if (obj.containsKey("color_id")) {
            return obj["color_id"].as<String>();
        }
        if (!colorObj.isNull() && colorObj.containsKey("id")) {
            return colorObj["id"].as<String>();
        }
        return String();
    };

    const String resolvedId = resolveId();

    bool shouldApply = false;

    if (!resolvedId.isEmpty()) {
        String id = resolvedId;
        ColorEntry* existing = findColor(id);
        if (!existing) {
            errorMessage = F("color not found");
            return false;
        }
        existing->colorA = colorA;
        existing->colorB = colorB;
        existing->label = label;
        ensureLabelForId(existing->id, existing->label);
        affectedId = existing->id;
        if (select) {
            activeColorId_ = existing->id;
            shouldApply = true;
        } else if (activeColorId_ == existing->id) {
            shouldApply = true;
        }
        if (select) {
        }
    } else {
        ColorEntry entry;
        entry.id = generateColorId();
        entry.label = label;
        ensureLabelForId(entry.id, entry.label);
        entry.colorA = colorA;
        entry.colorB = colorB;
        colors_.push_back(entry);
        affectedId = entry.id;
        if (select || activeColorId_.isEmpty()) {
            activeColorId_ = entry.id;
            shouldApply = true;
        }
    }

    if (!saveColorsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    if (shouldApply) {
        applyActiveToLights();
    }
    return true;
}

bool ColorsStore::deleteColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
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
    CRGB colorA;
    CRGB colorB;
    if (!parseColorPayload(colorVariant, colorA, colorB, errorMessage)) {
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
    previewBackupColorA_ = colorA;
    previewBackupColorB_ = colorB;
    params.RGB1 = colorA;
    params.RGB2 = colorB;
    PlayLightShow(params);
    previewActive_ = true;
    PF("[ColorsStore] preview applied\n");
    return true;
}

bool ColorsStore::previewColors(JsonVariantConst body, String& errorMessage)
{
    PatternStore& patternStore = PatternStore::instance();
    if (!patternStore.isReady())
    {
        patternStore.begin();
    }

    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull())
    {
        errorMessage = F("invalid payload");
        PF("[ColorsStore] previewColors reject: body not object\n");
        return false;
    }

    JsonVariantConst colorVariant = obj["color"];
    if (colorVariant.isNull()) {
        colorVariant = body;
    }
    CRGB colorA;
    CRGB colorB;
    if (!parseColorPayload(colorVariant, colorA, colorB, errorMessage))
    {
        PF("[ColorsStore] previewColors reject: color parse failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }

    LightShowParams params = patternStore.getActiveParams();

    String colorJson;
    serializeJson(colorVariant, colorJson);
    const char *colorId = obj["color_id"] | obj["id"] | "";
    PF("[ColorsStore] previewColors request colorId='%s' color=%s\n",
       colorId,
       colorJson.c_str());

    previewBackupParams_ = params;
    previewBackupColorA_ = colorA;
    previewBackupColorB_ = colorB;
    params.RGB1 = colorA;
    params.RGB2 = colorB;
    PlayLightShow(params);
    previewActive_ = true;
    PF("[ColorsStore] previewColors applied\n");
    return true;
}

void ColorsStore::ensureColorDefaults() {
    if (colors_.empty()) {
        loadDefaultColors();
    }
}

bool ColorsStore::loadColorsFromSD() {
    if (!SDManager::isReady()) {
        return false;
    }
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    if (!SD.exists(kColorPath)) {
        return false;
    }
    File file = SD.open(kColorPath, FILE_READ);
    if (!file) {
        return false;
    }

    colors_.clear();
    activeColorId_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(8);
    bool headerConsumed = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty()) {
            continue;
        }
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.charAt(0) == '#') {
            if (trimmed.startsWith(F("# active_color="))) {
                activeColorId_ = trimmed.substring(kActiveColorPrefixLen);
                activeColorId_.trim();
            }
            continue;
        }
        if (!headerConsumed) {
            headerConsumed = true;
            if (trimmed.startsWith(F("light_colors_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 4) {
            continue;
        }

        ColorEntry entry;
        entry.id = columns[0];
        entry.label = columns[1];
        PF("[ColorsStore] CSV row id='%s' label='%s'\n", entry.id.c_str(), entry.label.c_str());
        sanitizeLabel(entry.label);
        ensureLabelForId(entry.id, entry.label);
        const String rgb1 = columns[2];
        const String rgb2 = columns[3];
        if (entry.id.isEmpty() || rgb1.isEmpty() || rgb2.isEmpty()) {
            continue;
        }
        if (!parseHexColor(rgb1, entry.colorA) || !parseHexColor(rgb2, entry.colorB)) {
            PF("[ColorsStore] invalid hex in CSV id=%s\n", entry.id.c_str());
            continue;
        }
        colors_.push_back(entry);
    }

    file.close();
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
        entry.colorA = toCRGB(src.rgb1);
        entry.colorB = toCRGB(src.rgb2);
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

    if (!activeColorId_.isEmpty()) {
        file.print(F("# active_color="));
        file.println(activeColorId_);
    }

    file.println(F("light_colors_id;light_colors_name;rgb1_hex;rgb2_hex"));
    for (const auto& entry : colors_) {
        file.print(entry.id);
        file.print(';');
        file.print(entry.label);
        file.print(';');
        char buff[7];
        snprintf(buff, sizeof(buff), "%02X%02X%02X", entry.colorA.r, entry.colorA.g, entry.colorA.b);
        file.print('#');
        file.print(buff);
        file.print(';');
        snprintf(buff, sizeof(buff), "%02X%02X%02X", entry.colorB.r, entry.colorB.g, entry.colorB.b);
        file.print('#');
        file.print(buff);
        file.println();
    }

    file.close();
    return true;
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

void ColorsStore::sanitizeLabel(String& label) {
    label.trim();
    if (label.equalsIgnoreCase(F("null"))) {
        label.clear();
    }
}

void ColorsStore::ensureLabelForId(const String& id, String& label) {
    label.trim();
    if (!label.isEmpty()) {
        return;
    }
    const String fallback = lookupDefaultLabel(id);
    if (!fallback.isEmpty()) {
        label = fallback;
    } else if (!id.isEmpty()) {
        label = id;
    }
}

String ColorsStore::lookupDefaultLabel(const String& id) {
    for (const auto& color : kDefaultColors) {
        if (id.equals(color.id)) {
            return String(color.label);
        }
    }
    return String();
}

bool ColorsStore::parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage) {
    JsonObjectConst obj = src.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("color invalid");
        return false;
    }

    auto readColorHex = [&](const char* hexKey,
                            const char* plainKey,
                            const char* legacyHexKey,
                            const char* legacyPlainKey) -> String {
        if (hexKey && obj.containsKey(hexKey)) {
            return obj[hexKey].as<String>();
        }
        if (plainKey && obj.containsKey(plainKey)) {
            return obj[plainKey].as<String>();
        }
        if (legacyHexKey && obj.containsKey(legacyHexKey)) {
            return obj[legacyHexKey].as<String>();
        }
        if (legacyPlainKey && obj.containsKey(legacyPlainKey)) {
            return obj[legacyPlainKey].as<String>();
        }
        return String();
    };

    const String colorAHex = readColorHex("colorA_hex", "colorA", "rgb1_hex", "primary");
    const String colorBHex = readColorHex("colorB_hex", "colorB", "rgb2_hex", "secondary");

    if (!parseHexColor(colorAHex, a) || !parseHexColor(colorBHex, b)) {
        errorMessage = F("bad color");
        return false;
    }
    return true;
}

String ColorsStore::generateColorId() const {
    int maxIndex = 0;
    bool sawPrefixed = false;
    bool sawNumeric = false;
    for (const auto& entry : colors_) {
        const String& id = entry.id;
        int idx = 0;
        if (id.startsWith("color")) {
            sawPrefixed = true;
            idx = id.substring(5).toInt();
        } else if (isNumericId(id)) {
            sawNumeric = true;
            idx = id.toInt();
        }
        if (idx > maxIndex) {
            maxIndex = idx;
        }
    }
    int next = maxIndex + 1;
    if (!sawPrefixed || sawNumeric) {
        return String(next);
    }
    char buff[12];
    snprintf(buff, sizeof(buff), "color%03d", next);
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
        if (!color) {
            PF("[ColorsStore] Active color '%s' missing, clearing override\n", activeColorId_.c_str());
            activeColorId_.clear();
        }
    }

    const ColorEntry* fallbackColor = nullptr;
    if (!color && !colors_.empty()) {
        fallbackColor = &colors_.front();
    }

    if (color) {
        params.RGB1 = color->colorA;
        params.RGB2 = color->colorB;
    } else if (fallbackColor) {
        params.RGB1 = fallbackColor->colorA;
        params.RGB2 = fallbackColor->colorB;
    } else {
        params.RGB1 = toCRGB(kDefaultColors[0].rgb1);
        params.RGB2 = toCRGB(kDefaultColors[0].rgb2);
    }

    PF("[ColorsStore] Apply pattern=%s color=%s rgb1=%02X%02X%02X rgb2=%02X%02X%02X\n",
       patternId.c_str(),
       color ? color->id.c_str() : (fallbackColor ? fallbackColor->id.c_str() : "<default>"),
       params.RGB1.r, params.RGB1.g, params.RGB1.b,
       params.RGB2.r, params.RGB2.g, params.RGB2.b);
    PlayLightShow(params);
}

// Color shifting functions for dynamic adjustment
CRGB colorShiftHSV(const CRGB &oldRGB,
                   int hueShift,        // + = vooruit op hue-cirkel, − = terug
                   int satShift,        // + = meer kleur, − = richting wit
                   int valShift,        // + = helderder, − = donkerder
                   int whiteShift)      // extra wit = saturation omlaag
{
    // Convert RGB → HSV (use approx variant to avoid unavailable helper)
    CHSV hsv=rgb2hsv_approximate(oldRGB);

    // 1. Hue shift (wrap automatisch in uint8)
    hsv.h += hueShift;

    // 2. Saturation shift
    if (satShift >= 0)
        hsv.s = qadd8(hsv.s, (uint8_t)satShift);
    else
        hsv.s = qsub8(hsv.s, (uint8_t)(-satShift));

    // 3. Value shift
    if (valShift >= 0)
        hsv.v = qadd8(hsv.v, (uint8_t)valShift);
    else
        hsv.v = qsub8(hsv.v, (uint8_t)(-valShift));

    // 4. White-shift = saturation omlaag
    if (whiteShift != 0) {
        if (whiteShift > 0)
            hsv.s = qsub8(hsv.s, (uint8_t)whiteShift);
        else
            hsv.s = qadd8(hsv.s, (uint8_t)(-whiteShift));
    }

    return CRGB(hsv);
}

CRGB colorShiftRGB(const CRGB &oldRGB,
                int redShift,
                int greenShift,
                int blueShift,
                int whiteShift)
{
    uint8_t r = oldRGB.r;
    uint8_t g = oldRGB.g;
    uint8_t b = oldRGB.b;

    // R-shift
    r = (redShift >= 0)
        ? qadd8(r, (uint8_t)redShift)
        : qsub8(r, (uint8_t)(-redShift));

    // G-shift
    g = (greenShift >= 0)
        ? qadd8(g, (uint8_t)greenShift)
        : qsub8(g, (uint8_t)(-greenShift));

    // B-shift
    b = (blueShift >= 0)
        ? qadd8(b, (uint8_t)blueShift)
        : qsub8(b, (uint8_t)(-blueShift));

    // White-shift = tegelijk alle kanalen
    if (whiteShift != 0) {
        if (whiteShift > 0) {
            uint8_t w = (uint8_t)whiteShift;
            r = qadd8(r, w);
            g = qadd8(g, w);
            b = qadd8(b, w);
        } else {
            uint8_t w = (uint8_t)(-whiteShift);
            r = qsub8(r, w);
            g = qsub8(g, w);
            b = qsub8(b, w);
        }
    }

    return CRGB(r, g, b);
}

