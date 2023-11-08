#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t defaultROM[0x8000] = {
    #include "tetris.rom"
};

static const struct {
    bool battery;
    uint8_t mbcGen;
    char type[20];
} mbcTypes[] = {
    {false, 0, "NO MBC"             }, {false, 1, "MBC1"            }, {false, 1, "MBC1"        }, {true , 1, "MBC1+BATTERY" }, {false, 0, "UNKNOWN"     },
    {false, 2, "MBC2"               }, {true , 2, "MBC2+BATTERY"    }, {false, 0, "UNKNOWN"     }, {false, 0, "NO MBC"       }, {true , 0, "BATTERY"     },
    {false, 0, "UNKNOWN"            }, {false, 1, "MMM01"           }, {false, 1, "MMM01"       }, {true , 1, "MMM01+BATTERY"}, {false, 0, "UNKNOWN"     },
    {true , 3, "MBC3+RTC+BATTERY"   }, {true , 3, "MBC3+RTC+BATTERY"}, {false, 3, "MBC3"        }, {true , 3, "MBC3"         }, {false, 3, "MBC3+BATTERY"},
    {false, 0, "UNKNOWN"            }, {false, 0, "UNKNOWN"         }, {false, 0, "UNKNOWN"     }, {false, 0, "UNKNOWN"      }, {false, 0, "UNKNOWN"     },
    {false, 5, "MBC5"               }, {false, 5, "MBC5"            }, {true , 5, "MBC5+BATTERY"}, {false, 5, "MBC5+RUMBLE"  }, {false, 5, "MBC5+RUMBLE" },
    {true , 5, "MBC5+RUMBLE+BATTERY"}, {false, 0, "UNKNOWN"         }, {false, 6, "MBC6"        },
};

