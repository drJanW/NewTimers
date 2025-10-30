#include "AudioDirector.h"

#include "Globals.h"
#include "SDManager.h"
#include "AudioPolicy.h"
#include <SD.h>

namespace {

using DirScore   = uint32_t;
using FileScore  = uint32_t;

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

bool pickDirectory(uint8_t& outDir, DirScore& totalScore, const uint8_t* allowList = nullptr, size_t allowCount = 0) {
    DirEntry entry{};
    uint8_t  dirCount = 0;
    uint8_t  validDirs[SD_MAX_DIRS];
    DirScore dirScores[SD_MAX_DIRS];
    uint8_t  fallbackDirs[SD_MAX_DIRS];
    uint8_t  fallbackCount = 0;
    totalScore = 0;

    File rootFile = SD.open(ROOT_DIRS, FILE_READ);
    if (!rootFile) {
        PF("[AudioDirector] Can't open %s\n", ROOT_DIRS);
        return false;
    }

    auto& sd = SDManager::instance();
    const uint8_t highestDir = sd.getHighestDirNum();
    for (uint16_t dirNum = 1; dirNum <= highestDir; ++dirNum) {
        const uint32_t offset = (uint32_t)(dirNum - 1U) * (uint32_t)sizeof(DirEntry);
        if (!rootFile.seek(offset)) continue;
        if (rootFile.read(reinterpret_cast<uint8_t*>(&entry), sizeof(DirEntry)) != sizeof(DirEntry)) continue;

        if (!isAllowed(static_cast<uint8_t>(dirNum), allowList, allowCount)) {
            continue;
        }

        if (entry.file_count > 0) {
            if (fallbackCount < SD_MAX_DIRS) {
                fallbackDirs[fallbackCount++] = static_cast<uint8_t>(dirNum);
            }
        }

        if (entry.total_score > 0 && entry.file_count > 0) {
            if (dirCount >= SD_MAX_DIRS) break;
            validDirs[dirCount] = static_cast<uint8_t>(dirNum);
            dirScores[dirCount] = static_cast<DirScore>(entry.total_score);
            totalScore += dirScores[dirCount];
            ++dirCount;
        }
    }
    rootFile.close();

    if (dirCount == 0 || totalScore == 0) {
        if (fallbackCount == 0) {
            if (allowList && allowCount > 0) {
                PF("[AudioDirector] No directories available for active theme filter\n");
            } else {
                PF("[AudioDirector] No directories available\n");
            }
            return false;
        }

        // Fallback: uniform random over directories that contain files
        const uint8_t idx = static_cast<uint8_t>(random((long)fallbackCount));
        outDir = fallbackDirs[idx];
        return true;
    }

    DirScore ticket = static_cast<DirScore>(random((long)totalScore)) + 1U;
    DirScore cumulative = 0;
    uint8_t chosen = validDirs[dirCount - 1U];
    for (uint8_t i = 0; i < dirCount; ++i) {
        cumulative += dirScores[i];
        if (ticket <= cumulative) {
            chosen = validDirs[i];
            break;
        }
    }

    outDir = chosen;
    return true;
}

bool pickFile(uint8_t dir, uint8_t& outFile) {
    auto& sd = SDManager::instance();

    FileEntry entry{};
    FileScore totalScore = 0;
    uint8_t   fileCount = 0;
    uint8_t   validFiles[SD_MAX_FILES_PER_SUBDIR];
    FileScore fileScores[SD_MAX_FILES_PER_SUBDIR];
    uint8_t   fallbackFiles[SD_MAX_FILES_PER_SUBDIR];
    uint8_t   fallbackCount = 0;

    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; ++fileNum) {
        if (!sd.readFileEntry(dir, static_cast<uint8_t>(fileNum), &entry)) continue;
        if (entry.size_kb > 0) {
            if (fallbackCount < SD_MAX_FILES_PER_SUBDIR) {
                fallbackFiles[fallbackCount++] = static_cast<uint8_t>(fileNum);
            }
        }

        if (entry.score > 0 && entry.size_kb > 0) {
            if (fileCount >= SD_MAX_FILES_PER_SUBDIR) break;
            validFiles[fileCount] = static_cast<uint8_t>(fileNum);
            fileScores[fileCount] = static_cast<FileScore>(entry.score);
            totalScore += fileScores[fileCount];
            ++fileCount;
        }
    }

    if (fileCount == 0 || totalScore == 0) {
        if (fallbackCount == 0) {
            PF("[AudioDirector] No playable files in dir %03u\n", dir);
            return false;
        }

        const uint8_t idx = static_cast<uint8_t>(random((long)fallbackCount));
        outFile = fallbackFiles[idx];
        return true;
    }

    FileScore ticket = static_cast<FileScore>(random((long)totalScore)) + 1U;
    FileScore cumulative = 0;
    uint8_t chosen = validFiles[fileCount - 1U];
    for (uint8_t i = 0; i < fileCount; ++i) {
        cumulative += fileScores[i];
        if (ticket <= cumulative) {
            chosen = validFiles[i];
            break;
        }
    }

    outFile = chosen;
    return true;
}

} // namespace

void AudioDirector::plan() {
    PL("[Conduct][Plan] TODO: move audio orchestration here");
}

bool AudioDirector::selectRandomFragment(AudioFragment& outFrag) {
    uint8_t dir = 0;
    DirScore totalDirScore = 0;

    size_t themeCount = 0;
    const uint8_t* themeDirs = AudioPolicy::themeBoxDirs(themeCount);
    if (themeDirs && themeCount > 0) {
        if (!pickDirectory(dir, totalDirScore, themeDirs, themeCount)) {
            PF("[AudioDirector] Theme box %s unavailable, falling back to full directory pool\n",
               AudioPolicy::themeBoxId().c_str());
            if (!pickDirectory(dir, totalDirScore)) {
                return false;
            }
        }
    } else {
        if (!pickDirectory(dir, totalDirScore)) {
            return false;
        }
    }

    uint8_t file = 0;
    if (!pickFile(dir, file)) {
        return false;
    }

    FileEntry fileEntry{};
    if (!SDManager::instance().readFileEntry(dir, file, &fileEntry)) {
        PF("[AudioDirector] Failed to read file entry %03u/%03u\n", dir, file);
        return false;
    }

    const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.size_kb) * 1024UL / BYTES_PER_MS;
    if (rawDuration <= HEADER_MS + 100U) {
        PF("[AudioDirector] Fragment candidate too short %03u/%03u\n", dir, file);
        return false;
    }

    outFrag.dirIndex   = dir;
    outFrag.fileIndex  = file;
    outFrag.startMs    = HEADER_MS;
    outFrag.durationMs = rawDuration - HEADER_MS;
    outFrag.fadeMs     = 5000U; // TODO: derive from context rules

    return true;
}
