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

#include "cartridge.h"

#include "GB.h"

#define MBC_OFFSET  0x0147

struct {
    FILE *_rom_ptr;
    FILE *_save_ptr;

    uint8_t _ROM[_ROM_SIZE];
    uint8_t _EXT_ROM[_EXT_ROM_SIZE];

    uint8_t _EXT_RAM[_EXT_RAM_SIZE];
} _cartridge;

int set_rom_ptr(FILE *rom)
{
    rewind(rom);
    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);

    if(rom_size < _ROM_SIZE) {
        log_error("ROM file is smaller than 1 bank.\n");
        return 0;
    }

    rewind(rom);
    size_t result = fread(_cartridge._ROM, sizeof(uint8_t), _ROM_SIZE, rom);
    if(result != _ROM_SIZE) {
        log_error("Invalid amount of bytes read (Read: %d bytes, Expected: %d bytes)\n", result, _BIOS_SIZE);
        if(feof(rom)) {
            log_error("The end of the bios file was reached.\n");
        } else if(ferror(rom)) {
            log_error("Unknown error during read.\n", result, _BIOS_SIZE);
        }
        return 0;
    }

    switch(_cartridge._ROM[MBC_OFFSET]) {
        case 0x00:
            fread(_cartridge._EXT_ROM, sizeof(uint8_t), _EXT_ROM_SIZE, rom);
            break;
        default:
            log_error("Unknown MBC type found: %d\n", _cartridge._ROM[MBC_OFFSET]);
            return 0;
    }

    _cartridge._rom_ptr = rom;

    return 1;
}

int set_sav_ptr(FILE *sav)
{
    _cartridge._save_ptr = sav;
}

uint8_t rom_read_byte(uint16_t address)
{
    if(_cartridge._rom_ptr != NULL) {
        if( _EXT_ROM_OFFSET <= address ) {
            return _cartridge._EXT_ROM[ address - _EXT_ROM_OFFSET ];
        } else if ( _ROM_OFFSET <= address ) {
            return _cartridge._ROM[ address - _ROM_OFFSET ];
        }
    }
    return 0xFF;
}

void rom_write_byte(uint16_t address, uint8_t value)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_cartridge._ROM[MBC_OFFSET]) {
            case 0x00:
                log_error("Invalid write to ROM only cartridge ROM (address 0x%04X).\n", address);
            default:
                break;
        }
    }
}

uint8_t ext_ram_read_byte(uint16_t address)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_cartridge._ROM[MBC_OFFSET]) {
            default:
            case 0x00:
                break;
            case 0x01:
                if (_EXT_RAM_OFFSET <= address) {
                    return _cartridge._EXT_RAM[address - _EXT_RAM_OFFSET];
                }
                break;
        }
    }
    return 0xFF;
}

void ext_ram_write_byte(uint16_t address, uint8_t value)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_cartridge._ROM[MBC_OFFSET]) {
            default:
            case 0x00:
                break;
            case 0x01:
                if (_EXT_RAM_OFFSET <= address) {
                    _cartridge._EXT_RAM[address - _EXT_RAM_OFFSET] = value;
                }
        }
    }
}

void mbc_reset(void)
{

}