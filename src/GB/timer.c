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

#include "timer.h"

#include "LR35902.h"

#define DIV         0xFF04
#define TIMA        0xFF05
#define TMA         0xFF06
#define TAC         0xFF07

#define _TIMER_CLK_MAX      4194304
#define _4096HZ_DIV         1024
#define _16384HZ_DIV        256
#define _65536HZ_DIV        64
#define _262144HZ_DIV       16

static uint8_t _div = 0x00;
static uint8_t _tima = 0x00;
static uint8_t _tma = 0x00;
static uint8_t _tac = 0x00;

static uint32_t _timer_clk = 0; // 4,194,304Hz

uint8_t timer_read_byte(uint16_t address)
{
    switch (address) {
        case DIV:
            return _div;
        case TIMA:
            return _tima;
        case TMA:
            return _tma;
        case TAC:
            return _tac;
        default:
            return 0xFF;
    }
}

void timer_write_byte(uint16_t address, uint8_t value) {
    switch (address) {
        case DIV:
            _div = 0x00;
        case TIMA:
            _tima = value;
        case TMA:
            _tma = value;
        case TAC:
            _tac = (uint8_t) (value & 0x07);
        default:
            break;
    }
}

static inline void timer_step(void)
{
    switch (_tima) {
        case 0xFF:
            _tima = _tma;
            interrupt(TIMER_OVERFLOW);
            break;
        default:
            _tima++;
    }
}

void timer_update(uint8_t clk_tics)
{
    // Save current counter/clock
    uint32_t _old_timer_clk = _timer_clk;

    // Increment counter/clock
    _timer_clk += clk_tics;

    // DIV update
    _div += ((_timer_clk / _16384HZ_DIV) - (_old_timer_clk / _16384HZ_DIV));

    // TIMA update
    int steps = 0;
    if( _tac & 0x04 ) {
        switch( _tac & 0x03 ) {
            case 0x00:
                steps = ((_timer_clk / _4096HZ_DIV) - (_old_timer_clk / _4096HZ_DIV));
                break;
            case 0x01:
                steps = ((_timer_clk / _262144HZ_DIV) - (_old_timer_clk / _262144HZ_DIV));
                break;
            case 0x02:
                steps = ((_timer_clk / _65536HZ_DIV) - (_old_timer_clk / _65536HZ_DIV));
                break;
            case 0x03:
                steps = ((_timer_clk / _16384HZ_DIV) - (_old_timer_clk / _16384HZ_DIV));
                break;
            default:
                break;
        }

        for(int i = 0; i < steps; i++) {
            timer_step();
        }
    }

    // Modulo the counter/clock
    _timer_clk %= _TIMER_CLK_MAX;
}

void timer_reset(void)
{
    _div = 0x00;
    _tima = 0x00;
    _tma = 0x00;
    _tac = 0x00;

    _timer_clk = 0;
}