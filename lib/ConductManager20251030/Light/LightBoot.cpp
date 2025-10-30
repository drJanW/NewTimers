#include "LightBoot.h"
#include "Globals.h"
#include "LightManager.h"

void LightBoot::plan() {
    bootLightManager();
    setBaseBrightness(100);
    setWebBrightness(1.0f);
    PL("[Conduct][Plan] Light manager initialized");
}
