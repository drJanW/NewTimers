#include "SDVoting.h"
#include "Globals.h"
#include "AudioState.h"
#include "ContextManager.h"

#include <ESPAsyncWebServer.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <AsyncTCP.h>
#endif

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 0
#endif

#if WEBIF_LOG_LEVEL && !defined(WEBIF_LOG)
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#elif !defined(WEBIF_LOG)
#define WEBIF_LOG(...) do {} while (0)
#endif

uint8_t SDVoting::getRandomFile(uint8_t dir_num) {
  auto& sdMgr = SDManager::instance();
  DirEntry dir;
  if (!sdMgr.readDirEntry(dir_num, &dir) || dir.file_count == 0) return 0;

  uint8_t  fileNums[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  fileScores[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  count = 0;
  uint16_t total = 0;

  for (uint8_t i = 1; i <= SD_MAX_FILES_PER_SUBDIR; i++) {
    FileEntry fe;
  if (sdMgr.readFileEntry(dir_num, i, &fe) && fe.score > 0) {
      fileNums[count]   = i;
      fileScores[count] = fe.score;
      total += fe.score;
      if (++count >= dir.file_count) break;
    }
  }
  if (count == 0) return 0;

  uint16_t pick = random(1, total + 1), acc = 0;
  for (uint8_t i = 0; i < count; i++) {
    acc += fileScores[i];
    if (pick <= acc) return fileNums[i];
  }
  return fileNums[0];
}

void SDVoting::applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta) {
  auto& sdMgr = SDManager::instance();
  FileEntry fe; DirEntry dir;
  if (!sdMgr.readFileEntry(dir_num, file_num, &fe)) return;
  if (!sdMgr.readDirEntry (dir_num, &dir)) return;
  if (fe.score == 0) return;

  if (delta > 10)  delta = 10;
  if (delta < -10) delta = -10;

  int16_t ns = (int16_t)fe.score + (int16_t)delta;
  if (ns > 200) ns = 200;
  if (ns < 1)   ns = 1;

  dir.total_score += (ns - fe.score);
  fe.score = (uint8_t)ns;

  sdMgr.writeFileEntry(dir_num, file_num, &fe);
  sdMgr.writeDirEntry (dir_num, &dir);
}

void SDVoting::banFile(uint8_t dir_num, uint8_t file_num) {
  auto& sdMgr = SDManager::instance();
  FileEntry fe; DirEntry dir;
  if (!sdMgr.readFileEntry(dir_num, file_num, &fe)) return;
  if (!sdMgr.readDirEntry (dir_num, &dir)) return;

  if (fe.score > 0) {
    if (dir.total_score >= fe.score) dir.total_score -= fe.score;
    if (dir.file_count  > 0)         dir.file_count--;
    fe.score = 0;
    sdMgr.writeFileEntry(dir_num, file_num, &fe);
    sdMgr.writeDirEntry (dir_num, &dir);
  }
}

void SDVoting::deleteIndexedFile(uint8_t dir_num, uint8_t file_num) {
  auto& sdMgr = SDManager::instance();
  FileEntry fe; DirEntry dir;
  if (!sdMgr.readFileEntry(dir_num, file_num, &fe)) return;
  if (!sdMgr.readDirEntry (dir_num, &dir)) return;

  if (fe.score > 0) {
    if (dir.total_score >= fe.score) dir.total_score -= fe.score;
    if (dir.file_count  > 0)         dir.file_count--;
  }
  fe.score   = 0;
  fe.size_kb = 0;
  sdMgr.writeFileEntry(dir_num, file_num, &fe);
  sdMgr.writeDirEntry (dir_num, &dir);

  char path[64];
  snprintf(path, sizeof(path), "/%03u/%03u.mp3", dir_num, file_num);
  SD.remove(path);
}

bool SDVoting::getCurrentPlayable(uint8_t& d, uint8_t& f) {
  return getCurrentDirFile(d, f);
}

void SDVoting::attachVoteRoute(AsyncWebServer& server) {
  server.on("/vote", HTTP_ANY, [](AsyncWebServerRequest* req) {
    const bool doDel =
      req->hasParam("del") || req->hasParam("delete") ||
      req->hasParam("del", true) || req->hasParam("delete", true);

    const bool doBan =
      req->hasParam("ban") || req->hasParam("ban", true);

    int delta = 1;
    if (!doDel && !doBan) {
      if (req->hasParam("delta"))            delta = req->getParam("delta")->value().toInt();
      else if (req->hasParam("delta", true)) delta = req->getParam("delta", true)->value().toInt();
      if (delta > 10)  delta = 10;
      if (delta < -10) delta = -10;
    }

    uint8_t dir = 0, file = 0; bool have = false;
    auto getDF = [&](bool body){
      if (req->hasParam("dir", body) && req->hasParam("file", body)) {
        long d = req->getParam("dir",  body)->value().toInt();
        long f = req->getParam("file", body)->value().toInt();
        if (d >= 1 && d <= 255 && f >= 1 && f <= 255) { dir = (uint8_t)d; file = (uint8_t)f; have = true; }
      }
    };
    getDF(false); if (!have) getDF(true);
    if (!have && !SDVoting::getCurrentPlayable(dir, file)) {
      req->send(400, "text/plain", "no current playable; supply dir & file");
      return;
    }

    if (doDel) {
      PF("[WEB] DELETE requested dir=%u file=%u\n", dir, file);
      ContextManager_post(ContextManager::WebCmd::DeleteFile, dir, file);
      char b1[64]; snprintf(b1, sizeof(b1), "DELETE scheduled dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b1);
      return;
    }

    if (doBan) {
      SDVoting::banFile(dir, file);
      char b2[64]; snprintf(b2, sizeof(b2), "BANNED dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b2);
      return;
    }

  SDVoting::applyVote(dir, file, (int8_t)delta);
  WEBIF_LOG("[Web][Vote] dir=%u file=%u delta=%d\n", dir, file, delta);
    char b3[64]; snprintf(b3, sizeof(b3), "OK dir=%u file=%u delta=%d", dir, file, delta);
    req->send(200, "text/plain", b3);
  });

  server.on("/next", HTTP_ANY, [](AsyncWebServerRequest* req){
    PF("[audio][next] request\n");
    ContextManager_post(ContextManager::WebCmd::NextTrack);
    req->send(200, "text/plain", "NEXT scheduled");
  });
}
