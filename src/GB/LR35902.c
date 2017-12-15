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

/**********************************/
/** Default function map opcodes **/
/**********************************/

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

static void CALL_d16(void)
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

static void JP_Z_a16(void)
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

static void JP_NZ_a16(void)
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

static void OR_d8(void)
{
    OR8(read_byte(_r.pc++));
    _r.clk += 8;
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

static void LD_H_d8(void)
{
    _r.h = read_byte(_r.pc++);
    _r.clk += 8;
}

static void RLCA(void)
{
    RLC(&_r.a);
    _r.clk += 4;
    _r.f &= 0x70;
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

static void XOR_d8(void)
{
    XOR8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void SUB_mHL(void)
{
    SUB8(read_byte(_r.hl));
    _r.clk += 8;
}

static void DEC_HL(void)
{
    DEC16(&_r.hl);
    _r.clk += 8;
}

static void ADD_D(void)
{
    ADD8(_r.d);
    _r.clk += 4;
}

static void DEC_H(void)
{
    DEC8(&_r.h);
    _r.clk += 4;
}

static void OR_D(void)
{
    OR8(_r.d);
    _r.clk += 4;
}

static void LD_mBC_A(void)
{
    write_byte(_r.bc, _r.a);
    _r.clk += 8;
}

static void LD_m16_SP(void)
{
    uint16_t m16 = read_word(_r.pc);
    _r.pc += 2;
    write_word(m16, _r.sp);
    _r.clk += 20;
}

static void RRCA(void)
{
    RRC(&_r.a);
    _r.f &= 0x10;
    _r.clk += 4;
}

static void INC_D(void)
{
    INC8(&_r.d);
    _r.clk += 4;
}

static void RRA(void)
{
    RR(&_r.a);
    _r.f &= 0x10;
    _r.clk += 4;
}

static void ADD_HL_HL(void)
{
    ADD16(&_r.hl, _r.hl);
    _r.clk += 8;
}

static void INC_SP(void)
{
    INC16(&_r.sp);
    _r.clk += 8;
}

static void ADD_HL_SP(void)
{
    ADD16(&_r.hl, _r.sp);
    _r.clk += 8;
}

static void DEC_SP(void)
{
    DEC16(&_r.sp);
    _r.clk += 8;
}

static void LD_B_C(void)
{
    _r.b = _r.c;
    _r.clk += 4;
}

static void LD_B_D(void)
{
    _r.b = _r.d;
    _r.clk += 4;
}

static void LD_B_E(void)
{
    _r.b = _r.e;
    _r.clk += 4;
}

static void LD_B_H(void)
{
    _r.b = _r.h;
    _r.clk += 4;
}

static void LD_B_L(void)
{
    _r.b = _r.l;
    _r.clk += 4;
}

static void LD_C_B(void)
{
    _r.c = _r.b;
    _r.clk += 4;
}

static void LD_C_C(void)
{
    _r.c = _r.c;
    _r.clk += 4;
}

static void LD_C_D(void)
{
    _r.c = _r.d;
    _r.clk += 4;
}

static void LD_C_E(void)
{
    _r.c = _r.e;
    _r.clk += 4;
}

static void LD_C_H(void)
{
    _r.c = _r.h;
    _r.clk += 4;
}

static void LD_C_L(void)
{
    _r.c = _r.l;
    _r.clk += 4;
}

static void LD_D_B(void)
{
    _r.d = _r.b;
    _r.clk += 4;
}

static void LD_D_C(void)
{
    _r.d = _r.c;
    _r.clk += 4;
}

static void LD_D_D(void)
{
    _r.d = _r.d;
    _r.clk += 4;
}

static void LD_D_E(void)
{
    _r.d = _r.e;
    _r.clk += 4;
}

static void LD_D_L(void)
{
    _r.d = _r.l;
    _r.clk += 4;
}

static void LD_E_B(void)
{
    _r.e = _r.b;
    _r.clk += 4;
}

static void LD_E_C(void)
{
    _r.e = _r.c;
    _r.clk += 4;
}

static void LD_E_D(void)
{
    _r.e = _r.d;
    _r.clk += 4;
}

static void LD_E_E(void)
{
    _r.e = _r.e;
    _r.clk += 4;
}

static void LD_E_H(void)
{
    _r.e = _r.h;
    _r.clk += 4;
}

static void LD_H_E(void)
{
    _r.h = _r.e;
    _r.clk += 4;
}

static void LD_H_H(void)
{
    _r.h = _r.h;
    _r.clk += 4;
}

static void LD_H_L(void)
{
    _r.h = _r.l;
    _r.clk += 4;
}

static void LD_H_mHL(void)
{
    _r.h = read_byte(_r.hl);
    _r.clk += 8;
}

static void LD_L_B(void)
{
    _r.l = _r.b;
    _r.clk += 4;
}

static void LD_L_D(void)
{
    _r.l = _r.d;
    _r.clk += 4;
}

static void LD_L_H(void)
{
    _r.l = _r.h;
    _r.clk += 4;
}

static void LD_L_L(void)
{
    _r.l = _r.l;
    _r.clk += 4;
}

static void LD_L_mHL(void)
{
    _r.l = read_byte(_r.hl);
    _r.clk += 8;
}

static void LD_mHL_B(void)
{
    write_byte(_r.hl, _r.b);
    _r.clk += 8;
}

static void LD_mHL_H(void)
{
    write_byte(_r.hl, _r.h);
    _r.clk += 8;
}

static void LD_mHL_L(void)
{
    write_byte(_r.hl, _r.l);
    _r.clk += 8;
}

static void LD_A_A(void)
{
    _r.a = _r.a;
    _r.clk += 4;
}

static void ADD_C(void)
{
    ADD8(_r.c);
    _r.clk += 4;
}

static void ADD_E(void)
{
    ADD8(_r.e);
    _r.clk += 4;
}

static void ADD_H(void)
{
    ADD8(_r.h);
    _r.clk += 4;
}

static void ADC_B(void)
{
    ADC8(_r.b);
    _r.clk += 4;
}

static void ADC_D(void)
{
    ADC8(_r.d);
    _r.clk += 4;
}

static void ADC_E(void)
{
    ADC8(_r.e);
    _r.clk += 4;
}

static void ADC_H(void)
{
    ADC8(_r.h);
    _r.clk += 4;
}

static void ADC_L(void)
{
    ADC8(_r.l);
    _r.clk += 4;
}

static void ADC_A(void)
{
    ADC8(_r.a);
    _r.clk += 4;
}

static void SUB_C(void)
{
    SUB8(_r.c);
    _r.clk += 4;
}

static void SUB_D(void)
{
    SUB8(_r.d);
    _r.clk += 4;
}

static void SUB_E(void)
{
    SUB8(_r.e);
    _r.clk += 4;
}

static void SUB_H(void)
{
    SUB8(_r.h);
    _r.clk += 4;
}

static void SUB_L(void)
{
    SUB8(_r.l);
    _r.clk += 4;
}

static void SUB_A(void)
{
    SUB8(_r.a);
    _r.clk += 4;
}

static void SBC_B(void)
{
    SBC8(_r.b);
    _r.clk += 4;
}

static void SBC_C(void)
{
    SBC8(_r.c);
    _r.clk += 4;
}

static void SBC_D(void)
{
    SBC8(_r.d);
    _r.clk += 4;
}

static void SBC_E(void)
{
    SBC8(_r.e);
    _r.clk += 4;
}

static void SBC_H(void)
{
    SBC8(_r.h);
    _r.clk += 4;
}

static void SBC_L(void)
{
    SBC8(_r.l);
    _r.clk += 4;
}

static void SBC_mHL(void)
{
    SBC8(read_byte(_r.hl));
    _r.clk += 8;
}

static void SBC_A(void)
{
    SBC8(_r.a);
    _r.clk += 4;
}

static void AND_D(void)
{
    AND8(_r.d);
    _r.clk += 4;
}

static void AND_E(void)
{
    AND8(_r.e);
    _r.clk += 4;
}

static void AND_H(void)
{
    AND8(_r.h);
    _r.clk += 4;
}

static void AND_L(void)
{
    AND8(_r.l);
    _r.clk += 4;
}

static void AND_mHL(void)
{
    AND8(read_byte(_r.hl));
    _r.clk += 8;
}

static void XOR_D(void)
{
    XOR8(_r.d);
    _r.clk += 4;
}

static void XOR_E(void)
{
    XOR8(_r.e);
    _r.clk += 4;
}

static void XOR_H(void)
{
    XOR8(_r.h);
    _r.clk += 4;
}

static void XOR_L(void)
{
    XOR8(_r.l);
    _r.clk += 4;
}

static void XOR_mHL(void)
{
    XOR8(read_byte(_r.hl));
    _r.clk += 8;
}

static void OR_E(void)
{
    OR8(_r.e);
    _r.clk += 4;
}

static void OR_H(void)
{
    OR8(_r.h);
    _r.clk += 4;
}

static void OR_L(void)
{
    OR8(_r.l);
    _r.clk += 4;
}

static void OR_mHL(void)
{
    OR8(read_byte(_r.hl));
    _r.clk += 8;
}

static void CP_D(void)
{
    CP8(_r.d);
    _r.clk += 4;
}

static void CP_E(void)
{
    CP8(_r.e);
    _r.clk += 4;
}

static void CP_H(void)
{
    CP8(_r.h);
    _r.clk += 4;
}

static void CP_L(void)
{
    CP8(_r.l);
    _r.clk += 4;
}

static void CP_A(void)
{
    CP8(_r.a);
    _r.clk += 4;
}

static void CALL_NZ_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    CALL(NZ, a16);
    _r.clk += 12;
}

static void RST00(void)
{
    RST(0x00);
    _r.clk += 16;
}

static void CALL_Z_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    CALL(Z, a16);
    _r.clk += 12;
}

static void ADC_d8(void)
{
    ADC8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void RST08(void)
{
    RST(0x08);
    _r.clk += 16;
}

static void JP_NC_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    JP(NC, a16);
    _r.clk += 12;
}

static void CALL_NC_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    CALL(NC, a16);
    _r.clk += 12;
}

