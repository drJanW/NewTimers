#include "Calendar.h"
#include "Globals.h"
#include "SDManager.h"

#include <SD.h>
#include <ArduinoJson.h>

namespace {

constexpr const char* kCalendarFile       = "calendar.json";
constexpr const char* kThemeBoxJson       = "theme_boxes.json";
constexpr size_t   kCalendarJsonMinBytes  = 16384;
constexpr size_t   kCalendarJsonMaxBytes  = 196608;

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
	if (!ready_ || !fs_ || !SDManager::isReady()) {
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
	const String jsonPath = pathFor(kCalendarFile);
	File file = fs_->open(jsonPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", jsonPath.c_str());
		return false;
	}

	const size_t fileSize = file.size();
	// Scale the JSON buffer with the source file size so large calendars still parse on-device.
	size_t capacity = fileSize > 0 ? fileSize + (fileSize / 4) + 2048 : kCalendarJsonMinBytes;
	if (capacity < kCalendarJsonMinBytes) {
		capacity = kCalendarJsonMinBytes;
	}
	if (capacity > kCalendarJsonMaxBytes) {
		PF("[CalendarManager] calendar.json capacity clamped to %u bytes (file=%u)\n",
		   static_cast<unsigned>(kCalendarJsonMaxBytes),
		   static_cast<unsigned>(fileSize));
		capacity = kCalendarJsonMaxBytes;
	}
	file.seek(0);
	DynamicJsonDocument doc(capacity);
	DeserializationError err = deserializeJson(doc, file);
	file.close();
	if (err) {
		PF("[CalendarManager] JSON parse failed for %s: %s\n", jsonPath.c_str(), err.c_str());
		return false;
	}

	JsonArray entries = doc["entries"].as<JsonArray>();
	if (entries.isNull()) {
		PF("[CalendarManager] JSON entries array missing in %s\n", jsonPath.c_str());
		return false;
	}

	for (JsonObject item : entries) {
		JsonObject date = item["date"].as<JsonObject>();
		if (date.isNull()) {
			continue;
		}

		const uint16_t rowYear = static_cast<uint16_t>(date["year"] | 0);
		const uint8_t rowMonth = static_cast<uint8_t>(date["month"] | 0);
		const uint8_t rowDay = static_cast<uint8_t>(date["day"] | 0);
		if (rowYear != year || rowMonth != month || rowDay != day) {
			continue;
		}

		out.valid = true;
		out.year = rowYear;
		out.month = rowMonth;
		out.day = rowDay;

		JsonObject tts = item["tts"].as<JsonObject>();
		if (!tts.isNull()) {
			if (tts.containsKey("sentence")) {
				out.ttsSentence = tts["sentence"].as<String>();
			} else {
				out.ttsSentence = String();
			}
			if (tts.containsKey("interval_min")) {
				out.ttsIntervalMinutes = static_cast<uint16_t>(tts["interval_min"].as<int>());
			} else {
				out.ttsIntervalMinutes = static_cast<uint16_t>(tts["interval_minutes"] | 0);
			}
		} else {
			out.ttsSentence = String();
			out.ttsIntervalMinutes = 0;
		}

		JsonObject audio = item["audio"].as<JsonObject>();
		if (!audio.isNull()) {
			JsonVariant theme = audio["theme_box_id"];
			if (!theme.isNull()) {
				out.themeBoxId = theme.as<String>();
			}
		}
		if (out.themeBoxId.isEmpty()) {
			out.themeBoxId = item["theme_box_id"].as<String>();
		}

		JsonObject lights = item["lights"].as<JsonObject>();
		if (!lights.isNull()) {
			JsonVariant pattern = lights["pattern_id"];
			if (!pattern.isNull()) {
				out.patternId = pattern.as<String>();
			}
			JsonVariant color = lights["color_id"];
			if (!color.isNull()) {
				out.colorId = color.as<String>();
			}
		}
		if (out.patternId.isEmpty()) {
			out.patternId = item["pattern_id"].as<String>();
		}
		if (out.colorId.isEmpty()) {
			out.colorId = item["color_id"].as<String>();
		}

		JsonVariant note = item["note"];
		if (!note.isNull()) {
			out.note = note.as<String>();
		} else {
			out.note = String();
		}
		return true;
	}

	return false;
}

bool CalendarManager::loadThemeBox(const String& id, CalendarThemeBox& out) {
	ScopedSDBusy guard;
	const String jsonPath = pathFor(kThemeBoxJson);
	File file = fs_->open(jsonPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarManager] Failed to open %s\n", jsonPath.c_str());
		return false;
	}

	DynamicJsonDocument doc(8192);
	DeserializationError err = deserializeJson(doc, file);
	file.close();
	if (err) {
		PF("[CalendarManager] JSON parse failed for %s: %s\n", jsonPath.c_str(), err.c_str());
		return false;
	}

	JsonArray boxes = doc["theme_boxes"].as<JsonArray>();
	if (boxes.isNull()) {
		PF("[CalendarManager] JSON theme_boxes array missing in %s\n", jsonPath.c_str());
		return false;
	}

	for (JsonObject box : boxes) {
		const char* rawId = box["id"] | "";
		if (!rawId || !id.equalsIgnoreCase(rawId)) {
			continue;
		}

		JsonArray entries = box["entries"].as<JsonArray>();
		if (entries.isNull()) {
			continue;
		}

		String joined;
		bool first = true;
		for (JsonVariant v : entries) {
			int value = v.as<int>();
			if (value < 0 || value > 255) {
				continue;
			}
			if (!first) {
				joined += ',';
			}
			first = false;

			if (value < 10) {
				joined += "00";
			} else if (value < 100) {
				joined += '0';
			}
			joined += String(value);
		}

		out.valid = true;
		out.id = rawId;
		out.entries = joined;
		out.note = box["note"].as<String>();
		return true;
	}

	PF("[CalendarManager] Theme box %s not found in %s\n", id.c_str(), jsonPath.c_str());
	return false;
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
