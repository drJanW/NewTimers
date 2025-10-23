#include "Calendar.h"

bool CalendarManager::begin(fs::FS&, const char*) {
	return false;
}

void CalendarManager::setToday(uint16_t, uint8_t, uint8_t) {}

void CalendarManager::clearToday() {}
