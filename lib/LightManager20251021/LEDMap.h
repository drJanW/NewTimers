#ifndef LEDMAP_H
#define LEDMAP_H

struct LEDPos {
    float x;
    float y;
};

LEDPos getLEDPos(int index);
bool loadLEDMapFromSD(const char* path);

#endif
