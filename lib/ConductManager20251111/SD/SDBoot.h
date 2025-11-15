#pragma once

class SDBoot {
public:
    bool plan();
private:
    static void retryTimerHandler();
};
