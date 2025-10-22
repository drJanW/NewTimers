#include "SDManager.h"

// forward
static bool versionStringsEqual(const String& a, const char* b);

namespace {

// non-reentrant guard around long SD operations
class ScopedSDBusy {
public:
    ScopedSDBusy() : owns(!isSDBusy()) {
        if (owns)
            setSDBusy(true);
    }
    ~ScopedSDBusy() {
        if (owns)
            setSDBusy(false);
    }

private:
    bool owns;
};
} // namespace

bool SDManager::begin(uint8_t csPin) { return SD.begin(csPin); }
bool SDManager::begin(uint8_t csPin, SPIClass& spi, uint32_t hz) { return SD.begin(csPin, spi, hz); }

SDManager& SDManager::instance() {
    static SDManager inst;
    return inst;
}

void bootSDManager() {
    auto& sdManager = SDManager::instance();
    ScopedSDBusy busyGuard;
    if (!sdManager.begin(PIN_SD_CS, SPI, SPI_HZ)) {
        PF("[SDManager] SD init failed.\n");
        setSDReady(false); return;
    }
    if (SD.exists(SD_VERSION_FILENAME)) {
        File v = SD.open(SD_VERSION_FILENAME, FILE_READ);
        String sdver = v ? v.readString() : "";
        if (v) v.close();
        if (!versionStringsEqual(sdver, SD_INDEX_VERSION)) {
            PF("[SDManager][ERROR] SD version mismatch.\n");
            PF("  Card: %s\n  Need: %s\n", sdver.c_str(), SD_INDEX_VERSION);
            PF("HALT: Reindex required.\n");
            setSDReady(false);
            while (1) delay(1000);
        } else PF("[SDManager] SD version OK.\n");
    } else {
        PF("[SDManager] Version file missing. Rebuilding index...\n");
        sdManager.rebuildIndex();
    }

    if (!SD.exists(ROOT_DIRS)) {
        PF("[SDManager] No index. Rebuilding...\n");
        sdManager.rebuildIndex();
    } else {
        DirEntry dir; bool valid = false;
        for (uint8_t i=1;i<=SD_MAX_DIRS;i++){
            if (sdManager.readDirEntry(i,&dir) && dir.file_count>0){ valid=true; break; }
        }
        if (!valid){ PF("[SDManager] Empty index. Forced rebuild.\n"); sdManager.rebuildIndex(); }
        else PF("[SDManager] Using existing index.\n");
        sdManager.setHighestDirNum();
    }
    setSDReady(true);
}

void SDManager::rebuildIndex() {
    ScopedSDBusy guard;
    if (SD.exists(ROOT_DIRS)) SD.remove(ROOT_DIRS);
    File root = SD.open(ROOT_DIRS, FILE_WRITE);
    if (!root){ PF("[SDManager] Cannot create %s\n", ROOT_DIRS); return; }
    DirEntry empty={0,0};
    for (uint16_t i=0;i<SD_MAX_DIRS;i++) root.write((const uint8_t*)&empty,sizeof(DirEntry));
    root.close();

    for (uint8_t d=1; d<=SD_MAX_DIRS; d++) scanDirectory(d);

    File v = SD.open(SD_VERSION_FILENAME, FILE_WRITE);
    if (v){ v.print(SD_INDEX_VERSION); v.close(); PF("[SDManager] Wrote version %s\n", SD_INDEX_VERSION); }

    PF("[SDManager] Index rebuild complete.\n");
}

void SDManager::scanDirectory(uint8_t dir_num) {
    ScopedSDBusy guard;
    char dirPath[12]; snprintf(dirPath,sizeof(dirPath),"/%03u",dir_num);
    char filesDirPath[SDPATHLENGTH]; snprintf(filesDirPath,sizeof(filesDirPath),"%s%s",dirPath,FILES_DIR);

    File filesIndex = SD.open(filesDirPath, FILE_WRITE);
    if (!filesIndex){ PF("[SDManager] Open fail: %s\n", filesDirPath); return; }

    DirEntry dirEntry={0,0};

    for (uint8_t fnum=1; fnum<=SD_MAX_FILES_PER_SUBDIR; fnum++){
        FileEntry fe={0,0,0};
        char mp3path[SDPATHLENGTH]; snprintf(mp3path,sizeof(mp3path),"%s/%03u.mp3",dirPath,fnum);
        if (SD.exists(dirPath) && SD.exists(mp3path)) {
            File mp3 = SD.open(mp3path, FILE_READ);
            if (mp3){ fe.size_kb = mp3.size()/1024; mp3.close(); }
            fe.score = 100;
            dirEntry.file_count++;
            dirEntry.total_score += fe.score;
        }
        filesIndex.seek((fnum-1)*sizeof(FileEntry));
        filesIndex.write((const uint8_t*)&fe,sizeof(FileEntry));
    }
    filesIndex.close();

    if (SD.exists(dirPath)) writeDirEntry(dir_num,&dirEntry);
}

