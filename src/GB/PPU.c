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

#include "PPU.h"

#include <stdbool.h>

#include "MMU.h"
#include "LR35902.h"
#include "display.h"
#include "GB.h"

#define LAST_SCREEN_LINE    143
#define LAST_VBLANK_LINE    153

#define HBLANK_MODE_CLOCKS      204
#define VBLANK_MODE_CLOCKS      456
#define OAM_READ_MODE_CLOCKS    80
#define VRAM_READ_MODE_CLOCKS   172

#define SPRITES_PER_LINE        10
#define PIXEL_FIFO_SIZE         16

#define OAM_SPRITE_SIZE         (_OAM_SIZE / 4)
#define VRAM_TILE_DATA_SIZE     0x0800
#define VRAM_NUM_TILE_DATA      3
#define VRAM_TILE_MAP_SIZE      0x0400
#define VRAM_NUM_TILE_MAPS      2

#define LCDC_ADDRESS    0xFF40
#define STAT_ADDRESS    0xFF41
#define SCY_ADDRESS     0xFF42
#define SCX_ADDRESS     0xFF43
#define LY_ADDRESS      0xFF44
#define LYC_ADDRESS     0xFF45
#define DMA_ADDRESS     0xFF46
#define BGP_ADDRESS     0xFF47
#define OBP0_ADDRESS    0xFF48
#define OBP1_ADDRESS    0xFF49
#define WY_ADDRESS      0xFF4A
#define WX_ADDRESS      0xFF4B

static uint8_t _lcdc = 0x00;
static uint8_t _stat = 0x04;
static uint8_t _scy = 0x00;
static uint8_t _scx = 0x00;
static uint8_t _ly = 0x00;
static uint8_t _lyc = 0x00;
static uint8_t _dma = 0x00;
static uint8_t _bgp = 0x00;
static uint8_t _obp[2] = {0x00, 0x00};
static uint8_t _wx = 0x00;
static uint8_t _wy = 0x00;

static uint32_t _mode_clocks = 0;

struct sprite {
    uint8_t y;
    uint8_t x;
    uint8_t code;
    uint8_t flags;
};

static union {
    struct {
        uint8_t tile_data[VRAM_NUM_TILE_DATA][VRAM_TILE_DATA_SIZE];
        uint8_t tile_map[VRAM_NUM_TILE_MAPS][VRAM_TILE_MAP_SIZE];
    };
    uint8_t raw[_VRAM_SIZE];
} _vram;

static union {
    struct sprite structured[OAM_SPRITE_SIZE];
    uint8_t raw[_OAM_SIZE];
} _oam;

static struct sprite *_visible_sprites[SPRITES_PER_LINE] = {NULL};

enum fetch_state {
    FETCH_TILE_NO,
    FETCH_DATA0,
    FETCH_DATA1,
    FETCH_SAVE
};

enum fetch_source {
    FETCH_BG,
    FETCH_WINDOW,
    FETCH_OBJ
};

static struct {
    uint8_t scy;
    uint8_t scx;
    uint8_t ly;
    uint8_t lx;
    uint8_t lyc;
    uint8_t wy;
    uint8_t wx;

    struct {
        uint8_t read_ptr;
        uint8_t write_ptr;
        int8_t revs;
        struct {
            uint8_t data;
            uint8_t *palette;
        } pixel[PIXEL_FIFO_SIZE];
        bool idle;
    } fifo;

    struct {
        struct {
            uint16_t base;
            uint16_t x_offset;
            uint16_t y_offset;
        } address;
        uint8_t tile_no;
        uint8_t data0;
        uint8_t data1;
        enum fetch_state state;
        enum fetch_source src;
        struct sprite *sprite;
        bool idle;
    } fetch;
} _pipeline;

/**
 *
 * @param x
 * @return
 */
static struct sprite *find_sprite(const uint8_t x)
{
    for(int i = 0; i < SPRITES_PER_LINE; i++) {
        if(_visible_sprites[i]->x == x) {
            return _visible_sprites[i];
        }
    }
    return NULL;
}

/**
 *
 */
