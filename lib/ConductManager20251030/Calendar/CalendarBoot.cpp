#include "CalendarBoot.h"

#include "Calendar.h"
#include "CalendarPolicy.h"
#include "Globals.h"
#include "SDManager.h"

#include <SD.h>

CalendarBoot calendarBoot;

void CalendarBoot::plan() {
  PF("[CalendarBoot] Calendar subsystem temporarily disabled\n");
}
