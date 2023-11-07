#include "cpu.h"
#include "buttons.h"
#include <assert.h>
#include <string.h>

#define TURBO_INTERRUPTS
//#define THREADED_INTERPRETER
//#define PROFILE_PAIRS

#ifdef DEBUG
    #define inline __attribute__((noinline))
    #define UNREACHABLE assert(0)
#else
    #define inline inline __attribute__((always_inline))
    #define UNREACHABLE __builtin_unreachable()
#endif

#ifdef PROFILE_PAIRS
static uint32_t pairs[512 * 512] = {};
#endif

static const uint8_t bootROM[0x100] = {
    #include "boot.rom"
};

static inline uint8_t read8(const Memory *mem, const uint16_t address) {
/*
    switch (address) {
//        case 0xFF01 ... 0xFF03: printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
        case 0xFF04 ... 0xFF09: printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
//        case 0xFF0A ... 0xFF41: printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
        case 0xFF0F           : printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
//        case 0xFF44 ... 0xFF45: printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
//        case 0xFF47 ... 0xFF7F: printf("%X -> %02X\n", address, mem->IO[address & 0x7F]); break;
//        case 0xFFFF           : printf("%X -> %02X\n", address, mem->interruptReg      ); break;
//        case 0xFF00 ... 0xFFFF: printf("%X -> ??\n"  , address                         ); break;
    }
//*/
    switch (address) {
        case 0x0000 ... 0x00FF: if (UNLIKELY(!mem->IO[0x50])) return bootROM        [address & 0xFF  ]; FALLTHROUGH;
        case 0x0100 ... 0x3FFF: return mem->romBanks[mem->rom0Bank]                 [address         ];
        case 0x4000 ... 0x7FFF: return mem->romBanks[mem->romBank ]                 [address & 0x3FFF];
        case 0x8000 ... 0x9FFF: return (mem->IO[0x41] & 0x3) == 3 ? 0xFF : mem->VRAM[address & 0x1FFF];
        case 0xA000 ... 0xBFFF: return mem->ram ? mem->externalRAM[mem->ramBank]    [address & 0x1FFF] : 0xFF;
        case 0xC000 ... 0xDFFF: return mem->internalRAM                             [address & 0x1FFF];
        case 0xE000 ... 0xFDFF: return mem->patchMem                                [address & 0x1FFF];
        case 0xFE00 ... 0xFE9F: return (mem->IO[0x41] & 0x3) > 1 ? 0xFF : ((uint8_t*)mem->OAM)[address & 0xFF];
        case 0xFEA0 ... 0xFEFF: return 0;
        case 0xFF00 ... 0xFF4B: return mem->IO                                      [address & 0x7F  ];
        case 0xFF4C ... 0xFF7F: return 0xFF;
        case 0xFF80 ... 0xFFFE: return mem->HRAM                                    [address & 0x7F  ];
        case 0xFFFF           : return mem->interruptReg;
        default: UNREACHABLE; return 0xFF;
    }
}

static inline const uint8_t* readp(const Memory *mem, const uint16_t address) {
    switch (address) {
        case 0x0000 ... 0x00FF: if (!mem->IO[0x50]) return &bootROM   [address         ]; FALLTHROUGH;
        case 0x0100 ... 0x3FFF: return &mem->romBanks[mem->rom0Bank]  [address         ];
        case 0x4000 ... 0x7FFF: return &mem->romBanks[mem->romBank ]  [address & 0x3FFF];
        case 0x8000 ... 0x9FFF: return &mem->VRAM                     [address & 0x1FFF];
        case 0xA000 ... 0xBFFF: return &mem->externalRAM[mem->ramBank][address & 0x1FFF];
        case 0xC000 ... 0xDFFF: return &mem->internalRAM              [address & 0x1FFF];
        case 0xE000 ... 0xFDFF: return &mem->patchMem                 [address & 0x1FFF];
        case 0xFF80 ... 0xFFFE: return &mem->HRAM                     [address & 0x7F  ];
        default: UNREACHABLE; return NULL;
    }
}

static inline uint16_t pop16(const Memory *mem, uint16_t *sp) {
    const uint16_t value = *(uint16_t*)readp(mem, *sp);
    *sp += 2;
    return value;
}

