#include "CalendarConduct.h"

#include "CalendarPolicy.h"
#include "Calendar.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "SDBusyGuard.h"
#include "SDManager.h"
#include "TodayContext.h"

namespace {

constexpr uint32_t kCalendarRefreshIntervalMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kCalendarRetryIntervalMs   = 60UL * 1000UL;
constexpr uint32_t kCalendarInitialDelayMs    = 5UL * 1000UL;
constexpr uint32_t kCalendarBusyRetryMs       = 5UL * 1000UL;

TimerManager& timers() {
  return TimerManager::instance();
}

bool clockReady() {
  return PRTClock::instance().hasValidDate();
}

struct CalendarConductLogFlags {
  bool planManagerNotReady = false;
  bool loadManagerNotReady = false;
  bool loadSdBusy = false;
};

CalendarConductLogFlags s_logFlags;
bool s_initialDelayPending = true;

TodayContext s_todayContext;
bool s_todayContextValid = false;

void resetLoadFailureFlags() {
  s_logFlags.loadManagerNotReady = false;
  s_logFlags.loadSdBusy = false;
}

void clearTodayContextSnapshot() {
  s_todayContext = TodayContext{};
  s_todayContextValid = false;
}

void refreshTodayContextSnapshot() {
  TodayContext ctx;
  if (LoadTodayContext(ctx) && ctx.valid) {
    s_todayContext = ctx;
    s_todayContextValid = true;
  } else {
    clearTodayContextSnapshot();
  }
}

String s_sentence;
uint32_t s_sentenceIntervalMs = 0;

void clearSentenceTimer() {
  timers().cancel(CalendarConduct::cb_calendarSentence);
  s_sentence = "";
  s_sentenceIntervalMs = 0;
}

bool scheduleLoad(uint32_t intervalMs, int32_t repeat) {
  if (!timers().restart(intervalMs, repeat, CalendarConduct::cb_loadCalendar)) {
    PF("[CalendarConduct] Failed to schedule calendar reload (%lu ms, repeat=%ld)\n",
       static_cast<unsigned long>(intervalMs),
       static_cast<long>(repeat));
    return false;
  }
  return true;
}

bool ensureDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  auto& clock = PRTClock::instance();
  const uint16_t rawYear = clock.getYear();
  if (rawYear == 0) {
    return false;
  }
  year = rawYear >= 2000 ? rawYear : static_cast<uint16_t>(2000 + rawYear);
  month = clock.getMonth();
  day = clock.getDay();
  if (month == 0 || day == 0) {
    return false;
  }
  return true;
}

} // namespace

CalendarConduct calendarConduct;

void CalendarConduct::plan() {
  timers().cancel(CalendarConduct::cb_loadCalendar);
  clearSentenceTimer();

  if (!calendarManager.isReady()) {
    if (!s_logFlags.planManagerNotReady) {
      PF("[CalendarConduct] Calendar manager not ready, scheduling retry\n");
      s_logFlags.planManagerNotReady = true;
    }
    scheduleLoad(kCalendarRetryIntervalMs, 1);
    return;
  }
  s_logFlags.planManagerNotReady = false;

  if (!clockReady()) {
    scheduleLoad(kCalendarRetryIntervalMs, 1);
    return;
  }

  PF("[CalendarConduct] Calendar scheduling enabled\n");

  if (s_initialDelayPending) {
    if (!scheduleLoad(kCalendarInitialDelayMs, 1)) {
      PF("[CalendarConduct] Failed to arm initial calendar delay\n");
    } else {
      s_initialDelayPending = false;
    }
    return;
  }

  CalendarConduct::cb_loadCalendar();
}

void CalendarConduct::cb_loadCalendar() {
  auto reschedule = [](uint32_t intervalMs, int32_t repeat) {
    scheduleLoad(intervalMs, repeat);
  };

  if (!calendarManager.isReady()) {
    if (!s_logFlags.loadManagerNotReady) {
      PF("[CalendarConduct] Calendar manager not ready\n");
      s_logFlags.loadManagerNotReady = true;
    }
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }
  s_logFlags.loadManagerNotReady = false;

  if (!clockReady()) {
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }

  if (SDManager::isBusy()) {
    if (!s_logFlags.loadSdBusy) {
      PF("[CalendarConduct] SD busy, postponing calendar load\n");
      s_logFlags.loadSdBusy = true;
    }
    reschedule(kCalendarBusyRetryMs, 1);
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!ensureDate(year, month, day)) {
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }

  bool calendarLoaded = false;
  {
    SDBusyGuard guard;
    if (!guard.acquired()) {
      if (!s_logFlags.loadSdBusy) {
        PF("[CalendarConduct] SD busy, postponing calendar load\n");
        s_logFlags.loadSdBusy = true;
      }
      reschedule(kCalendarBusyRetryMs, 1);
      return;
    }
    s_logFlags.loadSdBusy = false;
    calendarLoaded = calendarManager.loadToday(year, month, day);
  }

  if (!calendarLoaded) {
    clearSentenceTimer();
    CalendarPolicy::handleThemeBox(CalendarThemeBox{});
    clearTodayContextSnapshot();
    reschedule(kCalendarRefreshIntervalMs, 0);
    return;
  }

  const auto& snapshot = calendarManager.snapshot();
  CalendarPolicy::Decision decision;
  if (!CalendarPolicy::evaluate(snapshot, decision)) {
    clearSentenceTimer();
    CalendarPolicy::handleThemeBox(CalendarThemeBox{});
    clearTodayContextSnapshot();
    reschedule(kCalendarRefreshIntervalMs, 0);
    return;
  }

  if (decision.hasSentence) {
    s_sentence = snapshot.day.ttsSentence;
    s_sentenceIntervalMs = decision.sentenceIntervalMs;

    if (s_sentenceIntervalMs > 0) {
      if (!timers().restart(s_sentenceIntervalMs, 0, CalendarConduct::cb_calendarSentence)) {
        PF("[CalendarConduct] Failed to arm calendar sentence timer (%lu ms)\n",
           static_cast<unsigned long>(s_sentenceIntervalMs));
      } else {
        // timer armed successfully
      }
    } else {
      clearSentenceTimer();
    }

    CalendarPolicy::dispatchSentence(snapshot.day.ttsSentence);
  } else {
    clearSentenceTimer();
  }

  if (decision.hasThemeBox) {
    CalendarPolicy::handleThemeBox(snapshot.theme);
  } else {
    CalendarPolicy::handleThemeBox(CalendarThemeBox{});
  }

  refreshTodayContextSnapshot();
  reschedule(kCalendarRefreshIntervalMs, 0);
  resetLoadFailureFlags();
}

void CalendarConduct::cb_calendarSentence() {
  if (s_sentence.isEmpty()) {
    return;
  }
  CalendarPolicy::dispatchSentence(s_sentence);
}

bool CalendarConduct::contextReady() const {
  return s_todayContextValid && s_todayContext.valid;
}

bool CalendarConduct::contextSnapshot(TodayContext& out) const {
  if (!contextReady()) {
    return false;
  }
  out = s_todayContext;
  return true;
}