static void RST10(void)
{
    RST(0x10);
    _r.clk += 16;
}

static void JP_C_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    JP(C, a16);
    _r.clk += 12;
}

static void CALL_C_a16(void)
{
    uint16_t a16 = read_word(_r.pc);
    _r.pc += 2;
    CALL(C, a16);
    _r.clk += 12;
}

static void SBC_d8(void)
{
    SBC8(read_byte(_r.pc++));
    _r.clk += 8;
}

static void RST18(void)
{
    RST(0x18);
    _r.clk += 16;
}

static void RST20(void)
{
    RST(0x20);
    _r.clk += 16;
}

static void ADD_SP_r8(void)
{
    ADD16(&_r.sp, read_byte(_r.pc++));
    _r.clk += 16;
}

static void LD_A_mC(void)
{
    _r.a = read_byte((uint16_t) (0xFF00 + _r.c));
    _r.clk += 8;
}

static void RST30(void)
{
    RST(0x30);
    _r.clk += 16;
}

static void LDHL_SP_r8(void)
{
    uint16_t _sp = _r.sp;
    ADD16(&_sp, read_byte(_r.pc++));
    _r.hl = _sp;
    _r.clk += 12;
}

static void LD_SP_HL(void)
{
    _r.sp = _r.hl;
    _r.clk += 8;
}

