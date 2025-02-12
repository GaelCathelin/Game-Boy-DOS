#include "screen.h"
#include <dpmi.h>
#include <pc.h>
#include <string.h>
#include <sys/nearptr.h>

#ifdef DEBUG
    #define inline __attribute__((noinline))
#else
    #define inline inline __attribute__((always_inline))
#endif

static inline bool setMode(const uint16_t n) {
    // http://www.faqs.org/faqs/msdos-programmer-faq/part4/section-5.html
    __dpmi_regs regs = {.h.ah = 0x12, .h.bl = 0x32};
    __dpmi_int(0x10, &regs);
    __dpmi_int(0x10, &(__dpmi_regs){.x.ax = n});
    return regs.h.al == 0x12;
}

static inline void tweakTimings() {
/*
    outportb(0x3D4, 0x00); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x01); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x02); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x03); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x04); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x05); printf("%X\n", inportw(0x3D4));

    outportb(0x3D4, 0x06); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x07); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x09); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x10); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x11); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x12); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x15); printf("%X ", inportw(0x3D4));
    outportb(0x3D4, 0x16); printf("%X\n", inportw(0x3D4));
//*/
    // wiki.osdev.org/VGA_Hardware
    // https://pdos.csail.mit.edu/6.828/2014/readings/hardware/vgadoc/VGAREGS.TXT
    outportb(0x3C2, 0xE3); // 640x480 (25 MHz, negative-negative)
/*
    const uint16_t vTotal = 525;
    const uint16_t vDisplayEnd = 480 - 1;
    const uint16_t vBlankingStart = vDisplayEnd + 1;
    const uint16_t vRetraceStart = vTotal - 36;
    const uint16_t vRetraceEnd = vTotal - 34;
    const uint16_t vBlankingEnd = vTotal - 1;
/*/
    const uint16_t vTotal = 525;
    const uint16_t vDisplayEnd = 288 - 1;
    const uint16_t vBlankingStart = vDisplayEnd + 0;
    const uint16_t vRetraceStart = vTotal - 132;
    const uint16_t vRetraceEnd = vTotal - 1;
    const uint16_t vBlankingEnd = vTotal - 1;
//*/
    outportw(0x3D4, 0x11 | (vRetraceEnd & 0xF) << 8); // vertical retrace end and indices 0-7 write enable
    outportw(0x3D4, 0x06 | ((vTotal - 2) & 0xFF) << 8); // vertical total
    outportw(0x3D4, 0x07 | (0x3E) << 8); // overflow register
    outportw(0x3D4, 0x09 | (0xC0) << 8); // start vertical blanking (2)
    outportw(0x3D4, 0x10 | (vRetraceStart & 0xFF) << 8); // vertical retrace start
    outportw(0x3D4, 0x12 | (vDisplayEnd & 0xFF) << 8); // vertical display end
    outportw(0x3D4, 0x15 | (vBlankingStart & 0xFF) << 8); // start vertical blanking
    outportw(0x3D4, 0x16 | (vBlankingEnd & 0xFF) << 8); // end vertical blanking
/*
    const uint8_t hTotal = 50;
    const uint8_t hDisplayEnd = 40 - 1;
    const uint8_t hBlankingStart = hDisplayEnd + 1;
    const uint8_t hRetraceStart = hTotal - 5;
    const uint8_t hRetraceEnd = hTotal - 1;
    const uint8_t hBlankingEnd = hTotal - 1;
/*/
    const uint8_t hTotal = 50;
    const uint8_t hDisplayEnd = 20 - 1;
    const uint8_t hBlankingStart = hDisplayEnd + 1;
    const uint8_t hRetraceStart = hTotal - 15;
    const uint8_t hRetraceEnd = hTotal - 1;
    const uint8_t hBlankingEnd = hTotal - 1;
//*/
    outportw(0x3D4, 0x00 | (hTotal - 5) << 8); // horizontal total
    outportw(0x3D4, 0x01 | hDisplayEnd << 8); // horizontal display end
    outportw(0x3D4, 0x02 | hBlankingStart << 8); // start horizontal blanking
    outportw(0x3D4, 0x03 | 0x8000 | (hBlankingEnd & 0x1F) << 8); // end horizontal blanking
    outportw(0x3D4, 0x04 | hRetraceStart << 8); // horizontal retrace start
    outportw(0x3D4, 0x05 | (hRetraceEnd & 0x1F) << 8 | (hBlankingEnd & 0x20) << 2); // horizontal retrace end

    outportw(0x3D4, 0x13 | 10 << 8); // scanline byte count / 2
}

