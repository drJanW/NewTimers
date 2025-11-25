#pragma once

#include <stdint.h>

// Time-of-day and context status flags (bit positions for uint64_t bitmask)
enum TimeStatus : uint8_t {
    STATUS_NIGHT = 0,
    STATUS_DAWN,
    STATUS_MORNING,
    STATUS_LIGHT,
    STATUS_DAY,
    STATUS_AFTERNOON,
    STATUS_DUSK,
    STATUS_EVENING,
    STATUS_DARK,
    STATUS_AM,
    STATUS_PM,
    // Future expansion: seasons, weather, etc.
    // STATUS_SPRING, STATUS_SUMMER, STATUS_AUTUMN, STATUS_WINTER,
    // STATUS_WEEKEND, STATUS_WEEKDAY,
    // STATUS_WARM, STATUS_COLD, STATUS_FREEZING,
    STATUS_COUNT
};

// Color parameters for shift system
enum ColorParam : uint8_t {
    COLOR_A_HUE = 0,
    COLOR_A_SAT,
    COLOR_A_BRIGHT,
    COLOR_A_VALUE,
    COLOR_B_HUE,
    COLOR_B_SAT,
    COLOR_B_BRIGHT,
    COLOR_B_VALUE,
    COLOR_PARAM_COUNT
};

// Pattern parameters for shift system
enum PatternParam : uint8_t {
    PAT_COLOR_CYCLE = 0,
    PAT_BRIGHT_CYCLE,
    PAT_FADE_WIDTH,
    PAT_MIN_BRIGHT,
    PAT_GRADIENT_SPEED,
    PAT_CENTER_X,
    PAT_CENTER_Y,
    PAT_RADIUS,
    PAT_WINDOW_WIDTH,
    PAT_RADIUS_OSC,
    PAT_X_AMP,
    PAT_Y_AMP,
    PAT_X_CYCLE,
    PAT_Y_CYCLE,
    PAT_PARAM_COUNT
};
