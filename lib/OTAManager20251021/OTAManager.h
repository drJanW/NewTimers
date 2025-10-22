#pragma once
#include <Arduino.h>

// 0=normal, 1=pending, 2=ota
void otaArm(uint32_t window_s = 300);
bool otaConfirmAndReboot();     // returns false if expired
void otaBootDispatcher();       // call very early in setup()