Memory* initMemory(const char *path, const bool boot, const uint8_t hackLevel) {
    Memory *mem = calloc(1, sizeof(Memory));
    mem->currROMBank = 1;
    mem->IO[0] = 0xFF; mem->IO[0x50] = boot ? 0 : 0xFF;
    memset(&mem->IO[0x51], 0xFF, sizeof(mem->IO) - 0x51);

    {
        FILE *file = fopen(path, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            mem->nbROMBanks = ftell(file) / ROM_BANK_SIZE;
            mem->romBanks = malloc(mem->nbROMBanks * ROM_BANK_SIZE);
            rewind(file);

            uint16_t matched = 0;
            for (uint16_t i = 0; i < mem->nbROMBanks; i++) {
                fread(mem->romBanks[i], 1, ROM_BANK_SIZE, file);

                // Replace busy waits with more efficient code
                if (hackLevel >= 2) {
                    for (uint16_t j = 0; j < ROM_BANK_SIZE; j++) {
                        if (mem->romBanks[i][j] == 0xF0 && mem->romBanks[i][j + 1] != 0xCC && (mem->romBanks[i][j + 2] == 0xA7 || mem->romBanks[i][j + 2] == 0xB7) && mem->romBanks[i][j + 3] == 0x28 && mem->romBanks[i][j + 4] == 0xFB) {
                            mem->romBanks[i][j + 3] = 0xD3;
                            mem->romBanks[i][j + 4] = mem->romBanks[i][j + 1];
                            mem->romBanks[i][j + 0] = 0x00;
                            mem->romBanks[i][j + 1] = 0x00;
                            mem->romBanks[i][j + 2] = 0x00;
                        } else if (mem->romBanks[i][j] == 0xFA && (mem->romBanks[i][j + 3] == 0xA7 || mem->romBanks[i][j + 3] == 0xB7) && mem->romBanks[i][j + 4] == 0x28 && mem->romBanks[i][j + 5] == 0xFA) {
                            mem->romBanks[i][j + 3] = 0xDD;
                            mem->romBanks[i][j + 4] = mem->romBanks[i][j + 1];
                            mem->romBanks[i][j + 5] = mem->romBanks[i][j + 2];
                            mem->romBanks[i][j + 0] = 0x00;
                            mem->romBanks[i][j + 1] = 0x00;
                            mem->romBanks[i][j + 2] = 0x00;
                        } else if (mem->romBanks[i][j] == 0xBE && mem->romBanks[i][j + 1] == 0x20 && mem->romBanks[i][j + 2] == 0xFD) {
                            mem->romBanks[i][j + 2] = 0xDB;
                            mem->romBanks[i][j    ] = 0x00;
                            mem->romBanks[i][j + 1] = 0x00;
                        } else if (hackLevel >= 3) {
                            int8_t k = 0;

                            if      (mem->romBanks[i][j + k] == 0xBE) k += 1;
                            else if (mem->romBanks[i][j + k] == 0xF0 && mem->romBanks[i][j + k + 1] >= 0x40 && mem->romBanks[i][j + k + 1] != 0xCC) k += 2;
                            else if (mem->romBanks[i][j + k] == 0xFA) k += 3;
                            else continue;

                            if (k > 1) {
                                if      (mem->romBanks[i][j + k] >= 0xA0 && mem->romBanks[i][j + k] <= 0xBF) k += 1;
                                else if (mem->romBanks[i][j + k] == 0xE6 || mem->romBanks[i][j + k] == 0xF6 || mem->romBanks[i][j + k] == 0xEE || mem->romBanks[i][j + k] == 0xFE) k += 2;
                                else continue;
                            }

                            if (mem->romBanks[i][j + k] == 0x20 || mem->romBanks[i][j + k] == 0x28 || mem->romBanks[i][j + k] == 0x30 || mem->romBanks[i][j + k] == 0x38) k += 1;
                            else continue;

                            if ((int8_t)mem->romBanks[i][j + k] == -(k + 1)) {
                                mem->patchMem[matched] = 0xF4;
                                memcpy(&mem->patchMem[matched + 1], &mem->romBanks[i][j], k);
                                mem->patchMem[matched + k + 1] = mem->romBanks[i][j + k] - 1;
                                mem->patchMem[matched + k + 2] = 0xC3;
                                const uint16_t returnAddress = j + 3 + (i > 0 ? ROM_BANK_SIZE : 0);
                                mem->patchMem[matched + k + 3] = returnAddress & 0xFF;
                                mem->patchMem[matched + k + 4] = returnAddress >> 8;

                                mem->romBanks[i][j] = 0xC3;
                                mem->romBanks[i][j + 1] = matched & 0xFF;
                                mem->romBanks[i][j + 2] = 0xE0 | matched >> 8;
                                memset(&mem->romBanks[i][j + 3], 0, k - 2);

                                matched += k + 5;
                                printf("%02u %04X %4X\t", i, j, matched);
                            }
                        }
                    }
                }
            }

            fclose(file);
//            exit(205);
        } else {
            printf("Failed to open '%s'\n", path);
            mem->nbROMBanks = sizeof(defaultROM) / ROM_BANK_SIZE;
            mem->romBanks = malloc(mem->nbROMBanks * ROM_BANK_SIZE);
            for (uint16_t i = 0; i < mem->nbROMBanks; i++)
                memcpy(mem->romBanks[i], defaultROM + i * ROM_BANK_SIZE, ROM_BANK_SIZE);
        }
    }

    const uint8_t mbcType = mem->romBanks[0][0x147];
    if (mbcType > ARRAY_SIZE(mbcTypes)) {
        printf("Unsupported MBC (%02X)\n", mbcType);
        memset(mem->romBanks, 0xFF, ROM_BANK_SIZE);
        return mem;
    }

    mem->mbcGen = mbcTypes[mbcType].mbcGen;
    const uint8_t ram = mem->mbcGen == 2 ? 8 : 2 << (mem->romBanks[0][0x149] << 1) >> 2;
    const uint16_t romSize = 1 << (mem->romBanks[0][0x148] + 5);
    assert(mem->nbROMBanks == (romSize << 10) / ROM_BANK_SIZE);
    printf("%s - %dk ROM", mbcTypes[mbcType].type, romSize);
    if (ram) printf(" - %dk RAM", ram);
    putchar('\n');

    mem->nbRAMBanks = (ram << 10) / RAM_SIZE;
    assert(mem->nbRAMBanks <= 4);
    mem->externalRAM = calloc(mem->nbRAMBanks, RAM_SIZE);

    if (mbcTypes[mbcType].battery) {
        assert(ram > 0);
        strcpy(mem->savePath, path);
        char *dot = strchr(mem->savePath, '.');
        if (dot) *dot = '\0';
        strcat(mem->savePath, ".sav");
        FILE *file = fopen(mem->savePath, "rb");
        if (file) {
            for (uint8_t i = 0; i < mem->nbRAMBanks; i++)
                fread(&mem->externalRAM[i], 1, RAM_SIZE, file);
            fclose(file);
        }
    }

    return mem;
}

void deleteMemory(Memory *mem) {
    if (mbcTypes[mem->romBanks[0][0x147]].battery) {
        FILE *file = fopen(mem->savePath, "wb");
        if (file) {
            for (uint8_t i = 0; i < mem->nbRAMBanks; i++)
                fwrite(mem->externalRAM[i], 1, RAM_SIZE, file);
            fclose(file);
        }
    }

    free(mem->externalRAM);
    free(mem->romBanks);
}
