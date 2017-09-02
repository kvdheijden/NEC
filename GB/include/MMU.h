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

#include <stdint.h>

/**
 * Load the specified ROM into memory.
 *
 * @param filename
 */
void load(char *filename);

/**
 * Read 8-bit byte from a given address.
 *
 * @param address
 * @return
 */
uint8_t readbyte( uint16_t address );

/**
 * Read 16-bit word from a given address.
 *
 * @param address
 * @return
 */
uint16_t readword( uint16_t address );

/**
 * Write 8-bit byte to a given address.
 *
 * @param address
 * @param val
 */
void writebyte( uint16_t address, uint8_t val );

/**
 * Write 16-bit word to a given address.
 *
 * @param address
 * @param val
 */
void writeword( uint16_t address, uint16_t val );

#endif /* NEC_MMU_H */
