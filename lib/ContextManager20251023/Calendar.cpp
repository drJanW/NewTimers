#include "Calendar.h"
#include "Globals.h"

#include <SD.h>
#include <stdlib.h>

namespace {

constexpr const char* kCalendarFile     = "calendar.csv";
constexpr const char* kThemeBoxFile     = "theme_boxes.csv";
constexpr const char* kLightShowFile    = "light_shows.csv";
constexpr const char* kColorRangeFile   = "color_ranges.csv";

constexpr size_t kCalendarColumns       = 9;
constexpr size_t kThemeBoxColumns       = 3;
constexpr size_t kLightShowColumns      = 18;
constexpr size_t kColorRangeColumns     = 4;

class ScopedSDBusy {
public:
	ScopedSDBusy() : owns_(!isSDBusy()) {
		if (owns_) {
			setSDBusy(true);
		}
	}

	~ScopedSDBusy() {
		if (owns_) {
			setSDBusy(false);
		}
	}

	ScopedSDBusy(const ScopedSDBusy&) = delete;
	ScopedSDBusy& operator=(const ScopedSDBusy&) = delete;

private:
	bool owns_{false};
};

inline bool isLineSkippable(const String& line) {
	if (line.isEmpty()) return true;
	char c = line[0];
	return c == '#' || c == '\r' || c == '\n';
}

inline int compareDate(uint16_t lhsYear, uint8_t lhsMonth, uint8_t lhsDay,
											 uint16_t rhsYear, uint8_t rhsMonth, uint8_t rhsDay) {
	if (lhsYear != rhsYear) return lhsYear < rhsYear ? -1 : 1;
	if (lhsMonth != rhsMonth) return lhsMonth < rhsMonth ? -1 : 1;
	if (lhsDay != rhsDay) return lhsDay < rhsDay ? -1 : 1;
	return 0;
}

} // namespace

CalendarManager calendarManager;

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

	snapshot_ = CalendarSnapshot{};
	hasSnapshot_ = false;
	ready_ = true;
	return true;
}

bool CalendarManager::loadToday(uint16_t year, uint8_t month, uint8_t day) {
	if (!ready_ || !fs_ || !isSDReady()) {
		PF("[CalendarManager] Cannot load calendar: SD not ready\n");
		return false;
	}

	CalendarSnapshot snapshot{};
	CalendarEntry entry{};
		if (!loadCalendarRow(year, month, day, entry)) {
		hasSnapshot_ = false;
		snapshot_ = CalendarSnapshot{};
		return false;
	}

	snapshot.valid = true;
	snapshot.day = entry;

	if (!entry.themeBoxId.isEmpty()) {
		CalendarThemeBox box;
		if (loadThemeBox(entry.themeBoxId, box)) {
			snapshot.theme = box;
		}
	}

	if (!entry.lightShowId.isEmpty()) {
		CalendarLightShow show;
		if (loadLightShow(entry.lightShowId, show)) {
			snapshot.light = show;
		}
	}

	if (!entry.colorRangeId.isEmpty()) {
		CalendarColorRange colors;
		if (loadColorRange(entry.colorRangeId, colors)) {
			snapshot.color = colors;
		}
	}

	snapshot_ = snapshot;
	hasSnapshot_ = true;
	return true;
}

const CalendarSnapshot& CalendarManager::snapshot() const {
	return snapshot_;
}

bool CalendarManager::hasSnapshot() const {
	return hasSnapshot_ && snapshot_.valid;
}

bool CalendarManager::isReady() const {
	return ready_ && fs_ != nullptr;
}

void CalendarManager::clear() {
	snapshot_ = CalendarSnapshot{};
	hasSnapshot_ = false;
}

bool CalendarManager::loadCalendarRow(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) {
	ScopedSDBusy guard;
	File file = fs_->open(pathFor(kCalendarFile).c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", pathFor(kCalendarFile).c_str());
		return false;
	}

	bool matched = false;
	while (file.available()) {
		String raw = file.readStringUntil('\n');
		trim(raw);
		if (isLineSkippable(raw)) {
			continue;
		}

		String columns[kCalendarColumns];
		const size_t count = split(raw, columns, kCalendarColumns);
		if (count < 7) {
			continue;
		}
		if (columns[0].equalsIgnoreCase("year")) {
			continue; // header row
		}

		const uint16_t rowYear = static_cast<uint16_t>(columns[0].toInt());
		const uint8_t  rowMonth = static_cast<uint8_t>(columns[1].toInt());
		const uint8_t  rowDay = static_cast<uint8_t>(columns[2].toInt());

		const int cmp = compareDate(rowYear, rowMonth, rowDay, year, month, day);
		if (cmp > 0) {
			break; // rows are ordered, so no match beyond this point
		}
		if (cmp < 0) {
			continue;
		}

		out.valid = true;
		out.year = rowYear;
		out.month = rowMonth;
		out.day = rowDay;
		out.ttsSentence = columns[3];
		out.ttsIntervalMinutes = static_cast<uint16_t>(columns[4].toInt());
		out.themeBoxId = columns[5];
		out.lightShowId = columns[6];
		out.colorRangeId = columns[7];
		if (count >= 9) {
			out.note = columns[8];
		} else {
			out.note = "";
		}

		matched = true;
		break;
	}

	file.close();
	return matched;
}

