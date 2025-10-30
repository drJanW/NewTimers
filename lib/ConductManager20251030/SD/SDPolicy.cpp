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

void showStatus() {
    PF("[SDPolicy] SD ready=%d busy=%d\n",
       SDManager::isReady(),
       SDManager::isBusy());
    // Could add more diagnostics here (e.g. number of indexed files)
}

}
