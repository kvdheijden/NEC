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

#define IS_ZERO         (_r.f & 0x80)
#define IS_NEGATIVE     (_r.f & 0x40)
#define IS_HALF_CARRY   (_r.f & 0x20)
#define IS_CARRY        (_r.f & 0x10)

#define SET_ZERO(c)         (c) ? (_r.f |= 0x80) : (_r.f &= 0x7F)
#define SET_NEGATIVE(c)     (c) ? (_r.f |= 0x40) : (_r.f &= 0xBF)
#define SET_HALF_CARRY(c)   (c) ? (_r.f |= 0x20) : (_r.f &= 0xDF)
#define SET_CARRY(c)        (c) ? (_r.f |= 0x10) : (_r.f &= 0xEF)

#include <stdbool.h>

#include "MMU.h"
#include "timer.h"
#include "PPU.h"
#include "audio.h"
#include "GB.h"

typedef void (*instruction)(void);

enum condition {
    NZ,
    Z,
    NC,
    C,
    T
};

struct registers _r = {
        .sp = 0xFFFE,
        .pc = 0x0000
};

#define NUM_OPCODES 0x100

uint8_t _IE = 0x00;
uint8_t _IF = 0x00;

static bool _IME = false;
static bool _HALT = false;
static bool _STOP = false;

static bool _DI_pending = false;
static bool _EI_pending = false;

/*
 * Debugging functions
 */

static void XX(void)
{
    log_error("Invalid instruction with opcode 0x%02X (ROM address 0x%04X)\n", read_byte((uint16_t) (_r.pc - 1)), _r.pc - 1);
    GB_exit();
}

static void YY(void)
{
    log_error("Invalid instruction in CB MAP with opcode 0x%02X (ROM address 0x%04X)\n", read_byte((uint16_t) (_r.pc - 1)), _r.pc - 1);
    GB_exit();
}

/*
 * Generic helper functions
 */

static inline void ADD8(int n)
{
    _r.f = 0x00;
    SET_HALF_CARRY(((_r.a & 0x0F) + (n & 0x0F)) > 0x0F);
    SET_CARRY((_r.a + n) > 0xFF);
    _r.a += n;
    SET_ZERO(!_r.a);
}

static inline void ADC8(int n)
{
    n += (IS_CARRY ? 0x01 : 0x00);
    _r.f = 0x00;
    SET_HALF_CARRY(((_r.a & 0x0F) + (n & 0x0F)) > 0x0F);
    SET_CARRY((_r.a + n) > 0xFF);
    _r.a += n;
    SET_ZERO(!_r.a);
}

static inline void SUB8(int n)
{
    _r.f = 0x40;
    SET_HALF_CARRY((_r.a & 0x0F) < (n & 0x0F));
    SET_CARRY(_r.a < n);
    _r.a -= n;
    SET_ZERO(!_r.a);
}

static inline void SBC8(int n)
{
    n += (IS_CARRY ? 0x01 : 0x00);
    _r.f = 0x40;
    SET_HALF_CARRY((_r.a & 0x0F) < (n & 0x0F));
    SET_CARRY(_r.a < n);
    _r.a -= n;
    SET_ZERO(!_r.a);
}

static inline void AND8(uint8_t n)
{
    _r.a &= n;
    _r.f = 0x20;
    SET_ZERO(!_r.a);
}

static inline void OR8(uint8_t n)
{
    _r.a |= n;
    _r.f = 0x00;
    SET_ZERO(!_r.a);
}

static inline void XOR8(uint8_t n)
{
    _r.a ^= n;
    _r.f = 0x00;
    SET_ZERO(!_r.a);
}

static inline void CP8(uint8_t n)
{
    _r.f = 0x40;
    SET_ZERO(_r.a == n);
    SET_HALF_CARRY((_r.a & 0x0F) < (n & 0x0F));
    SET_CARRY(_r.a < n);
}

static inline void INC8(uint8_t *n)
{
    _r.f &= 0x10;
    SET_HALF_CARRY((*n & 0x0F) == 0x0F);
    (*n)++;
    SET_ZERO(!(*n));
}

