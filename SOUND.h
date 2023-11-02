#pragma once

#include "global.h"

#define AUDIO_FREQ  240ull
#define SUPERSAMPLE 0

typedef enum {PC_SPEAKER, TANDY, ADLIB} SoundDevice;

typedef struct {
    bool channels[4], loudness;
    uint8_t *IO, volume[4];
    uint16_t length[4], lfsr, spkrFreq;
    uint64_t t[4], lengtht[4], freqt, volt[4], noiset;
    float lowpass[2], highpass[2];
    SoundDevice device;
    volatile uint64_t samples, *pixels;
} Sound;

Sound* initSound(uint8_t *IO, uint64_t *pixels, const SoundDevice device) WARN_UNUSED_RESULT;
void deleteSound(Sound **sound);

void nextAudio(Sound *sound);
