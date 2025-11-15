#pragma once

class CalendarConduct {
public:
  void plan();
  static void cb_loadCalendar();
  static void cb_calendarSentence();
};

extern CalendarConduct calendarConduct;
