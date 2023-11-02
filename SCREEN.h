#pragma once

#include "memory.h"

#define SCREEN_LINE_CLKS 114
#define SCREEN_ROWS      154
#define SCREEN_CLKS      (SCREEN_LINE_CLKS * SCREEN_ROWS)

typedef struct {
    bool enabled;
} Window;

typedef struct {
    bool originalColors;
    Window screen, background, window, tiles;

    uint8_t wy, delay, *IO, visibleSprites, currPalette[3];
    const uint8_t *VRAM;
    const Sprite *OAM;
    Sprite sprites[10];
    bool enabled;

    uint16_t physicalCycles;
    uint64_t cycles;
    uint8_t pixelBatch;
} Screen;

Screen initScreen(uint8_t *IO, const uint8_t *VRAM, const Sprite *OAM, const uint8_t pixelBatch) WARN_UNUSED_RESULT;
void deleteScreen(Screen *screen);

void setTitle(Screen *screen, const char *title);
void setPalette(Screen *screen, const bool originalColors);
bool nextPixels(Screen *screen, const bool draw);
