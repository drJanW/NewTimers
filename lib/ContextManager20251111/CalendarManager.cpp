#include "CalendarManager.h"

#include "Globals.h"
#include "SDManager.h"
#include "PRTClock.h"

#include <ArduinoJson.h>
#include <ctype.h>

namespace {

constexpr const char* kCalendarFile = "calendar.json";
constexpr size_t kCalendarEntryDocCapacity = 4096;

enum class EntryReadResult {
    kEntry,
    kEnd,
    kError,
};

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

String variantToString(JsonVariantConst value) {
    if (value.isNull()) {
        return String();
    }

    if (value.is<const char*>() || value.is<String>()) {
        const char* raw = value.as<const char*>();
        if (raw != nullptr) {
            return String(raw);
        }
        String temp = value.as<String>();
        if (temp.length() > 0) {
            return String(temp.c_str());
        }
        return String();
    }

    if (value.is<int>() || value.is<long>() || value.is<unsigned>() || value.is<unsigned long>()) {
        return String(static_cast<long>(value.as<long>()));
    }
    if (value.is<float>() || value.is<double>()) {
        return String(value.as<double>(), 3);
    }
    if (value.is<bool>()) {
        return value.as<bool>() ? String("true") : String("false");
    }
    return String();
}

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

bool parseCalendarEntry(JsonObjectConst item, CalendarEntry& out) {
    JsonObjectConst date = item["date"].as<JsonObjectConst>();
    if (date.isNull()) {
        PF("[CalendarManager] calendar entry missing date object\n");
        return false;
    }

    const uint16_t year = static_cast<uint16_t>(date["year"].as<int>());
    const uint8_t month = static_cast<uint8_t>(date["month"].as<int>());
    const uint8_t day = static_cast<uint8_t>(date["day"].as<int>());
    if (year == 0 || month == 0 || day == 0) {
        PF("[CalendarManager] invalid date in entry (y=%u m=%u d=%u)\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        return false;
    }

    JsonObjectConst tts = item["tts"].as<JsonObjectConst>();
    JsonObjectConst audio = item["audio"].as<JsonObjectConst>();
    JsonObjectConst lights = item["lights"].as<JsonObjectConst>();

    out.valid = true;
    out.year = year;
    out.month = month;
    out.day = day;
    out.iso = variantToString(date["iso"]);
    if (out.iso.isEmpty()) {
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u",
                 static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        out.iso = buffer;
    }

    if (!tts.isNull()) {
        out.ttsSentence = variantToString(tts["sentence"]);
        out.ttsIntervalMinutes = static_cast<uint16_t>(tts["interval_min"].as<int>());
    }

    String themeBoxId;
    if (!audio.isNull() && audio.containsKey("theme_box_id")) {
        themeBoxId = variantToString(audio["theme_box_id"]);
    }
    if (themeBoxId.isEmpty()) {
        PF("[CalendarManager] entry %u-%u-%u missing theme_box_id, will use defaults\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
    }
    out.themeBoxId = themeBoxId;

    String patternId;
    String colorId;
    if (!lights.isNull()) {
        if (lights.containsKey("pattern_id")) {
            patternId = variantToString(lights["pattern_id"]);
        }
        if (lights.containsKey("color_id")) {
            colorId = variantToString(lights["color_id"]);
        }
    }
    if (patternId.isEmpty() || colorId.isEmpty()) {
        PF("[CalendarManager] entry %u-%u-%u missing pattern/color ids, will use defaults\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
    }
    out.patternId = patternId;
    out.colorId = colorId;
    return true;
}

bool skipToEntriesArray(File& file) {
    if (!file.find("\"entries\"")) {
        return false;
    }

    while (file.available()) {
        int raw = file.read();
        if (raw < 0) {
            break;
        }
        const char c = static_cast<char>(raw);
        if (c == '[') {
            return true;
        }
        if (!isspace(static_cast<unsigned char>(c)) && c != ':') {
            break;
        }
    }
    return false;
}

bool readJsonObjectBody(File& file, String& out) {
    int depth = 1;
    bool inString = false;
    bool escape = false;

    while (file.available()) {
        int raw = file.read();
        if (raw < 0) {
            break;
        }
        const char c = static_cast<char>(raw);
        out += c;

        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }

            if (c == '\\') {
                escape = true;
                continue;
            }

            if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
            continue;
        }

        if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                return true;
            }
        }
    }
    return false;
}

