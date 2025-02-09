#include "buttons.h"
#include <bios.h>
#include <dpmi.h>
#include <go32.h>

// https://stackoverflow.com/questions/40961527/checking-if-a-key-is-down-in-ms-dos-c-c
// https://www.delorie.com/djgpp/doc/ug/interrupts/inthandlers2.html
// https://soulsphere.org/random/old-dos-code/docs/iea292.txt
// http://www.ctyme.com/intr/rb-0045.htm

static bool keyPressed[0x60] = {};

static VERBATIM void keybHandler() {
    const uint8_t keyCode = inportb(0x60);
    if (keyCode < 0xE0)
        keyPressed[keyCode & 0x7F] = (keyCode & 0x80) == 0;

    outportb(0x20, 0x20);
}

static _go32_dpmi_seginfo origHandler, myHandler;

Keyboard initKeyboard() {
    _go32_dpmi_get_protected_mode_interrupt_vector(0x9, &origHandler);
    _go32_dpmi_lock_code(keybHandler, (size_t)initKeyboard - (size_t)keybHandler);
    _go32_dpmi_lock_data(keyPressed, sizeof(keyPressed));

    myHandler.pm_offset = (size_t)keybHandler;
    myHandler.pm_selector = _go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&myHandler);
    _go32_dpmi_set_protected_mode_interrupt_vector(0x9, &myHandler);

    return (Keyboard){};
}

void deleteKeyboard(Keyboard *keyb) {
    UNUSED(keyb);
    _go32_dpmi_set_protected_mode_interrupt_vector(0x9, &origHandler);
    _go32_dpmi_free_iret_wrapper(&myHandler);
}

static uint8_t buttonPressedMasks[2] = {0x3F, 0x3F};

bool processEvents(bool channels[4], bool *bgViewer, bool *loudness) {
    buttonPressedMasks[0] = 0x30 |
        (keyPressed[0x2D] ? 0 : 0x1) | // A (X)
        (keyPressed[0x2E] ? 0 : 0x2) | // B (C)
        (keyPressed[0x39] ? 0 : 0x4) | // Select (Space)
        (keyPressed[0x36] ? 0 : 0x8);  // Start (Right Shift)
    buttonPressedMasks[1] = 0x30 |
        (keyPressed[0x4D] ? 0 : 0x1) | // Right
        (keyPressed[0x4B] ? 0 : 0x2) | // Left
        (keyPressed[0x48] ? 0 : 0x4) | // Up
        (keyPressed[0x50] ? 0 : 0x8);  // Down

    for (uint8_t i = 0; i < 4; i++) {
        if (keyPressed[0x3B + i]) channels[i] = true;
        if (keyPressed[0x3F + i]) channels[i] = false;
    }

    if (keyPressed[0x43]) {
        keyPressed[0x43] = false;
        *loudness = !*loudness;
    }

    if (keyPressed[0x44]) {
        keyPressed[0x44] = false;
        *bgViewer = !*bgViewer;
    }

    return keyPressed[0x01];
}

uint8_t updateInputReg(const uint8_t value) {
    switch (value & 0x30) {
        case 0x10: return buttonPressedMasks[0];
        case 0x20: return buttonPressedMasks[1];
        default: return 0x3F;
    }
}
