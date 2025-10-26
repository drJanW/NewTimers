#include "SDBoot.h"
#include "Globals.h"
#include "SDManager.h"
#include "SDPolicy.h"

void SDBoot::plan() {
    PL("[Conduct][Plan] SD boot starting");
    bootSDManager();
    SDPolicy::showStatus();
}
