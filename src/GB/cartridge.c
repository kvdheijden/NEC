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

#include <stdbool.h>
#include <mem.h>
#include <stdlib.h>

#include "GB.h"
#include "LR35902.h"

#define TITLE_OFFSET    0x0134
#define MBC_OFFSET      0x0147
#define RAM_SIZE_OFFSET 0x0149

#define MBC2_EXT_RAM_SIZE   0x200

#define _ROM_RAM_MODE_SELECT_OFFSET 0x6000
#define _RAM_ROM_BANK_NUMBER_OFFSET 0x4000
#define _ROM_BANK_NUMBER_OFFSET     0x2000

static uint8_t _ROM[_ROM_SIZE] = {0};
static uint8_t _EXT_ROM[_EXT_ROM_SIZE] = {0};

static uint8_t _EXT_RAM[_EXT_RAM_SIZE] = {0};

struct {
    FILE *_rom_ptr;
    FILE *_save_ptr;
} _cartridge;

/**
 *
 * @return
 */
static bool is_mbc2(void)
{
    return ((_ROM[MBC_OFFSET] == 0x05) || (_ROM[MBC_OFFSET] == 0x06));
}

static void init_save_file(FILE *sav)
{
    if(is_mbc2()) {
        uint8_t init[MBC2_EXT_RAM_SIZE];
        memset(&init, 0x0F, MBC2_EXT_RAM_SIZE);
        fwrite(&init, sizeof(uint8_t), MBC2_EXT_RAM_SIZE, sav);
        rewind(sav);
    } else {
        uint8_t rom_size_id = _ROM[RAM_SIZE_OFFSET];
        size_t size = (size_t) (_EXT_RAM_SIZE << (2 * (rom_size_id - 0x02)));
        switch (_ROM[RAM_SIZE_OFFSET]) {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
                break;
            default:
                return;
        }
        uint8_t init[size];
        memset(&init, 0xFF, size);
        fwrite(&init, sizeof(uint8_t), size, sav);
        rewind(sav);
    }
}


/*
 *  MBC
 */

/**
 *
 * @return
 */
static bool has_extram(void)
{
    return (_ROM[RAM_SIZE_OFFSET] != 0) && (_ROM[RAM_SIZE_OFFSET] <= 4);
}

/*
 * MBC 1
 */
struct {
    bool ext_ram_enabled;
    bool ram_bank_mode;
    uint8_t rom_bank_lo;
    uint8_t rom_bank_hi;
    uint8_t ram_bank;
    int current_rom_bank;
    int current_ram_bank;
} _mbc1 = {
        .ext_ram_enabled = false,
        .ram_bank_mode = false,
        .rom_bank_lo = 1,
        .rom_bank_hi = 0,
        .ram_bank = 0,
        .current_rom_bank = 1,
        .current_ram_bank = 0
};

/**
 *
 * @param ram_bank
 */
static void mbc1_load_ram_bank(int ram_bank)
{
    if(ram_bank != _mbc1.current_ram_bank) {
        fseek(_cartridge._save_ptr, _mbc1.current_ram_bank * _EXT_RAM_SIZE, SEEK_SET);
        fwrite(_EXT_RAM, sizeof(uint8_t), _EXT_RAM_SIZE, _cartridge._save_ptr);

        fseek(_cartridge._save_ptr, ram_bank * _EXT_RAM_SIZE, SEEK_SET);
        fread(_EXT_RAM, sizeof(uint8_t), _EXT_RAM_SIZE, _cartridge._save_ptr);
        _mbc1.current_ram_bank = ram_bank;
    }
}

/**
 *
 * @param rom_bank
 */
static void mbc1_load_rom_bank(int rom_bank)
{
    if(rom_bank != _mbc1.current_rom_bank) {
        fseek(_cartridge._rom_ptr, rom_bank * _EXT_ROM_SIZE, SEEK_SET);
        fread(_EXT_ROM, sizeof(uint8_t), _EXT_ROM_SIZE, _cartridge._rom_ptr);
        _mbc1.current_rom_bank = rom_bank;
    }
}

/**
 *
 * @param address
 * @param value
 */
static void mbc1_write_rom(uint16_t address, uint8_t value)
{
    if( _ROM_RAM_MODE_SELECT_OFFSET <= address ) {
        _mbc1.ram_bank_mode = ((value & 0x01) == 0x01);
    } else if( _RAM_ROM_BANK_NUMBER_OFFSET <= address ) {
        value &= 0x03;
        if(_mbc1.ram_bank_mode) {
            _mbc1.ram_bank = value;
        } else {
            _mbc1.rom_bank_hi = value;
        }
    } else if ( _ROM_BANK_NUMBER_OFFSET <= address ) {
        value &= 0x1F;
        if(!value) {
            value = 0x01;
        }
        _mbc1.rom_bank_lo = value;
    } else {
        _mbc1.ext_ram_enabled = ((value & 0x0F) == 0x0A);
    }

    if(_mbc1.ram_bank_mode) {
        mbc1_load_ram_bank(_mbc1.ram_bank);
        mbc1_load_rom_bank(_mbc1.rom_bank_lo);
    } else {
        mbc1_load_ram_bank(0);
        mbc1_load_rom_bank((_mbc1.rom_bank_hi << 5) | _mbc1.rom_bank_lo);
    }
}

