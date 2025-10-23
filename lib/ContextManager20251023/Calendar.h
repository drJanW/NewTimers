#pragma once
#include <Arduino.h>
#include <FS.h>

class CalendarManager {
public:
	bool begin(fs::FS& sd, const char* csvPath);
	void setToday(uint16_t year, uint8_t month, uint8_t day);
	void clearToday();
};