static inline void setColor(const uint8_t i, const uint8_t c) {
    outportb(0x3C0, i); outportb(0x3C0, ((c & 0x1) ? 0 : 0x38) | ((c & 0x2) ? 0 : 0x07));
}

static inline void updatePalette(Screen *screen) {
    bool modified = false;

    if (screen->currPalette[0] != screen->IO[0x47]) {
        screen->currPalette[0] = screen->IO[0x47];
        inportb(0x3DA);
        modified = true;

        const uint8_t palette[4] = {screen->IO[0x47] & 0x3, screen->IO[0x47] >> 2 & 0x3, screen->IO[0x47] >> 4 & 0x3, screen->IO[0x47] >> 6};
        for (uint8_t c = 0; c < 4; c++)
            setColor(c, palette[c]);
    }

    if (screen->currPalette[1] != screen->IO[0x48]) {
        screen->currPalette[1] = screen->IO[0x48];
        if (!modified) inportb(0x3DA);
        modified = true;

        const uint8_t palette[3] = {screen->IO[0x48] >> 2 & 0x3, screen->IO[0x48] >> 4 & 0x3, screen->IO[0x48] >> 6};
        for (uint8_t c = 0; c < 3; c++)
            setColor(c + 9, palette[c]);
    }

    if (screen->currPalette[2] != screen->IO[0x49]) {
        screen->currPalette[2] = screen->IO[0x49];
        if (!modified) inportb(0x3DA);
        modified = true;

        const uint8_t palette[3] = {screen->IO[0x49] >> 2 & 0x3, screen->IO[0x49] >> 4 & 0x3, screen->IO[0x49] >> 6};
        for (uint8_t c = 0; c < 3; c++)
            setColor(c + 13, palette[c]);
    }

    if (modified) {
        outportb(0x3C0, 0x0C); outportb(0x3C0, 0x08);
        outportb(0x3C0, 0x2C); outportb(0x3C0, 0x08);
    }
}

static inline void clear() {
    uint8_t *pixels = (uint8_t*)0xA0000 + __djgpp_conventional_base;
    outportw(0x3C4, 0x0F02); // select all planes
    outportw(0x3CE, 0x0000); // reset planes to bg palette
    memset(pixels, 0, 20 * 144);
}

void setPalette(Screen *screen, const bool originalColors) {
    static const uint8_t defaultPalette[2][4][3] = {
        {{0x38, 0x38, 0x38}, {0x28, 0x28, 0x28}, {0x18, 0x18, 0x18}, {0x08, 0x08, 0x08}},
        {{0x20, 0x26, 0x14}, {0x16, 0x20, 0x12}, {0x0C, 0x1A, 0x10}, {0x02, 0x14, 0x0E}},
    };

    if (screen->originalColors != originalColors) {
        screen->originalColors = originalColors;
        outportb(0x3C8,  0); outportb(0x3C9, defaultPalette[originalColors][3][0]); outportb(0x3C9, defaultPalette[originalColors][3][1]); outportb(0x3C9, defaultPalette[originalColors][3][2]);
        outportb(0x3C8, 56); outportb(0x3C9, defaultPalette[originalColors][2][0]); outportb(0x3C9, defaultPalette[originalColors][2][1]); outportb(0x3C9, defaultPalette[originalColors][2][2]);
        outportb(0x3C8,  7); outportb(0x3C9, defaultPalette[originalColors][1][0]); outportb(0x3C9, defaultPalette[originalColors][1][1]); outportb(0x3C9, defaultPalette[originalColors][1][2]);
        outportb(0x3C8, 63); outportb(0x3C9, defaultPalette[originalColors][0][0]); outportb(0x3C9, defaultPalette[originalColors][0][1]); outportb(0x3C9, defaultPalette[originalColors][0][2]);
    }
}