static void pixel_pipeline_reset(void)
{
    _pipeline.scy = 0x00;
    _pipeline.scx = 0x00;
    _pipeline.ly = 0x00;
    _pipeline.lx = 0x00;
    _pipeline.lyc = 0x00;
    _pipeline.wy = 0x00;
    _pipeline.wx = 0x00;

    _pipeline.fifo.read_ptr = 0;
    _pipeline.fifo.write_ptr = 0;
    _pipeline.fifo.revs = 0;
    _pipeline.fifo.idle = true;

    _pipeline.fetch.state = FETCH_TILE_NO;
    _pipeline.fetch.src = FETCH_BG;
    _pipeline.fetch.sprite = NULL;
    _pipeline.fetch.idle = false;
}

/**
 *
 * @param scy
 * @param scx
 * @param ly
 * @param lyc
 * @param wy
 * @param wx
 */
static void pixel_pipeline_init(uint8_t scy, uint8_t scx, uint8_t ly, uint8_t lyc, uint8_t wy, uint8_t wx)
{
    _pipeline.scy = scy;
    _pipeline.scx = (uint8_t) (scx & 0x07);
    _pipeline.ly = ly;
    _pipeline.lx = 0x00;
    _pipeline.lyc = lyc;
    _pipeline.wy = wy;
    _pipeline.wx = wx;

    _pipeline.fifo.read_ptr = 0;
    _pipeline.fifo.write_ptr = 0;
    _pipeline.fifo.revs = 0;
    _pipeline.fifo.idle = true;

    _pipeline.fetch.state = FETCH_TILE_NO;
    _pipeline.fetch.src = FETCH_BG;
    _pipeline.fetch.idle = false;

    _pipeline.fetch.address.base = (uint16_t) ((_lcdc & 0x08) ? 1 : 0);
    _pipeline.fetch.address.x_offset = (scx >> 3);
    _pipeline.fetch.address.y_offset = (uint16_t) ((((_ly + _scy) >> 3) & 0x1F) * 0x20);
}

/**
 *
 * @return
 */
static bool pixel_pipeline_step(void)
{
    size_t fifo_size = (size_t) ((_pipeline.fifo.revs * PIXEL_FIFO_SIZE) + _pipeline.fifo.write_ptr - _pipeline.fifo.read_ptr);

    // FIFO Shift
    if(!_pipeline.fifo.idle) {
        uint8_t color_idx = _pipeline.fifo.pixel[_pipeline.fifo.read_ptr].data;
        uint8_t palette = *_pipeline.fifo.pixel[_pipeline.fifo.read_ptr].palette;
        _pipeline.fifo.read_ptr++;
        if(_pipeline.fifo.read_ptr >= PIXEL_FIFO_SIZE) {
            _pipeline.fifo.revs--;
            _pipeline.fifo.read_ptr -= PIXEL_FIFO_SIZE;
        }
        fifo_size--;

        float color = 1.0f - ((float)((palette >> (color_idx * 2)) & 0x03) / 3.0f);
        if(!_pipeline.scx) {
            if((_lcdc & 0x80) && (_lcdc & 0x01)) {
                _display.lines[_pipeline.ly].dots[_pipeline.lx].r = color;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].g = color;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].b = color;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].a = color;
            } else {
                _display.lines[_pipeline.ly].dots[_pipeline.lx].r = 1.0f;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].g = 1.0f;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].b = 1.0f;
                _display.lines[_pipeline.ly].dots[_pipeline.lx].a = 1.0f;
            }
            _pipeline.lx++;
        } else {
            _pipeline.scx--;
        }
    }

    // Fetch
    if(!_pipeline.fetch.idle) {
        int tile_idx = ((_pipeline.fetch.tile_no & 0x80) ? 1 : ((_lcdc & 0x10) ? 0 : 2));
        int tile_data = ((_pipeline.fetch.tile_no & 0x7F) * 0x10) + (((_ly + _scy) & 0x07) * 0x02);
        switch (_pipeline.fetch.state) {
            case FETCH_TILE_NO:
                _pipeline.fetch.tile_no = _vram.tile_map[_pipeline.fetch.address.base][_pipeline.fetch.address.x_offset + _pipeline.fetch.address.y_offset];
                _pipeline.fetch.state = FETCH_DATA0;
                break;
            case FETCH_DATA0:
                _pipeline.fetch.data0 = _vram.tile_data[tile_idx][tile_data];
                _pipeline.fetch.state = FETCH_DATA1;
                break;
            case FETCH_DATA1:
                _pipeline.fetch.data1 = _vram.tile_data[tile_idx][tile_data + 1];
                _pipeline.fetch.state = FETCH_SAVE;
                break;
            case FETCH_SAVE:
                if(_pipeline.fetch.src == FETCH_OBJ) {
                } else {
                    if(fifo_size + 8 <= PIXEL_FIFO_SIZE) {
                        for(int i = 0; i < 8; i++) {
                            _pipeline.fifo.pixel[_pipeline.fifo.write_ptr].data = (uint8_t) ((((_pipeline.fetch.data0 >> (7 - i)) & 0x01) << 1) | ((_pipeline.fetch.data1 >> (7 - i)) & 0x01));
                            _pipeline.fifo.pixel[_pipeline.fifo.write_ptr].palette = &_bgp;
                            _pipeline.fifo.write_ptr++;
                            if(_pipeline.fifo.write_ptr >= PIXEL_FIFO_SIZE) {
                                _pipeline.fifo.revs++;
                                _pipeline.fifo.write_ptr -= PIXEL_FIFO_SIZE;
                            }
                            fifo_size++;
                        }
                        _pipeline.fifo.idle = (fifo_size <= 8);
                        _pipeline.fetch.address.x_offset = (uint16_t) ((_pipeline.fetch.address.x_offset + 1) & 0x1F);
                        _pipeline.fetch.state = FETCH_TILE_NO;
                    }
                }
                break;
        }
    }
    _pipeline.fetch.idle = !_pipeline.fetch.idle;

    // Return true if we're done with a line
    return (_pipeline.lx == 160);
}

