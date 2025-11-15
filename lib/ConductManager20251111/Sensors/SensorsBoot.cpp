#include "SensorsBoot.h"
#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"
#include <Wire.h>

void SensorsBoot::plan() {
    Wire.begin(I2C_SDA, I2C_SCL);
    SensorManager::init(/*ivUpdateMs=*/100);
    PL("[Conduct][Plan] Sensor manager initialized");
    SensorsPolicy::configure();
}
