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

#ifndef NEC_GB_H
#define NEC_GB_H

#include <stdint.h>

/**
 *
 */
enum GB_key {
    RIGHT = 0x01,
    LEFT = 0x02,
    UP = 0x04,
    DOWN = 0x08,
    A = 0x10,
    B = 0x20,
    SELECT = 0x40,
    START = 0x80
};

/**
 *
 * @param key
 */
void key_pressed(enum GB_key key);

/**
 *
 * @param key
 */
void key_released(enum GB_key key);

/**
 *
 * @param bios_file
 */
void GB_load_bios(const char *bios_file);

/**
 *
 * @param bios_file
 */
void GB_load_cartridge(const char *rom_file, char *save_file);

/**
 *
 */
void GB_start(void);

/**
 *
 */
void GB_stop(void);

/**
 *
 * @param save_state_file
 */
void GB_save_state(char *save_state_file);

/**
 *
 */
void GB_exit(void);

/**
 *
 * @return
 */
int GB_exit_code(void);

/**
 *
 */
void GB_reset(void);

/**
 *
 * @param message
 */
extern void log_error(char *message, ...);

/**
 *
 * @param message
 */
extern void log_warning(char *message, ...);

/**
 *
 */
extern void sync_frame(void);

/**
 *
 * @param data
 */
extern void serial_transfer_initiate(uint8_t data);

/**
 *
 * @param data
 */
void serial_transfer_complete(uint8_t data);

/**
 *
 * @param title
 */
extern void set_title(const char *title);

#endif /* NEC_GB_H */
