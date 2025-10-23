#include "AudioBoot.h"

#include "Globals.h"
#include "AudioManager.h"
#include "SDManager.h"

void AudioBoot::plan() {
    if (!isSDReady()) {
        PL("[Conduct][Plan] Audio boot deferred: SD not ready");
        return;
    }

    AudioManager::instance().begin();
    PL("[Conduct][Plan] Audio manager initialized");
}