/**
 *
 */
static void OAM_search(void)
{
    int s = 0;
    const int h = ((_lcdc & 0x04) ? 0x10 : 0x08);
    for(int i = 0; i < OAM_SPRITE_SIZE; i++) {
        if(_oam.structured[i].x && ((_ly + 0x10) >= _oam.structured[i].y) && ((_ly + 0x10) < (_oam.structured[i].y + h))) {
            _visible_sprites[s++] = &_oam.structured[i];
            if(s == SPRITES_PER_LINE) {
                return;
            }
        }
    }
    while (s < SPRITES_PER_LINE) {
        _visible_sprites[s++] = NULL;
    }
}

uint8_t vram_read_byte(uint16_t address)
{
    if (((_stat & 0x03) <= 0x02) || !(_lcdc & 0x80)) {
        return _vram.raw[address - _VRAM_OFFSET];
    }
    return 0xFF;
}

void vram_write_byte(uint16_t address, uint8_t value)
{
    if (((_stat & 0x03) <= 0x02) || !(_lcdc & 0x80)) {
        _vram.raw[address - _VRAM_OFFSET] = value;
    }
}

uint8_t oam_read_byte(uint16_t address)
{
    if (((_stat & 0x03) <= 0x01) || !(_lcdc & 0x80)) {
        return _oam.raw[address - _OAM_OFFSET];
    }
    return 0xFF;
}

void oam_write_byte(uint16_t address, uint8_t value)
{
    if (((_stat & 0x03) <= 0x01) || !(_lcdc & 0x80)) {
        _oam.raw[address - _OAM_OFFSET] = value;
    }
}

uint8_t video_read_byte(uint16_t address)
{
    switch (address) {
        case LCDC_ADDRESS:
            return _lcdc;
        case STAT_ADDRESS:
            return _stat;
        case SCY_ADDRESS:
            return _scy;
        case SCX_ADDRESS:
            return _scx;
        case LY_ADDRESS:
            return _ly;
        case LYC_ADDRESS:
            return _lyc;
        case DMA_ADDRESS:
            return _dma;
        case BGP_ADDRESS:
            return _bgp;
        case OBP0_ADDRESS:
            return _obp[0];
        case OBP1_ADDRESS:
            return _obp[1];
        case WY_ADDRESS:
            return _wy;
        case WX_ADDRESS:
            return _wx;
        default:
            return 0;
    }
}

