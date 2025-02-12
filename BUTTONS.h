#pragma once

#include "global.h"

typedef struct {
} Keyboard;

Keyboard __attribute__((no_reorder)) initKeyboard();
void deleteKeyboard(Keyboard *keyb);

bool processEvents(bool channels[4], bool *bgViewer, bool *loudness);
uint8_t updateInputReg(const uint8_t value);
