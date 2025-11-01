#include "AudioDirector.h"

#include "Globals.h"
#include "SDManager.h"
#include "AudioPolicy.h"
#include <SD.h>

namespace {

using DirScore   = uint32_t;
using FileScore  = uint32_t;

struct DirPick {
    uint8_t  id{};
    DirEntry entry{};
};

bool isAllowed(uint8_t dir, const uint8_t* allowList, size_t allowCount) {
    if (!allowList || allowCount == 0) {
        return true;
    }
    for (size_t i = 0; i < allowCount; ++i) {
        if (allowList[i] == dir) {
            return true;
        }
    }
    return false;
}

bool pickDirectory(DirPick& outDir, const uint8_t* allowList = nullptr, size_t allowCount = 0) {
    DirPick scored[SD_MAX_DIRS];
    uint8_t scoredCount = 0;
    DirScore totalScore = 0;

    auto recordEntry = [&](uint8_t dirNum, const DirEntry& entry) {
        if (entry.file_count == 0 || entry.total_score == 0) {
            return;
        }
        if (scoredCount < SD_MAX_DIRS) {
            scored[scoredCount].id = dirNum;
            scored[scoredCount].entry = entry;
            totalScore += static_cast<DirScore>(entry.total_score);
            ++scoredCount;
        }
    };

    auto& sd = SDManager::instance();

    if (allowList && allowCount > 0) {
        for (size_t idx = 0; idx < allowCount; ++idx) {
            const uint8_t dirNum = allowList[idx];
            DirEntry entry{};
            if (!sd.readDirEntry(dirNum, &entry)) {
                continue;
            }
            recordEntry(dirNum, entry);
        }
    } else {
        File rootFile = SD.open(ROOT_DIRS, FILE_READ);
        if (!rootFile) {
            PF("[AudioDirector] Can't open %s\n", ROOT_DIRS);
            return false;
        }

        const uint8_t highestDir = sd.getHighestDirNum();
        for (uint16_t dirNum = 1; dirNum <= highestDir; ++dirNum) {
            if (!isAllowed(static_cast<uint8_t>(dirNum), allowList, allowCount)) {
                continue;
            }

            const uint32_t offset = (uint32_t)(dirNum - 1U) * (uint32_t)sizeof(DirEntry);
            if (!rootFile.seek(offset)) {
                continue;
            }

            DirEntry entry{};
            if (rootFile.read(reinterpret_cast<uint8_t*>(&entry), sizeof(DirEntry)) != sizeof(DirEntry)) {
                continue;
            }
            recordEntry(static_cast<uint8_t>(dirNum), entry);
        }
        rootFile.close();
    }

    if (totalScore == 0 || scoredCount == 0) {
        if (allowList && allowCount > 0) {
            PF("[AudioDirector] No weighted directories for active theme filter\n");
        } else {
            PF("[AudioDirector] No weighted directories available\n");
        }
        return false;
    }

    DirScore ticket = static_cast<DirScore>(random((long)totalScore)) + 1U;
    DirScore cumulative = 0;
    for (uint8_t i = 0; i < scoredCount; ++i) {
        cumulative += static_cast<DirScore>(scored[i].entry.total_score);
        if (ticket <= cumulative) {
            outDir = scored[i];
            return true;
        }
    }

    // Should not reach here, but guard anyway.
    outDir = scored[scoredCount - 1];
    return true;
}

bool pickFile(const DirPick& dirPick, uint8_t& outFile) {
    char filesDirPath[SDPATHLENGTH];
    snprintf(filesDirPath, sizeof(filesDirPath), "/%03u%s", dirPick.id, FILES_DIR);

    File filesIndex = SD.open(filesDirPath, FILE_READ);
    if (!filesIndex) {
        PF("[AudioDirector] Can't open %s\n", filesDirPath);
        return false;
    }

    FileScore totalScore = 0;
    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; ++fileNum) {
        FileEntry entry{};
        if (filesIndex.read(reinterpret_cast<uint8_t*>(&entry), sizeof(FileEntry)) != sizeof(FileEntry)) {
            break;
        }
        if (entry.size_kb == 0 || entry.score == 0) {
            continue;
        }
        totalScore += static_cast<FileScore>(entry.score);
    }

    if (totalScore == 0) {
        filesIndex.close();
        PF("[AudioDirector] No weighted files in dir %03u\n", dirPick.id);
        return false;
    }

    const FileScore target = static_cast<FileScore>(random((long)totalScore)) + 1U;
    filesIndex.seek(0);
    FileScore cumulative = 0;
    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; ++fileNum) {
        FileEntry entry{};
        if (filesIndex.read(reinterpret_cast<uint8_t*>(&entry), sizeof(FileEntry)) != sizeof(FileEntry)) {
            break;
        }
        if (entry.size_kb == 0 || entry.score == 0) {
            continue;
        }
        cumulative += static_cast<FileScore>(entry.score);
        if (target <= cumulative) {
            filesIndex.close();
            outFile = static_cast<uint8_t>(fileNum);
            return true;
        }
    }

    filesIndex.close();
    PF("[AudioDirector] Weighted walk failed in dir %03u\n", dirPick.id);
    return false;
}

} // namespace

void AudioDirector::plan() {
    PL("[Conduct][Plan] TODO: move audio orchestration here");
}

bool AudioDirector::selectRandomFragment(AudioFragment& outFrag) {
    DirPick dirPick{};

    size_t themeCount = 0;
    const uint8_t* themeDirs = AudioPolicy::themeBoxDirs(themeCount);
    if (themeDirs && themeCount > 0) {
        if (!pickDirectory(dirPick, themeDirs, themeCount)) {
            PF("[AudioDirector] Theme box %s unavailable, falling back to full directory pool\n",
               AudioPolicy::themeBoxId().c_str());
            if (!pickDirectory(dirPick)) {
                return false;
            }
        }
    } else {
        if (!pickDirectory(dirPick)) {
            return false;
        }
    }

    uint8_t file = 0;
    if (!pickFile(dirPick, file)) {
        return false;
    }

    FileEntry fileEntry{};
    if (!SDManager::instance().readFileEntry(dirPick.id, file, &fileEntry)) {
        PF("[AudioDirector] Failed to read file entry %03u/%03u\n", dirPick.id, file);
        return false;
    }

    const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.size_kb) * 1024UL / BYTES_PER_MS;
    if (rawDuration <= HEADER_MS + 100U) {
    PF("[AudioDirector] Fragment candidate too short %03u/%03u\n", dirPick.id, file);
        return false;
    }

    outFrag.dirIndex   = dirPick.id;
    outFrag.fileIndex  = file;
    outFrag.startMs    = HEADER_MS;
    outFrag.durationMs = rawDuration - HEADER_MS;
    outFrag.fadeMs     = 5000U; // TODO: derive from context rules

    return true;
}
