#pragma once
#include <Arduino.h>
#include "SDManager.h"

class AsyncWebServer;

namespace SDVoting {
  uint8_t getRandomFile(uint8_t dir_num);
  void    applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta);
  void    banFile(uint8_t dir_num, uint8_t file_num);
  void    deleteIndexedFile(uint8_t dir_num, uint8_t file_num);
  bool    getCurrentPlayable(uint8_t& dirOut, uint8_t& fileOut);
  void    attachVoteRoute(AsyncWebServer& server);
}
