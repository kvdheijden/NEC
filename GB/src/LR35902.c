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

#include "LR35902.h"

#include "MMU.h"

#define ZERO(condition)             ( condition ? (_cpu._r.f |= 0x80 ) : (_cpu._r.f &=~ 0x80) )
#define IS_ZERO                     ( _cpu._r.f & 0x80 )
#define NEGATIVE(condition)         ( condition ? (_cpu._r.f |= 0x40 ) : (_cpu._r.f &=~ 0x40) )
#define IS_NEGATIVE                 ( _cpu._r.f & 0x40 )
#define HALF_CARRY(condition)       ( condition ? (_cpu._r.f |= 0x20 ) : (_cpu._r.f &=~ 0x20) )
#define IS_HALF_CARRY               ( _cpu._r.f & 0x20 )
#define CARRY(condition)            ( condition ? (_cpu._r.f |= 0x10 ) : (_cpu._r.f &=~ 0x10) )
#define IS_CARRY                    ( _cpu._r.f & 0x10 )

typedef void (*t_op)(void);

struct registers {
    uint8_t a, b, c, d, e, h, l, f;
    uint16_t pc, sp;
};

struct clock {
    unsigned long int m, t;
};

struct Z80 {
    struct clock _clock;
    struct registers _r;
};

static struct Z80 _cpu;

void reset_cpu(void)
{
    _cpu._r.a = 0;
    _cpu._r.b = 0;
    _cpu._r.d = 0;
    _cpu._r.e = 0;
    _cpu._r.f = 0;
    _cpu._r.h = 0;
    _cpu._r.l = 0;
    _cpu._r.f = 0;

    _cpu._r.pc = 0;
    _cpu._r.sp = 0;

    _cpu._clock.m = 0;
    _cpu._clock.t = 0;
}

