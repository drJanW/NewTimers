#pragma once

#include "SDManager.h"

class SDBusyGuard {
public:
    SDBusyGuard() : ownsBus_(!SDManager::isBusy()) {
        if (ownsBus_) {
            SDManager::setBusy(true);
        }
    }

    ~SDBusyGuard() {
        release();
    }

    bool acquired() const {
        return ownsBus_;
    }

    void release() {
        if (ownsBus_) {
            SDManager::setBusy(false);
            ownsBus_ = false;
        }
    }

private:
    bool ownsBus_ = false;
};
