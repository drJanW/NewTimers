#include "SDBoot.h"
#include "Globals.h"
#include "SDManager.h"
#include "SDPolicy.h"
#include "TimerManager.h"
#include "ConductManager.h"

namespace {

constexpr uint8_t kMaxAttempts = 5;
constexpr uint32_t kRetryDelayMs = 2000;

SDBoot* s_instance = nullptr;
uint8_t s_attemptCount = 0;

TimerManager& timers() {
    return TimerManager::instance();
}

} // namespace

bool SDBoot::plan() {
    if (!s_instance) {
        s_instance = this;
    }

    if (isSDReady()) {
        s_attemptCount = 0;
        SDPolicy::showStatus();
        return true;
    }

    const uint8_t currentAttempt = static_cast<uint8_t>(s_attemptCount + 1);

    if (s_attemptCount == 0) {
        PL("[Conduct][Plan] SD boot starting");
    } else {
        PF("[Conduct][Plan] SD boot retry %u/%u\n",
           static_cast<unsigned>(currentAttempt),
           static_cast<unsigned>(kMaxAttempts));
    }

    bootSDManager();

    if (isSDReady()) {
        s_attemptCount = 0;
        SDPolicy::showStatus();
        return true;
    }

    if (currentAttempt >= kMaxAttempts) {
        PF("[Conduct][Plan] SD boot failed after %u attempts\n",
           static_cast<unsigned>(kMaxAttempts));
        s_attemptCount = 0;
        SDPolicy::showStatus();
        return true;
    }

    s_attemptCount = currentAttempt;

    if (!timers().restart(kRetryDelayMs, 1, SDBoot::retryTimerHandler)) {
        PF("[Conduct][Plan] Failed to schedule SD retry timer\n");
        SDPolicy::showStatus();
        return true;
    }

    return false;
}

void SDBoot::retryTimerHandler() {
    if (!s_instance) {
        return;
    }

    if (s_instance->plan()) {
        ConductManager::resumeAfterSDBoot();
    }
}
