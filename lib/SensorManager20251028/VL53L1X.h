#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L1X.h>

#include "Globals.h"

#ifndef IF_VL53L1X_PRESENT
#define IF_VL53L1X_PRESENT 0
#endif

#ifndef VL53L1X_I2C_ADDR
#define VL53L1X_I2C_ADDR 0x29
#endif

// Init met optionele parameters. Wire.begin() wordt elders gedaan.
bool VL53L1X_begin(uint8_t address = VL53L1X_I2C_ADDR,
                   TwoWire &bus = Wire,
                   uint16_t timingBudgetMs = 50,   // 20..100+
                   bool longMode = false);          // false=Short, true=Long

// SensorManager leest via deze functie; geeft NAN als geen nieuwe sample.
float readVL53L1X();
