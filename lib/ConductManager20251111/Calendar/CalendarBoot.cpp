#include "CalendarBoot.h"

#include "Calendar.h"
#include "Globals.h"
#include "SDManager.h"
#include "TimerManager.h"
#include "PRTClock.h"

#include <SD.h>

bool InitTodayContext(fs::FS& sd, const char* rootPath = "/");

CalendarBoot calendarBoot;

namespace {

constexpr uint32_t kCalendarBootRetryMs = 60UL * 1000UL;
bool s_clockWaitLogged = false;
bool s_sdWaitLogged = false;
bool s_calendarInitFailedLogged = false;
bool s_todayContextFailedLogged = false;

void scheduleRetry();
void cancelRetry();

void calendarBootRetryTick() {
  calendarBoot.plan();
}

void scheduleRetry() {
  auto &timers = TimerManager::instance();
  if (!timers.restart(kCalendarBootRetryMs, 1, calendarBootRetryTick)) {
    if (!timers.isActive(calendarBootRetryTick) &&
        !timers.create(kCalendarBootRetryMs, 1, calendarBootRetryTick)) {
      PF("[CalendarBoot] Failed to schedule retry timer\n");
      return;
    }
  }
  PF("[CalendarBoot] Retry scheduled in %lus\n", static_cast<unsigned long>(kCalendarBootRetryMs / 1000UL));
}

void cancelRetry() {
  TimerManager::instance().cancel(calendarBootRetryTick);
}

} // namespace

void CalendarBoot::plan() {
  if (!SDManager::isReady()) {
    if (!s_sdWaitLogged) {
      PF("[CalendarBoot] SD not ready, delaying calendar init\n");
      s_sdWaitLogged = true;
    }
    scheduleRetry();
    return;
  }
  s_sdWaitLogged = false;

  auto &clock = PRTClock::instance();
  if (!clock.hasValidDate()) {
    if (!s_clockWaitLogged) {
      PF("[CalendarBoot] Waiting for valid clock (Wi-Fi or DS3231) before calendar init\n");
      s_clockWaitLogged = true;
    }
    scheduleRetry();
    return;
  }
  s_clockWaitLogged = false;

  if (!calendarManager.isReady()) {
    if (!calendarManager.begin(SD)) {
      if (!s_calendarInitFailedLogged) {
        PF("[CalendarBoot] Calendar manager init failed\n");
        s_calendarInitFailedLogged = true;
      }
      return;
    }
    s_calendarInitFailedLogged = false;
    PF("[CalendarBoot] Calendar manager initialised\n");
  }

  if (!InitTodayContext(SD)) {
    if (!s_todayContextFailedLogged) {
      PF("[CalendarBoot] Today context init failed\n");
      s_todayContextFailedLogged = true;
    }
    scheduleRetry();
    return;
  }
  s_todayContextFailedLogged = false;

  PF("[CalendarBoot] Today context initialised\n");
  cancelRetry();
}