static inline void updateBanks(Memory *mem) {
    mem->rom0Bank = mem->mbcGen == 5 ? 0 : (mem->currRAMBank & 0x3) << 5 & (mem->nbROMBanks - 1);
    mem->ramBank = mem->mbcGen != 1 || mem->mbcMode ? mem->currRAMBank & (mem->nbRAMBanks - 1) : 0;
    const uint16_t bank = mem->mbcGen == 5 ? mem->currROMBank : (mem->currRAMBank & 0x3) << 5 | mem->currROMBank;
    mem->romBank = bank & (mem->nbROMBanks - 1);
}

static inline uint8_t maskedWrite(uint8_t oldval, uint8_t newval, const uint8_t mask) {
    return oldval ^ ((oldval ^ newval) & mask);
}

static inline void write8(Memory *mem, const uint16_t address, const uint8_t value) {
/*
    switch (address) {
//        case 0x0000 ... 0x1FFF: printf("%X <- %02X\n", address, value); break;
//        case 0x2000 ... 0x3FFF: printf("%X <- %02X\n", address, value); break;
//        case 0x4000 ... 0x5FFF: printf("%X <- %02X\n", address, value); break;
//        case 0x6000 ... 0x7FFF: printf("%X <- %02X\n", address, value); break;
//        case 0xFF01 ... 0xFF03: printf("%X <- %02X\n", address, value); break;
        case 0xFF04 ... 0xFF09: printf("%X <- %02X\n", address, value); break;
//        case 0xFF10 ... 0xFF14: printf("%X <- %02X\n", address, value); break;
//        case 0xFF15 ... 0xFF19: printf("%X <- %02X\n", address, value); break;
//        case 0xFF1A ... 0xFF1E: printf("%X <- %02X\n", address, value); break;
//        case 0xFF1F ... 0xFF23: printf("%X <- %02X\n", address, value); break;
//        case 0xFF24 ... 0xFF26: printf("%X <- %02X\n", address, value); break;
//        case 0xFF27 ... 0xFF2F: printf("%X <- %02X\n", address, value); break;
//        case 0xFF30 ... 0xFF3F: printf("%X <- %02X\n", address, value); break;
//        case 0xFF40           : printf("%X <- %02X\n", address, value); break;
//        case 0xFF41           : printf("%X <- %02X\n", address, value); break;
//        case 0xFF42 ... 0xFF43: printf("%X <- %02X\n", address, value); break;
//        case 0xFF44 ... 0xFF45: printf("%X <- %02X\n", address, value); break;
//        case 0xFF47 ... 0xFF7F: printf("%X <- %02X\n", address, value); break;
//        case 0xFFFF           : printf("%X <- %02X\n", address, value); break;
    }
//*/
    switch (address) {
        case 0x0000 ... 0x1FFF: mem->ram = (value & 0xF) == 0xA; break;
        case 0x3000 ... 0x3FFF: if (mem->mbcGen == 5) {mem->currROMBank = (mem->currROMBank & 0xFF) | (value & 1) << 8; updateBanks(mem); break;} FALLTHROUGH;
        case 0x2000 ... 0x2FFF: mem->currROMBank = (mem->mbcGen == 5) ? (mem->currROMBank & 0xFF00) | value : (value & 0x1F) ? : 1; updateBanks(mem); break;
        case 0x4000 ... 0x5FFF: mem->currRAMBank = value; updateBanks(mem); break;
        case 0x6000 ... 0x7FFF: mem->mbcMode = value & 1; updateBanks(mem); break;
        case 0x8000 ... 0x9FFF: if ((mem->IO[0x41] & 0x3) != 3) mem->VRAM[address & 0x1FFF] = value; break;
        case 0xA000 ... 0xBFFF: if (mem->ram) mem->externalRAM[mem->ramBank][address & 0x1FFF] = value; break;
        case 0xC000 ... 0xDFFF: mem->internalRAM[address & 0x1FFF] = value; break;
        case 0xE000 ... 0xFDFF: mem->patchMem[address & 0x1FFF] = value; break;
        case 0xFE00 ... 0xFE9F: if ((mem->IO[0x41] & 0x3) < 2) ((uint8_t*)mem->OAM)[address & 0xFF] = value; break;
        case 0xFEA0 ... 0xFEFF: break;
        case 0xFF00           : mem->IO[address & 0x7F]    = updateInputReg(value); break;
        case 0xFF01           : mem->IO[0x01] = value; break;
        case 0xFF02           : mem->IO[0x02] = value; if (value == 0x81) {mem->IO[0x0F] |= 0x8; mem->IO[0x01] = 0xFF; mem->IO[0x02] = 0x01;} break;
        case 0xFF03 ... 0xFF04: *(uint16_t*)&mem->IO[0x03] = 0;     break;
        case 0xFF05 ... 0xFF06: mem->IO[address & 0x7F]    = value; break;
        case 0xFF07           : mem->IO[address & 0x7F]    = maskedWrite(mem->IO[address & 0x7F], value, 0x7); break;
        case 0xFF08 ... 0xFF11: mem->IO[address & 0x7F]    = value; break;
//        case 0xFF12           : if ((mem->IO[address & 0x7F] & 0xF) == 0x8 && (value & 0xF) == 0x8 && (mem->IO[0x26] & 0x1) != 0) mem->IO[address & 0x7F] += 0x10; else mem->IO[address & 0x7F] = value; break;
        case 0xFF12           : mem->IO[address & 0x7F]    = value; break;
        case 0xFF13 ... 0xFF16: mem->IO[address & 0x7F]    = value; break;
//        case 0xFF17           : if ((mem->IO[address & 0x7F] & 0xF) == 0x8 && (value & 0xF) == 0x8 && (mem->IO[0x26] & 0x2) != 0) mem->IO[address & 0x7F] += 0x10; else mem->IO[address & 0x7F] = value; break;
        case 0xFF17           : mem->IO[address & 0x7F]    = value; break;
        case 0xFF18 ... 0xFF25: mem->IO[address & 0x7F]    = value; break;
        case 0xFF26           : mem->IO[address & 0x7F]    = maskedWrite(mem->IO[address & 0x7F], value, 0x80); break;
        case 0xFF27 ... 0xFF40: mem->IO[address & 0x7F]    = value; break;
        case 0xFF41           : mem->IO[address & 0x7F]    = maskedWrite(mem->IO[address & 0x7F], value, 0x78); break;
        case 0xFF42 ... 0xFF43: mem->IO[address & 0x7F]    = value; break;
        case 0xFF44           : break;
        case 0xFF45           : mem->IO[address & 0x7F]    = value; break;
        case 0xFF46           : memcpy(mem->OAM, readp(mem, value << 8), sizeof(mem->OAM)); break;
        case 0xFF47 ... 0xFF50: mem->IO[address & 0x7F]    = value; break;
        case 0xFF51 ... 0xFF7F: break;
        case 0xFF80 ... 0xFFFE: mem->HRAM[address & 0x7F]  = value; break;
        case 0xFFFF           : mem->interruptReg          = value; break;
        default: UNREACHABLE;
    }
}

