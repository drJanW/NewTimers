#include "AudioBoot.h"

#include "Globals.h"
#include "AudioManager.h"
#include "SDManager.h"
#include "PlayPCM.h"
#include "AudioConduct.h"

void AudioBoot::plan() {
    if (!SDManager::isReady()) {
        PL("[Conduct][Plan] Audio boot deferred: SD not ready");
        return;
    }

    AudioManager::instance().begin();

    if (auto* clip = PlayPCM::loadFromSD("/ping.wav")) {
        setDistanceClipPointer(clip);
        AudioConduct::startDistanceResponse();
    } else {
        PL("[Conduct][Plan] Distance ping clip unavailable");
    }

    PL("[Conduct][Plan] Audio manager initialized");
}
