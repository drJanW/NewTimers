#include "SDPolicy.h"
#include "SDVoting.h"       // weighted voting/selection helpers
#include "Globals.h"        // for PF/PL macros if needed
#include "SDManager.h"
#include "AudioManager.h"
#include "Audio/AudioDirector.h"

namespace SDPolicy {

bool getRandomFragment(AudioFragment& outFrag) {
    // Delegate to director for weighted selection (will expand with context)
    return AudioDirector::selectRandomFragment(outFrag);
}

bool deleteFile(uint8_t dirIndex, uint8_t fileIndex) {
    // Policy: only delete if audio is idle
    auto& audio = AudioManager::instance();
    if (!audio.isBusy() && !audio.isSentencePlaying()) {
        SDVoting::deleteIndexedFile(dirIndex, fileIndex);
        return true;
    }
    // Otherwise reject (ConductManager can retry later)
    PF("[SDPolicy] Reject delete: audio busy\n");
    return false;
}

namespace {
bool s_stateInitialized = false;
bool s_lastReady = false;
bool s_lastBusy = false;
}

void showStatus(bool forceLog) {
    const bool ready = SDManager::isReady();
    const bool busy = SDManager::isBusy();

    if (!forceLog && s_stateInitialized && ready == s_lastReady && busy == s_lastBusy) {
        return;
    }

    s_stateInitialized = true;
    s_lastReady = ready;
    s_lastBusy = busy;

    PF("[SDPolicy] SD ready=%d busy=%d\n", ready, busy);
    // Could add more diagnostics here (e.g. number of indexed files)
}

}
