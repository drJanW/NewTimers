#include "ConductBoot.h"

#include "Globals.h"
#include "ContextManager.h"

void ConductBoot::plan() {
    PL("[Conduct][Plan] Conduct boot starting (seeding RNG)");
    bootRandomSeed();
    ContextManager_boot();
    PL("[Conduct][Plan] Context manager started; legacy setup logic now delegated to module boot planners");
}
