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

#ifndef NEC_DISPLAY_H
#define NEC_DISPLAY_H

#include <stdint.h>

#define BG_TILE_WIDTH       8
#define BG_TILE_HEIGHT      8
#define BYTES_PER_TEXEL     4
#define BYTES_PER_TILE      (BG_TILE_WIDTH * BG_TILE_HEIGHT * BYTES_PER_TEXEL)
#define BG_NUM_TILES        256

#define DISPLAY_WIDTH   160
#define DISPLAY_HEIGHT  144

#define TEXTURE_DIMENSION   256

struct dot {
    float r;
    float g;
    float b;
    float a;
};

struct line {
    struct dot dots[TEXTURE_DIMENSION];
};

struct display {
    struct line lines[TEXTURE_DIMENSION];
};

extern struct display _display;

/**
 *
 */
void display_setup(void);

/**
 *
 */
void display_frame(void);

/**
 *
 */
void display_teardown(void);

#endif //NEC_DISPLAY_H
