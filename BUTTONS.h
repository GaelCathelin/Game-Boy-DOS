#pragma once

#include "global.h"

typedef struct {
} Keyboard;

Keyboard VERBATIM initKeyboard();
void VERBATIM deleteKeyboard(Keyboard *keyb);

bool processEvents(bool channels[4], bool *bgViewer, bool *loudness);
uint8_t updateInputReg(const uint8_t value);