static inline void writep(Memory *mem, const uint16_t address, const uint16_t value) {
    switch (address) {
        case 0x8000 ... 0x9FFF: *(uint16_t*)&mem->VRAM                     [address & 0x1FFF] = value; return;
        case 0xA000 ... 0xBFFF: *(uint16_t*)&mem->externalRAM[mem->ramBank][address & 0x1FFF] = value; return;
        case 0xC000 ... 0xDFFF: *(uint16_t*)&mem->internalRAM              [address & 0x1FFF] = value; return;
        case 0xE000 ... 0xFDFF: *(uint16_t*)&mem->patchMem                 [address & 0x1FFF] = value; return;
        case 0xFF80 ... 0xFFFE: *(uint16_t*)&mem->HRAM                     [address & 0x7F  ] = value; return;
        default: UNREACHABLE;
    }
}

#ifdef DEBUG
typedef struct {
    uint8_t length, duration;
    char mnemonic[13];
} Instruction;

static Instruction instructions[512] = {
    #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) {_length, _duration, _mnemonic},
    #include "lr35902.inl"
    #undef INSTRUCTION
};
#endif

CPU initCPU(const char *cartridge, const bool bootSequence, const uint8_t hackLevel) {
    CPU cpu = {.mem = initMemory(cartridge, hackLevel)};
    updateBanks(cpu.mem);

    if (!bootSequence) {
        cpu.AF = 0x01B0;
        cpu.BC = 0x0013;
        cpu.DE = 0x00D8;
        cpu.HL = 0x014D;
        cpu.SP = 0xFFFE;
        cpu.PC = 0x0100;
        cpu.mem->IO[0x40] = 0x91;
        cpu.mem->IO[0x50] = 0x01;
    }

    return cpu;
}

