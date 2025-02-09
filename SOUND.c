#include "sound.h"
#include <pc.h>
#include <stdlib.h>

typedef union {
    struct {uint64_t
        freqSweeps:3, freqDecr:1, freqSweepTime:3, _pad1:1,
        length:6, duty:2,
        volSweepTime:3, volIncr:1, volume:4,
        frequency:11, _pad2:3, counter:1, restart:1;
    } tone;
    struct {uint64_t
        _pad1:7, active:1,
        length:8,
        _pad2:5, volume:2, _pad3:1,
        frequency:11, _pad4:3, counter:1, restart:1;
    } wave;
    struct {uint64_t
        _pad1:8,
        length:6, _pad2:2,
        volSweepTime:3, volIncr:1, volume:4,
        lfsrDiv:3, halfLFSR:1, lfsrShift:4,
        _pad3:6, counter:1, restart:1;
    } noise;
} Channel;
typedef struct {uint8_t lo:4, hi:4;} Sample;

static void setOpl2Register(const uint8_t reg, const uint8_t val) {
    static uint8_t cache[256] = {};
    if (cache[reg] != val) {
        cache[reg] = val;
        outportb(0x388, reg); for (uint8_t i = 0; i <  6; i++) inportb(0x388);
        outportb(0x389, val); for (uint8_t i = 0; i < 35; i++) inportb(0x389);
    }
}

static void setOpl3Register(const uint8_t reg, const uint8_t val) {
    outportb(0x222, reg); for (uint8_t i = 0; i <  6; i++) inportb(0x222);
    outportb(0x223, val); for (uint8_t i = 0; i < 35; i++) inportb(0x223);
}

static void setChannelOperator0(const uint8_t channel, const uint8_t baseReg, const uint8_t val) {
    static const uint8_t map[9] = {0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x10, 0x11, 0x12};
    setOpl2Register(baseReg + map[channel], val);
}

static void setChannelOperator1(const uint8_t channel, const uint8_t baseReg, const uint8_t val) {
    static const uint8_t map[9] = {0x3, 0x4, 0x5, 0xB, 0xC, 0xD, 0x13, 0x14, 0x15};
    setOpl2Register(baseReg + map[channel], val);
}

static void setChannelOperators(const uint8_t channel, const uint8_t baseReg, const uint8_t op0Val, const uint8_t op1Val) {
    setChannelOperator0(channel, baseReg, op0Val);
    setChannelOperator1(channel, baseReg, op1Val);
}

static void play(const Sound *sound, const uint8_t channel, const uint16_t wavelength, const uint8_t volume, const uint8_t duty) {
    if (sound->device == TANDY) {
        // Tandy 1000/PCjr: ports 0xC0 to 0xC7 (https://youtu.be/rPf_FHCxF64?t=347)
        // https://www.smspower.org/Development/SN76489
        static const uint8_t volumeTable[16] = {15, 12, 9, 7, 6, 5, 4, 3, 3, 2, 2, 1, 1, 1, 0, 0};

        if (channel == 3) {
            static uint8_t oldWavelength = 3;
            if (oldWavelength != wavelength) {
                oldWavelength = wavelength;
                outportb(0xC0, 0xE4 | wavelength);
            }

            outportb(0xC0, 0x90 + 0x20 * channel + MIN(volumeTable[volume] + 3, 15));
        } else {
            uint16_t freq = 0xDA7A6 * wavelength >> (channel == 2 ? 19 : 20);
            while (freq >= 0x400) freq >>= 1;
            outportb(0xC0, 0x80 + 0x20 * channel + (freq & 0xF));
            outportb(0xC0, freq >> 4);

            outportb(0xC0, 0x90 + 0x20 * channel + volumeTable[volume]);
        }
    }

    // https://moddingwiki.shikadi.net/wiki/OPL_chip
    // https://cosmodoc.org/topics/adlib-functions/
    if (sound->device == ADLIB) {
        const uint8_t channelBase = MIN(3 * channel, 8);
        if (volume == 0 || (channel < 3 && wavelength == 0)) {
            setOpl2Register(channelBase + 0xB0, 0x00); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
            if (channel < 2) {
                setOpl2Register(channelBase + 0xB1, 0x00); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
                setOpl2Register(channelBase + 0xB2, 0x00); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
            }
        } else {
            static const uint8_t volumeTable[16] = {63, 31, 23, 19, 15, 13, 11, 9, 7, 6, 5, 4, 3, 2, 1, 0};
            const uint8_t attenuation = volumeTable[volume];
            const uint8_t stereoMask = ((sound->IO[0x25] & 0x10 << channel) ? 0x10 : 0) | ((sound->IO[0x25] & 0x01 << channel) ? 0x20 : 0);
            setOpl2Register(channelBase + 0xC0, 0x0E | stereoMask); // 0x30: stereo, 0x0E: feedback, 0x01: FM/AM

            if (channel == 3) {
                setChannelOperator1(channelBase, 0x40, MIN(attenuation + 0, 63)); // 0xC0: KSL, 0x3F: attenuation
                setOpl2Register(channelBase + 0xB0, 0x3C); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
            } else {
                uint16_t freqE = channel == 2 ? 2 : 3, freqM = (1 << 30) / (0xC23 * wavelength);
                while (freqM > 0x3FF) {freqE++; freqM >>= 1;}

                setOpl2Register(channelBase + 0xA0, freqM & 0xFF); // 0xFF: frequency LSB
                setOpl2Register(channelBase + 0xB0, 0x20 | freqE << 2 | freqM >> 8); // 0x20: active, 0x1C: octave, 0x03: frequency MSB

                if (channel < 2 && duty != 0) {
                    setOpl2Register(channelBase + 0xB2, 0x00); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
                    if (duty == 2) setOpl2Register(channelBase + 0xB1, 0x00); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
                }

                if (duty == 2 || channel == 2) {
                    setChannelOperator1(channelBase, 0x40, attenuation); // 0xC0: KSL, 0x3F: attenuation
                } else if (channel < 2) {
                    const uint8_t attenuation2 = attenuation + (duty == 0 ? 6 : 4);
                    setChannelOperator1(channelBase, 0x40, MIN(attenuation2, 63)); // 0xC0: KSL, 0x3F: attenuation
                    setChannelOperator1(channelBase + 1, 0x40, MIN(attenuation2 + 8, 63)); // 0xC0: KSL, 0x3F: attenuation
                    setOpl2Register(channelBase + 0xC1, 0x0E | stereoMask); // 0x30: stereo, 0x0E: feedback, 0x01: FM/AM
                    setOpl2Register(channelBase + 0xA1, freqM & 0xFF); // 0xFF: frequency LSB
                    setOpl2Register(channelBase + 0xB1, 0x20 | freqE << 2 | freqM >> 8); // 0x20: active, 0x1C: octave, 0x03: frequency MSB

                    if (duty == 0) {
                        const uint8_t volume3 = MIN(attenuation2 + 16, 63);
                        setChannelOperator1(channelBase + 2, 0x40, volume3); // 0xC0: KSL, 0x3F: attenuation
                        setOpl2Register(channelBase + 0xC2, 0x0E | stereoMask); // 0x30: stereo, 0x0E: feedback, 0x01: FM/AM
                        setOpl2Register(channelBase + 0xA2, freqM & 0xFF); // 0xFF: frequency LSB
                        setOpl2Register(channelBase + 0xB2, 0x20 | freqE << 2 | freqM >> 8); // 0x20: active, 0x1C: octave, 0x03: frequency MSB
                    }
                }
            }
        }
    }
}

