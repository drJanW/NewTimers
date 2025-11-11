#pragma once

#include <Arduino.h>
#include <FS.h>

struct CalendarEntry {
	bool valid{false};
	uint16_t year{0};
	uint8_t month{0};
	uint8_t day{0};
	String ttsSentence;
	uint16_t ttsIntervalMinutes{0};
	String themeBoxId;
	String note;
};

struct CalendarThemeBox {
	bool valid{false};
	String id;
	String entries;
	String note;
};

struct CalendarSnapshot {
	bool valid{false};
	CalendarEntry day;
	CalendarThemeBox theme;
};

class CalendarManager {
public:
	bool begin(fs::FS& sd, const char* rootPath = "/");
	bool loadToday(uint16_t year, uint8_t month, uint8_t day);
	const CalendarSnapshot& snapshot() const;
	bool hasSnapshot() const;
	bool isReady() const;
	void clear();

private:
	bool loadCalendarRow(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out);
	bool loadThemeBox(const String& id, CalendarThemeBox& out);

		String pathFor(const char* file) const;
	static void trim(String& s);
	static size_t split(const String& line, String* columns, size_t maxColumns);

	fs::FS* fs_{nullptr};
	String root_{"/"};
	CalendarSnapshot snapshot_{};
	bool ready_{false};
	bool hasSnapshot_{false};
};

extern CalendarManager calendarManager;