static inline void DEC8(uint8_t *n)
{
    _r.f &= 0x10;
    _r.f |= 0x40;
    SET_HALF_CARRY((*n & 0x0F) == 0);
    (*n)--;
    SET_ZERO(!(*n));
}

static inline void ADD16(uint16_t *dest, uint16_t n)
{
    _r.f &= (dest == &_r.sp ? 0x00 : 0x80);
    SET_HALF_CARRY(((*dest & 0x0FFF) + (n & 0x0FFF)) > 0x0FFF);
    SET_CARRY((*dest + n) > 0xFFFF);
    *dest += n;
}

static inline void INC16(uint16_t *nn)
{
    (*nn)++;
}

static inline void DEC16(uint16_t *nn)
{
    (*nn)--;
}

static inline void SWAP(uint8_t *n)
{
    *n = (uint8_t) (((*n & 0x0F) << 4) | ((*n >> 4) & 0x0F));
    _r.f = 0x00;
    SET_ZERO(!(*n));
}

static void DAA(void)
{
    _r.f &= 0x40;
    if(IS_HALF_CARRY || ((_r.a & 0x0F) > 0x09)) {
        _r.a += 6;
    }

    if((_r.a & 0xF0) > 0x90) {
        _r.a += 0x60;
        _r.f |= 0x10;
    }

    SET_ZERO(!_r.a);
    _r.clk += 4;
}

static void CPL(void)
{
    _r.a = ~_r.a;
    _r.f = (uint8_t) ((_r.f & 0x90) | 0x60);
    _r.clk += 4;
}

static void CCF(void)
{
    _r.f &= 0x90;
    SET_CARRY(!IS_CARRY);
    _r.clk += 4;
}

static void SCF(void)
{
    _r.f = (uint8_t) ((_r.f & 0x80) | 0x10);
    _r.clk += 4;
}

static void NOP(void)
{
    _r.clk += 4;
}

static void HALT(void)
{
    _HALT = true;
    _r.clk += 4;
}

static void STOP(void)
{
    _r.pc++;
    _STOP = true;
    _r.clk += 4;
}

static void DI(void)
{
    _DI_pending = true;
    _r.clk += 4;
}

static void EI(void)
{
    _EI_pending = true;
    _r.clk += 4;
}

static inline void RLC(uint8_t *n)
{
    uint8_t c = (uint8_t) ((*n & 0x80) ? 0x01 : 0x00);
    *n = ((*n) << 1) | c;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c);
}

static inline void RL(uint8_t *n)
{
    uint8_t c_in = (uint8_t) (IS_CARRY ? 0x01 : 0x00);
    uint8_t c_out = (uint8_t) (*n & 0x80);
    *n = ((*n) << 1) | c_in;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c_out);
}

static inline void RRC(uint8_t *n)
{
    uint8_t c = (uint8_t) ((*n & 0x01) ? 0x80 : 0x00);
    *n = ((*n) >> 1) | c;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c);
}

static inline void RR(uint8_t *n)
{
    uint8_t c_in = (uint8_t) (IS_CARRY ? 0x80 : 0x00);
    uint8_t c_out = (uint8_t) (*n & 0x01);
    *n = ((*n) >> 1) | c_in;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c_out);
}

static inline void SLA(uint8_t *n)
{
    uint8_t c = (uint8_t) ((*n & 0x80) ? 0x01 : 0x00);
    *n = (*n) << 1;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c);
}

static inline void SRA(uint8_t *n)
{
    uint8_t c = (uint8_t) (*n & 0x01);
    *n = (uint8_t) ((*n & 0x80) | (*n >> 1));
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c);
}

static inline void SRL(uint8_t *n)
{
    uint8_t c = (uint8_t) (*n & 0x01);
    *n = (*n) >> 1;
    _r.f = 0x00;
    SET_ZERO(!(*n));
    SET_CARRY(c);
}

static inline void BIT(uint8_t b, uint8_t n)
{
    _r.f = (uint8_t) ((_r.f & 0x10) | 0x20);
    SET_ZERO(!(n & (0x01 << b)));
}

