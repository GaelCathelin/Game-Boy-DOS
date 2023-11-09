#pragma once

#include "global.h"

#define ROM_BANK_SIZE 0x4000
#define RAM_SIZE      0x2000

typedef struct {
    uint8_t y, x, tile, _unused:4, palette:1, xflip:1, yflip:1, priority:1;
} Sprite;

typedef struct {
    uint8_t (*romBanks)[ROM_BANK_SIZE], VRAM[RAM_SIZE], (*externalRAM)[RAM_SIZE], internalRAM[RAM_SIZE];
    uint8_t patchMem[RAM_SIZE];
    Sprite OAM[40];
    uint8_t IO[0x80], HRAM[0x7F], interruptReg;
    uint16_t currROMBank, nbROMBanks;
    uint8_t currRAMBank, nbRAMBanks, mbcGen, rom0Bank, romBank, ramBank;
    bool ram, mbcMode;
    char savePath[128];
} Memory;

Memory* initMemory(const char *path, const uint8_t hackLevel) WARN_UNUSED_RESULT;
void deleteMemory(Memory *mem);
