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

#include "GB.h"
#include "LR35902.h"
#include "cartridge.h"
#include "PPU.h"
#include "audio.h"
#include "joypad.h"
#include "serial.h"
#include "timer.h"

#define _BOOT_ADDRESS 0xFF50

static uint8_t _BIOS[_BIOS_SIZE];

static uint8_t _HRAM[_HRAM_SIZE];
static uint8_t _RAM[_RAM_SIZE];

static uint8_t _boot = 0x00;

uint8_t read_byte(uint16_t address)
{
    if( _ZERO_PAGE_OFFSET <= address ) {
        if( address == _IE_ADDRESS ) {
            return _IE;
        } else if( _HRAM_OFFSET <= address && address <= _HRAM_OFFSET_END) {
            return _HRAM[ address - _HRAM_OFFSET ];
        } else if( _IO_OFFSET <= address && address <= _IO_OFFSET_END ) {
            switch (address & 0x00F0) {
                case 0x00:
                    switch (address & 0x000F) {
                        case 0x00:
                            return joypad_read_byte(address);
                        case 0x01:
                        case 0x02:
                            return serial_read_byte(address);
                        case 0x03:
                        case 0x04:
                        case 0x05:
                        case 0x06:
                            return timer_read_byte(address);
                        case 0x0F:
                            return _IF;
                        default:
                            break;
                    }
                    break;
                case 0x10:
                case 0x20:
                case 0x30:
                    return audio_read_byte(address);
                case 0x40:
                    return video_read_byte(address);
                case 0x50:
                    if (address == _BOOT_ADDRESS) {
                        return _boot;
                    }
                    break;
                default:
                    break;
            }
        }
    } else if( _OAM_OFFSET <= address && address < _OAM_OFFSET_END ) {
        return oam_read_byte(address);
    } else if( _RAM_ECHO_OFFSET <= address ) {
        return _RAM[ address - _RAM_ECHO_OFFSET ];
    } else if( _RAM_OFFSET <= address ) {
        return _RAM[ address - _RAM_OFFSET ];
    } else if( _EXT_RAM_OFFSET <= address ) {
        return ext_ram_read_byte(address);
    } else if( _VRAM_OFFSET <= address ) {
        return vram_read_byte(address);
    } else if ( !_boot && address < _BIOS_SIZE) {
        return _BIOS[address];
    } else {
        return rom_read_byte(address);
    }
}

void write_byte(uint16_t address, uint8_t value)
{
    if( _ZERO_PAGE_OFFSET <= address ) {
        if( address == _IE_ADDRESS ) {
            _IE = value;
        } else if( _HRAM_OFFSET <= address && address <= _HRAM_OFFSET_END) {
            _HRAM[ address - _HRAM_OFFSET ] = value;
        } else if( _IO_OFFSET <= address && address <= _IO_OFFSET_END ) {
            switch (address & 0x00F0) {
                case 0x00:
                    switch (address & 0x000F) {
                        case 0x00:
                            joypad_write_byte(address, value);
                            break;
                        case 0x01:
                        case 0x02:
                            serial_write_byte(address, value);
                            break;
                        case 0x03:
                        case 0x04:
                        case 0x05:
                        case 0x06:
                            timer_write_byte(address, value);
                            break;
                        case 0x0F:
                            _IF = value;
                            break;
                        default:
                            break;
                    }
                    break;
                case 0x10:
                case 0x20:
                case 0x30:
                    audio_write_byte(address, value);
                    break;
                case 0x40:
                    video_write_byte(address, value);
                    break;
                case 0x50:
                    if (address == _BOOT_ADDRESS) {
                        _boot = value;
                    }
                    break;
                default:
                    break;
            }
        }
    } else if( _OAM_OFFSET <= address && address < _OAM_OFFSET_END ) {
        oam_write_byte(address, value);
    } else if( _RAM_ECHO_OFFSET <= address ) {
        _RAM[ address - _RAM_ECHO_OFFSET ] = value;
    } else if( _RAM_OFFSET <= address ) {
        _RAM[ address - _RAM_OFFSET ] = value;
    } else if( _EXT_RAM_OFFSET <= address ) {
        ext_ram_write_byte(address, value);
    } else if( _VRAM_OFFSET <= address ) {
        vram_write_byte(address, value);
    } else if( _ROM_OFFSET <= address ) {
        rom_write_byte(address, value);
    }
}

uint16_t read_word(uint16_t address)
{
    uint8_t lo = read_byte(address);
    uint8_t hi = read_byte((uint16_t) (address + 1));
    return lo + (hi << 8);
}

void write_word(uint16_t address, uint16_t value)
{
    write_byte(address, (uint8_t) (value & 0xFF));
    write_byte((uint16_t) (address + 1), (uint8_t) (value >> 8));
}

int mmu_load_bios(FILE *bios)
{
    rewind(bios);
    fseek(bios, 0, SEEK_END);

    long bios_size = ftell(bios);
    if(bios_size != _BIOS_SIZE) {
        log_error("Invalid BIOS file size (%d bytes)\n", bios_size);
        return 0;
    }

    rewind(bios);
    size_t result = fread(_BIOS, sizeof(uint8_t), _BIOS_SIZE, bios);
    if(result != _BIOS_SIZE) {
        log_error("Invalid amount of bytes read (Read: %d bytes, Expected: %d bytes)\n", result, _BIOS_SIZE);
        if(feof(bios)) {
            log_error("The end of the bios file was reached.\n");
        } else if(ferror(bios)) {
            log_error("Unknown error during read.\n", result, _BIOS_SIZE);
        }
        return 0;
    }
    return 1;
}

void mmu_reset(void)
{
    _boot = 0x00;
}