static inline void SET(uint8_t b, uint8_t *n)
{
    *n |= (0x01 << b);
}

static inline void RES(uint8_t b, uint8_t *n)
{
    *n &= ~(0x01 << b);
}

static inline void JP(enum condition c, uint16_t n)
{
    switch(c) {
        case NZ:
            if(IS_ZERO) {
                return;
            }
            break;
        case Z:
            if(!IS_ZERO) {
                return;
            }
            break;
        case NC:
            if(IS_CARRY) {
                return;
            }
            break;
        case C:
            if(!IS_CARRY) {
                return;
            }
            break;
        case T:
            break;
    }
    _r.clk += 4;
    _r.pc = n;
}

static inline void JR(enum condition c, int8_t n)
{
    switch(c) {
        case NZ:
            if(IS_ZERO) {
                return;
            }
            break;
        case Z:
            if(!IS_ZERO) {
                return;
            }
            break;
        case NC:
            if(IS_CARRY) {
                return;
            }
            break;
        case C:
            if(!IS_CARRY) {
                return;
            }
            break;
        case T:
            break;
    }
    _r.clk += 4;
    _r.pc += n;
}

static inline void CALL(enum condition c, uint16_t n)
{
    switch (c) {
        case NZ:
            if(IS_ZERO) {
                return;
            }
            break;
        case Z:
            if(!IS_ZERO) {
                return;
            }
            break;
        case NC:
            if(IS_CARRY) {
                return;
            }
            break;
        case C:
            if(!IS_CARRY) {
                return;
            }
            break;
        case T:
            break;
    }
    _r.clk += 12;
    write_word(--_r.sp, _r.pc);
    _r.sp -= 1;
    _r.pc = n;
}

static inline void RST(uint16_t n)
{
    write_word(--_r.sp, _r.pc);
    _r.sp -= 1;
    _r.pc = n;
}

static inline void RET_internal(enum condition c)
{
    switch (c) {
        case NZ:
            if(IS_ZERO) {
                return;
            }
            break;
        case Z:
            if(!IS_ZERO) {
                return;
            }
            break;
        case NC:
            if(IS_CARRY) {
                return;
            }
            break;
        case C:
            if(!IS_CARRY) {
                return;
            }
            break;
        case T:
            break;
    }
    _r.clk += 12;
    _r.pc = read_word(++_r.sp);
    _r.sp += 1;
}

static void RETI(void)
{
    _r.pc = read_word(_r.sp);
    _r.sp += 2;
    _r.clk += 12;
    _IME = true;
}

/*
 * Operation implementations
 */

static void LD_SP_d16(void)
{
    _r.sp = read_word(_r.pc);
    _r.pc += 2;
    _r.clk += 12;
}

static void XOR_A(void)
{
    XOR8(_r.a);
    _r.clk += 4;
}

static void LD_HL_d16(void)
{
    _r.hl = read_word(_r.pc);
    _r.pc += 2;
    _r.clk += 12;
}

static void LDD_HL_A(void)
{
    write_byte(_r.hl--, _r.a);
    _r.clk += 8;
}

static void BIT_7_H(void)
{
    BIT(7, _r.h);
    _r.clk += 8;
}

static void JR_NZ_r8(void)
{
    JR(NZ, read_byte(_r.pc++));
    _r.clk += 8;
}

static void LD_C_d8(void)
{
    _r.c = read_byte(_r.pc++);
    _r.clk += 8;
}

static void LD_A_d8(void)
{
    _r.a = read_byte(_r.pc++);
    _r.clk += 8;
}

static void LD_mC_A(void)
{
    write_byte((uint16_t) (0xFF00 + _r.c), _r.a);
    _r.clk += 8;
}

static void INC_C(void)
{
    INC8(&_r.c);
    _r.clk += 4;
}

static void LD_mHL_A(void)
{
    write_byte(_r.hl, _r.a);
    _r.clk += 8;
}

static void LDH_m8_A(void)
{
    write_byte((uint16_t) (0xFF00 + read_byte(_r.pc++)), _r.a);
    _r.clk += 12;
}