bool CalendarManager::loadThemeBox(const String& id, CalendarThemeBox& out) {
	ScopedSDBusy guard;
	File file = fs_->open(pathFor(kThemeBoxFile).c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", pathFor(kThemeBoxFile).c_str());
		return false;
	}

	bool matched = false;
	while (file.available()) {
		String raw = file.readStringUntil('\n');
		trim(raw);
		if (isLineSkippable(raw)) {
			continue;
		}

		String columns[kThemeBoxColumns];
		const size_t count = split(raw, columns, kThemeBoxColumns);
		if (count < 2) {
			continue;
		}
		if (columns[0].equalsIgnoreCase("theme_box_id")) {
			continue;
		}

		if (!columns[0].equalsIgnoreCase(id)) {
			continue;
		}

		out.valid = true;
		out.id = columns[0];
		out.entries = columns[1];
		out.note = count >= 3 ? columns[2] : "";
		matched = true;
		break;
	}

	file.close();
	return matched;
}

bool CalendarManager::loadLightShow(const String& id, CalendarLightShow& out) {
	ScopedSDBusy guard;
	File file = fs_->open(pathFor(kLightShowFile).c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", pathFor(kLightShowFile).c_str());
		return false;
	}

	bool matched = false;
	while (file.available()) {
		String raw = file.readStringUntil('\n');
		trim(raw);
		if (isLineSkippable(raw)) {
			continue;
		}

		String columns[kLightShowColumns];
		const size_t count = split(raw, columns, kLightShowColumns);
		if (count < 17) {
			continue;
		}
		if (columns[0].equalsIgnoreCase("light_show_id")) {
			continue;
		}
		if (!columns[0].equalsIgnoreCase(id)) {
			continue;
		}

		uint32_t rgb1 = 0;
		uint32_t rgb2 = 0;
		if (!parseHexColor(columns[1], rgb1) || !parseHexColor(columns[2], rgb2)) {
			PF("[CalendarManager] Light show %s has invalid color codes\n", id.c_str());
			break;
		}

		out.valid = true;
		out.id = columns[0];
		out.rgb1 = rgb1;
		out.rgb2 = rgb2;
		out.colorCycleSec  = static_cast<uint8_t>(columns[3].toInt());
		out.brightCycleSec = static_cast<uint8_t>(columns[4].toInt());
		out.fadeWidth      = columns[5].toFloat();
		out.minBrightness  = static_cast<uint8_t>(columns[6].toInt());
		out.gradientSpeed  = columns[7].toFloat();
		out.centerX        = columns[8].toFloat();
		out.centerY        = columns[9].toFloat();
		out.radius         = columns[10].toFloat();
		out.windowWidth    = columns[11].toInt();
		out.radiusOsc      = columns[12].toFloat();
		out.xAmp           = columns[13].toFloat();
		out.yAmp           = columns[14].toFloat();
		out.xCycleSec      = static_cast<uint8_t>(columns[15].toInt());
		out.yCycleSec      = static_cast<uint8_t>(columns[16].toInt());
		out.note           = count >= 18 ? columns[17] : "";
		matched = true;
		break;
	}

	file.close();
	return matched;
}

bool CalendarManager::loadColorRange(const String& id, CalendarColorRange& out) {
	ScopedSDBusy guard;
	File file = fs_->open(pathFor(kColorRangeFile).c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", pathFor(kColorRangeFile).c_str());
		return false;
	}

	bool matched = false;
	while (file.available()) {
		String raw = file.readStringUntil('\n');
		trim(raw);
		if (isLineSkippable(raw)) {
			continue;
		}

		String columns[kColorRangeColumns];
		const size_t count = split(raw, columns, kColorRangeColumns);
		if (count < 3) {
			continue;
		}
		if (columns[0].equalsIgnoreCase("color_range_id")) {
			continue;
		}
		if (!columns[0].equalsIgnoreCase(id)) {
			continue;
		}

		uint32_t startColor = 0;
		uint32_t endColor = 0;
		if (!parseHexColor(columns[1], startColor) || !parseHexColor(columns[2], endColor)) {
			PF("[CalendarManager] Color range %s has invalid hex\n", id.c_str());
			break;
		}

		out.valid = true;
		out.id = columns[0];
		out.startColor = startColor;
		out.endColor = endColor;
		out.note = count >= 4 ? columns[3] : "";
		matched = true;
		break;
	}

	file.close();
	return matched;
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

bool CalendarManager::parseHexColor(const String& text, uint32_t& color) {
	String value = text;
	trim(value);
	if (value.startsWith("#")) {
		value.remove(0, 1);
	}
	if (value.length() != 6) {
		return false;
	}

	char buffer[7] = {0};
	value.toUpperCase();
	value.toCharArray(buffer, sizeof(buffer));
	char* endPtr = nullptr;
	unsigned long parsed = strtoul(buffer, &endPtr, 16);
	if (!endPtr || *endPtr != '\0') {
		return false;
	}
	color = static_cast<uint32_t>(parsed & 0x00FFFFFFUL);
	return true;
}

void CalendarManager::trim(String& s) {
	s.trim();
}

size_t CalendarManager::split(const String& line, String* columns, size_t maxColumns) {
	if (!columns || maxColumns == 0) {
		return 0;
	}

	size_t count = 0;
	int start = 0;
	const int len = line.length();

	while (count + 1 < maxColumns) {
		int idx = line.indexOf(';', start);
		if (idx < 0) {
			break;
		}
		columns[count++] = line.substring(start, idx);
		start = idx + 1;
	}

	if (count < maxColumns) {
		columns[count++] = line.substring(start, len);
	}

	for (size_t i = 0; i < count; ++i) {
		trim(columns[i]);
	}

	return count;
}
