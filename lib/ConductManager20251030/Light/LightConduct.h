#pragma once

#include <Arduino.h>

class LightConduct {
public:
    void plan();

    static void handleDistanceReading(float distanceMm);

    static void animationTick();
};
