#include "Calendar.h"
#include "Globals.h"
#include "SDManager.h"
#include "SDBusyGuard.h"
#include "SdPathUtils.h"

#include <SD.h>
#include <vector>
#include <ctype.h>

#include "CsvUtils.h"
#include "CalendarCsv.h"

namespace {

constexpr const char* kCalendarFile       = "calendar.csv";
constexpr const char* kThemeBoxCsv        = "theme_boxes.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

bool parseUint8Strict(const String& value, uint8_t& out) {
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

CalendarManager calendarManager;

bool CalendarManager::begin(fs::FS& sd, const char* rootPath) {
	fs_ = &sd;

	const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
	const String sanitized = sanitizeSdPath(desiredRoot);
	if (sanitized.isEmpty()) {
		PF("[CalendarManager] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
		root_ = "/";
	} else {
		root_ = sanitized;
	}

	snapshot_ = CalendarSnapshot{};
	hasSnapshot_ = false;
	ready_ = true;
	return true;
}

bool CalendarManager::loadToday(uint16_t year, uint8_t month, uint8_t day) {
	if (!ready_ || !fs_ || !SDManager::isReady()) {
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

	if (entry.themeBoxId != 0) {
		CalendarThemeBox box;
		if (loadThemeBox(entry.themeBoxId, box)) {
			snapshot.theme = box;
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
	SDBusyGuard guard;
	if (!guard.acquired()) {
		return false;
	}
	const String csvPath = pathFor(kCalendarFile);
	File file = fs_->open(csvPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", csvPath.c_str());
		return false;
	}

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
		if (row.year != year || row.month != month || row.day != day) {
			continue;
		}

		out.valid = true;
		out.year = row.year;
		out.month = row.month;
		out.day = row.day;
		out.ttsSentence = row.sentence;
		out.ttsIntervalMinutes = row.intervalMinutes;
		out.themeBoxId = row.themeBoxId;
		out.patternId = row.patternId;
		out.colorId = row.colorId;
		out.note = String();
		file.close();
		return true;
	}

	file.close();
	return false;
}

bool CalendarManager::loadThemeBox(uint8_t id, CalendarThemeBox& out) {
	SDBusyGuard guard;
	if (!guard.acquired()) {
		return false;
	}
	const String csvPath = pathFor(kThemeBoxCsv);
	File file = fs_->open(csvPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", csvPath.c_str());
		return false;
	}

	String line;
	std::vector<String> columns;
	columns.reserve(4);
	bool headerSkipped = false;

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
		if (columns.empty()) {
			continue;
		}

		const String& rowIdStr = columns[0];
		uint8_t rowId = 0;
		if (!parseUint8Strict(rowIdStr, rowId)) {
			continue;
		}

		const String name = (columns.size() > 1) ? columns[1] : String();
		const String entries = (columns.size() > 2) ? columns[2] : String();

		if (rowId != id) {
			continue;
		}

		out.valid = true;
		out.id = rowId;
		out.entries = entries;
		out.note = name;
		file.close();
		return true;
	}

	file.close();
	PF("[CalendarManager] Theme box %u not found in %s\n", static_cast<unsigned>(id), csvPath.c_str());
	return false;
}

String CalendarManager::pathFor(const char* file) const {
	if (!file || !*file) {
		return String();
	}
	const String sanitizedFile = sanitizeSdFilename(String(file));
	if (sanitizedFile.isEmpty()) {
		return String();
	}
	String combined = buildUploadTarget(root_, sanitizedFile);
	if (!combined.isEmpty()) {
		return combined;
	}
	if (root_ == "/") {
		return String("/") + sanitizedFile;
	}
	return root_ + "/" + sanitizedFile;
}
