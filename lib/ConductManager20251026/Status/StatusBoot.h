#pragma once

// Recurring timer callback for status/time display
void timeDisplayTick();

class StatusBoot {
public:
    void plan();
};

extern StatusBoot statusBoot;