void deleteCPU(CPU *cpu) {
    deleteMemory(cpu->mem);

#ifdef PROFILE_PAIRS
    for (uint32_t i = 0; i < ARRAY_SIZE(pairs); i++)
        if (pairs[i] > 500000)
            printf("%03lX-%03lX:%10lu\n", i >> 9, i & 0x1FF, pairs[i]);
#endif
}

#ifdef DEBUG
static void logInstruction(CPU *cpu, FILE *logFile, const uint8_t pc, const uint16_t opcode, const uint16_t operand, const char* mnemonic, const uint8_t length) {
    if (logFile) {
        cpu->executedInstrs++;
        char instrStr[16];
        sprintf(instrStr, mnemonic, length == 2 ? (uint16_t)(int8_t)operand : operand);
        fprintf(logFile, "%2X %4X  %03X  %-15s", cpu->mem->currROMBank, pc, opcode, instrStr);
        fprintf(logFile, "%c%c%c%c  A:%2X  BC:%4X  DE:%4X  HL:%4X  SP:%4X   IME:%d  IE:%2X  IF:%2X", cpu->z ? 'Z' : '-', cpu->n ? 'N' : '-', cpu->h ? 'H' : '-', cpu->c ? 'C' : '-', cpu->A, cpu->BC, cpu->DE, cpu->HL, cpu->SP, cpu->IME, cpu->mem->interruptReg, cpu->mem->IO[0x0F]);
        fprintf(logFile, "   %7lu  %lu\n", cpu->executedInstrs, (unsigned long)cpu->cycles);
    }
}
#endif

static inline void applyFlags(CPU *cpu, const char flags[4]) {
    switch (flags[0]) {
        case 'A': cpu->z = (cpu->A == 0); break;
        case 'B': cpu->z = (cpu->B == 0); break;
        case 'C': cpu->z = (cpu->C == 0); break;
        case 'D': cpu->z = (cpu->D == 0); break;
        case 'E': cpu->z = (cpu->E == 0); break;
        case 'H': cpu->z = (cpu->H == 0); break;
        case 'L': cpu->z = (cpu->L == 0); break;
        case '0': cpu->z = 0; break;
        case '1': cpu->z = 1; break;
        default:;
    }

    switch (flags[1]) {
        case '0': cpu->n = 0; break;
        case '1': cpu->n = 1; break;
        default:;
    }

    switch (flags[2]) {
        case '0': cpu->h = 0; break;
        case '1': cpu->h = 1; break;
        default:;
    }

    switch (flags[3]) {
        case '0': cpu->c = 0; break;
        case '1': cpu->c = 1; break;
        default:;
    }
}

static inline void decode(const CPU *cpu, uint16_t *opcode, uint16_t *operand) {
    const uint8_t *mem = readp(cpu->mem, cpu->PC);
    *opcode = *mem++;
    if (*opcode == 0xCB)
        *opcode = 0x100 | *mem;
    else
        *operand = *(uint16_t*)mem;

#ifdef PROFILE_PAIRS
    static uint16_t prevOpCode = 0;
    pairs[prevOpCode << 9 | *opcode]++;
    prevOpCode = *opcode;
#endif
}

static inline void incrTimers(CPU *cpu, const uint8_t val) {
    static const uint16_t timerTable[4] = {256, 4, 16, 64};

    cpu->cycles += val;
    if (LIKELY(!cpu->stopped)) *(uint16_t*)&cpu->mem->IO[0x03] += val << 2;

    if (UNLIKELY(cpu->mem->IO[0x07] & 0x4)) {
        cpu->timer += val;

        const uint16_t timerPeriod = timerTable[cpu->mem->IO[0x07] & 0x3];
        while (cpu->timer >= timerPeriod) {
            if (__builtin_add_overflow(cpu->mem->IO[0x05], 1, &cpu->mem->IO[0x05])) {
                cpu->mem->IO[0x05] = cpu->mem->IO[0x06];
                cpu->mem->IO[0x0F] |= 0x4;
            }

            cpu->timer -= timerPeriod;
        }
    }
}

static inline void interrupts(CPU *cpu) {
    for (uint8_t i = 0; i < 4; i++) {
        if (cpu->mem->interruptReg & cpu->mem->IO[0x0F] & 1 << i) {
            cpu->stopped = cpu->halted = false;

            if (cpu->IME > 0) {
                cpu->IME = 0;
                cpu->mem->IO[0x0F] &= ~(1 << i);
                writep(cpu->mem, cpu->SP -= 2, cpu->PC);
                cpu->PC = 0x40 + i * 0x8;
                incrTimers(cpu, 5);
            }

            break;
        }
    }

    cpu->IME = cpu->IME ? 1 : 0;
}

