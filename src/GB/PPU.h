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

#ifndef NEC_GPU_H
#define NEC_GPU_H

#include <stdint.h>

#define _GPU_REG_OFFSET     0xFF40
#define _GPU_REG_OFFSET_END 0xFF4B

/**
 *
 * @param address
 * @return
 */
uint8_t vram_read_byte(uint16_t address);

/**
 *
 * @param address
 * @param value
 */
void vram_write_byte(uint16_t address, uint8_t value);

/**
 *
 * @param address
 * @return
 */
uint8_t oam_read_byte(uint16_t address);

/**
 *
 * @param address
 * @param value
 */
void oam_write_byte(uint16_t address, uint8_t value);

/**
 *
 * @param address
 * @return
 */
uint8_t video_read_byte(uint16_t address);

/**
 *
 * @param address
 * @param value
 */
void video_write_byte(uint16_t address, uint8_t value);

/**
 *
 * @param clk_tics
 */
void video_update(uint8_t clk_tics);

/**
 *
 */
void video_reset(void);

#endif //NEC_GPU_H
