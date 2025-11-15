#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

namespace Context {

class CalendarManager {
public:
	bool begin(fs::FS& sd, const char* rootPath = "/");
	bool ready() const;
	bool findEntry(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) const;
	void clear();

private:
	bool load();
	String pathFor(const char* file) const;

	fs::FS* fs_{nullptr};
	String root_{"/"};
	bool loaded_{false};
	std::vector<CalendarEntry> entries_;
};

} // namespace Context
