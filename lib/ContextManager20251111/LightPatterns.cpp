#include "LightPatterns.h"

#include "Globals.h"
#include "SDManager.h"

#include <ArduinoJson.h>

namespace {

constexpr const char* kLightPatternsFile = "light_patterns.json";

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

} // namespace

bool LightPatternStore::begin(fs::FS& sd, const char* rootPath) {
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

bool LightPatternStore::ready() const {
    return loaded_ && fs_ != nullptr;
}

const LightPattern* LightPatternStore::find(const String& id) const {
    if (!ready()) {
        return nullptr;
    }
    for (const auto& pattern : patterns_) {
        if (pattern.id == id) {
            return &pattern;
        }
    }
    return nullptr;
}

const LightPattern* LightPatternStore::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (!activePatternId_.isEmpty()) {
        const LightPattern* pattern = find(activePatternId_);
        if (pattern) {
            return pattern;
        }
    }
    if (!patterns_.empty()) {
        return &patterns_.front();
    }
    return nullptr;
}

void LightPatternStore::clear() {
    patterns_.clear();
    loaded_ = false;
    activePatternId_.clear();
}

bool LightPatternStore::load() {
    if (!fs_) {
        return false;
    }

    patterns_.clear();
    activePatternId_.clear();

    ScopedSDBusy guard;
    const String path = pathFor(kLightPatternsFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[LightPatternStore] failed to open %s\n", path.c_str());
        return false;
    }

    const size_t fileSize = file.size();
    size_t capacity = fileSize + (fileSize / 2) + 4096;
    if (capacity < 8192) {
        capacity = 8192;
    }
    file.seek(0);

    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        PF("[LightPatternStore] JSON parse failed for %s: %s\n", path.c_str(), err.c_str());
        return false;
    }

    auto parsePatterns = [&](JsonArrayConst items) -> size_t {
        if (items.isNull()) {
            PF("[LightPatternStore] patterns array missing\n");
            return 0;
        }

        size_t added = 0;
        patterns_.reserve(patterns_.size() + items.size());
        for (JsonObjectConst item : items) {
            LightPattern parsed;
            parsed.id = item["id"].as<String>();
            parsed.label = item["label"].as<String>();
            JsonObjectConst params = item["params"].as<JsonObjectConst>();
            if (parsed.id.isEmpty() || parsed.label.isEmpty() || params.isNull()) {
                PF("[LightPatternStore] skipping invalid pattern entry\n");
                continue;
            }

            parsed.color_cycle_sec = params["color_cycle_sec"].as<float>();
            parsed.bright_cycle_sec = params["bright_cycle_sec"].as<float>();
            parsed.fade_width = params["fade_width"].as<float>();
            parsed.min_brightness = params["min_brightness"].as<float>();
            parsed.gradient_speed = params["gradient_speed"].as<float>();
            parsed.center_x = params["center_x"].as<float>();
            parsed.center_y = params["center_y"].as<float>();
            parsed.radius = params["radius"].as<float>();
            parsed.window_width = params["window_width"].as<float>();
            parsed.radius_osc = params["radius_osc"].as<float>();
            parsed.x_amp = params["x_amp"].as<float>();
            parsed.y_amp = params["y_amp"].as<float>();
            parsed.x_cycle_sec = params["x_cycle_sec"].as<float>();
            parsed.y_cycle_sec = params["y_cycle_sec"].as<float>();

            parsed.valid = true;
            patterns_.push_back(parsed);
            ++added;
        }
        return added;
    };

    bool parsed = false;
    JsonVariantConst root = doc.as<JsonVariantConst>();
    if (!root.isNull() && !root["schema"].isNull()) {
        const int schema = root["schema"].as<int>();
        if (schema != 1) {
            PF("[LightPatternStore] Unsupported light_patterns schema=%d\n", schema);
            return false;
        }
        activePatternId_ = root["active_pattern"].as<String>();
        parsed = parsePatterns(root["patterns"].as<JsonArrayConst>()) > 0;
    } else if (!root.isNull() && !root["format_version"].isNull()) {
        const int formatVersion = root["format_version"].as<int>();
        if (formatVersion != 1) {
            PF("[LightPatternStore] Unsupported light_patterns format_version=%d\n", formatVersion);
            return false;
        }
        parsed = parsePatterns(root["patterns"].as<JsonArrayConst>()) > 0;
    } else {
        PF("[LightPatternStore] Missing schema or format_version\n");
        return false;
    }

    if (!parsed || patterns_.empty()) {
        PF("[LightPatternStore] no valid patterns loaded\n");
        return false;
    }

    if (activePatternId_.isEmpty()) {
        activePatternId_ = patterns_.front().id;
    }

    PF("[LightPatternStore] Loaded %u light patterns\n", static_cast<unsigned>(patterns_.size()));
    return true;
}

String LightPatternStore::pathFor(const char* file) const {
    if (!file || !*file) {
        return String();
    }
    if (root_.length() <= 1) {
        return String("/") + file;
    }
    return root_ + "/" + file;
}
