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
        .sp = 0x0000,
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
    _r.sp -= 2;
    write_word(_r.sp, _r.pc);
    _r.pc = n;
}

static inline void RST(uint16_t n)
{
    _r.sp -= 2;
    write_word(_r.sp, _r.pc);
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
    _r.pc = read_word(_r.sp);
    _r.sp += 2;
}

static void RETI(void)
{
    _r.pc = read_word(_r.sp);
    _r.sp += 2;
    _r.clk += 12;
    _IME = true;
}

static void PUSH(uint16_t nn)
{
    _r.sp -= 2;
    write_word(_r.sp, nn);
}

static void POP(uint16_t *nn)
{
    *nn = read_word(_r.sp);
    _r.sp += 2;
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
    PUSH(_r.bc);
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
    POP(&_r.bc);
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
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    JP(T, a16);
    _r.clk += 12;
}

static void LD_mHL_d8(void)
{
    write_byte(_r.hl, read_byte(_r.pc++));
    _r.clk += 12;
}

static void LDI_A_mHL(void)
{
    _r.a = read_byte(_r.hl++);
    _r.clk += 8;
}

static void LD_BC_d16(void)
{
    _r.bc = read_word(_r.pc);
    _r.pc += 2;
    _r.clk += 12;
}

static void DEC_BC(void)
{
    DEC16(&_r.bc);
    _r.clk += 8;
}

static void OR_C(void)
{
    OR8(_r.c);
    _r.clk += 4;
}

static void PUSH_AF(void)
{
    PUSH(_r.af);
    _r.clk += 16;
}

static void PUSH_DE(void)
{
    PUSH(_r.de);
    _r.clk += 16;
}

static void PUSH_HL(void)
{
    PUSH(_r.hl);
    _r.clk += 16;
}

static void AND_A(void)
{
    AND8(_r.a);
    _r.clk += 4;
}

