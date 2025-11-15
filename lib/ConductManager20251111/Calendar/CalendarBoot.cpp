#include "CalendarBoot.h"

#include "TodayContext.h"
#include "Globals.h"
#include "SDManager.h"

#include <SD.h>

CalendarBoot calendarBoot;

void CalendarBoot::plan() {
  if (!SDManager::isReady()) {
    PF("[CalendarBoot] SD not ready, delaying calendar init\n");
    return;
  }

  if (!InitTodayContext(SD)) {
    PF("[CalendarBoot] Today context init failed\n");
    return;
  }

  PF("[CalendarBoot] Today context initialised\n");
}
