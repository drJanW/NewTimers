#include "LightPatterns.h"

#include "Globals.h"
#include "SDManager.h"

#include <vector>
#include <ctype.h>

#include "CsvUtils.h"

namespace {

constexpr const char* kLightPatternsFile = "light_patterns.csv";

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

bool parsePatternId(const String& value, uint8_t& out) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(value.charAt(i)))) {
            return false;
        }
    }
    const long parsed = value.toInt();
    if (parsed <= 0 || parsed > 255) {
        return false;
    }
    out = static_cast<uint8_t>(parsed);
    return true;
}

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

const LightPattern* LightPatternStore::find(uint8_t id) const {
    if (!ready() || id == 0) {
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
    if (activePatternId_ != 0) {
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
    activePatternId_ = 0;
}

bool LightPatternStore::load() {
    if (!fs_) {
        return false;
    }

    patterns_.clear();
    activePatternId_ = 0;

    ScopedSDBusy guard;
    const String path = pathFor(kLightPatternsFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[LightPatternStore] failed to open %s\n", path.c_str());
        return false;
    }

    String line;
    std::vector<String> columns;
    columns.reserve(18);
    bool headerSkipped = false;
    size_t loaded = 0;

    auto toFloat = [](const String& value) -> float {
        return value.isEmpty() ? 0.0f : value.toFloat();
    };

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("light_pattern_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 16) {
            continue;
        }

        uint8_t id = 0;
        if (!parsePatternId(columns[0], id)) {
            continue;
        }

        LightPattern pattern;
        pattern.id = id;
        pattern.label = columns[1];

        pattern.color_cycle_sec  = toFloat(columns[2]);
        pattern.bright_cycle_sec = toFloat(columns[3]);
        pattern.fade_width       = toFloat(columns[4]);
        pattern.min_brightness   = toFloat(columns[5]);
        pattern.gradient_speed   = toFloat(columns[6]);
        pattern.center_x         = toFloat(columns[7]);
        pattern.center_y         = toFloat(columns[8]);
        pattern.radius           = toFloat(columns[9]);
        pattern.window_width     = toFloat(columns[10]);
        pattern.radius_osc       = toFloat(columns[11]);
        pattern.x_amp            = toFloat(columns[12]);
        pattern.y_amp            = toFloat(columns[13]);
        pattern.x_cycle_sec      = toFloat(columns[14]);
        pattern.y_cycle_sec      = toFloat(columns[15]);
        pattern.valid = true;

        patterns_.push_back(pattern);
        ++loaded;
    }

    file.close();

    if (patterns_.empty()) {
        PF("[LightPatternStore] no valid patterns loaded from %s\n", path.c_str());
        return false;
    }

    activePatternId_ = patterns_.front().id;
    PF("[LightPatternStore] Loaded %u light patterns\n", static_cast<unsigned>(loaded));
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
