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
	String lightShowId;
	String colorRangeId;
	String note;
};

struct CalendarThemeBox {
	bool valid{false};
	String id;
	String entries;
	String note;
};

struct CalendarLightPattern {
	bool valid{false};
	String id;
	float colorCycleSec{0.0f};
	float brightCycleSec{0.0f};
	float fadeWidth{0.0f};
	uint8_t minBrightness{0};
	float gradientSpeed{0.0f};
	float centerX{0.0f};
	float centerY{0.0f};
	float radius{0.0f};
	int windowWidth{0};
	float radiusOsc{0.0f};
	float xAmp{0.0f};
	float yAmp{0.0f};
	float xCycleSec{0.0f};
	float yCycleSec{0.0f};
	String note;
};

struct CalendarLightPalette {
	bool valid{false};
	String id;
	uint32_t primary{0};
	uint32_t secondary{0};
	String note;
};

struct CalendarLightShow {
	bool valid{false};
	String id;
	String patternId;
	String paletteId;
	CalendarLightPattern pattern;
	CalendarLightPalette palette;
	String note;
};

struct CalendarColorRange {
	bool valid{false};
	String id;
	uint32_t startColor{0};
	uint32_t endColor{0};
	String note;
};

struct CalendarSnapshot {
	bool valid{false};
	CalendarEntry day;
	CalendarThemeBox theme;
	CalendarLightShow light;
	CalendarColorRange color;
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
	bool loadLightShow(const String& id, CalendarLightShow& out);
	bool loadLightPattern(const String& id, CalendarLightPattern& out);
	bool loadLightPalette(const String& id, CalendarLightPalette& out);
	bool loadColorRange(const String& id, CalendarColorRange& out);

		String pathFor(const char* file) const;
	static bool parseHexColor(const String& text, uint32_t& color);
	static void trim(String& s);
	static size_t split(const String& line, String* columns, size_t maxColumns);

	fs::FS* fs_{nullptr};
	String root_{"/"};
	CalendarSnapshot snapshot_{};
	bool ready_{false};
	bool hasSnapshot_{false};
};

extern CalendarManager calendarManager;