/*
 * MBC 2
 */
struct {
    int rom_bank;
    int current_rom_bank;
    bool ext_ram_enabled;
} _mbc2 = {
        .rom_bank = 1,
        .current_rom_bank = 1,
        .ext_ram_enabled = false
};

/**
 *
 * @param rom_bank
 */
static void mbc2_load_rom_bank(int rom_bank)
{
    if(rom_bank != _mbc2.current_rom_bank) {
        fseek(_cartridge._rom_ptr, rom_bank * _EXT_ROM_SIZE, SEEK_SET);
        fread(_EXT_ROM, sizeof(uint8_t), _EXT_ROM_SIZE, _cartridge._rom_ptr);
        _mbc2.current_rom_bank = rom_bank;
    }
}

/**
 *
 * @param address
 * @param value
 */
static void mbc2_write_rom(uint16_t address, uint8_t value)
{
    if ( _ROM_BANK_NUMBER_OFFSET <= address && address <= _RAM_ROM_BANK_NUMBER_OFFSET && (address & 0x10) ) {
        value &= 0x0F;
        if(!value) {
            value = 0x01;
        }
        _mbc2.rom_bank = value;
    } else if(!(address & 0x10)) {
        _mbc2.ext_ram_enabled = ((value & 0x0F) == 0x0A);
    }

    mbc2_load_rom_bank(_mbc2.rom_bank);
}

/**
 *
 * @param address
 * @return
 */
static uint8_t mbc2_read_extram(uint16_t address)
{
    if(_EXT_RAM_OFFSET <= address && address < _EXT_RAM_OFFSET + MBC2_EXT_RAM_SIZE) {
        return (uint8_t) (_EXT_RAM[address - _EXT_RAM_OFFSET] & 0x0F);
    }
    return 0xFF;
}

/**
 *
 * @param address
 * @param value
 */
static void mbc2_write_extram(uint16_t address, uint8_t value)
{
    if(_EXT_RAM_OFFSET <= address && address < _EXT_RAM_OFFSET + MBC2_EXT_RAM_SIZE) {
        _EXT_RAM[address - _EXT_RAM_OFFSET] = (uint8_t) (value & 0x0F);
    }
}

/*
 * MBC 3
 */
struct {
    uint8_t latch;
    int rom_bank;
    int current_rom_bank;
    int ram_bank;
    int current_ram_bank;
    bool ext_ram_enabled;
} _mbc3 = {
        .latch = 0x00,
        .rom_bank = 1,
        .current_rom_bank = 1,
        .ram_bank = 0,
        .current_ram_bank = 0,
        .ext_ram_enabled = false
};

/**
 *
 * @param ram_bank
 */
static void mbc3_load_ram_bank(int ram_bank)
{
    if(ram_bank != _mbc3.current_ram_bank) {
        fseek(_cartridge._save_ptr, _mbc3.current_ram_bank * _EXT_RAM_SIZE, SEEK_SET);
        fwrite(_EXT_RAM, sizeof(uint8_t), _EXT_RAM_SIZE, _cartridge._save_ptr);

        fseek(_cartridge._save_ptr, ram_bank * _EXT_ROM_SIZE, SEEK_SET);
        fread(_EXT_RAM, sizeof(uint8_t), _EXT_ROM_SIZE, _cartridge._save_ptr);
        _mbc3.current_ram_bank = ram_bank;
    }
}

/**
 *
 * @param rom_bank
 */
static void mbc3_load_rom_bank(int rom_bank)
{
    if(rom_bank != _mbc3.current_rom_bank) {
        fseek(_cartridge._rom_ptr, rom_bank * _EXT_ROM_SIZE, SEEK_SET);
        fread(_EXT_ROM, sizeof(uint8_t), _EXT_ROM_SIZE, _cartridge._rom_ptr);
        _mbc3.current_rom_bank = rom_bank;
    }
}

/**
 *
 * @param address
 * @param value
 */
