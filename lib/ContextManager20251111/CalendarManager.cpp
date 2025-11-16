#include "CalendarManager.h"

#include "Globals.h"
#include "SDManager.h"
#include "PRTClock.h"

#include <ctype.h>
#include <vector>

#include "CsvUtils.h"
#include "CalendarCsv.h"

namespace {

constexpr const char* kCalendarFile = "calendar.csv";

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

bool resolveToday(uint16_t& year, uint8_t& month, uint8_t& day) {
    auto& clock = PRTClock::instance();
    const uint16_t rawYear = clock.getYear();
    month = clock.getMonth();
    day = clock.getDay();
    if (rawYear == 0 || month == 0 || day == 0) {
        return false;
    }

    if (rawYear >= 1900) {
        year = rawYear;
    } else {
        year = static_cast<uint16_t>(2000 + rawYear);
    }
    return true;
}

int compareDate(uint16_t lhsYear, uint8_t lhsMonth, uint8_t lhsDay,
                uint16_t rhsYear, uint8_t rhsMonth, uint8_t rhsDay) {
    if (lhsYear != rhsYear) {
        return lhsYear < rhsYear ? -1 : 1;
    }
    if (lhsMonth != rhsMonth) {
        return lhsMonth < rhsMonth ? -1 : 1;
    }
    if (lhsDay != rhsDay) {
        return lhsDay < rhsDay ? -1 : 1;
    }
    return 0;
}

String makeIso(uint16_t year, uint8_t month, uint8_t day) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u",
             static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
    return String(buffer);
}

} // namespace

namespace Context {

bool CalendarManager::begin(fs::FS& sd, const char* rootPath) {
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

    entries_.clear();
    loaded_ = load();
    return loaded_;
}

bool CalendarManager::ready() const {
    return loaded_ && fs_ != nullptr;
}

bool CalendarManager::findEntry(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) const {
    if (!ready()) {
        return false;
    }

    for (const auto& entry : entries_) {
        if (entry.year == year && entry.month == month && entry.day == day) {
            out = entry;
            return true;
        }
    }
    return false;
}

void CalendarManager::clear() {
    entries_.clear();
    loaded_ = false;
}

bool CalendarManager::load() {
    if (!fs_) {
        return false;
    }

    ScopedSDBusy guard;
    const String path = pathFor(kCalendarFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[CalendarManager] failed to open %s\n", path.c_str());
        return false;
    }

    uint16_t todayYear = 0;
    uint8_t todayMonth = 0;
    uint8_t todayDay = 0;
    if (!resolveToday(todayYear, todayMonth, todayDay)) {
        PF("[CalendarManager] Clock not ready, aborting calendar load\n");
        file.close();
        return false;
    }

    entries_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(10);
    bool headerSkipped = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("year"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        CalendarCsvRow row;
        if (!ParseCalendarCsvRow(columns, row)) {
            continue;
        }

        CalendarEntry entry{};
        entry.valid = true;
        entry.year = row.year;
        entry.month = row.month;
        entry.day = row.day;
        entry.iso = makeIso(row.year, row.month, row.day);
        entry.ttsSentence = row.sentence;
        entry.ttsIntervalMinutes = row.intervalMinutes;
        entry.themeBoxId = row.themeBoxId;
        entry.patternId = row.patternId;
        entry.colorId = row.colorId;

        if (entry.themeBoxId == 0) {
            PF("[CalendarManager] entry %u-%u-%u missing theme_box_id, will use defaults\n",
               static_cast<unsigned>(entry.year), static_cast<unsigned>(entry.month), static_cast<unsigned>(entry.day));
        }
        if (entry.patternId == 0 || entry.colorId == 0) {
            PF("[CalendarManager] entry %u-%u-%u missing pattern/color ids, will use defaults\n",
               static_cast<unsigned>(entry.year), static_cast<unsigned>(entry.month), static_cast<unsigned>(entry.day));
        }

        const int cmp = compareDate(entry.year, entry.month, entry.day,
                                    todayYear, todayMonth, todayDay);
        if (cmp < 0) {
            continue;
        }
        if (cmp > 0) {
            break;
        }

        entries_.push_back(entry);
    }

    file.close();

    if (entries_.empty()) {
        PF("[CalendarManager] No special entries for %04u-%02u-%02u\n",
           static_cast<unsigned>(todayYear), static_cast<unsigned>(todayMonth),
           static_cast<unsigned>(todayDay));
    } else {
        PF("[CalendarManager] Loaded %u calendar entries for %04u-%02u-%02u\n",
           static_cast<unsigned>(entries_.size()), static_cast<unsigned>(todayYear),
           static_cast<unsigned>(todayMonth), static_cast<unsigned>(todayDay));
    }
    return true;
}

String CalendarManager::pathFor(const char* file) const {
    if (!file || !*file) {
        return String();
    }
    if (root_.length() <= 1) {
        return String("/") + file;
    }
    return root_ + "/" + file;
}

} // namespace Context
