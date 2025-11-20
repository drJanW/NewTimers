#pragma once

struct TodayContext;

class CalendarConduct {
public:
  void plan();
  static void cb_loadCalendar();
  static void cb_calendarSentence();

  bool contextReady() const;
  bool contextSnapshot(TodayContext& out) const;
};

extern CalendarConduct calendarConduct;