static void setSquare(const uint8_t channel, const uint8_t freqMul) {
    setChannelOperators(channel, 0x20, 2 * freqMul, 1 * freqMul); // frequency factor
    setChannelOperator0(channel, 0x40, 0x9A); // 0xC0: KSL, 0x3F: attenuation
}

static void setBass(const uint8_t channel) {
    setChannelOperators(channel, 0x20, 1, 1); // frequency factor
    setChannelOperator0(channel, 0x40, 0x9A); // 0xC0: KSL, 0x3F: attenuation
}

static void setNoise(const uint8_t channel) {
    setChannelOperators(channel, 0x20, 0x0F, 0x00); // frequency factor
    setChannelOperator0(channel, 0x40, 0x00); // 0xC0: KSL, 0x3F: attenuation
    setOpl2Register(channel + 0xA0, 0x01); // 0xFF: frequency LSB
}

Sound* initSound(uint8_t *IO, uint64_t *pixels, const SoundDevice device) {
    Sound *sound = calloc(1, sizeof(Sound));
    sound->IO = IO;
    sound->pixels = pixels;
    sound->loudness = 0;
    sound->channels[0] = 1;
    sound->channels[1] = 1;
    sound->channels[2] = 1;
    sound->channels[3] = 1;
    sound->device = device;

    if (device == PC_SPEAKER)
        outportb(0x43, 0xB6);

    if (device == TANDY) {
        play(sound, 0, 0, 0, 0);
        play(sound, 1, 0, 0, 0);
        play(sound, 2, 0, 0, 0);
        play(sound, 3, 0, 0, 0);
    }

    if (device == ADLIB) {
        for (uint8_t r = 0; --r; )
            setOpl2Register(r, 0);

        setOpl3Register(4, 0); // 2-4 operator modes
        setOpl3Register(5, 1); // OPL3 mode (stereo)

        for (uint8_t ch = 0; ch < 9; ch++) {
            setChannelOperators(ch, 0x60, 0xF0, 0xF0); // 0xF0: attack, 0x0F: decay
            setChannelOperators(ch, 0x80, 0xFF, 0xFF); // 0xF0: sustain, 0x0F: release
        }

        setSquare(0, 1); setSquare(1, 2); setSquare(2, 4); // GB channel 0
        setSquare(3, 1); setSquare(4, 2); setSquare(5, 4); // GB channel 1
//        setSquare(6, 1); // GB channel 2
        setBass(6); // GB channel 2
        setNoise(8); // GB channel 3
    }

    return sound;
}

