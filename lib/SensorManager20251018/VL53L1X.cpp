#include "VL53L1X.h"

static SFEVL53L1X l1x;
static bool l1x_inited = false;
static uint16_t l1x_budget = 50;

bool VL53L1X_begin(uint8_t address, TwoWire &bus, uint16_t timingBudgetMs, bool longMode)
{
    if (!l1x_inited)
    {
        if (!l1x.begin(bus))
            return false;
        l1x.setI2CAddress(address);
        l1x_inited = true;
    }
    l1x_budget = timingBudgetMs ? timingBudgetMs : 50;
    if (longMode)
        l1x.setDistanceModeLong();
    else
        l1x.setDistanceModeShort();
    l1x.setTimingBudgetInMs(l1x_budget);
    l1x.startRanging();
    return true;
}

float readI2CSensor()
{
    if (!l1x_inited)
    {
        if (!VL53L1X_begin(VL53L1X_I2C_ADDR, Wire, 50, false))
            return NAN;
    }
    if (!l1x.checkForDataReady())
        return NAN;
    uint16_t mm = l1x.getDistance();
    l1x.clearInterrupt();
    return static_cast<float>(mm);
}
