#pragma once
#include <Arduino.h>

// Stilestijl
enum class TimeStyle {
    FORMAL,
    NORMAL,
    INFORMAL
};

// Zegt de tijd (speelt af)
void SayTime(uint8_t hour, uint8_t minute, TimeStyle style = TimeStyle::NORMAL);

// APPEND-mode: voeg woorden toe aan bestaande buffer (speelt NIET af)
void SayTime(uint8_t hour, uint8_t minute, TimeStyle style, uint8_t* out, uint8_t& idx);

// Datum (speelt af)
void SayDate(uint8_t day, uint8_t month, uint8_t year = 0, TimeStyle style = TimeStyle::NORMAL);

// APPEND-mode datum (speelt NIET af)
void SayDate(uint8_t day, uint8_t month, uint8_t year, TimeStyle style, uint8_t* out, uint8_t& idx);

// “Het is <dag> <datum> <tijd>” (met timers; sentence‑prioriteit blijft gelden)
void SayNow(TimeStyle style = TimeStyle::NORMAL);