Screen initScreen(uint8_t *IO, const uint8_t *VRAM, const Sprite *OAM, const uint8_t pixelBatch) {
    Screen screen = {.enabled = true, .originalColors = false, .IO = IO, .VRAM = VRAM, .OAM = OAM, .currPalette = {0xFF, 0xFF, 0xFF}, .pixelBatch = pixelBatch};

    if (setMode(0xD))
        tweakTimings();

    __djgpp_nearptr_enable();

    setPalette(&screen, true);
    updatePalette(&screen);

    outportw(0x3CE, 0x0C01); // enable reset for planes 2 & 3
    outportw(0x3CE, 0x0805); // write mode 0, read mode 1

    return screen;
}

void deleteScreen(Screen *screen) {
    UNUSED(screen);
    __djgpp_nearptr_disable();
    setMode(0x3);
}

void setTitle(Screen *screen, const char *title) {
    UNUSED(screen); UNUSED(title);
}

static inline uint8_t flipBits(uint8_t row) {
    row = (row >> 1 & 0x55) | (row & 0x55) << 1;
    row = (row >> 2 & 0x33) | (row & 0x33) << 2;
    return row >> 4 | row << 4;
}

static inline void insertSprite(Sprite *sprites, Sprite s, int8_t i) {
    while (i > 0 && sprites[i - 1].x > s.x) {
        sprites[i] = sprites[i - 1];
        i--;
    }
    sprites[i] = s;
}

static inline uint8_t selectSprites(Screen *screen, const uint8_t y, const uint8_t spritesHeight) {
    uint8_t visibleSprites = 0;
    for (int8_t i = 0; i < 40 && visibleSprites < 10; i++)
        if (screen->OAM[i].y <= y + 16 && y + 16 < screen->OAM[i].y + spritesHeight)
            insertSprite(screen->sprites, screen->OAM[i], visibleSprites++);

    return visibleSprites;
}

