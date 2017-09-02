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

#include "MMU.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define _ROM_OFFSET         0x0000
#define _EXT_ROM_OFFSET     0x4000
#define _VRAM_OFFSET        0x8000
#define _EXT_RAM_OFFSET     0xA000
#define _RAM_OFFSET         0xC000
#define _RAM_ECHO_OFFSET    0xE000
#define _OAM_OFFSET         0xFE00
#define _IO_OFFSET          0xFF00
#define _0PAGE_OFFSET       0xFF80
#define _MEM_SIZE           0x10000

#define _BIOS_SIZE          0x0100

#define TITLE_SIZE          16
#define CARTRIDGE_TITLE_OFFSET  0x0134

static uint8_t _BIOS[_BIOS_SIZE] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
    0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
    0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,
    0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
    0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
    0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
    0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
    0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
    0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
    0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
    0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50
};

static bool in_bios = true;
static FILE *cartridge = NULL;

static uint8_t _0PAGE[_MEM_SIZE - _0PAGE_OFFSET];
static uint8_t _IO[_0PAGE_OFFSET - _IO_OFFSET];
static uint8_t _OAM[_IO_OFFSET - _OAM_OFFSET];
static uint8_t _RAM[_RAM_ECHO_OFFSET - _RAM_OFFSET];
static uint8_t _ExtRAM[_RAM_OFFSET - _EXT_RAM_OFFSET];
static uint8_t _VRAM[_EXT_RAM_OFFSET - _VRAM_OFFSET];
static uint8_t _ExtROM[_VRAM_OFFSET - _EXT_ROM_OFFSET];
static uint8_t _ROM[_EXT_ROM_OFFSET - _ROM_OFFSET];

uint8_t title[TITLE_SIZE];


static uint8_t *memptr( uint16_t address )
{
    if( address > _0PAGE_OFFSET ) {
        return &_0PAGE[ address - _0PAGE_OFFSET ];
    } else if( address > _IO_OFFSET ) {
        return &_IO[ address - _IO_OFFSET ];
    } else if( address > _OAM_OFFSET ) {
        return &_OAM[ address - _OAM_OFFSET ];
    } else if( address > _RAM_ECHO_OFFSET ) {
        return &_RAM[ address - _RAM_ECHO_OFFSET ];
    } else if( address > _RAM_OFFSET ) {
        return &_RAM[ address - _RAM_OFFSET ];
    } else if( address > _EXT_RAM_OFFSET ) {
        return &_ExtRAM[ address - _EXT_RAM_OFFSET ];
    } else if( address > _VRAM_OFFSET ) {
        return &_VRAM[ address - _VRAM_OFFSET ];
    } else if( address > _EXT_ROM_OFFSET ) {
        return &_ExtROM[ address - _EXT_ROM_OFFSET ];
    } else if( address > _ROM_OFFSET ) {
        if( in_bios && address < _BIOS_SIZE ) {
            return &_BIOS[ address - _ROM_OFFSET ];
        }
        in_bios = false;
        return &_ROM[ address - _ROM_OFFSET ];
    } else {
        return NULL;
    }
}

void load(char *filename)
{
    if( cartridge ) {
        fclose(cartridge);
    }

    cartridge = fopen( filename, "rb" );
    if( !cartridge ) {
        /* ERROR */
        return;
    }

    /* MBC Check */
    fseek(cartridge, CARTRIDGE_TITLE_OFFSET, SEEK_SET);
    fread(&title, sizeof(uint8_t), TITLE_SIZE, cartridge);
}


uint8_t readbyte( uint16_t address )
{
    return *memptr( address );
}


uint16_t readword( uint16_t address )
{
    return readbyte(address) + (readbyte((uint16_t)(address + 1)) << 8);
}


void writebyte( uint16_t address, uint8_t val )
{
    *memptr( address ) = val;
}


void writeword( uint16_t address, uint16_t val )
{
    writebyte(address, (uint8_t)val);
    writebyte((uint16_t)(address + 1), (uint8_t)(val >> 8));
}
