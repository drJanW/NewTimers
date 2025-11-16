#include "ThemeBoxManager.h"

#include "Globals.h"
#include "SDManager.h"

#include <utility>
#include <vector>
#include <ctype.h>

#include "CsvUtils.h"

namespace {

constexpr const char* kThemeBoxesFile = "theme_boxes.csv";

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

bool parseThemeBoxId(const String& value, uint8_t& out) {
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

const ThemeBox* ThemeBoxManager::find(uint8_t id) const {
    if (!ready() || id == 0) {
        return nullptr;
    }
    for (const auto& box : boxes_) {
        if (box.id == id) {
            return &box;
        }
    }
    return nullptr;
}

const ThemeBox* ThemeBoxManager::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (activeThemeBoxId_ != 0) {
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
    activeThemeBoxId_ = 0;
}

bool ThemeBoxManager::load() {
    if (!fs_) {
        return false;
    }

    boxes_.clear();
    activeThemeBoxId_ = 0;

    ScopedSDBusy guard;
    const String path = pathFor(kThemeBoxesFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[ThemeBoxManager] failed to open %s\n", path.c_str());
        return false;
    }

    auto parseEntries = [](const String& csv, std::vector<uint16_t>& out) {
        out.clear();
        int start = 0;
        const int len = csv.length();
        while (start <= len) {
            int idx = csv.indexOf(',', start);
            String token = (idx < 0) ? csv.substring(start) : csv.substring(start, idx);
            token.trim();
            if (!token.isEmpty()) {
                long value = token.toInt();
                if (value >= 0 && value <= 65535) {
                    out.push_back(static_cast<uint16_t>(value));
                }
            }
            if (idx < 0) {
                break;
            }
            start = idx + 1;
        }
    };

    String line;
    std::vector<String> columns;
    columns.reserve(4);
    bool headerSkipped = false;
    size_t loaded = 0;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("theme_box_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 3) {
            continue;
        }

        uint8_t id = 0;
        if (!parseThemeBoxId(columns[0], id)) {
            continue;
        }

        ThemeBox box;
        box.id = id;
        box.name = columns[1];
        parseEntries(columns[2], box.entries);
        if (box.entries.empty()) {
            continue;
        }
        box.valid = true;
        boxes_.push_back(std::move(box));
        ++loaded;
    }

    file.close();

    if (boxes_.empty()) {
        PF("[ThemeBoxManager] no valid theme boxes loaded from %s\n", path.c_str());
        return false;
    }

    activeThemeBoxId_ = boxes_.front().id;
    PF("[ThemeBoxManager] Loaded %u theme boxes\n", static_cast<unsigned>(loaded));
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