static inline void drawPixels(Screen *screen, const uint8_t x, const uint8_t y, const bool windowEnabled) {
    const uint16_t tilesBase = screen->IO[0x40] & 0x10 ? 0x0 : 0x1000;
    volatile uint8_t *pixels = (uint8_t*)0xA0000 + __djgpp_conventional_base;

    inline void draw(const uint16_t indicesBase, const uint16_t tilesBaseY, uint16_t p, uint8_t x, uint8_t nbPixels) {
        inline const uint8_t* tile(const uint8_t x) {
            const uint8_t tileOffset = screen->VRAM[indicesBase + (x >> 3)];
            return &screen->VRAM[tilesBaseY + ((tilesBase ? (int8_t)tileOffset : tileOffset) << 4)];
        }

        if (nbPixels == 0)
            return;

        const uint8_t *tileRow = tile(x);
        if (p & 0x7) {
            const uint8_t pt = p & 0x7, head = 8 - pt;
            outportw(0x3CE, (0xFF00 >> pt & 0xFF00) | 0x08);
            tileRow = tile(x - pt);
            const uint8_t *tileRow2 = tile(x + head);
            const uint8_t xt = (x - pt) & 0x7;
            pixels[p >> 3];
            outportw(0x3C4, 0x0D02); pixels[p >> 3] = tileRow[0] << xt | tileRow2[0] >> (8 - xt);
            outportw(0x3C4, 0x0E02); pixels[p >> 3] = tileRow[1] << xt | tileRow2[1] >> (8 - xt);
            tileRow = tileRow2;
            x += head; p += head; nbPixels -= head;
        }

        outportw(0x3CE, 0xFF08);
        for (; nbPixels >= 8; p += 8, nbPixels -= 8) {
            const uint8_t *tileRow2 = tile(x += 8);
            const uint8_t xt = x & 0x7;
            outportw(0x3C4, 0x0D02); pixels[p >> 3] = tileRow[0] << xt | tileRow2[0] >> (8 - xt);
            outportw(0x3C4, 0x0E02); pixels[p >> 3] = tileRow[1] << xt | tileRow2[1] >> (8 - xt);
            tileRow = tileRow2;
        }

        if (nbPixels > 0) {
            outportw(0x3CE, 0xFF00 << (8 - nbPixels) | 0x08);
            const uint8_t *tileRow2 = tile(x + 8);
            const uint8_t xt = x & 0x7;
            pixels[p >> 3];
            outportw(0x3C4, 0x0D02); pixels[p >> 3] = tileRow[0] << xt | tileRow2[0] >> (8 - xt);
            outportw(0x3C4, 0x0E02); pixels[p >> 3] = tileRow[1] << xt | tileRow2[1] >> (8 - xt);
        }
    }

    outportw(0x3CE, 0x0000); // reset planes to bg palette

    const uint8_t xw = windowEnabled ? MAX((int16_t)x, screen->IO[0x4B] - 7) : 160;
    const uint8_t countw = MAX(0, MIN(160, (int16_t)x + screen->pixelBatch) - xw);
    if (windowEnabled && countw > 0) {
        const uint8_t yw = screen->wy - 1;
        const uint16_t indicesBase = (screen->IO[0x40] & 0x40 ? 0x1C00 : 0x1800) + (yw << 2 & 0x3E0);
        draw(indicesBase, tilesBase + (yw << 1 & 0xE), y * 160 + xw, xw + 7 - screen->IO[0x4B], countw);
    }

    if (xw > 0) {
        const bool background = screen->IO[0x40] & 0x1;
        if (background) {
            const uint8_t yb = y + screen->IO[0x42];
            const uint16_t indicesBase = (screen->IO[0x40] & 0x8 ? 0x1C00 : 0x1800) + (yb << 2 & 0x3E0);
            draw(indicesBase, tilesBase + (yb << 1 & 0xE), y * 160 + x, x + screen->IO[0x43], MIN(screen->pixelBatch, xw - x));
        } else {
            outportw(0x3C4, 0x0F02);
            const uint8_t count = MIN(MAX(0, (int16_t)xw - x), screen->pixelBatch);
            memset((void*)pixels + y * 20 + (x >> 3), 0, count >> 3);
            if (count & 0x7) {
                const uint16_t p = y * 20 + ((x + count) >> 3) + 1;
                outportw(0x3CE, 0xFF00 << (8 - (count & 0x7)) | 0x08);
                pixels[p] ^= pixels[p];
            }
        }
    }

    const bool spritesEnabled = screen->IO[0x40] & 0x2;
    if (spritesEnabled) {
        const bool bigSprites = (screen->IO[0x40] & 0x4) != 0;
        const uint8_t spritesHeight = bigSprites ? 16 : 8;

        for (int8_t i = screen->visibleSprites - 1; i >= 0; i--) {
            const Sprite s = screen->sprites[i];
            if (s.x <= x || s.x > x + screen->pixelBatch + 8)
                continue;

            const uint8_t spriteId = bigSprites ? (s.tile & 0xFE) : s.tile;
            const uint8_t ys = s.yflip ? spritesHeight - y + s.y - 17 : y - s.y + 16;
            const uint8_t *tileRow = &screen->VRAM[(spriteId << 4) + (ys << 1)];

            uint8_t row1 = tileRow[0], row2 = tileRow[1];
            if (s.xflip) {row1 = flipBits(row1); row2 = flipBits(row2);}
            outportw(0x3CE, s.palette ? 0x0C00 : 0x0800); // reset planes to sprite palette

            uint16_t p = y * 160 + s.x - 8;
            const uint8_t pt = p & 0x7;
            p >>= 3;
            const uint16_t mask = (row1 | row2) << (8 - pt);

            if (s.x >= 8) {
                const uint16_t mask2 = s.priority ? pixels[p] << 8 : (pixels[p], 0xFF00);
                outportw(0x3CE, (mask & mask2) | 0x08);
                outportw(0x3C4, 0x0D02); pixels[p] = row1 >> pt;
                outportw(0x3C4, 0x0E02); pixels[p] = row2 >> pt;
            }

            if (pt && s.x < 160) {
                p++;
                const uint16_t mask2 = s.priority ? pixels[p] << 8 : (pixels[p], 0xFF00);
                outportw(0x3CE, (mask << 8 & mask2) | 0x08);
                outportw(0x3C4, 0x0D02); pixels[p] = row1 << (8 - pt);
                outportw(0x3C4, 0x0E02); pixels[p] = row2 << (8 - pt);
            }
        }
    }
}

