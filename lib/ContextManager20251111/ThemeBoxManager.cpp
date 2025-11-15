#include "ThemeBoxManager.h"

#include "Globals.h"
#include "SDManager.h"

#include <ArduinoJson.h>
#include <utility>

namespace {

constexpr const char* kThemeBoxesFile = "theme_boxes.json";

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

bool ThemeBoxManager::begin(fs::FS& sd, const char* rootPath) {
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

bool ThemeBoxManager::ready() const {
    return loaded_ && fs_ != nullptr;
}

const ThemeBox* ThemeBoxManager::find(const String& id) const {
    if (!ready() || id.isEmpty()) {
        return nullptr;
    }
    for (const auto& box : boxes_) {
        if (box.id.equalsIgnoreCase(id)) {
            return &box;
        }
    }
    
    bool numeric = true;
    for (size_t i = 0; i < static_cast<size_t>(id.length()); ++i) {
        const char c = id.charAt(i);
        if (c < '0' || c > '9') {
            numeric = false;
            break;
        }
    }
    
    if (numeric) {
        long value = id.toInt();
        if (value >= 0 && value <= 65535) {
            fallback_ = ThemeBox{};
            fallback_.valid = true;
            fallback_.id = id;
            fallback_.entries.clear();
            fallback_.entries.push_back(static_cast<uint16_t>(value));
            return &fallback_;
        }
    }
    
    return nullptr;
}

const ThemeBox* ThemeBoxManager::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (!activeThemeBoxId_.isEmpty()) {
        const ThemeBox* box = find(activeThemeBoxId_);
        if (box) {
            return box;
        }
    }
    if (!boxes_.empty()) {
        return &boxes_.front();
    }
    return nullptr;
}

void ThemeBoxManager::clear() {
    boxes_.clear();
    loaded_ = false;
    activeThemeBoxId_.clear();
    fallback_ = ThemeBox{};
}

bool ThemeBoxManager::load() {
    if (!fs_) {
        return false;
    }

    boxes_.clear();
    fallback_ = ThemeBox{};
    activeThemeBoxId_.clear();

    ScopedSDBusy guard;
    const String path = pathFor(kThemeBoxesFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[ThemeBoxManager] failed to open %s\n", path.c_str());
        return false;
    }

    const size_t fileSize = file.size();
    size_t capacity = fileSize > 0 ? fileSize + (fileSize / 2) + 2048 : 4096;
    if (capacity < 4096) {
        capacity = 4096;
    }
    if (capacity > 65536) {
        capacity = 65536;
    }
    file.seek(0);

    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        PF("[ThemeBoxManager] JSON parse failed for %s: %s\n", path.c_str(), err.c_str());
        return false;
    }

    auto parseBoxes = [&](JsonArrayConst array, const char* entriesKey) -> size_t {
        if (array.isNull()) {
            PF("[ThemeBoxManager] theme_boxes array missing\n");
            return 0;
        }

        size_t added = 0;
        boxes_.reserve(boxes_.size() + array.size());
        for (JsonObjectConst box : array) {
            ThemeBox parsed;
            JsonVariantConst idValue = box["id"];
            if (idValue.isNull()) {
                PF("[ThemeBoxManager] theme box missing id, skipping\n");
                continue;
            }

            if (idValue.is<const char*>()) {
                parsed.id = String(idValue.as<const char*>());
            } else if (idValue.is<String>()) {
                parsed.id = idValue.as<String>();
            } else if (idValue.is<long>() || idValue.is<int>()) {
                parsed.id = String(idValue.as<long>());
            }

            JsonArrayConst entries = box[entriesKey].as<JsonArrayConst>();
            if (parsed.id.isEmpty() || entries.isNull()) {
                PF("[ThemeBoxManager] skipping invalid theme box entry\n");
                continue;
            }

            parsed.entries.reserve(entries.size());
            for (JsonVariantConst value : entries) {
                if (!value.is<int>()) {
                    continue;
                }
                int dir = value.as<int>();
                if (dir < 0 || dir > 65535) {
                    continue;
                }
                parsed.entries.push_back(static_cast<uint16_t>(dir));
            }

            if (parsed.entries.empty()) {
                PF("[ThemeBoxManager] theme box %s has no entries, skipping\n", parsed.id.c_str());
                continue;
            }

            parsed.valid = true;
            boxes_.push_back(std::move(parsed));
            ++added;
        }

        return added;
    };

    bool parsed = false;
    JsonVariantConst root = doc.as<JsonVariantConst>();
    if (!root.isNull() && !root["schema"].isNull()) {
        const int schema = root["schema"].as<int>();
        if (schema != 1) {
            PF("[ThemeBoxManager] Unsupported theme_boxes schema=%d\n", schema);
            return false;
        }
        activeThemeBoxId_ = root["active_theme_box"].as<String>();
        parsed = parseBoxes(root["theme_boxes"].as<JsonArrayConst>(), "entries") > 0;
    } else if (!root.isNull() && !root["format_version"].isNull()) {
        const int format = root["format_version"].as<int>();
        if (format != 1) {
            PF("[ThemeBoxManager] Unsupported theme_boxes format_version=%d\n", format);
            return false;
        }
        parsed = parseBoxes(root["theme_boxes"].as<JsonArrayConst>(), "dir_ids") > 0;
        if (parsed && activeThemeBoxId_.isEmpty() && !boxes_.empty()) {
            activeThemeBoxId_ = boxes_.front().id;
        }
    } else {
        PF("[ThemeBoxManager] Missing schema or format_version\n");
        return false;
    }

    if (!parsed || boxes_.empty()) {
        PF("[ThemeBoxManager] no valid theme boxes loaded\n");
        return false;
    }

    if (activeThemeBoxId_.isEmpty()) {
        activeThemeBoxId_ = boxes_.front().id;
    }

    PF("[ThemeBoxManager] Loaded %u theme boxes\n", static_cast<unsigned>(boxes_.size()));
    return true;
}

String ThemeBoxManager::pathFor(const char* file) const {
    if (!file || !*file) {
        return String();
    }
    if (root_.length() <= 1) {
        return String("/") + file;
    }
    return root_ + "/" + file;
}
