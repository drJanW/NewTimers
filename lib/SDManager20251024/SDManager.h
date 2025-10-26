#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Globals.h"

// ==== project globals verwacht ====
#ifndef PIN_SD_CS
#define PIN_SD_CS 5
#endif
#ifndef SPI_HZ
#define SPI_HZ 8000000
#endif
#ifndef PF
#define PF(...) Serial.printf(__VA_ARGS__)
#endif
#ifndef SD_INDEX_VERSION
#define SD_INDEX_VERSION "SDINDEX v1"
#endif
#ifndef SD_VERSION_FILENAME
#define SD_VERSION_FILENAME "/.sd_version.txt"
#endif
#ifndef SD_MAX_DIRS
#define SD_MAX_DIRS 255
#endif
#ifndef SD_MAX_FILES_PER_SUBDIR
#define SD_MAX_FILES_PER_SUBDIR 255
#endif
#ifndef SDPATHLENGTH
#define SDPATHLENGTH 64
#endif
#ifndef ROOT_DIRS
#define ROOT_DIRS "/.root_dirs"
#endif
#ifndef FILES_DIR
#define FILES_DIR "/.files_dir"
#endif

void setSDReady(bool ready);

// ===== index structs =====
struct DirEntry {
    uint16_t file_count;
    uint16_t total_score;
};

struct FileEntry {
    uint16_t size_kb;
    uint8_t  score;     // 1..200, 0=leeg
    uint8_t  reserved;
};

class SDManager {
public:
    static SDManager& instance();

    SDManager(const SDManager&) = delete;
    SDManager& operator=(const SDManager&) = delete;

    bool begin(uint8_t csPin);
    bool begin(uint8_t csPin, SPIClass& spi, uint32_t hz);

    void rebuildIndex();
    void scanDirectory(uint8_t dir_num);

    void setHighestDirNum();
    uint8_t getHighestDirNum() const;

    bool readDirEntry (uint8_t dir_num, DirEntry* entry);
    bool writeDirEntry(uint8_t dir_num, const DirEntry* entry);
    bool readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry);
    bool writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry);

    bool   fileExists(const char* fullPath);
    bool   writeTextFile(const char* path, const char* text);
    String readTextFile(const char* path);
    bool   deleteFile(const char* path);
    bool   renameFile(const char* oldPath, const char* newPath);

private:
    SDManager() : highestDirNum(0) {}

private:
    uint8_t highestDirNum;
};

// init + version check + index validate
void bootSDManager();

// Pad generator: "/DDD/FFF.mp3"
const char* getMP3Path(uint8_t dirID, uint8_t fileID);
