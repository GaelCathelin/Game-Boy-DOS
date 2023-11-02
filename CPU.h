#pragma once

#include "memory.h"
#include <stdio.h>

typedef struct {
    union {
        uint16_t AF;
        struct {uint8_t F, A;};
        struct {uint8_t _unused:4, c:1, h:1, n:1, z:1;};
        struct {uint8_t _F:8, Al:4, Ah:4;};
    };
    union {
        uint16_t BC;
        struct {uint8_t C, B;};
        struct {uint8_t Cl:4, Ch:4, Bl:4, Bh:4;};
    };
    union {
        uint16_t DE;
        struct {uint8_t E, D;};
        struct {uint8_t El:4, Eh:4, Dl:4, Dh:4;};
    };
    union {
        uint16_t HL;
        struct {uint8_t L, H;};
        struct {uint8_t Ll:4, Lh:4, Hl:4, Hh:4;};
    };
    uint16_t SP, PC, timer;
    uint8_t IME;
    bool stopped, halted;

    Memory *mem;

    uint64_t cycles;

#ifdef DEBUG
    uint32_t executedInstrs;
#endif
} CPU;

CPU initCPU(const char *cartridge, const bool bootSequence, const uint8_t hackLevel) WARN_UNUSED_RESULT;
void deleteCPU(CPU *cpu);

bool nextInstructions(CPU *cpu, const uint64_t breakAt, FILE *logFile);