static void LD_DE_d16(void)
{
    _r.de = read_word(_r.pc);
    _r.pc += 2;
    _r.clk += 12;
}

static void LD_A_mDE(void)
{
    _r.a = read_byte(_r.de);
    _r.clk += 8;
}

static void CALL_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    CALL(T, a16);
    _r.clk += 12;
}

static void LD_C_A(void)
{
    _r.c = _r.a;
    _r.clk += 4;
}

static void LD_B_d8(void)
{
    _r.b = read_byte(_r.pc++);
    _r.clk += 8;
}

static void PUSH_BC(void)
{
    write_word(--_r.sp, _r.bc);
    _r.sp -= 1;
    _r.clk += 16;
}

static void RL_C(void)
{
    RL(&_r.c);
    _r.clk += 8;
}

static void RLA(void)
{
    RL(&_r.a);
    _r.clk += 4;
    _r.f &= 0x70;
}

static void POP_BC(void)
{
    _r.bc = read_word(++_r.sp);
    _r.sp += 1;
    _r.clk += 12;
}

static void DEC_B(void)
{
    DEC8(&_r.b);
    _r.clk += 4;
}

static void LDI_mHL_A(void)
{
    write_byte(_r.hl++, _r.a);
    _r.clk += 8;
}

static void INC_HL(void)
{
    INC16(&_r.hl);
    _r.clk += 8;
}

static void RET(void)
{
    RET_internal(T);
    _r.clk += 8;
}

static void LDH_A_m8(void)
{
    _r.a = read_byte((uint16_t) (0xFF00 + read_byte(_r.pc++)));
    _r.clk+= 12;
}

static void INC_DE(void)
{
    INC16(&_r.de);
    _r.clk += 8;
}

static void LD_A_E(void)
{
    _r.a = _r.e;
    _r.clk += 4;
}

