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

#include "joypad.h"
#include "GB.h"

#include "LR35902.h"

#define _OUTPUTS_MASK   0x30
#define _INPUTS_MASK    0x0F

static uint8_t _keys = 0xFF;
static uint8_t _mask = 0x00;

void key_pressed(enum GB_key key)
{
    _keys &= ~key;
    interrupt(BUTTON_PRESSED);
}

void key_released(enum GB_key key)
{
    _keys |= key;
}

uint8_t joypad_read_byte(uint16_t address)
{
    switch (address) {
        case P1_OFFSET:
            switch(_mask) {
                default:
                case 0:
                    return 0;
                case 1:
                    return (uint8_t) ((_keys >> 4) & _INPUTS_MASK);
                case 2:
                    return (uint8_t) (_keys & _INPUTS_MASK);
                case 3:
                    return (uint8_t) (((_keys >> 4) & _INPUTS_MASK) | (_keys & _INPUTS_MASK));
            }
        default:
            return 0xFF;
    }
}

void joypad_write_byte(uint16_t address, uint8_t value)
{
    switch (address) {
        case P1_OFFSET:
            _mask = (uint8_t) ((value & _OUTPUTS_MASK) >> 4);
            break;
        default:
            break;
    }
}

void joypad_reset(void)
{
    _keys = 0xFF;
    _mask = 0x00;
}