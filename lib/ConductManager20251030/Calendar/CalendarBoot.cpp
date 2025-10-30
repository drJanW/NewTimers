#include "CalendarBoot.h"

#include "Calendar.h"
#include "CalendarPolicy.h"
#include "Globals.h"
#include "SDManager.h"

#include <SD.h>

CalendarBoot calendarBoot;

void CalendarBoot::plan() {
  if (!SDManager::isReady()) {
    PF("[CalendarBoot] SD not ready, skip calendar init\n");
    return;
  }

  if (!calendarManager.begin(SD, "/")) {
    PF("[CalendarBoot] Calendar init failed\n");
    return;
  }

  CalendarPolicy::configure();
  PF("[CalendarBoot] Calendar CSV access configured\n");
}
