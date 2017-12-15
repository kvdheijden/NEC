/* MIT License
 *
 * Copyright (c) 2017 Koen van der Heijden.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "sound.h"

#include <stdbool.h>
#include <math.h>

#include "audio.h"
#include "cartridge.h"
#include "LR35902.h"
#include "GB.h"

#define NR10_ADDRESS    0xFF10
#define NR11_ADDRESS    0xFF11
#define NR12_ADDRESS    0xFF12
#define NR13_ADDRESS    0xFF13
#define NR14_ADDRESS    0xFF14
#define NR21_ADDRESS    0xFF16
#define NR22_ADDRESS    0xFF17
#define NR23_ADDRESS    0xFF18
#define NR24_ADDRESS    0xFF19
#define NR30_ADDRESS    0xFF1A
#define NR31_ADDRESS    0xFF1B
#define NR32_ADDRESS    0xFF1C
#define NR33_ADDRESS    0xFF1D
#define NR34_ADDRESS    0xFF1E
#define NR41_ADDRESS    0xFF20
#define NR42_ADDRESS    0xFF21
#define NR43_ADDRESS    0xFF22
#define NR44_ADDRESS    0xFF23
#define NR50_ADDRESS    0xFF24
#define NR51_ADDRESS    0xFF25
#define NR52_ADDRESS    0xFF26
#define _WAVE_PATTERN_RAM_OFFSET        0xFF30
#define _WAVE_PATTERN_RAM_OFFSET_END    0xFF40
#define _WAVE_PATTERN_RAM_SIZE          (_WAVE_PATTERN_RAM_OFFSET_END - _WAVE_PATTERN_RAM_OFFSET)

#define _512HZ_DIV      8192
#define _131072_DIV     32

static uint8_t _nr10 = 0x00;
static uint8_t _nr11 = 0x00;
static uint8_t _nr12 = 0x00;
static uint8_t _nr13 = 0x00;
static uint8_t _nr14 = 0x00;
static uint8_t _nr21 = 0x00;
static uint8_t _nr22 = 0x00;
static uint8_t _nr23 = 0x00;
static uint8_t _nr24 = 0x00;
static uint8_t _nr30 = 0x00;
static uint8_t _nr31 = 0x00;
static uint8_t _nr32 = 0x00;
static uint8_t _nr33 = 0x00;
static uint8_t _nr34 = 0x00;
static uint8_t _nr41 = 0x00;
static uint8_t _nr42 = 0x00;
static uint8_t _nr43 = 0x00;
static uint8_t _nr44 = 0x00;
static uint8_t _nr50 = 0x00;
static uint8_t _nr51 = 0x00;
static uint8_t _nr52 = 0x00;

static struct sound _sound;

static uint8_t _wave_pattern_ram[_WAVE_PATTERN_RAM_SIZE];

uint8_t sound_read_byte(uint16_t address)
{
    if(_WAVE_PATTERN_RAM_OFFSET <= address && address < _WAVE_PATTERN_RAM_OFFSET_END) {
        return _wave_pattern_ram[address - _WAVE_PATTERN_RAM_OFFSET];
    } else if (address == NR10_ADDRESS) {
        return (uint8_t) (_nr10 | 0x80);
    } else if (address == NR11_ADDRESS) {
        return (uint8_t) (_nr11 | 0x3F);
    } else if (address == NR12_ADDRESS) {
        return _nr12;
    } else if (address == NR13_ADDRESS) {
        return 0xFF;
    } else if (address == NR14_ADDRESS) {
        return (uint8_t) (_nr14 | 0xBF);
    } else if (address == NR21_ADDRESS) {
        return (uint8_t) (_nr21 | 0x3F);
    } else if (address == NR22_ADDRESS) {
        return _nr22;
    } else if (address == NR23_ADDRESS) {
        return 0xFF;
    } else if (address == NR24_ADDRESS) {
        return (uint8_t) (_nr24 | 0xBF);
    } else if (address == NR30_ADDRESS) {
        return (uint8_t) (_nr30 | 0x7F);
    } else if (address == NR31_ADDRESS) {
        return 0xFF;
    } else if (address == NR32_ADDRESS) {
        return (uint8_t) (_nr32 | 0x9F);
    } else if (address == NR33_ADDRESS) {
        return 0xFF;
    } else if (address == NR34_ADDRESS) {
        return (uint8_t) (_nr34 | 0xBF);
    } else if (address == NR41_ADDRESS) {
        return 0xFF;
    } else if (address == NR42_ADDRESS) {
        return _nr42;
    } else if (address == NR43_ADDRESS) {
        return _nr43;
    } else if (address == NR44_ADDRESS) {
        return (uint8_t) (_nr44 | 0xBF);
    } else if (address == NR50_ADDRESS) {
        return _nr50;
    } else if (address == NR51_ADDRESS) {
        return _nr51;
    } else if (address == NR52_ADDRESS) {
        return (uint8_t) (_nr52 | 0x70);
    }

    return 0xFF;
}

void sound_write_byte(uint16_t address, uint8_t value)
{
    if(_WAVE_PATTERN_RAM_OFFSET <= address && address < _WAVE_PATTERN_RAM_OFFSET_END) {
        _wave_pattern_ram[address - _WAVE_PATTERN_RAM_OFFSET] = value;
    } else if (address == NR10_ADDRESS) {
        _nr10 = value;
    } else if (address == NR11_ADDRESS) {
        _nr11 = value;
    } else if (address == NR12_ADDRESS) {
        _nr12 = value;
    } else if (address == NR13_ADDRESS) {
        _nr13 = value;
    } else if (address == NR14_ADDRESS) {
        _nr14 = value;
        if(value & 0x80) {
            _nr52 |= 0x01;
        } else {
            _nr52 &= 0xFE;
        }
    } else if (address == NR21_ADDRESS) {
        _nr21 = value;
    } else if (address == NR22_ADDRESS) {
        _nr22 = value;
    } else if (address == NR23_ADDRESS) {
        _nr23 = value;
    } else if (address == NR24_ADDRESS) {
        _nr24 = value;
        if(value & 0x80) {
            _nr52 |= 0x02;
        } else {
            _nr52 &= 0xFD;
        }
    } else if (address == NR30_ADDRESS) {
        _nr30 = value;
    } else if (address == NR31_ADDRESS) {
        _nr31 = value;
    } else if (address == NR32_ADDRESS) {
        _nr32 = value;
    } else if (address == NR33_ADDRESS) {
        _nr33 = value;
    } else if (address == NR34_ADDRESS) {
        _nr34 = value;
        if(value & 0x80) {
            _nr52 |= 0x04;
        } else {
            _nr52 &= 0xFB;
        }
    } else if (address == NR41_ADDRESS) {
        _nr41 = value;
    } else if (address == NR42_ADDRESS) {
        _nr42 = value;
    } else if (address == NR43_ADDRESS) {
        _nr43 = value;
    } else if (address == NR44_ADDRESS) {
        _nr44 = value;
        if(value & 0x80) {
            _nr52 |= 0x08;
        } else {
            _nr52 &= 0xF7;
        }
    }else if (address == NR50_ADDRESS) {
        _nr50 = value;
    } else if (address == NR51_ADDRESS) {
        _nr51 = value;
    } else if (address == NR52_ADDRESS) {
        _nr52 = (uint8_t) ((value & 0x80) | (_nr52 & 0x0F));
        if(value & 0x80) {
            audio_enable();
        } else {
            audio_disable();
        }
    }
}

static inline int8_t mix(int8_t a, int8_t b)
{
    return a + b - (a * b);
}

static uint32_t _timer_clk = 0;

void audio_update(uint8_t clk_tics)
{
    // Save current counter/clock
    uint32_t _old_timer_clk = _timer_clk;

    // Increment counter/clock
    _timer_clk += clk_tics;

    int8_t data[4] = {
            0,
            0,
            0,
            0
    };

    _sound.S01 = 0;
    _sound.S02 = 0;

    if(_nr52 & 0x80) {


        // Mixing step
        if(_nr50 & 0x08) {
            // Output Vin to SO1
            _sound.S01 = mix(_sound.S01, (int8_t) (get_vin() * (_nr50 & 0x07) * (INT8_MAX / 7)));
        }
        if(_nr50 & 0x80) {
            // Output Vin to SO2
            _sound.S02 = mix(_sound.S02, (int8_t) (get_vin() * ((_nr50 >> 4) & 0x07) * (INT8_MAX / 7)));
        }
        if(_nr51 & 0x01) {
            // Output sound 1 to SO1
            _sound.S01 = mix(_sound.S01, data[0]);
        }
        if(_nr51 & 0x02) {
            // Output sound 2 to SO1
            _sound.S01 = mix(_sound.S01, data[1]);
        }
        if(_nr51 & 0x04) {
            // Output sound 3 to SO1
            _sound.S01 = mix(_sound.S01, data[2]);
        }
        if(_nr51 & 0x08) {
            // Output sound 4 to SO1
            _sound.S01 = mix(_sound.S01, data[3]);
        }
        if(_nr51 & 0x10) {
            // Output sound 1 to SO2
            _sound.S02 = mix(_sound.S02, data[0]);
        }
        if(_nr51 & 0x20) {
            // Output sound 2 to SO2
            _sound.S02 = mix(_sound.S02, data[1]);
        }
        if(_nr51 & 0x40) {
            // Output sound 3 to SO2
            _sound.S02 = mix(_sound.S02, data[2]);
        }
        if(_nr51 & 0x80) {
            // Output sound 4 to SO2
            _sound.S02 = mix(_sound.S02, data[3]);
        }
    }

    audio_play(&_sound);

    _timer_clk %= _512HZ_DIV;
}

void audio_reset(void)
{
    _nr10 = 0x00;
    _nr11 = 0x00;
    _nr12 = 0x00;
    _nr13 = 0x00;
    _nr14 = 0x00;
    _nr21 = 0x00;
    _nr22 = 0x00;
    _nr23 = 0x00;
    _nr24 = 0x00;
    _nr30 = 0x00;
    _nr31 = 0x00;
    _nr32 = 0x00;
    _nr33 = 0x00;
    _nr34 = 0x00;
    _nr41 = 0x00;
    _nr42 = 0x00;
    _nr43 = 0x00;
    _nr44 = 0x00;
    _nr50 = 0x00;
    _nr51 = 0x00;
    _nr52 = 0x00;

    for(int i = 0; i < _WAVE_PATTERN_RAM_SIZE; i++) {
        _wave_pattern_ram[i] = 0;
    }

    _timer_clk = 0;
}