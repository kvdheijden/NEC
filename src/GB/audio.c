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

#include "audio.h"

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

static uint8_t _nr10;
static uint8_t _nr11;
static uint8_t _nr12;
static uint8_t _nr13;
static uint8_t _nr14;
static uint8_t _nr21;
static uint8_t _nr22;
static uint8_t _nr23;
static uint8_t _nr24;
static uint8_t _nr30;
static uint8_t _nr31;
static uint8_t _nr32;
static uint8_t _nr33;
static uint8_t _nr34;
static uint8_t _nr41;
static uint8_t _nr42;
static uint8_t _nr43;
static uint8_t _nr44;
static uint8_t _nr50;
static uint8_t _nr51;
static uint8_t _nr52;
static uint8_t _wave_pattern_ram[_WAVE_PATTERN_RAM_SIZE];

uint8_t audio_read_byte(uint16_t address)
{
    if(_WAVE_PATTERN_RAM_OFFSET <= address && address < _WAVE_PATTERN_RAM_OFFSET_END) {
        return _wave_pattern_ram[address - _WAVE_PATTERN_RAM_OFFSET];
    } else if (address == NR10_ADDRESS) {
        return _nr10;
    } else if (address == NR11_ADDRESS) {
        return _nr11;
    } else if (address == NR12_ADDRESS) {
        return _nr12;
    } else if (address == NR13_ADDRESS) {
        return _nr13;
    } else if (address == NR14_ADDRESS) {
        return _nr14;
    } else if (address == NR21_ADDRESS) {
        return _nr21;
    } else if (address == NR22_ADDRESS) {
        return _nr22;
    } else if (address == NR23_ADDRESS) {
        return _nr23;
    } else if (address == NR24_ADDRESS) {
        return _nr24;
    } else if (address == NR30_ADDRESS) {
        return _nr30;
    } else if (address == NR31_ADDRESS) {
        return _nr31;
    } else if (address == NR32_ADDRESS) {
        return _nr32;
    } else if (address == NR33_ADDRESS) {
        return _nr33;
    } else if (address == NR34_ADDRESS) {
        return _nr34;
    } else if (address == NR41_ADDRESS) {
        return _nr41;
    } else if (address == NR42_ADDRESS) {
        return _nr42;
    } else if (address == NR43_ADDRESS) {
        return _nr43;
    } else if (address == NR44_ADDRESS) {
        return _nr44;
    } else if (address == NR50_ADDRESS) {
        return _nr50;
    } else if (address == NR51_ADDRESS) {
        return _nr51;
    } else if (address == NR52_ADDRESS) {
        return _nr52;
    }

    return 0xFF;
}

void audio_write_byte(uint16_t address, uint8_t value)
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
    } else if (address == NR21_ADDRESS) {
        _nr21 = value;
    } else if (address == NR22_ADDRESS) {
        _nr22 = value;
    } else if (address == NR23_ADDRESS) {
        _nr23 = value;
    } else if (address == NR24_ADDRESS) {
        _nr24 = value;
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
    } else if (address == NR41_ADDRESS) {
        _nr41 = value;
    } else if (address == NR42_ADDRESS) {
        _nr42 = value;
    } else if (address == NR43_ADDRESS) {
        _nr43 = value;
    } else if (address == NR44_ADDRESS) {
        _nr44 = value;
    } else if (address == NR50_ADDRESS) {
        _nr50 = value;
    } else if (address == NR51_ADDRESS) {
        _nr51 = value;
    } else if (address == NR52_ADDRESS) {
        _nr52 = value;
    }
}

void audio_update(uint8_t clk_tics)
{

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
}