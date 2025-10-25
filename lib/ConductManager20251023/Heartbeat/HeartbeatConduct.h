#pragma once

#include <Arduino.h>

class HeartbeatConduct {
public:
	void plan();
	void setRate(uint32_t intervalMs);
	uint32_t currentRate() const;

private:
	static void pulse();
};

extern HeartbeatConduct heartbeatConduct;