void video_write_byte(uint16_t address, uint8_t value)
{
    switch (address) {
        case LCDC_ADDRESS:
            if(!(_lcdc & 0x80) && (value & 0x80)) {
                _ly = 0;
                _mode_clocks = 0;
                _stat = (uint8_t) ((_stat & 0xFC) | 0x02);
            }
            _lcdc = value;
            break;
        case STAT_ADDRESS:
            _stat = (uint8_t) ((value & 0x78) | (_stat & 0x03));
            break;
        case SCY_ADDRESS:
            _scy = value;
            break;
        case SCX_ADDRESS:
            _scx = value;
            break;
        case LY_ADDRESS:
            _ly = 0;
            break;
        case LYC_ADDRESS:
            _lyc = value;
            break;
        case DMA_ADDRESS:
            _dma = value;
            break;
        case BGP_ADDRESS:
            _bgp = value;
            break;
        case OBP0_ADDRESS:
            _obp[0] = value;
            break;
        case OBP1_ADDRESS:
            _obp[1] = value;
            break;
        case WY_ADDRESS:
            _wy = value;
            break;
        case WX_ADDRESS:
            _wx = value;
            break;
        default:
            break;
    }

    // Check coincidence
    if(_ly == _lyc) {
        _stat |= 0x04;
    } else {
        _stat &= 0xFB;
    }

    // Coincidence interrupt
    if(((_stat & 0x40) && (_stat & 0x04))) {
        interrupt(LCDC);
    }
}

void video_update(uint8_t clk_tics)
{
    _mode_clocks += clk_tics;

    switch (_stat & 0x03) {
        default:
        case 0x00: // HBLANK
            if (_mode_clocks >= HBLANK_MODE_CLOCKS) {
                _mode_clocks -= HBLANK_MODE_CLOCKS;

                // Increment line
                _ly++;

                // Check if V-Blank or new line
                if (_ly > LAST_SCREEN_LINE) {
                    _stat = (uint8_t) ((_stat & 0xFC) | 0x01);

                    // Starting VBLANK period
                    display_frame();
                    sync_frame();
                    interrupt(VBLANK);
                } else {
                    _stat = (uint8_t) ((_stat & 0xFC) | 0x02);
                }
            }
            break;
        case 0x01: // VBLANK
            if (_mode_clocks >= VBLANK_MODE_CLOCKS) {
                _mode_clocks -= VBLANK_MODE_CLOCKS;

                // Increment line
                _ly++;

                // Check if done with V-Blank
                if (_ly > LAST_VBLANK_LINE) {
                    _ly = 0;
                    _stat = (uint8_t) ((_stat & 0xFC) | 0x02);
                }
            }
            break;
        case 0x02: // OAM search
            if (_mode_clocks >= OAM_READ_MODE_CLOCKS) {
                _mode_clocks -= OAM_READ_MODE_CLOCKS;
                _stat = (uint8_t) ((_stat & 0xFC) | 0x03);

                // Find all visible sprites in the current line
                OAM_search();

                // Initialize pixel pipeline
                pixel_pipeline_init(_scy, _scx, _ly, _lyc, _wy, _wx);
            }
            break;
        case 0x03: // Transferring data to LCD driver
            for(int i = 0; i < clk_tics; i++) {
                bool line_done = pixel_pipeline_step();
                if(line_done) {
                    _mode_clocks -= VRAM_READ_MODE_CLOCKS;
                    _stat = (uint8_t) (_stat & 0xFC);
                    break;
                }
            }
            break;
    }

    // Check coincidence
    if(_ly == _lyc) {
        _stat |= 0x04;
    } else {
        _stat &= 0xFB;
    }

    if(((_stat & 0x40) && (_stat & 0x04)) ||                // Coincidence interrupt
            (((_stat & 0x03) == 0x00) && (_stat & 0x08)) || // H-Blank interrupt
            (((_stat & 0x03) == 0x01) && (_stat & 0x10)) || // V-Blank interrupt
            (((_stat & 0x03) == 0x02) && (_stat & 0x20))) { // OAM interrupt
        interrupt(LCDC);
    }
}

void video_reset(void)
{
    _lcdc = 0x00;
    _stat = 0x04;
    _scy = 0x00;
    _scx = 0x00;
    _ly = 0x00;
    _lyc = 0x00;
    _dma = 0x00;
    _bgp = 0x00;
    _obp[0] = 0x00;
    _obp[1] = 0x00;
    _wx = 0x00;
    _wy = 0x00;

    _mode_clocks = 0;
    pixel_pipeline_reset();
}