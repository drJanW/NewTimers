#include "LightColors.h"

#include "Globals.h"
#include "SDManager.h"

#include <ArduinoJson.h>

namespace {

constexpr const char* kLightColorsFile = "light_colors.json";

class ScopedSDBusy {
public:
    ScopedSDBusy() : owns_(!SDManager::isBusy()) {
        if (owns_) {
            SDManager::setBusy(true);
        }
    }

    ~ScopedSDBusy() {
        if (owns_) {
            SDManager::setBusy(false);
        }
    }

    ScopedSDBusy(const ScopedSDBusy&) = delete;
    ScopedSDBusy& operator=(const ScopedSDBusy&) = delete;

private:
    bool owns_{false};
};

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

uint8_t parseByte(const String& hex, size_t offset) {
    int hi = hexValue(hex[offset]);
    int lo = hexValue(hex[offset + 1]);
    if (hi < 0 || lo < 0) {
        return 0;
    }
    return static_cast<uint8_t>((hi << 4) | lo);
}

} // namespace

bool HexToRgb(const String& hex, RgbColor& out) {
    if (hex.length() != 7 || hex.charAt(0) != '#') {
        return false;
    }
    for (size_t i = 1; i < hex.length(); ++i) {
        if (hexValue(hex[i]) < 0) {
            return false;
        }
    }
    out.r = parseByte(hex, 1);
    out.g = parseByte(hex, 3);
    out.b = parseByte(hex, 5);
    return true;
}

bool LightColorStore::begin(fs::FS& sd, const char* rootPath) {
    fs_ = &sd;
    if (rootPath && *rootPath) {
        root_ = rootPath;
    } else {
        root_ = "/";
    }

    root_.trim();
    if (root_.isEmpty()) {
        root_ = "/";
    }
    if (!root_.startsWith("/")) {
        root_ = String("/") + root_;
    }
    if (root_.length() > 1 && root_.endsWith("/")) {
        root_.remove(root_.length() - 1);
    }

    clear();
    loaded_ = load();
    return loaded_;
}

bool LightColorStore::ready() const {
    return loaded_ && fs_ != nullptr;
}

const LightColor* LightColorStore::find(const String& id) const {
    if (!ready()) {
        return nullptr;
    }
    for (const auto& color : colors_) {
        if (color.id == id) {
            return &color;
        }
    }
    return nullptr;
}

const LightColor* LightColorStore::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (!activeColorId_.isEmpty()) {
        const LightColor* color = find(activeColorId_);
        if (color) {
            return color;
        }
    }
    if (!colors_.empty()) {
        return &colors_.front();
    }
    return nullptr;
}

void LightColorStore::clear() {
    colors_.clear();
    loaded_ = false;
    activeColorId_.clear();
}

bool LightColorStore::load() {
    if (!fs_) {
        return false;
    }

    colors_.clear();
    activeColorId_.clear();

    ScopedSDBusy guard;
    const String path = pathFor(kLightColorsFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[LightColorStore] failed to open %s\n", path.c_str());
        return false;
    }

    const size_t fileSize = file.size();
    size_t capacity = fileSize + (fileSize / 2) + 2048;
    if (capacity < 4096) {
        capacity = 4096;
    }
    file.seek(0);

    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        PF("[LightColorStore] JSON parse failed for %s: %s\n", path.c_str(), err.c_str());
        return false;
    }

    auto parseColors = [&](JsonArrayConst colors) -> size_t {
        if (colors.isNull()) {
            PF("[LightColorStore] colors array missing\n");
            return 0;
        }

        size_t added = 0;
        colors_.reserve(colors_.size() + colors.size());
        for (JsonObjectConst item : colors) {
            LightColor parsed;
            parsed.id = item["id"].as<String>();
            parsed.label = item["label"].as<String>();
            const String rgb1 = item["rgb1_hex"].as<String>();
            const String rgb2 = item["rgb2_hex"].as<String>();
            if (parsed.id.isEmpty() || parsed.label.isEmpty() || rgb1.isEmpty() || rgb2.isEmpty()) {
                PF("[LightColorStore] skipping invalid color entry\n");
                continue;
            }
            if (!HexToRgb(rgb1, parsed.primary) || !HexToRgb(rgb2, parsed.secondary)) {
                PF("[LightColorStore] invalid hex colors for id=%s\n", parsed.id.c_str());
                continue;
            }
            parsed.valid = true;
            colors_.push_back(parsed);
            ++added;
        }

        return added;
    };

    bool parsed = false;
    JsonVariantConst root = doc.as<JsonVariantConst>();
    if (!root.isNull() && !root["schema"].isNull()) {
        const int schema = root["schema"].as<int>();
        if (schema != 1) {
            PF("[LightColorStore] Unsupported light_colors schema=%d\n", schema);
            return false;
        }
        activeColorId_ = root["active_color"].as<String>();
        parsed = parseColors(root["colors"].as<JsonArrayConst>()) > 0;
    } else if (!root.isNull() && !root["format_version"].isNull()) {
        const int formatVersion = root["format_version"].as<int>();
        if (formatVersion != 1) {
            PF("[LightColorStore] Unsupported light_colors format_version=%d\n", formatVersion);
            return false;
        }
        parsed = parseColors(root["colors"].as<JsonArrayConst>()) > 0;
    } else {
        PF("[LightColorStore] Missing schema or format_version\n");
        return false;
    }

    if (!parsed || colors_.empty()) {
        PF("[LightColorStore] no valid colors loaded\n");
        return false;
    }

    if (activeColorId_.isEmpty()) {
        activeColorId_ = colors_.front().id;
    }

    PF("[LightColorStore] Loaded %u light colors\n", static_cast<unsigned>(colors_.size()));
    return true;
}

String LightColorStore::pathFor(const char* file) const {
    if (!file || !*file) {
        return String();
    }
    if (root_.length() <= 1) {
        return String("/") + file;
    }
    return root_ + "/" + file;
}
