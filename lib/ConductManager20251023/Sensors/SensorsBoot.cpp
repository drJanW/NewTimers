#include "SensorsBoot.h"
#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"
#include <Wire.h>

void SensorsBoot::plan() {
    Wire.begin(I2C_SDA, I2C_SCL);
    SensorManager::init(/*adcPin=*/33, /*ivUpdateMs=*/100, /*thrHigh=*/2000, /*thrLow=*/2000, /*adcPollMs=*/200);
    PL("[Conduct][Plan] Sensor manager initialized");
    SensorsPolicy::configure();
}