EntryReadResult readNextEntry(File& file, String& out) {
    while (file.available()) {
        int peeked = file.peek();
        if (peeked < 0) {
            return EntryReadResult::kEnd;
        }

        const char c = static_cast<char>(peeked);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',') {
            file.read();
            continue;
        }

        if (c == ']') {
            file.read();
            return EntryReadResult::kEnd;
        }

        if (c == '{') {
            file.read();
            out = "{";
            if (!readJsonObjectBody(file, out)) {
                return EntryReadResult::kError;
            }
            return EntryReadResult::kEntry;
        }

        // Unexpected content inside entries array; skip it.
        file.read();
    }
    return EntryReadResult::kEnd;
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

    StaticJsonDocument<64> metaFilter;
    metaFilter["schema"] = true;

    DynamicJsonDocument metaDoc(128);
    DeserializationError metaErr = deserializeJson(metaDoc, file, DeserializationOption::Filter(metaFilter));
    if (metaErr) {
        PF("[CalendarManager] Failed to read calendar metadata: %s\n", metaErr.c_str());
        file.close();
        return false;
    }

    const int schemaVersion = metaDoc["schema"].as<int>();
    if (schemaVersion != 1) {
        PF("[CalendarManager] Unsupported calendar schema=%d\n", schemaVersion);
        file.close();
        return false;
    }

    if (!file.seek(0)) {
        PF("[CalendarManager] failed to rewind calendar file\n");
        file.close();
        return false;
    }

    if (!skipToEntriesArray(file)) {
        PF("[CalendarManager] entries array not found in calendar file\n");
        file.close();
        return false;
    }

    uint16_t todayYear = 0;
    uint8_t todayMonth = 0;
    uint8_t todayDay = 0;
    const bool filterByToday = resolveToday(todayYear, todayMonth, todayDay);

    if (!filterByToday) {
        PF("[CalendarManager] Clock not ready, loading entire calendar for now\n");
    }

    DynamicJsonDocument entryDoc(kCalendarEntryDocCapacity);
    while (true) {
        String entryJson;
        const EntryReadResult result = readNextEntry(file, entryJson);
        if (result == EntryReadResult::kEnd) {
            break;
        }
        if (result == EntryReadResult::kError) {
            PF("[CalendarManager] failed to read calendar entry from stream\n");
            file.close();
            entries_.clear();
            return false;
        }

    entryDoc.clear();
    const char* entryData = entryJson.c_str();
    DeserializationError entryErr = deserializeJson(entryDoc, entryData, entryJson.length());
        if (entryErr) {
            PF("[CalendarManager] JSON parse error in calendar entry: %s\n", entryErr.c_str());
            continue;
        }

        CalendarEntry entry{};
        if (!parseCalendarEntry(entryDoc.as<JsonObjectConst>(), entry)) {
            continue;
        }

        if (filterByToday) {
            const int cmp = compareDate(entry.year, entry.month, entry.day,
                                        todayYear, todayMonth, todayDay);
            if (cmp < 0) {
                continue;
            }
            if (cmp > 0) {
                break;
            }
        }

        entries_.push_back(entry);
    }

    file.close();

    if (entries_.empty()) {
        PF("[CalendarManager] no valid calendar entries loaded\n");
        return false;
    }

    if (filterByToday) {
        PF("[CalendarManager] Loaded %u calendar entries for %04u-%02u-%02u\n",
           static_cast<unsigned>(entries_.size()), static_cast<unsigned>(todayYear),
           static_cast<unsigned>(todayMonth), static_cast<unsigned>(todayDay));
    } else {
        PF("[CalendarManager] Loaded %u calendar entries\n", static_cast<unsigned>(entries_.size()));
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
