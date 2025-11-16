#include "CalendarConduct.h"

#include "CalendarPolicy.h"
#include "Calendar.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"

namespace {

constexpr uint32_t kCalendarRefreshIntervalMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kCalendarRetryIntervalMs   = 60UL * 1000UL;

TimerManager& timers() {
  return TimerManager::instance();
}

bool clockReady() {
  return PRTClock::instance().hasValidDate();
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
    PF("[CalendarConduct] Calendar manager not ready, scheduling retry\n");
    scheduleLoad(kCalendarRetryIntervalMs, 1);
    return;
  }

  if (!clockReady()) {
    PF("[CalendarConduct] Waiting for valid clock before scheduling\n");
    scheduleLoad(kCalendarRetryIntervalMs, 1);
    return;
  }

  PF("[CalendarConduct] Calendar scheduling enabled\n");
  CalendarConduct::cb_loadCalendar();
}

void CalendarConduct::cb_loadCalendar() {
  auto reschedule = [](uint32_t intervalMs, int32_t repeat) {
    scheduleLoad(intervalMs, repeat);
  };

  if (!calendarManager.isReady()) {
    PF("[CalendarConduct] Calendar manager not ready\n");
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }

  if (!clockReady()) {
    PF("[CalendarConduct] Clock not initialised yet\n");
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!ensureDate(year, month, day)) {
    PF("[CalendarConduct] Failed to resolve clock date\n");
    reschedule(kCalendarRetryIntervalMs, 1);
    return;
  }

  if (!calendarManager.loadToday(year, month, day)) {
    clearSentenceTimer();
    CalendarPolicy::handleThemeBox(CalendarThemeBox{});
    reschedule(kCalendarRefreshIntervalMs, 0);
    return;
  }

  const auto& snapshot = calendarManager.snapshot();
  CalendarPolicy::Decision decision;
  if (!CalendarPolicy::evaluate(snapshot, decision)) {
    clearSentenceTimer();
    CalendarPolicy::handleThemeBox(CalendarThemeBox{});
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

  reschedule(kCalendarRefreshIntervalMs, 0);
}

void CalendarConduct::cb_calendarSentence() {
  if (s_sentence.isEmpty()) {
    return;
  }
  CalendarPolicy::dispatchSentence(s_sentence);
}