void deleteSound(Sound **sound) {
    if ((*sound)->device == PC_SPEAKER)
        outportb(0x61, inportb(0x61) & ~0x3);

    if ((*sound)->device == TANDY) {
        play(*sound, 0, 0, 0, 0);
        play(*sound, 1, 0, 0, 0);
        play(*sound, 2, 0, 0, 0);
        play(*sound, 3, 0, 0, 0);
    }

    if ((*sound)->device == ADLIB) {
        for (uint8_t r = 0; --r; )
            setOpl2Register(r, 0);

        setOpl3Register(5, 0);
    }

    (*sound)->samples = 0;
    free(*sound);
}

void nextAudio(Sound *sound) {
    static const uint8_t channelOrder[4] = {3, 2, 0, 1};
    uint16_t spkrFreq = 0;

    while (sound->samples * 17556 < AUDIO_FREQ / FPS * *sound->pixels) {
        for (uint8_t c = 0; c < 4; c++) {
            const uint8_t channel = channelOrder[c];
            const bool sweepvoice = channel == 0, wave = channel == 2, noise = channel == 3;
            Channel *ch = (Channel*)&sound->IO[0x10 + channel * 0x5];

            if (ch->tone.counter && sound->length[channel] == 0)
                sound->IO[0x26] &= ~(1 << channel);

            if (ch->tone.restart) {
                ch->tone.restart = 0;
                sound->length[channel] = wave ? 256 - ch->wave.length : 64 - ch->tone.length;
                sound->lengtht[channel] = 0;
                if (sweepvoice) sound->freqt = 0;
                sound->volt[channel] = 0;
                sound->IO[0x26] |= 1 << channel;
            }

            const bool active = sound->channels[channel] && sound->IO[0x26] & 0x80 && sound->IO[0x26] & 1 << channel && (!wave || ch->wave.active);
            if (!active) {
                play(sound, channel, 0, 0, 0);
                continue;
            }

            if (wave) {
//                static const uint8_t volumeTable[4] = {0x0, 0xF, 0x7, 0x3};
                static const uint8_t volumeTable[4] = {0x0, 0xC, 0x6, 0x3};
//                static const uint8_t volumeTable[4] = {0x0, 0x8, 0x4, 0x2};
                const uint16_t wavelength = 2048 - ch->tone.frequency;
                if (ch->wave.volume > 0)
                    spkrFreq = 0x1234DD * wavelength >> 16;

                play(sound, channel, wavelength, volumeTable[ch->wave.volume], 0);
            } else {
                if (noise) {
                    const uint8_t div = MIN(2, (ch->noise.lfsrDiv << 1 ? : 1) << ch->noise.lfsrShift >> 7);
                    play(sound, channel, div, ch->noise.volume, 0);
                } else {
                    uint16_t wavelength = 2048 - ch->tone.frequency;

                    if (ch->tone.volume > 1)
                        spkrFreq = 0x1234DD * wavelength >> 17;

                    play(sound, channel, wavelength, ch->tone.volume, ch->tone.duty);

                    if (sweepvoice) {
                        sound->freqt += 1 << 7;
                        while (ch->tone.freqSweeps > 0 && ch->tone.freqSweepTime > 0 && sound->freqt >= AUDIO_FREQ * ch->tone.freqSweepTime) {
                            uint16_t dFreq = ch->tone.frequency >> ch->tone.freqSweeps;
                            uint16_t newFreq = MAX(0, ch->tone.frequency + (ch->tone.freqDecr ? -dFreq : dFreq));
                            if (newFreq > 2047) {
                                sound->IO[0x26] &= ~0x1;
                            } else {
                                ch->tone.frequency = newFreq;
                                wavelength = 2048 - newFreq;
                            }

                            dFreq = newFreq >> ch->tone.freqSweeps;
                            newFreq += ch->tone.freqDecr ? -dFreq : dFreq;
                            if (newFreq > 2047)
                                sound->IO[0x26] &= ~0x1;

                            sound->freqt -= AUDIO_FREQ * ch->tone.freqSweepTime;
                        }
                    }
                }

                if (ch->tone.volSweepTime > 0) sound->volt[channel] += 1 << 6;
                while (ch->tone.volSweepTime > 0 && sound->volt[channel] >= AUDIO_FREQ * ch->tone.volSweepTime) {
                    ch->tone.volume = MIN(0xF, MAX(0, ch->tone.volume + ch->tone.volIncr * 2 - 1));
                    sound->volt[channel] -= AUDIO_FREQ * ch->tone.volSweepTime;
                }
            }

            sound->lengtht[channel] += 1 << 8;
            while (sound->lengtht[channel] >= AUDIO_FREQ) {
                sound->length[channel]--;
                sound->lengtht[channel] -= AUDIO_FREQ;
            }
        }

        sound->samples++;
    }

    if (sound->device == PC_SPEAKER && sound->spkrFreq != spkrFreq) {
        outportb(0x42, spkrFreq);
        outportb(0x42, spkrFreq >> 8);

        if      (spkrFreq        == 0) outportb(0x61, inportb(0x61) & ~0x3);
        else if (sound->spkrFreq == 0) outportb(0x61, inportb(0x61) |  0x3);

        sound->spkrFreq = spkrFreq;
    }
}
