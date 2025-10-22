#include "SensorsBoot.h"
#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"

void SensorsBoot::plan() {
    SensorManager::init(/*adcPin=*/33, /*ivUpdateMs=*/100, /*thrHigh=*/2000, /*thrLow=*/2000, /*adcPollMs=*/200);
    PL("[Conduct][Plan] Sensor manager initialized");
    SensorsPolicy::configure();
}
