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

#ifndef NEC_MMU_H
#define NEC_MMU_H

#include <stdio.h>
#include <stdint.h>

#define _BIOS_SIZE          0x0100

#define _ROM_OFFSET         0x0000
#define _EXT_ROM_OFFSET     0x4000
#define _VRAM_OFFSET        0x8000
#define _EXT_RAM_OFFSET     0xA000
#define _RAM_OFFSET         0xC000
#define _RAM_ECHO_OFFSET    0xE000
#define _OAM_OFFSET         0xFE00
#define _OAM_OFFSET_END     0xFEA0
#define _IO_OFFSET          0xFF00
#define _IO_OFFSET_END      0xFF50
#define _HRAM_OFFSET        0xFF80
#define _HRAM_OFFSET_END    0xFFFE
#define _IE_ADDRESS         0xFFFF

#define _ZERO_PAGE_OFFSET   0xFE00

#define _ROM_SIZE           (_EXT_ROM_OFFSET - _ROM_OFFSET)
#define _EXT_ROM_SIZE       (_VRAM_OFFSET - _EXT_ROM_OFFSET)
#define _VRAM_SIZE          (_EXT_RAM_OFFSET - _VRAM_OFFSET)
#define _EXT_RAM_SIZE       (_RAM_OFFSET - _EXT_RAM_OFFSET)
#define _RAM_SIZE           (_RAM_ECHO_OFFSET - _RAM_OFFSET)
#define _OAM_SIZE           (_OAM_OFFSET_END - _OAM_OFFSET)
#define _HRAM_SIZE          (_IE_ADDRESS - _HRAM_OFFSET)

/**
 *
 * @param address
 * @return
 */
uint8_t read_byte(uint16_t address);

/**
 *
 * @param address
 * @param value
 */
void write_byte(uint16_t address, uint8_t value);

/**
 *
 * @param address
 * @return
 */
uint16_t read_word(uint16_t address);

/**
 *
 * @param address
 * @param value
 */
void write_word(uint16_t address, uint16_t value);

/**
 *
 * @param bios
 * @return
 */
int mmu_load_bios(FILE *bios);

/**
 *
 */
void mmu_reset(void);

#endif /* NEC_MMU_H */