bool nextInstructions(CPU *cpu, const uint64_t breakAt, FILE *logFile) {
    UNUSED(logFile);

#if defined(TURBO_INTERRUPTS) || defined(THREADED_INTERPRETER)
    if (cpu->mem->interruptReg & cpu->mem->IO[0x0F] & 0xF) {
        interrupts(cpu);
    } else if (cpu->halted || cpu->stopped) {
        incrTimers(cpu, breakAt - cpu->cycles + 1);
        return true;
    }
#endif

#ifdef THREADED_INTERPRETER
    static const void* instrs[] = {
        #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) &&_##_opcode,
        #include "lr35902.inl"
        #undef INSTRUCTION
    };

    uint16_t opcode, operand = 0;
    decode(cpu, &opcode, &operand);
    goto *instrs[opcode];

    #define read(_address) read8(cpu->mem, _address)
    #define write(_address, _value) write8(cpu->mem, _address, _value)
    #define push(_value) writep(cpu->mem, cpu->SP -= 2, _value)
    #define pop() pop16(cpu->mem, &cpu->SP)
    #define addCycles(_value) cycles = _value;
    #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) _##_opcode: { \
        cpu->PC += _length; \
        uint8_t cycles = 0; \
        _code; \
        if (_flags[2] != '-') applyFlags(cpu, _flags); \
        incrTimers(cpu, _length + _duration + cycles); \
        if (cpu->cycles >= breakAt) return true; \
        decode(cpu, &opcode, &operand); \
        goto *instrs[opcode];}
        #include "lr35902.inl"
    #undef INSTRUCTION
    #undef addCycles
    #undef pop
    #undef push
    #undef write
    #undef read
#else
    while (LIKELY(cpu->cycles < breakAt)) {
    #ifndef TURBO_INTERRUPTS
        if (UNLIKELY(cpu->halted || cpu->stopped)) {
            incrTimers(cpu, 1);
        } else {
    #endif
            uint8_t cycles = 0; UNUSED(cycles);
            uint16_t opcode, operand = 0;
            decode(cpu, &opcode, &operand);

            switch (opcode) {
                #define read(_address) read8(cpu->mem, _address)
                #define write(_address, _value) write8(cpu->mem, _address, _value)
                #define push(_value) writep(cpu->mem, cpu->SP -= 2, _value)
                #define pop() pop16(cpu->mem, &cpu->SP)
                #if defined(DEBUG)
                    #define addCycles(_value) incrTimers(cpu, _value)
                    #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) case _opcode: { \
                        incrTimers(cpu, _length); \
                        cpu->PC += _length; \
                        _code; \
                        if (_flags[2] != '-') applyFlags(cpu, _flags); \
                        if (_duration) incrTimers(cpu, _duration); \
                        break;}
                #elif defined(TEST)
                    #define addCycles(_value) cycles = _value;
                    #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) case _opcode: { \
                        if ((_opcode == 0x18 && (int8_t)operand == -2) || (_opcode == 0xC3 && operand == cpu->PC)) return false; \
                        cpu->PC += _length; \
                        _code; \
                        if (_flags[2] != '-') applyFlags(cpu, _flags); \
                        incrTimers(cpu, _length + _duration + cycles); \
                        break;}
                #else
                    #define addCycles(_value) cycles = _value;
                    #define INSTRUCTION(_opcode, _mnemonic, _length, _duration, _flags, _code) case _opcode: { \
                        cpu->PC += _length; \
                        _code; \
                        if (_flags[2] != '-') applyFlags(cpu, _flags); \
                        incrTimers(cpu, _length + _duration + cycles); \
                        break;}
                #endif
                    #include "lr35902.inl"
                #undef INSTRUCTION
                #undef addCycles
                #undef pop
                #undef push
                #undef write
                #undef read
                default: UNREACHABLE;
            }

            #ifdef DEBUG
            const Instruction *instr = &instructions[opcode];
            logInstruction(cpu, logFile, cpu->PC, opcode, operand, instr->mnemonic, instr->length);
            #endif
    #ifndef TURBO_INTERRUPTS
        }

        interrupts(cpu);
    #endif
    }

    return true;
#endif
}
