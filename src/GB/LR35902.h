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

#ifndef NEC_CPU_H
#define NEC_CPU_H

#include "config.h"
#include <stdint.h>

extern struct registers {
    union {
        struct {
#if IS_BIG_ENDIAN
            uint8_t a;
            uint8_t f;
#else
            uint8_t f;
            uint8_t a;
#endif
        };
        uint16_t af;
    };

    union {
        struct {
#if IS_BIG_ENDIAN
            uint8_t b;
            uint8_t c;
#else
            uint8_t c;
            uint8_t b;
#endif
        };
        uint16_t bc;
    };

    union {
        struct {
#if IS_BIG_ENDIAN
            uint8_t d;
            uint8_t e;
#else
            uint8_t e;
            uint8_t d;
#endif
        };
        uint16_t de;
    };

    union {
        struct {
#if IS_BIG_ENDIAN
            uint8_t h;
            uint8_t l;
#else
            uint8_t l;
            uint8_t h;
#endif
        };
        uint16_t hl;
    };

    uint16_t sp;
    uint16_t pc;
    uint64_t clk;
} _r;

extern uint8_t _IE;
extern uint8_t _IF;

/**
 * Possible interrupt sources identified by a bit mask.
 */
enum int_src {
    VBLANK = 0x01,
    LCDC = 0x02,
    TIMER_OVERFLOW = 0x04,
    SERIAL_TRANSFER = 0x08,
    BUTTON_PRESSED = 0x10
};

/**
 * Generate an interrupt with specified source.
 *
 * @param src The source of the interrupt.
 */
void interrupt(enum int_src src);

/**
 *
 */
void dispatch(void);

/**
 *
 */
void cpu_reset(void);

#endif //NEC_CPU_H