static void AND_d8(void)
{
    AND8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void SWAP_A(void)
{
    SWAP(&_r.a);
    _r.clk += 8;
}

static void LD_B_A(void)
{
    _r.b = _r.a;
    _r.clk += 4;
}

static void OR_B(void)
{
    OR8(_r.b);
    _r.clk += 4;
}

static void XOR_C(void)
{
    XOR8(_r.c);
    _r.clk += 4;
}

static void AND_C(void)
{
    AND8(_r.c);
    _r.clk += 4;
}

static void LD_A_C(void)
{
    _r.a = _r.c;
    _r.clk += 4;
}

static void RST28(void)
{
    RST(0x28);
    _r.clk += 16;
}

static void ADD_A(void)
{
    ADD8(_r.a);
    _r.clk += 4;
}

static void POP_HL(void)
{
    POP(&_r.hl);
    _r.clk += 12;
}

static void LD_E_A(void)
{
    _r.e = _r.a;
    _r.clk += 4;
}

static void ADD_HL_DE(void)
{
    ADD16(&_r.hl, _r.de);
    _r.clk += 8;
}

static void LD_E_mHL(void)
{
    _r.e = read_byte(_r.hl);
    _r.clk += 8;
}

static void LD_D_mHL(void)
{
    _r.d = read_byte(_r.hl);
    _r.clk += 8;
}

static void JP_mHL(void)
{
    JP(T, _r.hl);
    _r.clk += 4;
}

static void RES_0_A(void)
{
    RES(0, &_r.a);
    _r.clk += 8;
}

static void RET_NZ(void)
{
    RET_internal(NZ);
    _r.clk += 8;
}

static void LD_A_m16(void)
{
    uint16_t m16 = read_word(_r.pc);
    _r.pc += 2;
    _r.a = read_byte(m16);
    _r.clk += 16;
}

static void RET_Z(void)
{
    RET_internal(Z);
    _r.clk += 8;
}

static void INC_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    INC8(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void INC_A(void)
{
    INC8(&_r.a);
    _r.clk += 4;
}

static void POP_DE(void)
{
    POP(&_r.de);
    _r.clk += 12;
}

static void POP_AF(void)
{
    POP(&_r.af);
    _r.clk += 12;
}

static void LD_mDE_A(void)
{
    write_byte(_r.de, _r.a);
    _r.clk += 8;
}

static void INC_E(void)
{
    INC8(&_r.e);
    _r.clk += 4;
}

static void JP_Z_d16(void)
{
    uint16_t d16 = read_word(_r.pc);
    _r.pc += 2;
    JP(Z, d16);
    _r.clk += 12;
}

static void LD_A_mHL(void)
{
    _r.a = read_byte(_r.hl);
    _r.clk += 8;
}

static void DEC_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    DEC8(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void INC_L(void)
{
    INC8(&_r.l);
    _r.clk += 4;
}

static void SLA_A(void)
{
    SLA(&_r.a);
    _r.clk += 8;
}

static void ADD_HL_BC(void)
{
    ADD16(&_r.hl, _r.bc);
    _r.clk += 8;
}

static void LD_C_mHL(void)
{
    _r.c = read_byte(_r.hl);
    _r.clk += 8;
}

static void LD_B_mHL(void)
{
    _r.b = read_byte(_r.hl);
    _r.clk += 8;
}

static void LD_L_C(void)
{
    _r.l = _r.c;
    _r.clk += 4;
}

static void LD_H_B(void)
{
    _r.h = _r.b;
    _r.clk += 4;
}

static void LD_A_mBC(void)
{
    _r.a = read_byte(_r.bc);
    _r.clk += 8;
}

static void INC_BC(void)
{
    INC16(&_r.bc);
    _r.clk += 8;
}

static void ADD_L(void)
{
    ADD8(_r.l);
    _r.clk += 4;
}

static void LD_L_A(void)
{
    _r.l = _r.a;
    _r.clk += 4;
}

static void JP_NZ_d16(void)
{
    uint16_t d16 = read_word(_r.pc);
    _r.pc += 2;
    JP(NZ, d16);
    _r.clk += 12;
}

static void LDD_A_mHL(void)
{
    _r.a = read_byte(_r.hl--);
    _r.clk += 8;
}

static void LD_A_D(void)
{
    _r.a = _r.d;
    _r.clk += 4;
}

static void LD_mHL_E(void)
{
    write_byte(_r.hl, _r.e);
    _r.clk += 8;
}

static void LD_mHL_D(void)
{
    write_byte(_r.hl, _r.d);
    _r.clk += 8;
}

static void LD_mHL_C(void)
{
    write_byte(_r.hl, _r.c);
    _r.clk += 8;
}

static void DEC_L(void)
{
    DEC8(&_r.l);
    _r.clk += 4;
}

static void ADD_d8(void)
{
    ADD8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void LD_E_L(void)
{
    _r.e = _r.l;
    _r.clk += 4;
}

static void LD_D_H(void)
{
    _r.d = _r.h;
    _r.clk += 4;
}

static void BIT_7_A(void)
{
    BIT(7, _r.a);
    _r.clk += 8;
}

static void OR_d8(void)
{
    OR8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void RES_0_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(0, &mHL);
    write_byte(_r.hl, mHL);
}

static void LD_L_E(void)
{
    _r.l = _r.e;
    _r.clk += 4;
}

static void LD_H_D(void)
{
    _r.h = _r.d;
    _r.clk += 4;
}

static void LD_B_B(void)
{
    _r.b = _r.b;
    _r.clk += 4;
}

static void BIT_2_B(void)
{
    BIT(2, _r.b);
    _r.clk += 8;
}

static void BIT_4_B(void)
{
    BIT(4, _r.b);
    _r.clk += 8;
}

static void BIT_5_B(void)
{
    BIT(5, _r.b);
    _r.clk += 8;
}

static void BIT_3_B(void)
{
    BIT(3, _r.b);
    _r.clk += 8;
}

static void BIT_7_mHL(void)
{
    BIT(7, read_byte(_r.hl));
    _r.clk += 16;
}

static void SRL_A(void)
{
    SRL(&_r.a);
    _r.clk += 8;
}

static void LD_H_d8(void)
{
    _r.h = read_byte(_r.pc++);
    _r.clk += 8;
}

static void BIT_0_B(void)
{
    BIT(0, _r.b);
    _r.clk += 8;
}

static void BIT_3_A(void)
{
    BIT(3, _r.a);
    _r.clk += 8;
}

static void SWAP_E(void)
{
    SWAP(&_r.e);
    _r.clk += 8;
}

static void RLCA(void)
{
    RLC(&_r.a);
    _r.clk += 4;
    _r.f &= 0x70;
}

static void BIT_6_A(void)
{
    BIT(6, _r.a);
    _r.clk += 8;
}

static void ADD_B(void)
{
    ADD8(_r.b);
    _r.clk += 4;
}

static void ADC_C(void)
{
    ADC8(_r.c);
    _r.clk += 4;
}

static void BIT_5_A(void)
{
    BIT(5, _r.a);
    _r.clk += 8;
}

static void CP_B(void)
{
    CP8(_r.b);
    _r.clk += 4;
}

static void XOR_B(void)
{
    XOR8(_r.b);
    _r.clk += 4;
}

static void AND_B(void)
{
    AND8(_r.b);
    _r.clk += 4;
}

static void BIT_1_B(void)
{
    BIT(1, _r.b);
    _r.clk += 8;
}

static void BIT_4_C(void)
{
    BIT(4, _r.c);
    _r.clk += 8;
}

static void BIT_5_C(void)
{
    BIT(5, _r.c);
    _r.clk += 8;
}

static void SUB_d8(void)
{
    SUB8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void ADC_mHL(void)
{
    ADC8(read_byte(_r.hl));
    _r.clk += 8;
}

static void RET_NC(void)
{
    RET_internal(NC);
    _r.clk += 8;
}

static void CP_C(void)
{
    CP8(_r.c);
    _r.clk += 4;
}

static void BIT_0_A(void)
{
    BIT(0, _r.a);
    _r.clk += 8;
}

static void JR_NC_r8(void)
{
    JR(NC, read_byte(_r.pc++));
    _r.clk += 8;
}

static void LD_H_C(void)
{
    _r.h = _r.c;
    _r.clk += 4;
}

static void RET_C(void)
{
    RET_internal(C);
    _r.clk += 8;
}

static void DEC_DE(void)
{
    DEC16(&_r.de);
    _r.clk += 8;
}

static void OR_A(void)
{
    OR8(_r.a);
    _r.clk += 4;
}

static void JR_C_r8(void)
{
    JR(C, read_byte(_r.pc++));
    _r.clk += 8;
}

static instruction _cb_map[NUM_OPCODES] = {
/* 0x */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* 1x */YY, RL_C, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* 2x */YY, YY, YY, YY, YY, YY, YY, SLA_A, YY, YY, YY, YY, YY, YY, YY, YY,
/* 3x */YY, YY, YY, SWAP_E, YY, YY, YY, SWAP_A, YY, YY, YY, YY, YY, YY, YY, SRL_A,
/* 4x */BIT_0_B, YY, YY, YY, YY, YY, YY, BIT_0_A, BIT_1_B, YY, YY, YY, YY, YY, YY, YY,
/* 5x */BIT_2_B, YY, YY, YY, YY, YY, YY, YY, BIT_3_B, YY, YY, YY, YY, YY, YY, BIT_3_A,
/* 6x */BIT_4_B, BIT_4_C, YY, YY, YY, YY, YY, YY, BIT_5_B, BIT_5_C, YY, YY, YY, YY, YY, BIT_5_A,
/* 7x */YY, YY, YY, YY, YY, YY, YY, BIT_6_A, YY, YY, YY, YY, BIT_7_H, YY, BIT_7_mHL, BIT_7_A,
/* 8x */YY, YY, YY, YY, YY, YY, RES_0_mHL, RES_0_A, YY, YY, YY, YY, YY, YY, YY, YY,
/* 9x */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Ax */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Bx */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Cx */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Dx */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Ex */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
/* Fx */YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY, YY,
};

static void PREFIX_CB(void)
{
    _cb_map[read_byte(_r.pc++)]();
//    _r.clk += 4;
}

static instruction _map[NUM_OPCODES] = {
/* 0x */NOP, LD_BC_d16, XX, INC_BC, INC_B, DEC_B, LD_B_d8, RLCA, XX, ADD_HL_BC, LD_A_mBC, DEC_BC, INC_C, DEC_C, LD_C_d8, XX,
/* 1x */XX, LD_DE_d16, LD_mDE_A, INC_DE, XX, DEC_D, LD_D_d8, RLA, JR_r8, ADD_HL_DE, LD_A_mDE, DEC_DE, INC_E, DEC_E, LD_E_d8, XX,
/* 2x */JR_NZ_r8, LD_HL_d16, LDI_mHL_A, INC_HL, INC_H, XX, LD_H_d8, DAA, JR_Z_r8, XX, LDI_A_mHL, XX, INC_L, DEC_L, LD_L_d8, CPL,
/* 3x */JR_NC_r8, LD_SP_d16, LDD_HL_A, XX, INC_mHL, DEC_mHL, LD_mHL_d8, XX, JR_C_r8, XX, LDD_A_mHL, XX, INC_A, DEC_A, LD_A_d8, XX,
/* 4x */LD_B_B, XX, XX, XX, XX, XX, LD_B_mHL, LD_B_A, XX, XX, XX, XX, XX, XX, LD_C_mHL, LD_C_A,
/* 5x */XX, XX, XX, XX, LD_D_H, XX, LD_D_mHL, LD_D_A, XX, XX, XX, XX, XX, LD_E_L, LD_E_mHL, LD_E_A,
/* 6x */LD_H_B, LD_H_C, LD_H_D, XX, XX, XX, XX, LD_H_A, XX, LD_L_C, XX, LD_L_E, XX, XX, XX, LD_L_A,
/* 7x */XX, LD_mHL_C, LD_mHL_D, LD_mHL_E, XX, XX, XX, LD_mHL_A, LD_A_B, LD_A_C, LD_A_D, LD_A_E, LD_A_H, LD_A_L, LD_A_mHL, XX,
/* 8x */ADD_B, XX, XX, XX, XX, ADD_L, ADD_mHL, ADD_A, XX, ADC_C, XX, XX, XX, XX, ADC_mHL, XX,
/* 9x */SUB_B, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
/* Ax */AND_B, AND_C, XX, XX, XX, XX, XX, AND_A, XOR_B, XOR_C, XX, XX, XX, XX, XX, XOR_A,
/* Bx */OR_B, OR_C, XX, XX, XX, XX, XX, OR_A, CP_B, CP_C, XX, XX, XX, XX, CP_mHL, XX,
/* Cx */RET_NZ, POP_BC, JP_NZ_d16, JP_a16, XX, PUSH_BC, ADD_d8, XX, RET_Z, RET, JP_Z_d16, PREFIX_CB, XX, CALL_a16, XX, XX,
/* Dx */RET_NC, POP_DE, XX, XX, XX, PUSH_DE, SUB_d8, XX, RET_C, RETI, XX, XX, XX, XX, XX, XX,
/* Ex */LDH_m8_A, POP_HL, LD_mC_A, XX, XX, PUSH_HL, AND_d8, XX, XX, JP_mHL, LD_m16_A, XX, XX, XX, XX, RST28,
/* Fx */LDH_A_m8, POP_AF, XX, DI, XX, PUSH_AF, OR_d8, XX, XX, XX, LD_A_m16, EI, XX, XX, CP_d8, XX,
};

void interrupt(enum int_src src)
{
    _IF |= src;
    if(src == BUTTON_PRESSED) {
        _STOP = false;
    }
}

static inline void interrupt_check(void)
{
    uint8_t interrupt = _IE & _IF;
    if(_IME && interrupt) {
        _IME = false;
        _HALT = false;

        if(interrupt & VBLANK) {
            _IF &= ~VBLANK;
            RST(0x40);
        } else if (interrupt & LCDC) {
            _IF &= ~LCDC;
            RST(0x48);
        } else if(interrupt & TIMER_OVERFLOW) {
            _IF &= ~TIMER_OVERFLOW;
            RST(0x50);
        } else if(interrupt & SERIAL_TRANSFER) {
            _IF &= ~SERIAL_TRANSFER;
            RST(0x58);
        } else if(interrupt & BUTTON_PRESSED) {
            _IF &= ~BUTTON_PRESSED;
            RST(0x60);
        }
    }
}

void dispatch(void)
{
    if(!_STOP) {
        bool _local_di = _DI_pending;
        bool _local_ei = _EI_pending;

        if(!_HALT) {
            _map[read_byte(_r.pc++)]();
        }

        interrupt_check();
        
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