void SDManager::setHighestDirNum(){
    ScopedSDBusy guard;
    highestDirNum=0; DirEntry e;
    for (int16_t d=SD_MAX_DIRS; d>=1; --d){
        if (readDirEntry(d,&e) && e.file_count>0){ highestDirNum=d; return; }
    }
}
uint8_t SDManager::getHighestDirNum() const { return highestDirNum; }

bool SDManager::readDirEntry(uint8_t dir_num, DirEntry* entry){
    ScopedSDBusy guard;
    File f=SD.open(ROOT_DIRS, FILE_READ); if(!f) return false;
    f.seek((dir_num-1)*sizeof(DirEntry));
    bool ok = f.read((uint8_t*)entry,sizeof(DirEntry))==sizeof(DirEntry);
    f.close(); return ok;
}
bool SDManager::writeDirEntry(uint8_t dir_num, const DirEntry* entry){
    ScopedSDBusy guard;
    File f=SD.open(ROOT_DIRS, FILE_WRITE); if(!f) return false;
    f.seek((dir_num-1)*sizeof(DirEntry));
    bool ok = f.write((const uint8_t*)entry,sizeof(DirEntry))==sizeof(DirEntry);
    f.close(); return ok;
}
bool SDManager::readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry){
    ScopedSDBusy guard;
    char p[SDPATHLENGTH]; snprintf(p,sizeof(p),"/%03u%s",dir_num,FILES_DIR);
    File f=SD.open(p, FILE_READ); if(!f) return false;
    f.seek((file_num-1)*sizeof(FileEntry));
    bool ok = f.read((uint8_t*)entry,sizeof(FileEntry))==sizeof(FileEntry);
    f.close(); return ok;
}
bool SDManager::writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry){
    ScopedSDBusy guard;
    char p[SDPATHLENGTH]; snprintf(p,sizeof(p),"/%03u%s",dir_num,FILES_DIR);
    File f=SD.open(p, FILE_WRITE); if(!f) return false;
    f.seek((file_num-1)*sizeof(FileEntry));
    bool ok = f.write((const uint8_t*)entry,sizeof(FileEntry))==sizeof(FileEntry);
    f.close(); return ok;
}

bool SDManager::fileExists(const char* fullPath){ return SD.exists(fullPath); }
bool SDManager::writeTextFile(const char* path,const char* text){
    ScopedSDBusy guard;
    File f=SD.open(path, FILE_WRITE); if(!f) return false; f.print(text); f.close(); return true;
}
String SDManager::readTextFile(const char* path){
    ScopedSDBusy guard;
    File f=SD.open(path, FILE_READ); if(!f) return ""; String s=f.readString(); f.close(); return s;
}
bool SDManager::deleteFile(const char* path){
    ScopedSDBusy guard;
    return SD.exists(path) && SD.remove(path);
}
bool SDManager::renameFile(const char* oldPath,const char* newPath){
    ScopedSDBusy guard;
    return SD.rename(oldPath,newPath);
}

const char* getMP3Path(uint8_t dirID, uint8_t fileID){
    static char path[SDPATHLENGTH];
    snprintf(path,sizeof(path),"/%03u/%03u.mp3",dirID,fileID);
    return path;
}

static bool versionStringsEqual(const String& a, const char* b){
    int ia=0, ib=0;
    while (a[ia]!=0 && b[ib]!=0){
        while (a[ia]=='\r'||a[ia]=='\n'||a[ia]==' '||a[ia]=='\t') ia++;
        while (b[ib]=='\r'||b[ib]=='\n'||b[ib]==' '||b[ib]=='\t') ib++;
        if (a[ia]!=b[ib]) return false;
        if (a[ia]==0 || b[ib]==0) break;
        ia++; ib++;
    }
    while (a[ia]=='\r'||a[ia]=='\n'||a[ia]==' '||a[ia]=='\t') ia++;
    while (b[ib]=='\r'||b[ib]=='\n'||b[ib]==' '||b[ib]=='\t') ib++;
    return (a[ia]==0 && b[ib]==0);
}