static void CP_d8(void)
{
    CP8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void LD_m16_A(void)
{
    write_byte(read_word(_r.pc), _r.a);
    _r.pc += 2;
    _r.clk += 16;
}

static void DEC_A(void)
{
    DEC8(&_r.a);
    _r.clk += 4;
}

static void JR_Z_r8(void)
{
    JR(Z, read_byte(_r.pc++));
    _r.clk += 8;
}

static void DEC_C(void)
{
    DEC8(&_r.c);
    _r.clk += 4;
}

static void LD_L_d8(void)
{
    _r.l = read_byte(_r.pc++);
    _r.clk += 4;
}

static void JR_r8(void)
{
    JR(T, read_byte(_r.pc++));
    _r.clk += 8;
}

static void LD_H_A(void)
{
    _r.h = _r.a;
    _r.clk += 4;
}

static void LD_D_A(void)
{
    _r.d = _r.a;
    _r.clk += 4;
}

static void INC_B(void)
{
    INC8(&_r.b);
    _r.clk += 4;
}

static void LD_E_d8(void)
{
    _r.e = read_byte(_r.pc++);
    _r.clk += 8;
}

static void DEC_E(void)
{
    DEC8(&_r.e);
    _r.clk += 4;
}

static void INC_H(void)
{
    INC8(&_r.h);
    _r.clk += 4;
}

static void LD_A_H(void)
{
    _r.a = _r.h;
    _r.clk += 4;
}

static void SUB_B(void)
{
    SUB8(_r.b);
    _r.clk += 4;
}

static void DEC_D(void)
{
    DEC8(&_r.d);
    _r.clk += 4;
}

static void LD_D_d8(void)
{
    _r.d = read_byte(_r.pc++);
    _r.clk += 8;
}

static void CP_mHL(void)
{
    CP8(read_byte(_r.hl));
    _r.clk += 8;
}

static void LD_A_L(void)
{
    _r.a = _r.l;
    _r.clk += 4;
}

static void LD_A_B(void)
{
    _r.a = _r.b;
    _r.clk += 4;
}

static void ADD_mHL(void)
{
    ADD8(read_byte(_r.hl));
    _r.clk += 8;
}

static void JP_a16(void)
{
    JP(T, read_word(_r.pc++));
    _r.clk += 12;
}

static void LD_mHL_d8(void)
{
    write_byte(_r.hl, read_byte(_r.pc++));
    _r.clk += 12;
}

static instruction _cb_map[NUM_OPCODES] = {
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, RL_C, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, BIT_7_H, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
        YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
};

static void PREFIX_CB(void)
{
    _cb_map[read_byte(_r.pc++)]();
    _r.clk += 4;
}

static instruction _map[NUM_OPCODES] = {
        XX, XX, XX, XX, INC_B, DEC_B, LD_B_d8, XX, XX, XX, XX, XX, INC_C, DEC_C, LD_C_d8, XX,
        XX, LD_DE_d16, XX, INC_DE, XX, DEC_D, LD_D_d8, RLA, JR_r8, XX, LD_A_mDE, XX, XX, DEC_E, LD_E_d8, XX,
        JR_NZ_r8, LD_HL_d16, LDI_mHL_A, INC_HL, INC_H, XX, XX, XX, JR_Z_r8, XX, XX, XX, XX, XX, LD_L_d8, XX,
        XX, LD_SP_d16, LDD_HL_A, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, DEC_A, LD_A_d8, XX,
        XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, LD_C_A,
        XX, XX, XX, XX, XX, XX, XX, LD_D_A, XX, XX, XX, XX, XX, XX, XX, XX,
        XX, XX, XX, XX, XX, XX, XX, LD_H_A, XX, XX, XX, XX, XX, XX, XX, XX,
        XX, XX, XX, XX, XX, XX, XX, LD_mHL_A, LD_A_B, XX, XX, LD_A_E, LD_A_H, LD_A_L, XX, XX,
        XX, XX, XX, XX, XX, XX, ADD_mHL, XX, XX, XX, XX, XX, XX, XX, XX, XX,
        SUB_B, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
        XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XOR_A,
        XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, CP_mHL, XX,
        XX, POP_BC, XX, XX, XX, PUSH_BC, XX, XX, XX, RET, XX, PREFIX_CB, XX, CALL_a16, XX, XX,
        XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
        LDH_m8_A, XX, LD_mC_A, XX, XX, XX, XX, XX, XX, XX, LD_m16_A, XX, XX, XX, XX, XX,
        LDH_A_m8, XX, XX, DI, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, CP_d8, XX,
};

void interrupt(enum int_src src)
{
    if(_IME && (_IE & src)) {
        _IF |= src;
    }
    if(src == BUTTON_PRESSED) {
        _STOP = false;
    }
}

static inline int interrupt_check(void)
{
    uint8_t interrupt = _IE & _IF;
    if(_IME && interrupt) {
        _IME = false;
        _HALT = false;

        if(interrupt & VBLANK) {
            RST(0x40);
        } else if (interrupt & LCDC) {
            RST(0x48);
        } else if(interrupt & TIMER_OVERFLOW) {
            RST(0x50);
        } else if(interrupt & SERIAL_TRANSFER) {
            RST(0x58);
        } else if(interrupt & BUTTON_PRESSED) {
            RST(0x60);
        } else {
            return 0;
        }
        return 1;
    }
    return 0;
}

void dispatch(void)
{
    if(!_STOP) {
        bool _local_di = _DI_pending;
        bool _local_ei = _EI_pending;

        if(!interrupt_check()) {
            if(!_HALT) {
                _map[read_byte(_r.pc++)]();
                if(_local_di) {
                    _IME = false;
                    _DI_pending = false;
                }
                if(_local_ei) {
                    _IME = true;
                    _EI_pending = false;
                }
            }
        }
    }
}

void cpu_reset(void)
{
    _r.af = 0x0000;
    _r.bc = 0x0000;
    _r.de = 0x0000;
    _r.hl = 0x0000;
    _r.pc = 0x0000;
    _r.sp = 0xFFFE;
    _r.clk = 0;

    _IE = 0x00;
    _IF = 0x00;

    _IME = false;
    _HALT = false;
    _STOP = false;

    _DI_pending = false;
    _EI_pending = false;
}