static void mbc3_write_rom(uint16_t address, uint8_t value)
{
    if( _ROM_RAM_MODE_SELECT_OFFSET <= address ) {
        if((value & 0x01) && !(_mbc3.latch & 0x01)) {
            // TODO: latch
        }
        _mbc3.latch = value;
    } else if( _RAM_ROM_BANK_NUMBER_OFFSET <= address ) {
        if(value & 0x0C) {
            switch (value & 0x0F) {
                case 0x08:
                    break;
                case 0x09:
                    break;
                case 0x0A:
                    break;
                case 0x0B:
                    break;
                case 0x0C:
                    break;
                default:
                    break;
            }
        } else {
            value &= 0x03;
            if(_mbc1.ram_bank_mode) {
                _mbc1.ram_bank = value;
            } else {
                _mbc1.rom_bank_hi = value;
            }
        }
    } else if ( _ROM_BANK_NUMBER_OFFSET <= address ) {
        value &= 0x3F;
        if(!value) {
            value = 0x01;
        }
        _mbc3.rom_bank = value;
    } else {
        _mbc3.ext_ram_enabled = ((value & 0x0F) == 0x0A);
    }

    mbc3_load_ram_bank(_mbc3.ram_bank);
    mbc3_load_rom_bank(_mbc3.rom_bank);
}

int load_cartridge(const char *rom, char *sav)
{
    FILE *_rom_ptr = fopen(rom, "rb");
    if(_rom_ptr == NULL) {
        log_error("ROM file could not be opened: %d.\n", errno);
        return 0;
    }

    fseek(_rom_ptr, 0, SEEK_END);
    long rom_size = ftell(_rom_ptr);

    if(rom_size < _ROM_SIZE + _EXT_ROM_SIZE) {
        log_error("ROM file is smaller than 2 banks.\n");
        return 0;
    }

    rewind(_rom_ptr);
    size_t result = fread(_ROM, sizeof(uint8_t), _ROM_SIZE, _rom_ptr);
    if(result != _ROM_SIZE) {
        log_error("Invalid amount of bytes read (Read: %d bytes, Expected: %d bytes)\n", result, _ROM_SIZE);
        if(feof(_rom_ptr)) {
            log_error("The end of the ROM file was reached.\n");
        } else if(ferror(_rom_ptr)) {
            log_error("Unknown error during read.\n");
        }
        return 0;
    }

    result = fread(_EXT_ROM, sizeof(uint8_t), _EXT_ROM_SIZE, _rom_ptr);
    if(result != _EXT_ROM_SIZE) {
        log_error("Invalid amount of bytes read (Read: %d bytes, Expected: %d bytes)\n", result, _EXT_ROM_SIZE);
        if(feof(_rom_ptr)) {
            log_error("The end of the ROM file was reached.\n");
        } else if(ferror(_rom_ptr)) {
            log_error("Unknown error during read.\n");
        }
        return 0;
    }

    _cartridge._rom_ptr = _rom_ptr;
    set_title((const char *) &_ROM[TITLE_OFFSET]);

    if(has_extram()) {
        FILE *_sav_ptr;
        if(sav == NULL) {
            const char *filepath = strrchr(rom, '.');
            size_t i = filepath - rom;
            char new_save_file[i + 5];
            memcpy(new_save_file, rom, i + 1);
            new_save_file[i + 1] = 's';
            new_save_file[i + 2] = 'a';
            new_save_file[i + 3] = 'v';
            new_save_file[i + 4] = '\0';

            _sav_ptr = fopen(new_save_file, "w+b");
            if(_sav_ptr == NULL) {
                log_error("SAV file could not be opened %d.\n", errno);
                return 0;
            }

            init_save_file(_sav_ptr);
        } else {
            _sav_ptr = fopen(sav, "r+b");
            if(_sav_ptr == NULL) {
                log_error("SAV file could not be opened %d.\n", errno);
                return 0;
            }
        }

        uint8_t rom_size_id = _ROM[RAM_SIZE_OFFSET];
        size_t size = (size_t) (_EXT_RAM_SIZE << (2 * (rom_size_id - 0x02)));
        size = (size > _EXT_RAM_SIZE ? _EXT_RAM_SIZE : size);
        if(is_mbc2()) {
            size = MBC2_EXT_RAM_SIZE;
        }
        result = fread(_EXT_RAM, sizeof(uint8_t), size, _sav_ptr);
        if(result != size) {
            log_error("Invalid amount of bytes read (Read: %d bytes, Expected: %d bytes)\n", result, size);
            if(feof(_sav_ptr)) {
                log_error("The end of the SAV file was reached.\n");
            } else if(ferror(_sav_ptr)) {
                log_error("Unknown error during read.\n");
            }
            return 0;
        }
        _cartridge._save_ptr = _sav_ptr;
    } else {
        _cartridge._save_ptr = NULL;
    }

    return 1;
}