static void RLC_B(void)
{
    unsigned int b = _cpu._r.b << 1;
    ZERO(!b);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(b > 0xFF);

    if( IS_CARRY ) {
        b |= 1;
    }

    _cpu._r.b = (uint8_t)b;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_C(void)
{
    unsigned int c = _cpu._r.c << 1;
    ZERO(!c);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(c > 0xFF);

    if( IS_CARRY ) {
        c |= 1;
    }

    _cpu._r.c = (uint8_t)c;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_D(void)
{
    unsigned int d = _cpu._r.d << 1;
    ZERO(!d);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(d > 0xFF);

    if( IS_CARRY ) {
        d |= 1;
    }

    _cpu._r.d = (uint8_t)d;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_E(void)
{
    unsigned int e = _cpu._r.e << 1;
    ZERO(!e);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(e > 0xFF);

    if( IS_CARRY ) {
        e |= 1;
    }

    _cpu._r.e = (uint8_t)e;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_H(void)
{
    unsigned int h = _cpu._r.h << 1;
    ZERO(!h);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(h > 0xFF);

    if( IS_CARRY ) {
        h |= 1;
    }

    _cpu._r.h = (uint8_t)h;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_L(void)
{
    unsigned int l = _cpu._r.l << 1;
    ZERO(!l);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(l > 0xFF);

    if( IS_CARRY ) {
        l |= 1;
    }

    _cpu._r.l = (uint8_t)l;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RLC_mHL(void)
{
    const uint16_t hl = (_cpu._r.h << 8) + _cpu._r.l;
    unsigned int mHL = readbyte( hl ) << 1;
    ZERO(!mHL);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(mHL > 0xFF);

    if( IS_CARRY ) {
        mHL |= 1;
    }

    writebyte( hl, (uint8_t)mHL );
    _cpu._clock.m += 4;
    _cpu._clock.t += 16;
}

static void RLC_A(void)
{
    unsigned int a = _cpu._r.a << 1;
    ZERO(!a);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(a > 0xFF);

    if( IS_CARRY ) {
        a |= 1;
    }

    _cpu._r.a = (uint8_t)a;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_B(void)
{
    unsigned int b = _cpu._r.b >> 1;
    ZERO(!b);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.b & 1);

    if( IS_CARRY ) {
        b |= 0x80;
    }

    _cpu._r.b = (uint8_t)b;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_C(void)
{
    unsigned int c = _cpu._r.c >> 1;
    ZERO(!c);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.c & 1);

    if( IS_CARRY ) {
        c |= 0x80;
    }

    _cpu._r.c = (uint8_t)c;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_D(void)
{
    unsigned int d = _cpu._r.d >> 1;
    ZERO(!d);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.d & 1);

    if( IS_CARRY ) {
        d |= 0x80;
    }

    _cpu._r.d = (uint8_t)d;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_E(void)
{
    unsigned int e = _cpu._r.e >> 1;
    ZERO(!e);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.e & 1);

    if( IS_CARRY ) {
        e |= 0x80;
    }

    _cpu._r.e = (uint8_t)e;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_H(void)
{
    unsigned int h = _cpu._r.h >> 1;
    ZERO(!h);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.h & 1);

    if( IS_CARRY ) {
        h |= 0x80;
    }

    _cpu._r.h = (uint8_t)h;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_L(void)
{
    unsigned int l = _cpu._r.l >> 1;
    ZERO(!l);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.l & 1);

    if( IS_CARRY ) {
        l |= 0x80;
    }

    _cpu._r.l = (uint8_t)l;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RRC_mHL(void)
{
    const uint16_t hl = (_cpu._r.h << 8) + _cpu._r.l;
    unsigned int mHL = readbyte( hl ) >> 1;
    ZERO(!mHL);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(readbyte( hl ) & 1);

    if( IS_CARRY ) {
        mHL |= 0x80;
    }

    writebyte( hl, (uint8_t)mHL );
    _cpu._clock.m += 4;
    _cpu._clock.t += 16;
}

static void RRC_A(void)
{
    unsigned int a = _cpu._r.a >> 1;
    ZERO(!a);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.a & 1);

    if( IS_CARRY ) {
        a |= 0x80;
    }

    _cpu._r.a = (uint8_t)a;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_B(void)
{
    unsigned int b = _cpu._r.b << 1;
    if( IS_CARRY ) {
        b |= 1;
    }

    ZERO(!b);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(b > 0xFF);

    _cpu._r.b = (uint8_t)b;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_C(void)
{
    unsigned int c = _cpu._r.c << 1;
    if( IS_CARRY ) {
        c |= 1;
    }

    ZERO(!c);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(c > 0xFF);

    _cpu._r.c = (uint8_t)c;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_D(void)
{
    unsigned int d = _cpu._r.d << 1;
    if( IS_CARRY ) {
        d |= 1;
    }

    ZERO(!d);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(d > 0xFF);

    _cpu._r.d = (uint8_t)d;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_E(void)
{
    unsigned int e = _cpu._r.e << 1;
    if( IS_CARRY ) {
        e |= 1;
    }

    ZERO(!e);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(e > 0xFF);

    _cpu._r.e = (uint8_t)e;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_H(void)
{
    unsigned int h = _cpu._r.h << 1;
    if( IS_CARRY ) {
        h |= 1;
    }

    ZERO(!h);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(h > 0xFF);

    _cpu._r.h = (uint8_t)h;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_L(void)
{
    unsigned int l = _cpu._r.l << 1;
    if( IS_CARRY ) {
        l |= 1;
    }

    ZERO(!l);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(l > 0xFF);

    _cpu._r.l = (uint8_t)l;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RL_mHL(void)
{
    const uint16_t hl = (_cpu._r.h << 8) + _cpu._r.l;
    unsigned int mHL = readbyte( hl ) << 1;
    if( IS_CARRY ) {
        mHL |= 1;
    }

    ZERO(!mHL);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(mHL > 0xFF);

    writebyte( hl, (uint8_t)mHL );
    _cpu._clock.m += 4;
    _cpu._clock.t += 16;
}

static void RL_A(void)
{
    unsigned int a = _cpu._r.a << 1;
    if( IS_CARRY ) {
        a |= 1;
    }

    ZERO(!a);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(a > 0xFF);

    _cpu._r.a = (uint8_t)a;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_B(void)
{
    unsigned int b = _cpu._r.b >> 1;
    if( IS_CARRY ) {
        b |= 0x80;
    }

    ZERO(!b);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.b & 1);

    _cpu._r.b = (uint8_t)b;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_C(void)
{
    unsigned int c = _cpu._r.c >> 1;
    if( IS_CARRY ) {
        c |= 0x80;
    }

    ZERO(!c);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.c & 1);

    _cpu._r.c = (uint8_t)c;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_D(void)
{
    unsigned int d = _cpu._r.d >> 1;
    if( IS_CARRY ) {
        d |= 0x80;
    }

    ZERO(!d);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.d & 1);

    _cpu._r.d = (uint8_t)d;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_E(void)
{
    unsigned int e = _cpu._r.e >> 1;
    if( IS_CARRY ) {
        e |= 0x80;
    }

    ZERO(!e);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.e & 1);

    _cpu._r.e = (uint8_t)e;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_H(void)
{
    unsigned int h = _cpu._r.h >> 1;
    if( IS_CARRY ) {
        h |= 0x80;
    }

    ZERO(!h);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.h & 1);

    _cpu._r.h = (uint8_t)h;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_L(void)
{
    unsigned int l = _cpu._r.l >> 1;
    if( IS_CARRY ) {
        l |= 0x80;
    }

    ZERO(!l);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.l & 1);

    _cpu._r.l = (uint8_t)l;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void RR_mHL(void)
{
    const uint16_t hl = (_cpu._r.h << 8) + _cpu._r.l;
    unsigned int mHL = readbyte( hl ) >> 1;
    if( IS_CARRY ) {
        mHL |= 0x80;
    }

    ZERO(!mHL);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(readbyte( hl ) & 1);

    writebyte( hl, (uint8_t)mHL );
    _cpu._clock.m += 4;
    _cpu._clock.t += 16;
}

static void RR_A(void)
{
    unsigned int a = _cpu._r.a >> 1;
    if( IS_CARRY ) {
        a |= 0x80;
    }

    ZERO(!a);
    NEGATIVE(0);
    HALF_CARRY(0);
    CARRY(_cpu._r.a & 1);

    _cpu._r.a = (uint8_t)a;
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void SLA_B(void)
{
    CARRY(_cpu._r.a & 0x80);
    _cpu._r.a <<= 1;
    ZERO(!_cpu._r.a);
    NEGATIVE(0);
    HALF_CARRY(0);
    _cpu._clock.m += 2;
    _cpu._clock.t += 8;
}

static void SLA_C(void)
{
    /* TODO: Implement */
}

static void SLA_D(void)
{
    /* TODO: Implement */
}

static void SLA_E(void)
{
    /* TODO: Implement */
}

static void SLA_H(void)
{
    /* TODO: Implement */
}

static void SLA_L(void)
{
    /* TODO: Implement */
}

static void SLA_mHL(void)
{
    /* TODO: Implement */
}

static void SLA_A(void)
{
    /* TODO: Implement */
}

static void SRA_B(void)
{
    /* TODO: Implement */
}

static void SRA_C(void)
{
    /* TODO: Implement */
}

static void SRA_D(void)
{
    /* TODO: Implement */
}

static void SRA_E(void)
{
    /* TODO: Implement */
}

static void SRA_H(void)
{
    /* TODO: Implement */
}

static void SRA_L(void)
{
    /* TODO: Implement */
}

static void SRA_mHL(void)
{
    /* TODO: Implement */
}

static void SRA_A(void)
{
    /* TODO: Implement */
}


static void SWAP_B(void)
{
    /* TODO: Implement */
}

static void SWAP_C(void)
{
    /* TODO: Implement */
}

static void SWAP_D(void)
{
    /* TODO: Implement */
}

static void SWAP_E(void)
{
    /* TODO: Implement */
}

static void SWAP_H(void)
{
    /* TODO: Implement */
}

static void SWAP_L(void)
{
    /* TODO: Implement */
}

static void SWAP_mHL(void)
{
    /* TODO: Implement */
}

static void SWAP_A(void)
{
    /* TODO: Implement */
}

static void SRL_B(void)
{
    /* TODO: Implement */
}

static void SRL_C(void)
{
    /* TODO: Implement */
}

static void SRL_D(void)
{
    /* TODO: Implement */
}

static void SRL_E(void)
{
    /* TODO: Implement */
}

static void SRL_H(void)
{
    /* TODO: Implement */
}

static void SRL_L(void)
{
    /* TODO: Implement */
}

static void SRL_mHL(void)
{
    /* TODO: Implement */
}

static void SRL_A(void)
{
    /* TODO: Implement */
}


static void BIT_0_B(void)
{
    /* TODO: Implement */
}

static void BIT_0_C(void)
{
    /* TODO: Implement */
}

static void BIT_0_D(void)
{
    /* TODO: Implement */
}

static void BIT_0_E(void)
{
    /* TODO: Implement */
}

static void BIT_0_H(void)
{
    /* TODO: Implement */
}

static void BIT_0_L(void)
{
    /* TODO: Implement */
}

static void BIT_0_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_0_A(void)
{
    /* TODO: Implement */
}

static void BIT_1_B(void)
{
    /* TODO: Implement */
}

static void BIT_1_C(void)
{
    /* TODO: Implement */
}

static void BIT_1_D(void)
{
    /* TODO: Implement */
}

static void BIT_1_E(void)
{
    /* TODO: Implement */
}

static void BIT_1_H(void)
{
    /* TODO: Implement */
}

static void BIT_1_L(void)
{
    /* TODO: Implement */
}

static void BIT_1_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_1_A(void)
{
    /* TODO: Implement */
}


static void BIT_2_B(void)
{
    /* TODO: Implement */
}

static void BIT_2_C(void)
{
    /* TODO: Implement */
}

static void BIT_2_D(void)
{
    /* TODO: Implement */
}

static void BIT_2_E(void)
{
    /* TODO: Implement */
}

static void BIT_2_H(void)
{
    /* TODO: Implement */
}

static void BIT_2_L(void)
{
    /* TODO: Implement */
}

static void BIT_2_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_2_A(void)
{
    /* TODO: Implement */
}

static void BIT_3_B(void)
{
    /* TODO: Implement */
}

static void BIT_3_C(void)
{
    /* TODO: Implement */
}

static void BIT_3_D(void)
{
    /* TODO: Implement */
}

static void BIT_3_E(void)
{
    /* TODO: Implement */
}

static void BIT_3_H(void)
{
    /* TODO: Implement */
}

static void BIT_3_L(void)
{
    /* TODO: Implement */
}

static void BIT_3_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_3_A(void)
{
    /* TODO: Implement */
}


static void BIT_4_B(void)
{
    /* TODO: Implement */
}

static void BIT_4_C(void)
{
    /* TODO: Implement */
}

static void BIT_4_D(void)
{
    /* TODO: Implement */
}

static void BIT_4_E(void)
{
    /* TODO: Implement */
}

static void BIT_4_H(void)
{
    /* TODO: Implement */
}

static void BIT_4_L(void)
{
    /* TODO: Implement */
}

static void BIT_4_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_4_A(void)
{
    /* TODO: Implement */
}

static void BIT_5_B(void)
{
    /* TODO: Implement */
}

static void BIT_5_C(void)
{
    /* TODO: Implement */
}

static void BIT_5_D(void)
{
    /* TODO: Implement */
}

static void BIT_5_E(void)
{
    /* TODO: Implement */
}

static void BIT_5_H(void)
{
    /* TODO: Implement */
}

static void BIT_5_L(void)
{
    /* TODO: Implement */
}

static void BIT_5_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_5_A(void)
{
    /* TODO: Implement */
}


static void BIT_6_B(void)
{
    /* TODO: Implement */
}

static void BIT_6_C(void)
{
    /* TODO: Implement */
}

static void BIT_6_D(void)
{
    /* TODO: Implement */
}

static void BIT_6_E(void)
{
    /* TODO: Implement */
}

static void BIT_6_H(void)
{
    /* TODO: Implement */
}

static void BIT_6_L(void)
{
    /* TODO: Implement */
}

static void BIT_6_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_6_A(void)
{
    /* TODO: Implement */
}

static void BIT_7_B(void)
{
    /* TODO: Implement */
}

static void BIT_7_C(void)
{
    /* TODO: Implement */
}

static void BIT_7_D(void)
{
    /* TODO: Implement */
}

static void BIT_7_E(void)
{
    /* TODO: Implement */
}

static void BIT_7_H(void)
{
    /* TODO: Implement */
}

static void BIT_7_L(void)
{
    /* TODO: Implement */
}

static void BIT_7_mHL(void)
{
    /* TODO: Implement */
}

static void BIT_7_A(void)
{
    /* TODO: Implement */
}


static void RES_0_B(void)
{
    /* TODO: Implement */
}

static void RES_0_C(void)
{
    /* TODO: Implement */
}

static void RES_0_D(void)
{
    /* TODO: Implement */
}

static void RES_0_E(void)
{
    /* TODO: Implement */
}

static void RES_0_H(void)
{
    /* TODO: Implement */
}

static void RES_0_L(void)
{
    /* TODO: Implement */
}

static void RES_0_mHL(void)
{
    /* TODO: Implement */
}

static void RES_0_A(void)
{
    /* TODO: Implement */
}

static void RES_1_B(void)
{
    /* TODO: Implement */
}

static void RES_1_C(void)
{
    /* TODO: Implement */
}

static void RES_1_D(void)
{
    /* TODO: Implement */
}

static void RES_1_E(void)
{
    /* TODO: Implement */
}

static void RES_1_H(void)
{
    /* TODO: Implement */
}

static void RES_1_L(void)
{
    /* TODO: Implement */
}

static void RES_1_mHL(void)
{
    /* TODO: Implement */
}

static void RES_1_A(void)
{
    /* TODO: Implement */
}


static void RES_2_B(void)
{
    /* TODO: Implement */
}

static void RES_2_C(void)
{
    /* TODO: Implement */
}

static void RES_2_D(void)
{
    /* TODO: Implement */
}

static void RES_2_E(void)
{
    /* TODO: Implement */
}

static void RES_2_H(void)
{
    /* TODO: Implement */
}

static void RES_2_L(void)
{
    /* TODO: Implement */
}

static void RES_2_mHL(void)
{
    /* TODO: Implement */
}

static void RES_2_A(void)
{
    /* TODO: Implement */
}

static void RES_3_B(void)
{
    /* TODO: Implement */
}

static void RES_3_C(void)
{
    /* TODO: Implement */
}

static void RES_3_D(void)
{
    /* TODO: Implement */
}

static void RES_3_E(void)
{
    /* TODO: Implement */
}

static void RES_3_H(void)
{
    /* TODO: Implement */
}

static void RES_3_L(void)
{
    /* TODO: Implement */
}

static void RES_3_mHL(void)
{
    /* TODO: Implement */
}

static void RES_3_A(void)
{
    /* TODO: Implement */
}


static void RES_4_B(void)
{
    /* TODO: Implement */
}

static void RES_4_C(void)
{
    /* TODO: Implement */
}

static void RES_4_D(void)
{
    /* TODO: Implement */
}

static void RES_4_E(void)
{
    /* TODO: Implement */
}

static void RES_4_H(void)
{
    /* TODO: Implement */
}

static void RES_4_L(void)
{
    /* TODO: Implement */
}

static void RES_4_mHL(void)
{
    /* TODO: Implement */
}

static void RES_4_A(void)
{
    /* TODO: Implement */
}

static void RES_5_B(void)
{
    /* TODO: Implement */
}

static void RES_5_C(void)
{
    /* TODO: Implement */
}

static void RES_5_D(void)
{
    /* TODO: Implement */
}

static void RES_5_E(void)
{
    /* TODO: Implement */
}

static void RES_5_H(void)
{
    /* TODO: Implement */
}

static void RES_5_L(void)
{
    /* TODO: Implement */
}

static void RES_5_mHL(void)
{
    /* TODO: Implement */
}

static void RES_5_A(void)
{
    /* TODO: Implement */
}


static void RES_6_B(void)
{
    /* TODO: Implement */
}

static void RES_6_C(void)
{
    /* TODO: Implement */
}

static void RES_6_D(void)
{
    /* TODO: Implement */
}

static void RES_6_E(void)
{
    /* TODO: Implement */
}

static void RES_6_H(void)
{
    /* TODO: Implement */
}

static void RES_6_L(void)
{
    /* TODO: Implement */
}

static void RES_6_mHL(void)
{
    /* TODO: Implement */
}

static void RES_6_A(void)
{
    /* TODO: Implement */
}

static void RES_7_B(void)
{
    /* TODO: Implement */
}

static void RES_7_C(void)
{
    /* TODO: Implement */
}

static void RES_7_D(void)
{
    /* TODO: Implement */
}

static void RES_7_E(void)
{
    /* TODO: Implement */
}

static void RES_7_H(void)
{
    /* TODO: Implement */
}

static void RES_7_L(void)
{
    /* TODO: Implement */
}

static void RES_7_mHL(void)
{
    /* TODO: Implement */
}

static void RES_7_A(void)
{
    /* TODO: Implement */
}


static void SET_0_B(void)
{
    /* TODO: Implement */
}

static void SET_0_C(void)
{
    /* TODO: Implement */
}

static void SET_0_D(void)
{
    /* TODO: Implement */
}

static void SET_0_E(void)
{
    /* TODO: Implement */
}

static void SET_0_H(void)
{
    /* TODO: Implement */
}

static void SET_0_L(void)
{
    /* TODO: Implement */
}

static void SET_0_mHL(void)
{
    /* TODO: Implement */
}

static void SET_0_A(void)
{
    /* TODO: Implement */
}

static void SET_1_B(void)
{
    /* TODO: Implement */
}

static void SET_1_C(void)
{
    /* TODO: Implement */
}

static void SET_1_D(void)
{
    /* TODO: Implement */
}

static void SET_1_E(void)
{
    /* TODO: Implement */
}

static void SET_1_H(void)
{
    /* TODO: Implement */
}

static void SET_1_L(void)
{
    /* TODO: Implement */
}

static void SET_1_mHL(void)
{
    /* TODO: Implement */
}

static void SET_1_A(void)
{
    /* TODO: Implement */
}


static void SET_2_B(void)
{
    /* TODO: Implement */
}

static void SET_2_C(void)
{
    /* TODO: Implement */
}

static void SET_2_D(void)
{
    /* TODO: Implement */
}

static void SET_2_E(void)
{
    /* TODO: Implement */
}

static void SET_2_H(void)
{
    /* TODO: Implement */
}

static void SET_2_L(void)
{
    /* TODO: Implement */
}

static void SET_2_mHL(void)
{
    /* TODO: Implement */
}

static void SET_2_A(void)
{
    /* TODO: Implement */
}

static void SET_3_B(void)
{
    /* TODO: Implement */
}

static void SET_3_C(void)
{
    /* TODO: Implement */
}

static void SET_3_D(void)
{
    /* TODO: Implement */
}

static void SET_3_E(void)
{
    /* TODO: Implement */
}

static void SET_3_H(void)
{
    /* TODO: Implement */
}

static void SET_3_L(void)
{
    /* TODO: Implement */
}

static void SET_3_mHL(void)
{
    /* TODO: Implement */
}

static void SET_3_A(void)
{
    /* TODO: Implement */
}


static void SET_4_B(void)
{
    /* TODO: Implement */
}

static void SET_4_C(void)
{
    /* TODO: Implement */
}

static void SET_4_D(void)
{
    /* TODO: Implement */
}

static void SET_4_E(void)
{
    /* TODO: Implement */
}

static void SET_4_H(void)
{
    /* TODO: Implement */
}

static void SET_4_L(void)
{
    /* TODO: Implement */
}

static void SET_4_mHL(void)
{
    /* TODO: Implement */
}

static void SET_4_A(void)
{
    /* TODO: Implement */
}

static void SET_5_B(void)
{
    /* TODO: Implement */
}

static void SET_5_C(void)
{
    /* TODO: Implement */
}

static void SET_5_D(void)
{
    /* TODO: Implement */
}

static void SET_5_E(void)
{
    /* TODO: Implement */
}

static void SET_5_H(void)
{
    /* TODO: Implement */
}

static void SET_5_L(void)
{
    /* TODO: Implement */
}

static void SET_5_mHL(void)
{
    /* TODO: Implement */
}

static void SET_5_A(void)
{
    /* TODO: Implement */
}


static void SET_6_B(void)
{
    /* TODO: Implement */
}

static void SET_6_C(void)
{
    /* TODO: Implement */
}

static void SET_6_D(void)
{
    /* TODO: Implement */
}

static void SET_6_E(void)
{
    /* TODO: Implement */
}

static void SET_6_H(void)
{
    /* TODO: Implement */
}

static void SET_6_L(void)
{
    /* TODO: Implement */
}

static void SET_6_mHL(void)
{
    /* TODO: Implement */
}

static void SET_6_A(void)
{
    /* TODO: Implement */
}

static void SET_7_B(void)
{
    /* TODO: Implement */
}

static void SET_7_C(void)
{
    /* TODO: Implement */
}

static void SET_7_D(void)
{
    /* TODO: Implement */
}

static void SET_7_E(void)
{
    /* TODO: Implement */
}

static void SET_7_H(void)
{
    /* TODO: Implement */
}

static void SET_7_L(void)
{
    /* TODO: Implement */
}

static void SET_7_mHL(void)
{
    /* TODO: Implement */
}

static void SET_7_A(void)
{
    /* TODO: Implement */
}

static const t_op _cb_map[0x100] = {
    RLC_B,   RLC_C,   RLC_D,   RLC_E,   RLC_H,   RLC_L,   RLC_mHL,   RLC_A,   RRC_B,   RRC_C,   RRC_D,   RRC_E,   RRC_H,   RRC_L,   RRC_mHL,   RRC_A,
    RL_B,    RL_C,    RL_D,    RL_E,    RL_H,    RL_L,    RL_mHL,    RL_A,    RR_B,    RR_C,    RR_D,    RR_E,    RR_H,    RR_L,    RR_mHL,    RR_A,
    SLA_B,   SLA_C,   SLA_D,   SLA_E,   SLA_H,   SLA_L,   SLA_mHL,   SLA_A,   SRA_B,   SRA_C,   SRA_D,   SRA_E,   SRA_H,   SRA_L,   SRA_mHL,   SRA_A,
    SWAP_B,  SWAP_C,  SWAP_D,  SWAP_E,  SWAP_H,  SWAP_L,  SWAP_mHL,  SWAP_A,  SRL_B,   SRL_C,   SRL_D,   SRL_E,   SRL_H,   SRL_L,   SRL_mHL,   SRL_A,
    BIT_0_B, BIT_0_C, BIT_0_D, BIT_0_E, BIT_0_H, BIT_0_L, BIT_0_mHL, BIT_0_A, BIT_1_B, BIT_1_C, BIT_1_D, BIT_1_E, BIT_1_H, BIT_1_L, BIT_1_mHL, BIT_1_A,
    BIT_2_B, BIT_2_C, BIT_2_D, BIT_2_E, BIT_2_H, BIT_2_L, BIT_2_mHL, BIT_2_A, BIT_3_B, BIT_3_C, BIT_3_D, BIT_3_E, BIT_3_H, BIT_3_L, BIT_3_mHL, BIT_3_A,
    BIT_4_B, BIT_4_C, BIT_4_D, BIT_4_E, BIT_4_H, BIT_4_L, BIT_4_mHL, BIT_4_A, BIT_5_B, BIT_5_C, BIT_5_D, BIT_5_E, BIT_5_H, BIT_5_L, BIT_5_mHL, BIT_5_A,
    BIT_6_B, BIT_6_C, BIT_6_D, BIT_6_E, BIT_6_H, BIT_6_L, BIT_6_mHL, BIT_6_A, BIT_7_B, BIT_7_C, BIT_7_D, BIT_7_E, BIT_7_H, BIT_7_L, BIT_7_mHL, BIT_7_A,
    RES_0_B, RES_0_C, RES_0_D, RES_0_E, RES_0_H, RES_0_L, RES_0_mHL, RES_0_A, RES_1_B, RES_1_C, RES_1_D, RES_1_E, RES_1_H, RES_1_L, RES_1_mHL, RES_1_A,
    RES_2_B, RES_2_C, RES_2_D, RES_2_E, RES_2_H, RES_2_L, RES_2_mHL, RES_2_A, RES_3_B, RES_3_C, RES_3_D, RES_3_E, RES_3_H, RES_3_L, RES_3_mHL, RES_3_A,
    RES_4_B, RES_4_C, RES_4_D, RES_4_E, RES_4_H, RES_4_L, RES_4_mHL, RES_4_A, RES_5_B, RES_5_C, RES_5_D, RES_5_E, RES_5_H, RES_5_L, RES_5_mHL, RES_5_A,
    RES_6_B, RES_6_C, RES_6_D, RES_6_E, RES_6_H, RES_6_L, RES_6_mHL, RES_6_A, RES_7_B, RES_7_C, RES_7_D, RES_7_E, RES_7_H, RES_7_L, RES_7_mHL, RES_7_A,
    SET_0_B, SET_0_C, SET_0_D, SET_0_E, SET_0_H, SET_0_L, SET_0_mHL, SET_0_A, SET_1_B, SET_1_C, SET_1_D, SET_1_E, SET_1_H, SET_1_L, SET_1_mHL, SET_1_A,
    SET_2_B, SET_2_C, SET_2_D, SET_2_E, SET_2_H, SET_2_L, SET_2_mHL, SET_2_A, SET_3_B, SET_3_C, SET_3_D, SET_3_E, SET_3_H, SET_3_L, SET_3_mHL, SET_3_A,
    SET_4_B, SET_4_C, SET_4_D, SET_4_E, SET_4_H, SET_4_L, SET_4_mHL, SET_4_A, SET_5_B, SET_5_C, SET_5_D, SET_5_E, SET_5_H, SET_5_L, SET_5_mHL, SET_5_A,
    SET_6_B, SET_6_C, SET_6_D, SET_6_E, SET_6_H, SET_6_L, SET_6_mHL, SET_6_A, SET_7_B, SET_7_C, SET_7_D, SET_7_E, SET_7_H, SET_7_L, SET_7_mHL, SET_7_A
};


static void XX(void)
{
    /* OPCODE NOT IMPLEMENTED */
}

static void NOP(void)
{
    /* TODO: Implement */
}

static void LD_BC_nn(void)
{
    /* TODO: Implement */
}

static void LD_mBC_A(void)
{
    /* TODO: Implement */
}

static void INC_BC(void)
{
    /* TODO: Implement */
}

static void INC_B(void)
{
    /* TODO: Implement */
}

static void DEC_B(void)
{
    /* TODO: Implement */
}

static void LD_B_n(void)
{
    /* TODO: Implement */
}

static void RLCA(void)
{
    /* TODO: Implement */
}

static void LD_mm_SP(void)
{
    /* TODO: Implement */
}

static void ADD_HL_BC(void)
{
    /* TODO: Implement */
}

static void LD_A_mBC(void)
{
    /* TODO: Implement */
}

static void DEC_BC(void)
{
    /* TODO: Implement */
}

static void INC_C(void)
{
    /* TODO: Implement */
}

static void DEC_C(void)
{
    /* TODO: Implement */
}

static void LD_C_n(void)
{
    /* TODO: Implement */
}

static void RRCA(void)
{
    /* TODO: Implement */
}


static void STOP(void)
{
    /* TODO: Implement */
}

static void LD_DE_nn(void)
{
    /* TODO: Implement */
}

static void LD_mDE_A(void)
{
    /* TODO: Implement */
}

static void INC_DE(void)
{
    /* TODO: Implement */
}

static void INC_D(void)
{
    /* TODO: Implement */
}

static void DEC_D(void)
{
    /* TODO: Implement */
}

static void LD_D_n(void)
{
    /* TODO: Implement */
}

static void RLA(void)
{
    /* TODO: Implement */
}

static void JR_n(void)
{
    /* TODO: Implement */
}

static void ADD_HL_DE(void)
{
    /* TODO: Implement */
}

static void LD_A_mDE(void)
{
    /* TODO: Implement */
}

static void DEC_DE(void)
{
    /* TODO: Implement */
}

static void INC_E(void)
{
    /* TODO: Implement */
}

static void DEC_E(void)
{
    /* TODO: Implement */
}

static void LD_E_n(void)
{
    /* TODO: Implement */
}

static void RRA(void)
{
    /* TODO: Implement */
}


static void JR_NZ_n(void)
{
    /* TODO: Implement */
}

static void LD_HL_nn(void)
{
    /* TODO: Implement */
}

static void LDI_mHL_A(void)
{
    /* TODO: Implement */
}

static void INC_HL(void)
{
    /* TODO: Implement */
}

static void INC_H(void)
{
    /* TODO: Implement */
}

static void DEC_H(void)
{
    /* TODO: Implement */
}

static void LD_H_n(void)
{
    /* TODO: Implement */
}

static void DAA(void)
{
    /* TODO: Implement */
}

static void JR_Z_n(void)
{
    /* TODO: Implement */
}

static void ADD_HL_HL(void)
{
    /* TODO: Implement */
}

static void LDI_A_mHL(void)
{
    /* TODO: Implement */
}

static void DEC_HL(void)
{
    /* TODO: Implement */
}

static void INC_L(void)
{
    /* TODO: Implement */
}

static void DEC_L(void)
{
    /* TODO: Implement */
}

static void LD_L_n(void)
{
    /* TODO: Implement */
}

static void CPL(void)
{
    /* TODO: Implement */
}


static void JR_NC_n(void)
{
    /* TODO: Implement */
}

static void LD_SP_nn(void)
{
    /* TODO: Implement */
}

static void LDD_mHL_A(void)
{
    /* TODO: Implement */
}

static void INC_SP(void)
{
    /* TODO: Implement */
}

static void INC_mHL(void)
{
    /* TODO: Implement */
}

static void DEC_mHL(void)
{
    /* TODO: Implement */
}

static void LD_mHL_n(void)
{
    /* TODO: Implement */
}

static void SCF(void)
{
    /* TODO: Implement */
}

static void JR_C_n(void)
{
    /* TODO: Implement */
}

static void ADD_HL_SP(void)
{
    /* TODO: Implement */
}

static void LDD_A_mHL(void)
{
    /* TODO: Implement */
}

static void DEC_SP(void)
{
    /* TODO: Implement */
}

static void INC_A(void)
{
    /* TODO: Implement */
}

static void DEC_A(void)
{
    /* TODO: Implement */
}

static void LD_A_n(void)
{
    /* TODO: Implement */
}

static void CCF(void)
{
    /* TODO: Implement */
}


static void LD_B_B(void)
{
    /* TODO: Implement */
}

static void LD_B_C(void)
{
    /* TODO: Implement */
}

static void LD_B_D(void)
{
    /* TODO: Implement */
}

static void LD_B_E(void)
{
    /* TODO: Implement */
}

static void LD_B_H(void)
{
    /* TODO: Implement */
}

static void LD_B_L(void)
{
    /* TODO: Implement */
}

static void LD_B_mHL(void)
{
    /* TODO: Implement */
}

static void LD_B_A(void)
{
    /* TODO: Implement */
}

static void LD_C_B(void)
{
    /* TODO: Implement */
}

static void LD_C_C(void)
{
    /* TODO: Implement */
}

static void LD_C_D(void)
{
    /* TODO: Implement */
}

static void LD_C_E(void)
{
    /* TODO: Implement */
}

static void LD_C_H(void)
{
    /* TODO: Implement */
}

static void LD_C_L(void)
{
    /* TODO: Implement */
}

static void LD_C_mHL(void)
{
    /* TODO: Implement */
}

static void LD_C_A(void)
{
    /* TODO: Implement */
}


static void LD_D_B(void)
{
    /* TODO: Implement */
}

static void LD_D_C(void)
{
    /* TODO: Implement */
}

static void LD_D_D(void)
{
    /* TODO: Implement */
}

static void LD_D_E(void)
{
    /* TODO: Implement */
}

static void LD_D_H(void)
{
    /* TODO: Implement */
}

static void LD_D_L(void)
{
    /* TODO: Implement */
}

static void LD_D_mHL(void)
{
    /* TODO: Implement */
}

static void LD_D_A(void)
{
    /* TODO: Implement */
}

static void LD_E_B(void)
{
    /* TODO: Implement */
}

static void LD_E_C(void)
{
    /* TODO: Implement */
}

static void LD_E_D(void)
{
    /* TODO: Implement */
}

static void LD_E_E(void)
{
    /* TODO: Implement */
}

static void LD_E_H(void)
{
    /* TODO: Implement */
}

static void LD_E_L(void)
{
    /* TODO: Implement */
}

static void LD_E_mHL(void)
{
    /* TODO: Implement */
}

static void LD_E_A(void)
{
    /* TODO: Implement */
}


static void LD_H_B(void)
{
    /* TODO: Implement */
}

static void LD_H_C(void)
{
    /* TODO: Implement */
}

static void LD_H_D(void)
{
    /* TODO: Implement */
}

static void LD_H_E(void)
{
    /* TODO: Implement */
}

static void LD_H_H(void)
{
    /* TODO: Implement */
}

static void LD_H_L(void)
{
    /* TODO: Implement */
}

static void LD_H_mHL(void)
{
    /* TODO: Implement */
}

static void LD_H_A(void)
{
    /* TODO: Implement */
}

static void LD_L_B(void)
{
    /* TODO: Implement */
}

static void LD_L_C(void)
{
    /* TODO: Implement */
}

static void LD_L_D(void)
{
    /* TODO: Implement */
}

static void LD_L_E(void)
{
    /* TODO: Implement */
}

static void LD_L_H(void)
{
    /* TODO: Implement */
}

static void LD_L_L(void)
{
    /* TODO: Implement */
}

static void LD_L_mHL(void)
{
    /* TODO: Implement */
}

static void LD_L_A(void)
{
    /* TODO: Implement */
}


static void LD_mHL_B(void)
{
    /* TODO: Implement */
}

static void LD_mHL_C(void)
{
    /* TODO: Implement */
}

static void LD_mHL_D(void)
{
    /* TODO: Implement */
}

static void LD_mHL_E(void)
{
    /* TODO: Implement */
}

static void LD_mHL_H(void)
{
    /* TODO: Implement */
}

static void LD_mHL_L(void)
{
    /* TODO: Implement */
}

static void HALT(void)
{
    /* TODO: Implement */
}

static void LD_mHL_A(void)
{
    /* TODO: Implement */
}

static void LD_A_B(void)
{
    /* TODO: Implement */
}

static void LD_A_C(void)
{
    /* TODO: Implement */
}

static void LD_A_D(void)
{
    /* TODO: Implement */
}

static void LD_A_E(void)
{
    /* TODO: Implement */
}

static void LD_A_H(void)
{
    /* TODO: Implement */
}

static void LD_A_L(void)
{
    /* TODO: Implement */
}

static void LD_A_mHL(void)
{
    /* TODO: Implement */
}

static void LD_A_A(void)
{
    /* TODO: Implement */
}


static void ADD_A_B(void)
{
    /* TODO: Implement */
}

static void ADD_A_C(void)
{
    /* TODO: Implement */
}

static void ADD_A_D(void)
{
    /* TODO: Implement */
}

static void ADD_A_E(void)
{
    /* TODO: Implement */
}

static void ADD_A_H(void)
{
    /* TODO: Implement */
}

static void ADD_A_L(void)
{
    /* TODO: Implement */
}

static void ADD_A_mHL(void)
{
    /* TODO: Implement */
}

static void ADD_A_A(void)
{
    /* TODO: Implement */
}

static void ADC_A_B(void)
{
    /* TODO: Implement */
}

static void ADC_A_C(void)
{
    /* TODO: Implement */
}

static void ADC_A_D(void)
{
    /* TODO: Implement */
}

static void ADC_A_E(void)
{
    /* TODO: Implement */
}

static void ADC_A_H(void)
{
    /* TODO: Implement */
}

static void ADC_A_L(void)
{
    /* TODO: Implement */
}

static void ADC_A_mHL(void)
{
    /* TODO: Implement */
}

static void ADC_A_A(void)
{
    /* TODO: Implement */
}


static void SUB_B(void)
{
    /* TODO: Implement */
}

static void SUB_C(void)
{
    /* TODO: Implement */
}

static void SUB_D(void)
{
    /* TODO: Implement */
}

static void SUB_E(void)
{
    /* TODO: Implement */
}

static void SUB_H(void)
{
    /* TODO: Implement */
}

static void SUB_L(void)
{
    /* TODO: Implement */
}

static void SUB_mHL(void)
{
    /* TODO: Implement */
}

static void SUB_A(void)
{
    /* TODO: Implement */
}

static void SBC_A_B(void)
{
    /* TODO: Implement */
}

static void SBC_A_C(void)
{
    /* TODO: Implement */
}

static void SBC_A_D(void)
{
    /* TODO: Implement */
}

static void SBC_A_E(void)
{
    /* TODO: Implement */
}

static void SBC_A_H(void)
{
    /* TODO: Implement */
}

static void SBC_A_L(void)
{
    /* TODO: Implement */
}

static void SBC_A_mHL(void)
{
    /* TODO: Implement */
}

static void SBC_A_A(void)
{
    /* TODO: Implement */
}


static void AND_B(void)
{
    /* TODO: Implement */
}

static void AND_C(void)
{
    /* TODO: Implement */
}

static void AND_D(void)
{
    /* TODO: Implement */
}

static void AND_E(void)
{
    /* TODO: Implement */
}

static void AND_H(void)
{
    /* TODO: Implement */
}

static void AND_L(void)
{
    /* TODO: Implement */
}

static void AND_mHL(void)
{
    /* TODO: Implement */
}

static void AND_A(void)
{
    /* TODO: Implement */
}

static void XOR_B(void)
{
    /* TODO: Implement */
}

static void XOR_C(void)
{
    /* TODO: Implement */
}

static void XOR_D(void)
{
    /* TODO: Implement */
}

static void XOR_E(void)
{
    /* TODO: Implement */
}

static void XOR_H(void)
{
    /* TODO: Implement */
}

static void XOR_L(void)
{
    /* TODO: Implement */
}

static void XOR_mHL(void)
{
    /* TODO: Implement */
}

static void XOR_A(void)
{
    /* TODO: Implement */
}

static void OR_B(void)
{
    /* TODO: Implement */
}

static void OR_C(void)
{
    /* TODO: Implement */
}

static void OR_D(void)
{
    /* TODO: Implement */
}

static void OR_E(void)
{
    /* TODO: Implement */
}

static void OR_H(void)
{
    /* TODO: Implement */
}

static void OR_L(void)
{
    /* TODO: Implement */
}

static void OR_mHL(void)
{
    /* TODO: Implement */
}

static void OR_A(void)
{
    /* TODO: Implement */
}

static void CP_B(void)
{
    /* TODO: Implement */
}

static void CP_C(void)
{
    /* TODO: Implement */
}

static void CP_D(void)
{
    /* TODO: Implement */
}

static void CP_E(void)
{
    /* TODO: Implement */
}

static void CP_H(void)
{
    /* TODO: Implement */
}

static void CP_L(void)
{
    /* TODO: Implement */
}

static void CP_mHL(void)
{
    /* TODO: Implement */
}

static void CP_A(void)
{
    /* TODO: Implement */
}

static void RET_NZ(void)
{
    /* TODO: Implement */
}

static void POP_BC(void)
{
    /* TODO: Implement */
}

static void JP_NZ_nn(void)
{
    /* TODO: Implement */
}

static void JP_nn(void)
{
    /* TODO: Implement */
}

static void CALL_NZ_nn(void)
{
    /* TODO: Implement */
}

static void PUSH_BC(void)
{
    /* TODO: Implement */
}

static void ADD_A_n(void)
{
    /* TODO: Implement */
}

static void RST_00H(void)
{
    /* TODO: Implement */
}

static void RET_Z(void)
{
    /* TODO: Implement */
}

static void RET(void)
{
    /* TODO: Implement */
}

static void JP_Z_nn(void)
{
    /* TODO: Implement */
}

static void MAP_CB(void)
{
    uint8_t op = readbyte( _cpu._r.pc++ );
    _cb_map[op]();
}

static void CALL_Z_nn(void)
{
    /* TODO: Implement */
}

static void CALL_nn(void)
{
    /* TODO: Implement */
}

static void ADC_A_n(void)
{
    /* TODO: Implement */
}

static void RST_08H(void)
{
    /* TODO: Implement */
}


static void RET_NC(void)
{
    /* TODO: Implement */
}

static void POP_DE(void)
{
    /* TODO: Implement */
}

static void JP_NC_nn(void)
{
    /* TODO: Implement */
}

static void CALL_NC_nn(void)
{
    /* TODO: Implement */
}

static void PUSH_DE(void)
{
    /* TODO: Implement */
}

static void SUB_n(void)
{
    /* TODO: Implement */
}

static void RST_10H(void)
{
    /* TODO: Implement */
}

static void RET_C(void)
{
    /* TODO: Implement */
}

static void RETI(void)
{
    /* TODO: Implement */
}

static void JP_C_nn(void)
{
    /* TODO: Implement */
}

static void CALL_C_nn(void)
{
    /* TODO: Implement */
}

static void SBC_A_n(void)
{
    /* TODO: Implement */
}

static void RST_18H(void)
{
    /* TODO: Implement */
}


static void LDH_m_A(void)
{
    /* TODO: Implement */
}

static void POP_HL(void)
{
    /* TODO: Implement */
}

static void LD_mC_A(void)
{
    /* TODO: Implement */
}

static void PUSH_HL(void)
{
    /* TODO: Implement */
}

static void AND_n(void)
{
    /* TODO: Implement */
}

static void RST_20H(void)
{
    /* TODO: Implement */
}

static void ADD_SP_n(void)
{
    /* TODO: Implement */
}

static void JP_mHL(void)
{
    /* TODO: Implement */
}

static void LD_mm_A(void)
{
    /* TODO: Implement */
}

static void XOR_n(void)
{
    /* TODO: Implement */
}

static void RST_28H(void)
{
    /* TODO: Implement */
}

static void LDH_A_m(void)
{
    /* TODO: Implement */
}

static void POP_AF(void)
{
    /* TODO: Implement */
}

static void LD_A_mC(void)
{
    /* TODO: Implement */
}

static void DI(void)
{
    /* TODO: Implement */
}

static void PUSH_AF(void)
{
    /* TODO: Implement */
}

static void OR_n(void)
{
    /* TODO: Implement */
}

static void RST_30H(void)
{
    /* TODO: Implement */
}

static void LD_HL_SPn(void)
{
    /* TODO: Implement */
}

static void LD_SP_HL(void)
{
    /* TODO: Implement */
}

static void LD_A_mm(void)
{
    /* TODO: Implement */
}

static void EI(void)
{
    /* TODO: Implement */
}

static void CP_n(void)
{
    /* TODO: Implement */
}

static void RST_38H(void)
{
    /* TODO: Implement */
}

static const t_op _map[0x100] = {
    NOP,      LD_BC_nn, LD_mBC_A,  INC_BC,   INC_B,      DEC_B,    LD_B_n,    RLCA,     LD_mm_SP,  ADD_HL_BC, LD_A_mBC,  DEC_BC,  INC_C,     DEC_C,   LD_C_n,    RRCA,
    STOP,     LD_DE_nn, LD_mDE_A,  INC_DE,   INC_D,      DEC_D,    LD_D_n,    RLA,      JR_n,      ADD_HL_DE, LD_A_mDE,  DEC_DE,  INC_E,     DEC_E,   LD_E_n,    RRA,
    JR_NZ_n,  LD_HL_nn, LDI_mHL_A, INC_HL,   INC_H,      DEC_H,    LD_H_n,    DAA,      JR_Z_n,    ADD_HL_HL, LDI_A_mHL, DEC_HL,  INC_L,     DEC_L,   LD_L_n,    CPL,
    JR_NC_n,  LD_SP_nn, LDD_mHL_A, INC_SP,   INC_mHL,    DEC_mHL,  LD_mHL_n,  SCF,      JR_C_n,    ADD_HL_SP, LDD_A_mHL, DEC_SP,  INC_A,     DEC_A,   LD_A_n,    CCF,
    LD_B_B,   LD_B_C,   LD_B_D,    LD_B_E,   LD_B_H,     LD_B_L,   LD_B_mHL,  LD_B_A,   LD_C_B,    LD_C_C,    LD_C_D,    LD_C_E,  LD_C_H,    LD_C_L,  LD_C_mHL,  LD_C_A,
    LD_D_B,   LD_D_C,   LD_D_D,    LD_D_E,   LD_D_H,     LD_D_L,   LD_D_mHL,  LD_D_A,   LD_E_B,    LD_E_C,    LD_E_D,    LD_E_E,  LD_E_H,    LD_E_L,  LD_E_mHL,  LD_E_A,
    LD_H_B,   LD_H_C,   LD_H_D,    LD_H_E,   LD_H_H,     LD_H_L,   LD_H_mHL,  LD_H_A,   LD_L_B,    LD_L_C,    LD_L_D,    LD_L_E,  LD_L_H,    LD_L_L,  LD_L_mHL,  LD_L_A,
    LD_mHL_B, LD_mHL_C, LD_mHL_D,  LD_mHL_E, LD_mHL_H,   LD_mHL_L, HALT,      LD_mHL_A, LD_A_B,    LD_A_C,    LD_A_D,    LD_A_E,  LD_A_H,    LD_A_L,  LD_A_mHL,  LD_A_A,
    ADD_A_B,  ADD_A_C,  ADD_A_D,   ADD_A_E,  ADD_A_H,    ADD_A_L,  ADD_A_mHL, ADD_A_A,  ADC_A_B,   ADC_A_C,   ADC_A_D,   ADC_A_E, ADC_A_H,   ADC_A_L, ADC_A_mHL, ADC_A_A,
    SUB_B,    SUB_C,    SUB_D,     SUB_E,    SUB_H,      SUB_L,    SUB_mHL,   SUB_A,    SBC_A_B,   SBC_A_C,   SBC_A_D,   SBC_A_E, SBC_A_H,   SBC_A_L, SBC_A_mHL, SBC_A_A,
    AND_B,    AND_C,    AND_D,     AND_E,    AND_H,      AND_L,    AND_mHL,   AND_A,    XOR_B,     XOR_C,     XOR_D,     XOR_E,   XOR_H,     XOR_L,   XOR_mHL,   XOR_A,
    OR_B,     OR_C,     OR_D,      OR_E,     OR_H,       OR_L,     OR_mHL,    OR_A,     CP_B,      CP_C,      CP_D,      CP_E,    CP_H,      CP_L,    CP_mHL,    CP_A,
    RET_NZ,   POP_BC,   JP_NZ_nn,  JP_nn,    CALL_NZ_nn, PUSH_BC,  ADD_A_n,   RST_00H,  RET_Z,     RET,       JP_Z_nn,   MAP_CB,  CALL_Z_nn, CALL_nn, ADC_A_n,   RST_08H,
    RET_NC,   POP_DE,   JP_NC_nn,  XX,       CALL_NC_nn, PUSH_DE,  SUB_n,     RST_10H,  RET_C,     RETI,      JP_C_nn,   XX,      CALL_C_nn, XX,      SBC_A_n,   RST_18H,
    LDH_m_A,  POP_HL,   LD_mC_A,   XX,       XX,         PUSH_HL,  AND_n,     RST_20H,  ADD_SP_n,  JP_mHL,    LD_mm_A,   XX,      XX,        XX,      XOR_n,     RST_28H,
    LDH_A_m,  POP_AF,   LD_A_mC,   DI,       XX,         PUSH_AF,  OR_n,      RST_30H,  LD_HL_SPn, LD_SP_HL,  LD_A_mm,   EI,      XX,        XX,      CP_n,      RST_38H
};

void dispatch(void)
{
    uint8_t op = readbyte( _cpu._r.pc++ );
    _map[op]();
}