static inline void incrTimers(Screen *screen, const uint8_t val) {
    screen->cycles += val;
    screen->physicalCycles += val;
}

bool nextPixels(Screen *screen, const bool draw) {
    const bool enabled = screen->IO[0x40] >> 7;
    if (screen->physicalCycles >= SCREEN_CLKS) screen->physicalCycles -= SCREEN_CLKS;
    const uint8_t y = screen->physicalCycles / SCREEN_LINE_CLKS;
    const uint8_t x = screen->physicalCycles - y * SCREEN_LINE_CLKS;

    if (!enabled) {
        screen->physicalCycles = 0;
        if (screen->enabled) {
            screen->IO[0x41] &= 0xFC;
            screen->IO[0x44] = 0;
            clear();
        }
        incrTimers(screen, 2 * SCREEN_LINE_CLKS - 1);
    } else {
        if (draw && y < 144 && x > 21 && x <= 64 + screen->delay) {
            const bool windowEnabled = screen->IO[0x40] & 0x20 && y >= screen->IO[0x4A] && y < screen->IO[0x4A] + 144 && screen->IO[0x4B] < 167;
            const uint8_t x2 = MIN(160 - screen->pixelBatch, (x - (21 + screen->pixelBatch / 4)) << 2);
            drawPixels(screen, x2, y, windowEnabled);
        }

        if ((y > 0 && x == 1) || (y == 153 && x == 3)) {
            if ((screen->IO[0x41] & 0x40) && screen->IO[0x44] == screen->IO[0x45]) screen->IO[0x0F] |= 0x2;
            screen->IO[0x41] = (screen->IO[0x41] & 0xFB) | (screen->IO[0x44] == screen->IO[0x45] ? 0x4 : 0x0);
        }
        if ((y > 0 && x == 0) || (y == 153 && x == 1))
            screen->IO[0x44] = (y == 153 && x == 1) ? 0 : y;

        if (y < 144) {
            if (x == 1) {
                if (screen->IO[0x41] & 0x20) screen->IO[0x0F] |= 0x2;
                screen->IO[0x41] = (screen->IO[0x41] & 0xFC) | 0x2;

                const bool bigSprites = (screen->IO[0x40] & 0x4) != 0;
                screen->visibleSprites = selectSprites(screen, y, bigSprites ? 16 : 8);

                incrTimers(screen, 19);
                screen->delay = 1;
            } else if (x >= 21 && x < 61) {
                if (x == 21) {
                    if (y == 0) {
                        screen->wy = 0;
                        while (!(inportb(0x3DA) & 0x8));
                        if (draw) updatePalette(screen);
                        while (inportb(0x3DA) & 0x8);
                    }
                    const bool windowEnabled = screen->IO[0x40] & 0x20 && y >= screen->IO[0x4A] && y < screen->IO[0x4A] + 144 && screen->IO[0x4B] < 167;
                    if (windowEnabled) screen->wy++;
                    screen->IO[0x41] = (screen->IO[0x41] & 0xFC) | 0x3;
                }
                incrTimers(screen, x == 61 - screen->pixelBatch / 4 ? 2 + screen->pixelBatch / 4 + screen->delay : screen->pixelBatch / 4 - 1);
            } else if (x == 64 + screen->delay) {
                if (screen->IO[0x41] & 0x8) screen->IO[0x0F] |= 0x2;
                screen->IO[0x41] &= 0xFC;
                incrTimers(screen, SCREEN_LINE_CLKS - 1 - x);
            }
        } else if (y == 144 && x == 1) {
            screen->IO[0x0F] |= 0x1;
            if (screen->IO[0x41] & 0x10) screen->IO[0x0F] |= 0x2;
            screen->IO[0x41] = (screen->IO[0x41] & 0xFC) | 0x1;
//            putchar('\n');
            incrTimers(screen, SCREEN_LINE_CLKS - 3);
        } else if (y < 153 && x == 1) {
            incrTimers(screen, SCREEN_LINE_CLKS - 2);
        } else if (y == 153 && x == 3) {
            incrTimers(screen, SCREEN_LINE_CLKS - 1 - x);
        }
    }

    incrTimers(screen, 1);
    screen->enabled = enabled;
    return y == 144 && x == 1;
}