void unload_cartridge(void)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_ROM[MBC_OFFSET]) {
            default:
            case 0x00:
            case 0x08:
            case 0x09:
                break;
            case 0x01:
            case 0x02:
            case 0x03:
                fseek(_cartridge._save_ptr, _mbc1.current_ram_bank * _EXT_RAM_SIZE, SEEK_SET);
                fwrite(_EXT_RAM, sizeof(uint8_t), _EXT_RAM_SIZE, _cartridge._save_ptr);
                break;
            case 0x05:
            case 0x06:
                fseek(_cartridge._save_ptr, 0, SEEK_SET);
                fwrite(_EXT_RAM, sizeof(uint8_t), MBC2_EXT_RAM_SIZE, _cartridge._save_ptr);
                break;
            case 0x0B:
            case 0x0C:
            case 0x0D:
                //mmm01_write_rom(address, value);
                break;
            case 0x0F:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
                fseek(_cartridge._save_ptr, _mbc3.current_ram_bank * _EXT_RAM_SIZE, SEEK_SET);
                fwrite(_EXT_RAM, sizeof(uint8_t), _EXT_RAM_SIZE, _cartridge._save_ptr);
                break;
            case 0x19:
            case 0x1A:
            case 0x1B:
            case 0x1C:
            case 0x1D:
            case 0x1E:
                //mbc5_write_rom(address, value);
                break;
            case 0x1F:
                // Pocket Camera
                break;
            case 0xFD:
                // Bandai TAMA5
                break;
            case 0xFE:
                // Hudson HuC 3
                break;
            case 0xFF:
                // Hudson HuC 1
                break;
        }
    }
}

uint8_t rom_read_byte(uint16_t address)
{
    if(_cartridge._rom_ptr != NULL) {
        if( _EXT_ROM_OFFSET <= address ) {
            return _EXT_ROM[ address - _EXT_ROM_OFFSET ];
        } else if ( _ROM_OFFSET <= address ) {
            return _ROM[ address - _ROM_OFFSET ];
        }
    }
    return 0xFF;
}

void rom_write_byte(uint16_t address, uint8_t value)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_ROM[MBC_OFFSET]) {
            case 0x00:
            case 0x08:
            case 0x09:
                log_error("Invalid write to ROM only cartridge (ROM address 0x%04X, Instruction opcode 0x%02X at address 0x%04x).\n",
                          address, read_byte((uint16_t) (_r.pc - 1)), _r.pc - 1);
                break;
            case 0x01:
            case 0x02:
            case 0x03:
                mbc1_write_rom(address, value);
                break;
            case 0x05:
            case 0x06:
                mbc2_write_rom(address, value);
                break;
            case 0x0B:
            case 0x0C:
            case 0x0D:
                //mmm01_write_rom(address, value);
                break;
            case 0x0F:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
                mbc3_write_rom(address, value);
                break;
            case 0x19:
            case 0x1A:
            case 0x1B:
            case 0x1C:
            case 0x1D:
            case 0x1E:
                //mbc5_write_rom(address, value);
                break;
            case 0x1F:
                // Pocket Camera
                break;
            case 0xFD:
                // Bandai TAMA5
                break;
            case 0xFE:
                // Hudson HuC 3
                break;
            case 0xFF:
                // Hudson HuC 1
                break;
            default:
                break;
        }
    }
}

uint8_t ext_ram_read_byte(uint16_t address)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_ROM[RAM_SIZE_OFFSET]) {
            default:
            case 0x00:
                if(is_mbc2()) return mbc2_read_extram(address);
                break;
            case 0x01:
                if (_EXT_RAM_OFFSET <= address && address < _EXT_RAM_OFFSET + 0x800) {
                    return _EXT_RAM[address - _EXT_RAM_OFFSET];
                }
                break;
            case 0x02:
            case 0x03:
            case 0x04:
                if (_EXT_RAM_OFFSET <= address) {
                    return _EXT_RAM[address - _EXT_RAM_OFFSET];
                }
                break;
        }
    }
    return 0xFF;
}

void ext_ram_write_byte(uint16_t address, uint8_t value)
{
    if(_cartridge._rom_ptr != NULL) {
        switch (_ROM[RAM_SIZE_OFFSET]) {
            default:
            case 0x00:
                if(is_mbc2()) mbc2_write_extram(address, value);
                break;
            case 0x01:
                if (_EXT_RAM_OFFSET <= address && address < _EXT_RAM_OFFSET + 0x800) {
                    _EXT_RAM[address - _EXT_RAM_OFFSET] = value;
                }
                break;
            case 0x02:
            case 0x03:
            case 0x04:
                if (_EXT_RAM_OFFSET <= address) {
                    _EXT_RAM[address - _EXT_RAM_OFFSET] = value;
                }
                break;
        }
    }
}

float get_vin(void)
{
    return 0;
}

void mbc_reset(void)
{

}