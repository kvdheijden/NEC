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

#include "GB.h"

#include <stdlib.h>
#include <stdio.h>

#include "LR35902.h"
#include "MMU.h"
#include "timer.h"
#include "PPU.h"
#include "display.h"
#include "audio.h"
#include "sound.h"
#include "cartridge.h"
#include "serial.h"
#include "joypad.h"

static int _exit_code = EXIT_SUCCESS;

static enum GB_state {
    INIT = 0x00,
    BIOS_LOADED = 0x01,
    CARTRIDGE_LOADED = 0x02,
    RUNNING = 0x03,
    STOPPED = 0x04
} _state = INIT;

static FILE *_rom_ptr = NULL;
static FILE *_save_ptr = NULL;

void GB_load_bios(const char *bios_file)
{
    FILE *_bios_ptr = fopen(bios_file, "rb");
    if(_bios_ptr == NULL) {
        log_error("Invalid BIOS file: %s\n", bios_file);
        GB_exit();
        return;
    }

    int status = mmu_load_bios(_bios_ptr);
    fclose(_bios_ptr);
    if(!status) {
        log_error("Invalid BIOS file: %s\n", bios_file);
        GB_exit();
        return;
    }

    _state |= BIOS_LOADED;
}

void GB_load_cartridge(const char *rom_file, const char *save_file)
{
    if((_state & BIOS_LOADED) != BIOS_LOADED) {
        GB_exit();
        return;
    }

    _rom_ptr = fopen(rom_file, "rb");
    if(_rom_ptr == NULL) {
        log_error("Invalid ROM file: %s\n", rom_file);
        GB_exit();
        return;
    }

    int status = set_rom_ptr(_rom_ptr);
    if(!status) {
        GB_exit();
        return;
    }

    if(save_file != NULL) {
        _save_ptr = fopen(save_file, "r+b");
        if(_save_ptr == NULL) {
            log_warning("Invalid SAV file: %s\nStarting without save.\n", save_file);
        } else {
            status = set_sav_ptr(_save_ptr);
            if(!status) {
                GB_exit();
                return;
            }
        }
    }

    _state |= CARTRIDGE_LOADED;
}

void GB_start(void)
{
    if(_state == STOPPED) {
        GB_exit();
        return;
    }

    if((_state & BIOS_LOADED) != BIOS_LOADED) {
        log_error("BIOS not yet loaded.\n");
        GB_exit();
        return;
    }

    // Setup display and sound
    display_setup();
    sound_setup();

    // Main dispatch loop
    while(_state <= RUNNING) {
        uint64_t _local_clk = _r.clk;

        dispatch();

        uint8_t _clock_tics;
        if(_r.clk < _local_clk) {
            _clock_tics = (uint8_t) (1 + _r.clk + ~_local_clk);
        } else {
            _clock_tics = (uint8_t) (_r.clk - _local_clk);
        }

        video_update(_clock_tics);
        audio_update(_clock_tics);
        timer_update(_clock_tics);
    }

    // Destroy display and sound
    display_teardown();
    sound_teardown();
}

void GB_stop(void)
{
    if(_save_ptr != NULL) {
        fclose(_save_ptr);
        _save_ptr = NULL;
    }
    if(_rom_ptr != NULL) {
        fclose(_rom_ptr);
        _rom_ptr = NULL;
    }

    _state = STOPPED;
}

void GB_save_state(char *save_state_file)
{
    // TODO
}

void GB_exit(void)
{
    _exit_code = EXIT_FAILURE;
    GB_stop();
}

int GB_exit_code(void)
{
    return _exit_code;
}

void GB_reset(void)
{
    _exit_code = EXIT_SUCCESS;

    cpu_reset();
    mmu_reset();
    mbc_reset();
    video_reset();
    audio_reset();
    timer_reset();
    serial_reset();
    joypad_reset();
}