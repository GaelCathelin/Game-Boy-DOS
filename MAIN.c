#include "cpu.h"
#include "screen.h"
#include "sound.h"
#include "buttons.h"
#include <stddef.h>

#define LOG 0

static void emulate(const char *rom, const SoundDevice device, const bool bootSequence, const uint8_t frameSkip, const uint8_t hackLevel) {
    SCOPED(CPU) cpu = initCPU(rom, bootSequence, hackLevel);

#ifndef DEBUG
    const uint8_t mbc = cpu.mem->romBanks[0][0x147];
    if ((mbc > 0x9 && mbc < 0x11) || (mbc > 0x1B && mbc < 0xFF)) {
        puts("ERROR: Cartridge not supported! RTC and Rumble are not supported.");
        return;
    }
#endif

    SCOPED(Screen) screen = initScreen(cpu.mem->IO, cpu.mem->VRAM, cpu.mem->OAM, hackLevel >= 1 ? 160 : 8);
    SCOPED(Sound) *sound = initSound(cpu.mem->IO, &screen.cycles, device);
    SCOPED(Keyboard) keyb = initKeyboard();
    uint8_t skip = frameSkip;

    while (!processEvents(sound->channels, &screen.background.enabled, &sound->loudness)) {
        screen.tiles.enabled = screen.window.enabled = screen.background.enabled;
        setPalette(&screen, !sound->loudness);

        while (!nextPixels(&screen, skip == 0)) {
            nextInstructions(&cpu, screen.cycles, (FILE*)(LOG * (ptrdiff_t)stdout));
        }

        nextAudio(sound);
        skip = skip-- ? skip : frameSkip;
    }
}

int main(int argc, char *argv[]) {
    bool bootSequence = false;
    uint8_t frameSkip = 0, hackLevel = 1;
    SoundDevice device = ADLIB;
    for (uint8_t i = 1; i < argc; i++) {
        if (argv[i][0] != '/' && argv[i][0] != '-')
            continue;

        switch (argv[i][1]) {
            case 'b': bootSequence = true; break;
            case 'p': device = PC_SPEAKER; break;
            case 't': device = TANDY; break;
            case 'a': device = ADLIB; break;
            case 's': if (argv[i][2] >= '0' && argv[i][2] <= '9') frameSkip = argv[i][2] - '0';  break;
            case 'h': if (argv[i][2] >= '0' && argv[i][2] <= '9') { hackLevel = argv[i][2] - '0'; break; } FALLTHROUGH;
            case '?': FALLTHROUGH;
            case '-': puts(
                "Game Boy emulator for DOS, by Gael Cathelin (C) 2025\n\n"
                "GAMEBOY romfile [/boot] [/pcspeaker | /tandy | /adlib] [/s<n>] [/h<n>]\n\n"
                "romfile\t\tPath of the ROM to execute. Defaults to embedded Tetris game.\n"
                "\t\tOnly no-MBC, MBC1, MBC2 and MBC5 cartridges are supported.\n"
                "/boot\t\tRun the DMG-01 boot sequence.\n"
                "/pcspeaker\tUse the PC Speaker for sound.\n"
                "/tandy\t\tUse the Tandy/PCjr 3 voice system on port C0h for sound.\n"
                "/adlib\t\tUse the Adlib/Sound Blaster FM synth for sound (default).\n"
                "/s<n>\t\tSkip n frame(s) after every displayed frame (default: 0).\n"
                "/h0\t\tHack level 0. Slower and more accurate emulation. CPU and\n"
                "\t\tscreen are emulated at 8 pixels granularity, required for some\n"
                "\t\trare special effects (e.g. wobble).\n"
                "/h1 (default)\tHack level 1. CPU and screen are emulated at scanline\n"
                "\t\tgranularity. Might cause rare and harmless glitches.\n"
                "/h2\t\tHack level 2. h1 with some common wait loop patterns detection\n"
                "\t\tand replacement with more efficient emulation code. Can speed\n"
                "\t\tup some games drastically, but can cause some glitches that\n"
                "\t\tshould be mostly harmless.\n"
                "/h3\t\tHack level 3. h2 with all remaining wait loop patterns skipping\n"
                "\t\tCPU emulation until next interrupt. Glitches and crashes are\n"
                "\t\tvery likely but if it works, it should be faster.");
                return 0;
        }
    }

    emulate(argv[1], device, bootSequence, frameSkip, hackLevel);
    return 0;
}