static void RST38(void)
{
    RST(0x38);
    _r.clk += 16;
}

/*****************************/
/** CB Function map opcodes **/
/*****************************/

static void RLC_B(void)
{
    RLC(&_r.b);
    _r.clk += 4;
}

static void RLC_C(void)
{
    RLC(&_r.c);
    _r.clk += 4;
}

static void RLC_D(void)
{
    RLC(&_r.d);
    _r.clk += 4;
}

static void RLC_E(void)
{
    RLC(&_r.e);
    _r.clk += 4;
}

static void RLC_H(void)
{
    RLC(&_r.h);
    _r.clk += 4;
}

static void RLC_L(void)
{
    RLC(&_r.l);
    _r.clk += 4;
}

static void RLC_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RLC(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RLC_A(void)
{
    RLC(&_r.a);
    _r.clk += 4;
}

static void RRC_B(void)
{
    RRC(&_r.b);
    _r.clk += 4;
}

static void RRC_C(void)
{
    RRC(&_r.c);
    _r.clk += 4;
}

static void RRC_D(void)
{
    RRC(&_r.d);
    _r.clk += 4;
}

static void RRC_E(void)
{
    RRC(&_r.e);
    _r.clk += 4;
}

static void RRC_H(void)
{
    RRC(&_r.h);
    _r.clk += 4;
}

static void RRC_L(void)
{
    RRC(&_r.l);
    _r.clk += 4;
}

static void RRC_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RRC(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RRC_A(void)
{
    RRC(&_r.a);
    _r.clk += 4;
}

static void RL_B(void)
{
    RL(&_r.b);
    _r.clk += 4;
}

static void RL_C(void)
{
    RL(&_r.c);
    _r.clk += 4;
}

static void RL_D(void)
{
    RL(&_r.d);
    _r.clk += 4;
}

static void RL_E(void)
{
    RL(&_r.e);
    _r.clk += 4;
}

static void RL_H(void)
{
    RL(&_r.h);
    _r.clk += 4;
}

static void RL_L(void)
{
    RL(&_r.l);
    _r.clk += 4;
}

static void RL_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RL(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RL_A(void)
{
    RL(&_r.a);
    _r.clk += 4;
}

static void RR_B(void)
{
    RR(&_r.b);
    _r.clk += 4;
}

static void RR_C(void)
{
    RR(&_r.c);
    _r.clk += 4;
}

static void RR_D(void)
{
    RR(&_r.d);
    _r.clk += 4;
}

static void RR_E(void)
{
    RR(&_r.e);
    _r.clk += 4;
}

static void RR_H(void)
{
    RR(&_r.h);
    _r.clk += 4;
}

static void RR_L(void)
{
    RR(&_r.l);
    _r.clk += 4;
}

static void RR_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RR(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RR_A(void)
{
    RR(&_r.a);
    _r.clk += 4;
}

static void SLA_B(void)
{
    SLA(&_r.b);
    _r.clk += 4;
}

static void SLA_C(void)
{
    SLA(&_r.c);
    _r.clk += 4;
}

static void SLA_D(void)
{
    SLA(&_r.d);
    _r.clk += 4;
}

static void SLA_E(void)
{
    SLA(&_r.e);
    _r.clk += 4;
}

static void SLA_H(void)
{
    SLA(&_r.h);
    _r.clk += 4;
}

static void SLA_L(void)
{
    SLA(&_r.l);
    _r.clk += 4;
}

static void SLA_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SLA(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SLA_A(void)
{
    SLA(&_r.a);
    _r.clk += 4;
}

static void SRA_B(void)
{
    SRA(&_r.b);
    _r.clk += 4;
}

static void SRA_C(void)
{
    SRA(&_r.c);
    _r.clk += 4;
}

static void SRA_D(void)
{
    SRA(&_r.d);
    _r.clk += 4;
}

static void SRA_E(void)
{
    SRA(&_r.e);
    _r.clk += 4;
}

static void SRA_H(void)
{
    SRA(&_r.h);
    _r.clk += 4;
}

static void SRA_L(void)
{
    SRA(&_r.l);
    _r.clk += 4;
}

static void SRA_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SRA(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SRA_A(void)
{
    SRA(&_r.a);
    _r.clk += 4;
}

static void SWAP_B(void)
{
    SWAP(&_r.b);
    _r.clk += 4;
}

static void SWAP_C(void)
{
    SWAP(&_r.c);
    _r.clk += 4;
}

static void SWAP_D(void)
{
    SWAP(&_r.d);
    _r.clk += 4;
}

static void SWAP_E(void)
{
    SWAP(&_r.e);
    _r.clk += 4;
}

static void SWAP_H(void)
{
    SWAP(&_r.h);
    _r.clk += 4;
}

static void SWAP_L(void)
{
    SWAP(&_r.l);
    _r.clk += 4;
}

static void SWAP_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SWAP(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SWAP_A(void)
{
    SWAP(&_r.a);
    _r.clk += 4;
}

static void SRL_B(void)
{
    SRL(&_r.b);
    _r.clk += 4;
}

static void SRL_C(void)
{
    SRL(&_r.c);
    _r.clk += 4;
}

static void SRL_D(void)
{
    SRL(&_r.d);
    _r.clk += 4;
}

static void SRL_E(void)
{
    SRL(&_r.e);
    _r.clk += 4;
}

static void SRL_H(void)
{
    SRL(&_r.h);
    _r.clk += 4;
}

static void SRL_L(void)
{
    SRL(&_r.l);
    _r.clk += 4;
}

static void SRL_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SRL(&mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SRL_A(void)
{
    SRL(&_r.a);
    _r.clk += 4;
}

static void BIT_0_B(void)
{
    BIT(0, _r.b);
    _r.clk += 4;
}

static void BIT_0_C(void)
{
    BIT(0, _r.c);
    _r.clk += 4;
}

static void BIT_0_D(void)
{
    BIT(0, _r.d);
    _r.clk += 4;
}

static void BIT_0_E(void)
{
    BIT(0, _r.e);
    _r.clk += 4;
}

static void BIT_0_H(void)
{
    BIT(0, _r.h);
    _r.clk += 4;
}

static void BIT_0_L(void)
{
    BIT(0, _r.l);
    _r.clk += 4;
}

static void BIT_0_mHL(void)
{
    BIT(0, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_0_A(void)
{
    BIT(0, _r.a);
    _r.clk += 4;
}

static void BIT_1_B(void)
{
    BIT(1, _r.b);
    _r.clk += 4;
}

static void BIT_1_C(void)
{
    BIT(1, _r.c);
    _r.clk += 4;
}

static void BIT_1_D(void)
{
    BIT(1, _r.d);
    _r.clk += 4;
}

static void BIT_1_E(void)
{
    BIT(1, _r.e);
    _r.clk += 4;
}

static void BIT_1_H(void)
{
    BIT(1, _r.h);
    _r.clk += 4;
}

static void BIT_1_L(void)
{
    BIT(1, _r.l);
    _r.clk += 4;
}

static void BIT_1_mHL(void)
{
    BIT(1, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_1_A(void)
{
    BIT(1, _r.a);
    _r.clk += 4;
}

static void BIT_2_B(void)
{
    BIT(2, _r.b);
    _r.clk += 4;
}

static void BIT_2_C(void)
{
    BIT(2, _r.c);
    _r.clk += 4;
}

static void BIT_2_D(void)
{
    BIT(2, _r.d);
    _r.clk += 4;
}

static void BIT_2_E(void)
{
    BIT(2, _r.e);
    _r.clk += 4;
}

static void BIT_2_H(void)
{
    BIT(2, _r.h);
    _r.clk += 4;
}

static void BIT_2_L(void)
{
    BIT(2, _r.l);
    _r.clk += 4;
}

static void BIT_2_mHL(void)
{
    BIT(2, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_2_A(void)
{
    BIT(2, _r.a);
    _r.clk += 4;
}

static void BIT_3_B(void)
{
    BIT(3, _r.b);
    _r.clk += 4;
}

static void BIT_3_C(void)
{
    BIT(3, _r.c);
    _r.clk += 4;
}

static void BIT_3_D(void)
{
    BIT(3, _r.d);
    _r.clk += 4;
}

static void BIT_3_E(void)
{
    BIT(3, _r.e);
    _r.clk += 4;
}

static void BIT_3_H(void)
{
    BIT(3, _r.h);
    _r.clk += 4;
}

static void BIT_3_L(void)
{
    BIT(3, _r.l);
    _r.clk += 4;
}

static void BIT_3_mHL(void)
{
    BIT(3, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_3_A(void)
{
    BIT(3, _r.a);
    _r.clk += 4;
}

static void BIT_4_B(void)
{
    BIT(4, _r.b);
    _r.clk += 4;
}

static void BIT_4_C(void)
{
    BIT(4, _r.c);
    _r.clk += 4;
}

static void BIT_4_D(void)
{
    BIT(4, _r.d);
    _r.clk += 4;
}

static void BIT_4_E(void)
{
    BIT(4, _r.e);
    _r.clk += 4;
}

static void BIT_4_H(void)
{
    BIT(4, _r.h);
    _r.clk += 4;
}

static void BIT_4_L(void)
{
    BIT(4, _r.l);
    _r.clk += 4;
}

static void BIT_4_mHL(void)
{
    BIT(4, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_4_A(void)
{
    BIT(4, _r.a);
    _r.clk += 4;
}

static void BIT_5_B(void)
{
    BIT(5, _r.b);
    _r.clk += 4;
}

static void BIT_5_C(void)
{
    BIT(5, _r.c);
    _r.clk += 4;
}

static void BIT_5_D(void)
{
    BIT(5, _r.d);
    _r.clk += 4;
}

static void BIT_5_E(void)
{
    BIT(5, _r.e);
    _r.clk += 4;
}

static void BIT_5_H(void)
{
    BIT(5, _r.h);
    _r.clk += 4;
}

static void BIT_5_L(void)
{
    BIT(5, _r.l);
    _r.clk += 4;
}

static void BIT_5_mHL(void)
{
    BIT(5, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_5_A(void)
{
    BIT(5, _r.a);
    _r.clk += 4;
}

static void BIT_6_B(void)
{
    BIT(6, _r.b);
    _r.clk += 4;
}

static void BIT_6_C(void)
{
    BIT(6, _r.c);
    _r.clk += 4;
}

static void BIT_6_D(void)
{
    BIT(6, _r.d);
    _r.clk += 4;
}

static void BIT_6_E(void)
{
    BIT(6, _r.e);
    _r.clk += 4;
}

static void BIT_6_H(void)
{
    BIT(6, _r.h);
    _r.clk += 4;
}

static void BIT_6_L(void)
{
    BIT(6, _r.l);
    _r.clk += 4;
}

static void BIT_6_mHL(void)
{
    BIT(6, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_6_A(void)
{
    BIT(6, _r.a);
    _r.clk += 4;
}

static void BIT_7_B(void)
{
    BIT(7, _r.b);
    _r.clk += 4;
}

static void BIT_7_C(void)
{
    BIT(7, _r.c);
    _r.clk += 4;
}

static void BIT_7_D(void)
{
    BIT(7, _r.d);
    _r.clk += 4;
}

static void BIT_7_E(void)
{
    BIT(7, _r.e);
    _r.clk += 4;
}

static void BIT_7_H(void)
{
    BIT(7, _r.h);
    _r.clk += 4;
}

static void BIT_7_L(void)
{
    BIT(7, _r.l);
    _r.clk += 4;
}

static void BIT_7_mHL(void)
{
    BIT(7, read_byte(_r.hl));
    _r.clk += 12;
}

static void BIT_7_A(void)
{
    BIT(7, _r.a);
    _r.clk += 4;
}

static void RES_0_B(void)
{
    RES(0, &_r.b);
    _r.clk += 4;
}

static void RES_0_C(void)
{
    RES(0, &_r.c);
    _r.clk += 4;
}

static void RES_0_D(void)
{
    RES(0, &_r.d);
    _r.clk += 4;
}

static void RES_0_E(void)
{
    RES(0, &_r.e);
    _r.clk += 4;
}

static void RES_0_H(void)
{
    RES(0, &_r.h);
    _r.clk += 4;
}

static void RES_0_L(void)
{
    RES(0, &_r.l);
    _r.clk += 4;
}

static void RES_0_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(0, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_0_A(void)
{
    RES(0, &_r.a);
    _r.clk += 4;
}

static void RES_1_B(void)
{
    RES(1, &_r.b);
    _r.clk += 4;
}

static void RES_1_C(void)
{
    RES(1, &_r.c);
    _r.clk += 4;
}

static void RES_1_D(void)
{
    RES(1, &_r.d);
    _r.clk += 4;
}

static void RES_1_E(void)
{
    RES(1, &_r.e);
    _r.clk += 4;
}

static void RES_1_H(void)
{
    RES(1, &_r.h);
    _r.clk += 4;
}

static void RES_1_L(void)
{
    RES(1, &_r.l);
    _r.clk += 4;
}

static void RES_1_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(1, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_1_A(void)
{
    RES(1, &_r.a);
    _r.clk += 4;
}

static void RES_2_B(void)
{
    RES(2, &_r.b);
    _r.clk += 4;
}

static void RES_2_C(void)
{
    RES(2, &_r.c);
    _r.clk += 4;
}

static void RES_2_D(void)
{
    RES(2, &_r.d);
    _r.clk += 4;
}

static void RES_2_E(void)
{
    RES(2, &_r.e);
    _r.clk += 4;
}

static void RES_2_H(void)
{
    RES(2, &_r.h);
    _r.clk += 4;
}

static void RES_2_L(void)
{
    RES(2, &_r.l);
    _r.clk += 4;
}

static void RES_2_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(2, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_2_A(void)
{
    RES(2, &_r.a);
    _r.clk += 4;
}

static void RES_3_B(void)
{
    RES(3, &_r.b);
    _r.clk += 4;
}

static void RES_3_C(void)
{
    RES(3, &_r.c);
    _r.clk += 4;
}

static void RES_3_D(void)
{
    RES(3, &_r.d);
    _r.clk += 4;
}

static void RES_3_E(void)
{
    RES(3, &_r.e);
    _r.clk += 4;
}

static void RES_3_H(void)
{
    RES(3, &_r.h);
    _r.clk += 4;
}

static void RES_3_L(void)
{
    RES(3, &_r.l);
    _r.clk += 4;
}

static void RES_3_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(3, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_3_A(void)
{
    RES(3, &_r.a);
    _r.clk += 4;
}

static void RES_4_B(void)
{
    RES(4, &_r.b);
    _r.clk += 4;
}

static void RES_4_C(void)
{
    RES(4, &_r.c);
    _r.clk += 4;
}

static void RES_4_D(void)
{
    RES(4, &_r.d);
    _r.clk += 4;
}

static void RES_4_E(void)
{
    RES(4, &_r.e);
    _r.clk += 4;
}

static void RES_4_H(void)
{
    RES(4, &_r.h);
    _r.clk += 4;
}

static void RES_4_L(void)
{
    RES(4, &_r.l);
    _r.clk += 4;
}

static void RES_4_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(4, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_4_A(void)
{
    RES(4, &_r.a);
    _r.clk += 4;
}

static void RES_5_B(void)
{
    RES(5, &_r.b);
    _r.clk += 4;
}

static void RES_5_C(void)
{
    RES(5, &_r.c);
    _r.clk += 4;
}

static void RES_5_D(void)
{
    RES(5, &_r.d);
    _r.clk += 4;
}

static void RES_5_E(void)
{
    RES(5, &_r.e);
    _r.clk += 4;
}

static void RES_5_H(void)
{
    RES(5, &_r.h);
    _r.clk += 4;
}

static void RES_5_L(void)
{
    RES(5, &_r.l);
    _r.clk += 4;
}

static void RES_5_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(5, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_5_A(void)
{
    RES(5, &_r.a);
    _r.clk += 4;
}

static void RES_6_B(void)
{
    RES(6, &_r.b);
    _r.clk += 4;
}

static void RES_6_C(void)
{
    RES(6, &_r.c);
    _r.clk += 4;
}

static void RES_6_D(void)
{
    RES(6, &_r.d);
    _r.clk += 4;
}

static void RES_6_E(void)
{
    RES(6, &_r.e);
    _r.clk += 4;
}

static void RES_6_H(void)
{
    RES(6, &_r.h);
    _r.clk += 4;
}

static void RES_6_L(void)
{
    RES(6, &_r.l);
    _r.clk += 4;
}

static void RES_6_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(6, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_6_A(void)
{
    RES(6, &_r.a);
    _r.clk += 4;
}

static void RES_7_B(void)
{
    RES(7, &_r.b);
    _r.clk += 4;
}

static void RES_7_C(void)
{
    RES(7, &_r.c);
    _r.clk += 4;
}

static void RES_7_D(void)
{
    RES(7, &_r.d);
    _r.clk += 4;
}

static void RES_7_E(void)
{
    RES(7, &_r.e);
    _r.clk += 4;
}

static void RES_7_H(void)
{
    RES(7, &_r.h);
    _r.clk += 4;
}

static void RES_7_L(void)
{
    RES(7, &_r.l);
    _r.clk += 4;
}

static void RES_7_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    RES(7, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void RES_7_A(void)
{
    RES(7, &_r.a);
    _r.clk += 4;
}

static void SET_0_B(void)
{
    SET(0, &_r.b);
    _r.clk += 4;
}

static void SET_0_C(void)
{
    SET(0, &_r.c);
    _r.clk += 4;
}

static void SET_0_D(void)
{
    SET(0, &_r.d);
    _r.clk += 4;
}

static void SET_0_E(void)
{
    SET(0, &_r.e);
    _r.clk += 4;
}

static void SET_0_H(void)
{
    SET(0, &_r.h);
    _r.clk += 4;
}

static void SET_0_L(void)
{
    SET(0, &_r.l);
    _r.clk += 4;
}

static void SET_0_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(0, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_0_A(void)
{
    SET(0, &_r.a);
    _r.clk += 4;
}

static void SET_1_B(void)
{
    SET(1, &_r.b);
    _r.clk += 4;
}

static void SET_1_C(void)
{
    SET(1, &_r.c);
    _r.clk += 4;
}

static void SET_1_D(void)
{
    SET(1, &_r.d);
    _r.clk += 4;
}

static void SET_1_E(void)
{
    SET(1, &_r.e);
    _r.clk += 4;
}

static void SET_1_H(void)
{
    SET(1, &_r.h);
    _r.clk += 4;
}

static void SET_1_L(void)
{
    SET(1, &_r.l);
    _r.clk += 4;
}

static void SET_1_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(1, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_1_A(void)
{
    SET(1, &_r.a);
    _r.clk += 4;
}

static void SET_2_B(void)
{
    SET(2, &_r.b);
    _r.clk += 4;
}

static void SET_2_C(void)
{
    SET(2, &_r.c);
    _r.clk += 4;
}

static void SET_2_D(void)
{
    SET(2, &_r.d);
    _r.clk += 4;
}

static void SET_2_E(void)
{
    SET(2, &_r.e);
    _r.clk += 4;
}

static void SET_2_H(void)
{
    SET(2, &_r.h);
    _r.clk += 4;
}

static void SET_2_L(void)
{
    SET(2, &_r.l);
    _r.clk += 4;
}

static void SET_2_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(2, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_2_A(void)
{
    SET(2, &_r.a);
    _r.clk += 4;
}

static void SET_3_B(void)
{
    SET(3, &_r.b);
    _r.clk += 4;
}

static void SET_3_C(void)
{
    SET(3, &_r.c);
    _r.clk += 4;
}

static void SET_3_D(void)
{
    SET(3, &_r.d);
    _r.clk += 4;
}

static void SET_3_E(void)
{
    SET(3, &_r.e);
    _r.clk += 4;
}

static void SET_3_H(void)
{
    SET(3, &_r.h);
    _r.clk += 4;
}

static void SET_3_L(void)
{
    SET(3, &_r.l);
    _r.clk += 4;
}

static void SET_3_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(3, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_3_A(void)
{
    SET(3, &_r.a);
    _r.clk += 4;
}

static void SET_4_B(void)
{
    SET(4, &_r.b);
    _r.clk += 4;
}

static void SET_4_C(void)
{
    SET(4, &_r.c);
    _r.clk += 4;
}

static void SET_4_D(void)
{
    SET(4, &_r.d);
    _r.clk += 4;
}

static void SET_4_E(void)
{
    SET(4, &_r.e);
    _r.clk += 4;
}

static void SET_4_H(void)
{
    SET(4, &_r.h);
    _r.clk += 4;
}

static void SET_4_L(void)
{
    SET(4, &_r.l);
    _r.clk += 4;
}

static void SET_4_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(4, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_4_A(void)
{
    SET(4, &_r.a);
    _r.clk += 4;
}

static void SET_5_B(void)
{
    SET(5, &_r.b);
    _r.clk += 4;
}

static void SET_5_C(void)
{
    SET(5, &_r.c);
    _r.clk += 4;
}

static void SET_5_D(void)
{
    SET(5, &_r.d);
    _r.clk += 4;
}

static void SET_5_E(void)
{
    SET(5, &_r.e);
    _r.clk += 4;
}

static void SET_5_H(void)
{
    SET(5, &_r.h);
    _r.clk += 4;
}

static void SET_5_L(void)
{
    SET(5, &_r.l);
    _r.clk += 4;
}

static void SET_5_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(5, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_5_A(void)
{
    SET(5, &_r.a);
    _r.clk += 4;
}

static void SET_6_B(void)
{
    SET(6, &_r.b);
    _r.clk += 4;
}

static void SET_6_C(void)
{
    SET(6, &_r.c);
    _r.clk += 4;
}

static void SET_6_D(void)
{
    SET(6, &_r.d);
    _r.clk += 4;
}

static void SET_6_E(void)
{
    SET(6, &_r.e);
    _r.clk += 4;
}

static void SET_6_H(void)
{
    SET(6, &_r.h);
    _r.clk += 4;
}

static void SET_6_L(void)
{
    SET(6, &_r.l);
    _r.clk += 4;
}

static void SET_6_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(6, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_6_A(void)
{
    SET(6, &_r.a);
    _r.clk += 4;
}

static void SET_7_B(void)
{
    SET(7, &_r.b);
    _r.clk += 4;
}

static void SET_7_C(void)
{
    SET(7, &_r.c);
    _r.clk += 4;
}

static void SET_7_D(void)
{
    SET(7, &_r.d);
    _r.clk += 4;
}

static void SET_7_E(void)
{
    SET(7, &_r.e);
    _r.clk += 4;
}

static void SET_7_H(void)
{
    SET(7, &_r.h);
    _r.clk += 4;
}

static void SET_7_L(void)
{
    SET(7, &_r.l);
    _r.clk += 4;
}

static void SET_7_mHL(void)
{
    uint8_t mHL = read_byte(_r.hl);
    SET(7, &mHL);
    write_byte(_r.hl, mHL);
    _r.clk += 12;
}

static void SET_7_A(void)
{
    SET(7, &_r.a);
    _r.clk += 4;
}

static instruction _cb_map[NUM_OPCODES] = {
/*      x0       x1       x2       x3       x4       x5       x6         x7       x8       x9       xA       xB       xC       xD       xE         xF       */
/* 0x */RLC_B,   RLC_C,   RLC_D,   RLC_E,   RLC_H,   RLC_L,   RLC_mHL,   RLC_A,   RRC_B,   RRC_C,   RRC_D,   RRC_E,   RRC_H,   RRC_L,   RRC_mHL,   RRC_A,
/* 1x */RL_B,    RL_C,    RL_D,    RL_E,    RL_H,    RL_L,    RL_mHL,    RL_A,    RR_B,    RR_C,    RR_D,    RR_E,    RR_H,    RR_L,    RR_mHL,    RR_A,
/* 2x */SLA_B,   SLA_C,   SLA_D,   SLA_E,   SLA_H,   SLA_L,   SLA_mHL,   SLA_A,   SRA_B,   SRA_C,   SRA_D,   SRA_E,   SRA_H,   SRA_L,   SRA_mHL,   SRA_A,
/* 3x */SWAP_B,  SWAP_C,  SWAP_D,  SWAP_E,  SWAP_H,  SWAP_L,  SWAP_mHL,  SWAP_A,  SRL_B,   SRL_C,   SRL_D,   SRL_E,   SRL_H,   SRL_L,   SRL_mHL,   SRL_A,
/* 4x */BIT_0_B, BIT_0_C, BIT_0_D, BIT_0_E, BIT_0_H, BIT_0_L, BIT_0_mHL, BIT_0_A, BIT_1_B, BIT_1_C, BIT_1_D, BIT_1_E, BIT_1_H, BIT_1_L, BIT_1_mHL, BIT_1_A,
/* 5x */BIT_2_B, BIT_2_C, BIT_2_D, BIT_2_E, BIT_2_H, BIT_2_L, BIT_2_mHL, BIT_2_A, BIT_3_B, BIT_3_C, BIT_3_D, BIT_3_E, BIT_3_H, BIT_3_L, BIT_3_mHL, BIT_3_A,
/* 6x */BIT_4_B, BIT_4_C, BIT_4_D, BIT_4_E, BIT_4_H, BIT_4_L, BIT_4_mHL, BIT_4_A, BIT_5_B, BIT_5_C, BIT_5_D, BIT_5_E, BIT_5_H, BIT_5_L, BIT_5_mHL, BIT_5_A,
/* 7x */BIT_6_B, BIT_6_C, BIT_6_D, BIT_6_E, BIT_6_H, BIT_6_L, BIT_6_mHL, BIT_6_A, BIT_7_B, BIT_7_C, BIT_7_D, BIT_7_E, BIT_7_H, BIT_7_L, BIT_7_mHL, BIT_7_A,
/* 8x */RES_0_B, RES_0_C, RES_0_D, RES_0_E, RES_0_H, RES_0_L, RES_0_mHL, RES_0_A, RES_1_B, RES_1_C, RES_1_D, RES_1_E, RES_1_H, RES_1_L, RES_1_mHL, RES_1_A,
/* 9x */RES_2_B, RES_2_C, RES_2_D, RES_2_E, RES_2_H, RES_2_L, RES_2_mHL, RES_2_A, RES_3_B, RES_3_C, RES_3_D, RES_3_E, RES_3_H, RES_3_L, RES_3_mHL, RES_3_A,
/* Ax */RES_4_B, RES_4_C, RES_4_D, RES_4_E, RES_4_H, RES_4_L, RES_4_mHL, RES_4_A, RES_5_B, RES_5_C, RES_5_D, RES_5_E, RES_5_H, RES_5_L, RES_5_mHL, RES_5_A,
/* Bx */RES_6_B, RES_6_C, RES_6_D, RES_6_E, RES_6_H, RES_6_L, RES_6_mHL, RES_6_A, RES_7_B, RES_7_C, RES_7_D, RES_7_E, RES_7_H, RES_7_L, RES_7_mHL, RES_7_A,
/* Cx */SET_0_B, SET_0_C, SET_0_D, SET_0_E, SET_0_H, SET_0_L, SET_0_mHL, SET_0_A, SET_1_B, SET_1_C, SET_1_D, SET_1_E, SET_1_H, SET_1_L, SET_1_mHL, SET_1_A,
/* Dx */SET_2_B, SET_2_C, SET_2_D, SET_2_E, SET_2_H, SET_2_L, SET_2_mHL, SET_2_A, SET_3_B, SET_3_C, SET_3_D, SET_3_E, SET_3_H, SET_3_L, SET_3_mHL, SET_3_A,
/* Ex */SET_4_B, SET_4_C, SET_4_D, SET_4_E, SET_4_H, SET_4_L, SET_4_mHL, SET_4_A, SET_5_B, SET_5_C, SET_5_D, SET_5_E, SET_5_H, SET_5_L, SET_5_mHL, SET_5_A,
/* Fx */SET_6_B, SET_6_C, SET_6_D, SET_6_E, SET_6_H, SET_6_L, SET_6_mHL, SET_6_A, SET_7_B, SET_7_C, SET_7_D, SET_7_E, SET_7_H, SET_7_L, SET_7_mHL, SET_7_A,
};

static void PREFIX_CB(void)
{
    _cb_map[read_byte(_r.pc++)]();
    _r.clk += 4;
}

static instruction _map[NUM_OPCODES] = {
/*      x0        x1         x2         x3        x4           x5        x6         x7        x8          x9         xA         xB         xC          xD        xE        xF     */
/* 0x */NOP,      LD_BC_d16, LD_mBC_A,  INC_BC,   INC_B,       DEC_B,    LD_B_d8,   RLCA,     LD_m16_SP,  ADD_HL_BC, LD_A_mBC,  DEC_BC,    INC_C,      DEC_C,    LD_C_d8,  RRCA,
/* 1x */STOP,     LD_DE_d16, LD_mDE_A,  INC_DE,   INC_D,       DEC_D,    LD_D_d8,   RLA,      JR_r8,      ADD_HL_DE, LD_A_mDE,  DEC_DE,    INC_E,      DEC_E,    LD_E_d8,  RRA,
/* 2x */JR_NZ_r8, LD_HL_d16, LDI_mHL_A, INC_HL,   INC_H,       DEC_H,    LD_H_d8,   DAA,      JR_Z_r8,    ADD_HL_HL, LDI_A_mHL, DEC_HL,    INC_L,      DEC_L,    LD_L_d8,  CPL,
/* 3x */JR_NC_r8, LD_SP_d16, LDD_HL_A,  INC_SP,   INC_mHL,     DEC_mHL,  LD_mHL_d8, SCF,      JR_C_r8,    ADD_HL_SP, LDD_A_mHL, DEC_SP,    INC_A,      DEC_A,    LD_A_d8,  CCF,
/* 4x */LD_B_B,   LD_B_C,    LD_B_D,    LD_B_E,   LD_B_H,      LD_B_L,   LD_B_mHL,  LD_B_A,   LD_C_B,     LD_C_C,    LD_C_D,    LD_C_E,    LD_C_H,     LD_C_L,   LD_C_mHL, LD_C_A,
/* 5x */LD_D_B,   LD_D_C,    LD_D_D,    LD_D_E,   LD_D_H,      LD_D_L,   LD_D_mHL,  LD_D_A,   LD_E_B,     LD_E_C,    LD_E_D,    LD_E_E,    LD_E_H,     LD_E_L,   LD_E_mHL, LD_E_A,
/* 6x */LD_H_B,   LD_H_C,    LD_H_D,    LD_H_E,   LD_H_H,      LD_H_L,   LD_H_mHL,  LD_H_A,   LD_L_B,     LD_L_C,    LD_L_D,    LD_L_E,    LD_L_H,     LD_L_L,   LD_L_mHL, LD_L_A,
/* 7x */LD_mHL_B, LD_mHL_C,  LD_mHL_D,  LD_mHL_E, LD_mHL_H,    LD_mHL_L, HALT,      LD_mHL_A, LD_A_B,     LD_A_C,    LD_A_D,    LD_A_E,    LD_A_H,     LD_A_L,   LD_A_mHL, LD_A_A,
/* 8x */ADD_B,    ADD_C,     ADD_D,     ADD_E,    ADD_H,       ADD_L,    ADD_mHL,   ADD_A,    ADC_B,      ADC_C,     ADC_D,     ADC_E,     ADC_H,      ADC_L,    ADC_mHL,  ADC_A,
/* 9x */SUB_B,    SUB_C,     SUB_D,     SUB_E,    SUB_H,       SUB_L,    SUB_mHL,   SUB_A,    SBC_B,      SBC_C,     SBC_D,     SBC_E,     SBC_H,      SBC_L,    SBC_mHL,  SBC_A,
/* Ax */AND_B,    AND_C,     AND_D,     AND_E,    AND_H,       AND_L,    AND_mHL,   AND_A,    XOR_B,      XOR_C,     XOR_D,     XOR_E,     XOR_H,      XOR_L,    XOR_mHL,  XOR_A,
/* Bx */OR_B,     OR_C,      OR_D,      OR_E,     OR_H,        OR_L,     OR_mHL,    OR_A,     CP_B,       CP_C,      CP_D,      CP_E,      CP_H,       CP_L,     CP_mHL,   CP_A,
/* Cx */RET_NZ,   POP_BC,    JP_NZ_a16, JP_a16,   CALL_NZ_a16, PUSH_BC,  ADD_d8,    RST00,    RET_Z,      RET,       JP_Z_a16,  PREFIX_CB, CALL_Z_a16, CALL_d16, ADC_d8,   RST08,
/* Dx */RET_NC,   POP_DE,    JP_NC_a16, XX,       CALL_NC_a16, PUSH_DE,  SUB_d8,    RST10,    RET_C,      RETI,      JP_C_a16,  XX,        CALL_C_a16, XX,       SBC_d8,   RST18,
/* Ex */LDH_m8_A, POP_HL,    LD_mC_A,   XX,       XX,          PUSH_HL,  AND_d8,    RST20,    ADD_SP_r8,  JP_mHL,    LD_m16_A,  XX,        XX,         XX,       XOR_d8,   RST28,
/* Fx */LDH_A_m8, POP_AF,    LD_A_mC,   DI,       XX,          PUSH_AF,  OR_d8,     RST30,    LDHL_SP_r8, LD_SP_HL,  LD_A_m16,  EI,        XX,         XX,       CP_d8,    RST38
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

        if(_HALT) {
            NOP();
        } else {
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