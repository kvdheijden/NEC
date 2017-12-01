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

#include "serial.h"

#include "GB.h"
#include "LR35902.h"

#define SB  0xFF01
#define SC  0xFF02

static uint8_t _sb = 0x00;
static uint8_t _sc = 0x00;

uint8_t serial_read_byte(uint16_t address)
{
    switch(address) {
        case SC:
            return _sc;
        case SB:
            if(!(_sc & 0x80)) {
                return _sb;
            }
        default:
            return 0xFF;
    }
}

void serial_write_byte(uint16_t address, uint8_t value)
{
    switch (address) {
        case SC:
            _sc = (uint8_t) (value & 0x83);

            if(_sc & 0x80) {
                serial_transfer_initiate(_sb);
            }
            break;
        case SB:
            if((_sc & 0x80)) {
                return;
            }
            _sb = value;
            break;
        default:
            break;
    }
}

void serial_transfer_complete(uint8_t data)
{
    _sb = data;
    _sc &= 0x7F;
    interrupt(SERIAL_TRANSFER);
}

void serial_reset(void)
{
    _sb = 0x00;
    _sc = 0x00;
}