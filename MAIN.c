#include "cpu.h"
#include "screen.h"
#include "sound.h"
#include "buttons.h"
#include <stddef.h>

#define LOG 0

static void emulate(const char *rom, const SoundDevice device, const bool bootSequence, const uint8_t frameSkip, const uint8_t hackLevel) {
    SCOPED(CPU) cpu;
#if defined(DEBUG) || defined(TEST)
    cpu = initCPU(rom, BOOT, hackLevel);
#else
    cpu = initCPU(rom, bootSequence, hackLevel);

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
/*
            #ifndef TURBO
                while (AUDIO_FREQ / (4 * FPS) * screen.cycles > (SCREEN_CLKS / 4) * sound->samples);
            #endif
//*/
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

//        if (argv[i][1] == 'b') bootSequence = true;
        if (argv[i][1] == 'h' && argv[i][2] >= '0' && argv[i][2] <= '9') {hackLevel = argv[i][2] - '0'; continue;}
        if (argv[i][1] == 's' && argv[i][2] >= '0' && argv[i][2] <= '9') frameSkip = argv[i][2] - '0';
        if (argv[i][1] == 'p') device = PC_SPEAKER;
        if (argv[i][1] == 't') device = TANDY;
        if (argv[i][1] == 'a') device = ADLIB;

        if (argv[i][1] == '?' || argv[i][1] == '-' || argv[i][1] == 'h') {
            puts("Game Boy emulator for DOS, by Gael Cathelin (C) 2023\n");
            puts("GAMEBOY romfile [/pcspeaker | /tandy | /adlib] [/sn] [/hn]\n");
            puts("romfile\t\tPath of the ROM to execute. Defaults to embedded Tetris game.");
            puts("\t\tOnly no-MBC, MBC1, MBC2 and MBC5 cartridges are supported.");
//            puts("/boot\t\tRun the DMG-01 boot sequence.");
            puts("/pcspeaker\tUse the PC Speaker for sound.");
            puts("/tandy\t\tUse the Tandy/PCjr 3 voice system on port C0h for sound.");
            puts("/adlib\t\tUse the Adlib/Sound Blaster FM synth for sound (default).");
            puts("/sn\t\tSkip n frame(s) after every displayed frame (default: 0).");
            puts("/h0\t\tHack level 0. Slower and more accurate emulation. CPU and");
            puts("\t\tscreen are emulated at 8 pixels granularity, required for some");
            puts("\t\trare special effects (e.g. wobble).");
            puts("/h1 (default)\tHack level 1. CPU and screen are emulated at scanline");
            puts("\t\tgranularity. Might cause rare and harmless glitches.");
            puts("/h2\t\tHack level 2. h1 with some common wait loop patterns detection");
            puts("\t\tand replacement with more efficient emulation code. Can speed");
            puts("\t\tup some games drastically, but can cause some glitches that");
            puts("\t\tshould be mostly harmless.");
            puts("/h3\t\tHack level 3. h2 with all remaining wait loop patterns skipping");
            puts("\t\tCPU emulation until next interrupt. Glitches and crashes are");
            puts("\t\tvery likely but if it works, it should be faster.");
            return 0;
        }
    }

#ifdef DEBUG
//    static char logBuff[0x400]; setvbuf(stdout, logBuff, _IOFBF, sizeof(logBuff));

//    emulate("../roms/tests/dmg-acid2.gb"                     , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/cpu_instrs.gb"                , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/instr_timing.gb"              , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/mem_timing.gb"                , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/interrupt_time.gb"            , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/cpu/halt_bug.gb"                  , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/01-registers.gb"            , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/02-len ctr.gb"              , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/03-trigger.gb"              , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/04-sweep.gb"                , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/05-sweep details.gb"        , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/06-overflow on trigger.gb"  , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/07-len sweep period sync.gb", device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/08-len ctr during power.gb" , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/09-wave read while on.gb"   , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/10-wave trigger while on.gb", device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/11-regs after power.gb"     , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/12-wave write while on.gb"  , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/tests/sound/dmg_sound.gb"               , device, bootSequence, frameSkip, hackLevel); // Failed
//    emulate("../roms/Dr Mario.gb"            , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Tetris.gb"              , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Super Mario Land.gb"    , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Super Mario Land 2.gb"  , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Wario Land.gb"          , device, bootSequence, frameSkip, hackLevel);
    emulate("../roms/Link's Awakening.gb"    , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Donkey Kong Land.gb"    , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Prehistorik Man.gb"     , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/20y.gb"                 , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/oh.gb"                  , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/gejmbaj.gb"             , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/pocket.gb"              , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/demo3d.gb"              , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Kirby's Pinball Land.gb", device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Wario Land 2.gb"        , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tomoni.gb"              , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Pokemon Jaune.gb"       , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Wario Land 2.gbc"       , device, bootSequence, frameSkip, hackLevel);
#elif defined(TEST)
//    emulate("../roms/tests/cpu/cpu_instrs.gb"  , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/instr_timing.gb", device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/tests/cpu/mem_timing.gb"  , device, bootSequence, frameSkip, hackLevel);
//    emulate("../roms/Link's Awakening.gb"      , device, bootSequence, frameSkip, hackLevel);
#else
    emulate(argv[1], device, bootSequence, frameSkip, hackLevel);
#endif

    return 0;
}
