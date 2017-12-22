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

#include "sound.h"

#include <stdbool.h>

#include "audio.h"
#include "cartridge.h"

#define NR10_ADDRESS    0xFF10
#define NR11_ADDRESS    0xFF11
#define NR12_ADDRESS    0xFF12
#define NR13_ADDRESS    0xFF13
#define NR14_ADDRESS    0xFF14
#define NR21_ADDRESS    0xFF16
#define NR22_ADDRESS    0xFF17
#define NR23_ADDRESS    0xFF18
#define NR24_ADDRESS    0xFF19
#define NR30_ADDRESS    0xFF1A
#define NR31_ADDRESS    0xFF1B
#define NR32_ADDRESS    0xFF1C
#define NR33_ADDRESS    0xFF1D
#define NR34_ADDRESS    0xFF1E
#define NR41_ADDRESS    0xFF20
#define NR42_ADDRESS    0xFF21
#define NR43_ADDRESS    0xFF22
#define NR44_ADDRESS    0xFF23
#define NR50_ADDRESS    0xFF24
#define NR51_ADDRESS    0xFF25
#define NR52_ADDRESS    0xFF26
#define _WAVE_PATTERN_RAM_OFFSET        0xFF30
#define _WAVE_PATTERN_RAM_OFFSET_END    0xFF40
#define _WAVE_PATTERN_RAM_SIZE          (_WAVE_PATTERN_RAM_OFFSET_END - _WAVE_PATTERN_RAM_OFFSET)

#define CPU_CLK_SPEED   4194304
#define _512HZ_DIV      8192

#define WAVEFORM_PERIOD 8

static uint32_t _timer_clk = 0;
static uint8_t _frame_seq = 0;

/*
 * Audio control
 */
static uint8_t _nr10 = 0x00;
static uint8_t _nr11 = 0x00;
static uint8_t _nr12 = 0x00;
static uint8_t _nr13 = 0x00;
static uint8_t _nr14 = 0x00;
static uint8_t _nr21 = 0x00;
static uint8_t _nr22 = 0x00;
static uint8_t _nr23 = 0x00;
static uint8_t _nr24 = 0x00;
static uint8_t _nr30 = 0x00;
static uint8_t _nr31 = 0x00;
static uint8_t _nr32 = 0x00;
static uint8_t _nr33 = 0x00;
static uint8_t _nr34 = 0x00;
static uint8_t _nr41 = 0x00;
static uint8_t _nr42 = 0x00;
static uint8_t _nr43 = 0x00;
static uint8_t _nr44 = 0x00;
static uint8_t _nr50 = 0x00;
static uint8_t _nr51 = 0x00;
static uint8_t _nr52 = 0x00;
static uint8_t _wave_pattern_ram[_WAVE_PATTERN_RAM_SIZE];

static inline void reset_regs(void)
{
    _nr10 = 0x00;
    _nr11 = 0x00;
    _nr12 = 0x00;
    _nr13 = 0x00;
    _nr14 = 0x00;
    _nr21 = 0x00;
    _nr22 = 0x00;
    _nr23 = 0x00;
    _nr24 = 0x00;
    _nr30 = 0x00;
    _nr31 = 0x00;
    _nr32 = 0x00;
    _nr33 = 0x00;
    _nr34 = 0x00;
    _nr41 = 0x00;
    _nr42 = 0x00;
    _nr43 = 0x00;
    _nr44 = 0x00;
    _nr50 = 0x00;
    _nr51 = 0x00;
    _nr52 = 0x00;
}

struct frequency_sweep {
    uint16_t shadow_frequency;
    uint8_t timer;
    bool enabled;
};

struct length_counter {
    uint16_t timer;
    bool enabled;
};

struct volume_envelope {
    uint8_t timer;
    uint8_t period;
    uint8_t volume;
    bool direction;
    bool enabled;
};

struct lfsr {
    uint16_t shift_reg;
};

static struct {
    struct frequency_sweep sweep;
    uint16_t timer;
    uint8_t duty;
    struct length_counter length;
    struct volume_envelope envelope;

    uint8_t output;
} _square_1;

static struct {
    uint16_t timer;
    uint8_t duty;
    struct length_counter length;
    struct volume_envelope envelope;

    uint8_t output;
} _square_2;

static struct {
    uint16_t timer;
    struct length_counter length;
    uint8_t volume;
    uint8_t sample;

    uint8_t output;
} _wave;

static struct {
    uint32_t timer;
    struct lfsr lfsr;
    struct length_counter length;
    struct volume_envelope envelope;

    uint8_t output;
} _noise;

static inline void square_1_untrigger(void)
{
    _nr52 &= 0xFE;
}

static inline void square_2_untrigger(void)
{
    _nr52 &= 0xFD;
}

static inline void wave_untrigger(void)
{
    _nr52 &= 0xFB;
}

static inline void noise_untrigger(void)
{
    _nr52 &= 0xF7;
}

static inline bool square_1_is_triggered(void)
{
    return (_nr52 & 0x01) == 0x01;
}

static inline bool square_2_is_triggered(void)
{
    return (_nr52 & 0x02) == 0x02;
}

static inline bool wave_is_triggered(void)
{
    return (_nr52 & 0x04) == 0x04;
}

static inline bool noise_is_triggered(void)
{
    return (_nr52 & 0x08) == 0x08;
}

static void length_counter_step(void)
{
    if(square_1_is_triggered() && _square_1.length.enabled) {
        if (--_square_1.length.timer == 0) {
            square_1_untrigger();
        }
    }
    if(square_2_is_triggered() && _square_2.length.enabled) {
        if (--_square_2.length.timer == 0) {
            square_2_untrigger();
        }
    }
    if(wave_is_triggered() && _wave.length.enabled) {
        if (--_wave.length.timer == 0) {
            wave_untrigger();
        }
    }
    if(noise_is_triggered() && _noise.length.enabled) {
        if (--_noise.length.timer == 0) {
            noise_untrigger();
        }
    }
}

static void volume_envelope_step(void)
{
    for(int i = 0; i < 4; i++) {
        struct volume_envelope *env;
        switch (i) {
            case 0:
                env = &_square_1.envelope;
                break;
            case 1:
                env = &_square_2.envelope;
                break;
            case 3:
                env = &_noise.envelope;
                break;
            case 2:
            default:
                continue;
        }

        if(env->enabled) {
            if(--env->timer == 0) {
                env->timer = env->period;
                if(env->direction && env->volume < 0x0F) {
                    env->volume++;
                } else if(!env->direction && env->volume > 0x00) {
                    env->volume--;
                } else {
                    env->enabled = false;
                }
            }
        }
    }
}

static inline uint16_t frequency_sweep_calc(struct frequency_sweep *sweep)
{
    // Frequency calculation
    if(_nr10 & 0x08) {
        return sweep->shadow_frequency - (sweep->shadow_frequency >> (_nr10 & 0x07));
    }
    return sweep->shadow_frequency + (sweep->shadow_frequency >> (_nr10 & 0x07));
}

static void frequency_sweep_step(void)
{
    struct frequency_sweep *sweep = &_square_1.sweep;

    if(sweep->enabled && ((_nr10 & 0x70) != 0) && --sweep->timer == 0) {
        sweep->timer = (uint8_t) ((_nr10 & 0x70) >> 4);
        uint16_t new_freq = frequency_sweep_calc(sweep);
        if(new_freq > 0x7FF) {
            square_1_untrigger();
        } else {
            _square_1.sweep.shadow_frequency = new_freq;
            _nr13 = (uint8_t) (new_freq & 0xFF);
            _nr14 = (uint8_t) ((_nr14 & 0xFC) | ((new_freq >> 8) & 0x07));

            if(frequency_sweep_calc(sweep) > 0x7FF) {
                square_1_untrigger();
            }
        }
    }
}

static void frequency_sweep_trigger(uint16_t freq)
{
    _square_1.sweep.shadow_frequency = freq;
    _square_1.sweep.timer = (uint8_t) ((_nr10 >> 4) & 0x07);
    _square_1.sweep.enabled = ((_nr10 & 0x77) != 0);

    if(_nr10 & 0x07) {
        if(frequency_sweep_calc(&_square_1.sweep) > 0x7FF) {
            square_1_untrigger();
        }
    }
}

/*
 * Audio source 1: Square 1
 */
static void square_1_trigger(bool lc_enabled)
{
    uint16_t freq = (uint16_t) (((_nr14 & 0x07) << 8) | _nr13);

    _nr52 |= 0x01;

    _square_1.length.enabled = lc_enabled;
    if(_square_1.length.timer == 0) {
        _square_1.length.timer = 64;
    }

    _square_1.timer = (uint16_t) (4 * (2048 - freq));

    _square_1.envelope.period = (uint8_t) (_nr12 & 0x07);
    _square_1.envelope.timer = _square_1.envelope.period;
    _square_1.envelope.direction = (_nr12 & 0x08) == 0x08;
    _square_1.envelope.volume = (uint8_t) ((_nr12 & 0xF0) >> 4);
    _square_1.envelope.enabled = (_square_1.envelope.period != 0);

    frequency_sweep_trigger(freq);

    // Check if DAC is on
    if((_nr12 & 0xF8) == 0) {
        square_1_untrigger();
    }
}

static void square_1_step(void)
{
    if(square_1_is_triggered()) {
        if(--_square_1.timer == 0) {
            _square_1.timer = (uint16_t) (4 * (2048 - (((_nr14 & 0x07) << 8) | _nr13)));

            switch ((_nr11 >> 6) & 0x03) {
                case 0x00:
                    if (_square_1.duty == 7) {
                        _square_1.output = _square_1.envelope.volume;
                    } else {
                        _square_1.output = 0;
                    }
                    break;
                case 0x01:
                    if (_square_1.duty == 0 || _square_1.duty == 7) {
                        _square_1.output = _square_1.envelope.volume;
                    } else {
                        _square_1.output = 0;
                    }
                    break;
                case 0x02:
                    if (_square_1.duty == 0 || _square_1.duty >= 5) {
                        _square_1.output = _square_1.envelope.volume;
                    } else {
                        _square_1.output = 0;
                    }
                    break;
                case 0x03:
                    if(_square_1.duty == 0 || _square_1.duty == 7){
                        _square_1.output = 0;
                    } else {
                        _square_1.output = _square_1.envelope.volume;
                    }
                    break;
                default:
                    break;

            }
            _square_1.duty = (uint8_t) ((_square_1.duty + 1) % WAVEFORM_PERIOD);
        }
    } else {
        _square_1.duty = 0;
        _square_1.output = 0;
    }
}

/*
 * Audio source 2: Square 2
 */
static void square_2_trigger(bool lc_enabled)
{
    uint16_t freq = (uint16_t) (((_nr24 & 0x07) << 8) | _nr23);

    _nr52 |= 0x02;

    _square_2.length.enabled = lc_enabled;
    if(_square_2.length.timer == 0) {
        _square_2.length.timer = 64;
    }

    _square_2.timer = (uint16_t) (4 * (2048 - freq));

    _square_2.envelope.period = (uint8_t) (_nr22 & 0x07);
    _square_2.envelope.timer = _square_2.envelope.period;
    _square_2.envelope.direction = (_nr22 & 0x08) == 0x08;
    _square_2.envelope.volume = (uint8_t) ((_nr22 & 0xF0) >> 4);
    _square_2.envelope.enabled = (_square_2.envelope.period != 0);

    // Check if DAC is on
    if((_nr22 & 0xF8) == 0) {
        square_2_untrigger();
    }
}

static void square_2_step(void)
{
    if(square_2_is_triggered()) {
        if(--_square_2.timer == 0) {
            _square_2.timer = (uint16_t) (4 * (2048 - (((_nr24 & 0x07) << 8) | _nr23)));

            switch ((_nr21 >> 6) & 0x03) {
                case 0x00:
                    if (_square_2.duty == 7) {
                        _square_2.output = _square_2.envelope.volume;
                    } else {
                        _square_2.output = 0;
                    }
                    break;
                case 0x01:
                    if (_square_2.duty == 0 || _square_2.duty == 7) {
                        _square_2.output = _square_2.envelope.volume;
                    } else {
                        _square_2.output = 0;
                    }
                    break;
                case 0x02:
                    if (_square_2.duty == 0 || _square_2.duty >= 5) {
                        _square_2.output = _square_2.envelope.volume;
                    } else {
                        _square_2.output = 0;
                    }
                    break;
                case 0x03:
                    if(_square_2.duty == 0 || _square_2.duty == 7){
                        _square_2.output = 0;
                    } else {
                        _square_2.output = _square_2.envelope.volume;
                    }
                    break;
                default:
                    break;

            }
            _square_2.duty = (uint8_t) ((_square_2.duty + 1) % WAVEFORM_PERIOD);
        }
    } else {
        _square_2.duty = 0;
        _square_2.output = 0;
    }
}

/*
 * Audio source 3: Wave
 */
static void wave_trigger(bool lc_enabled)
{
    _nr52 |= 0x04;

    _wave.length.enabled = lc_enabled;
    if(_wave.length.timer == 0) {
        _wave.length.timer = 256;
    }

    _wave.timer = (uint16_t) (2 * (2048 - (((_nr34 & 0x07) << 8) | _nr33)));

    _wave.volume = (uint8_t) ((_nr32 >> 5) & 0x03);

    _wave.sample = 0;

    // Check if DAC is on
    if((_nr30 & 0x80) == 0) {
        wave_untrigger();
    }
}

static void wave_step(void)
{
    if(wave_is_triggered()) {
        if(--_wave.timer == 0) {
            _wave.timer = (uint16_t) (2 * (2048 - (((_nr34 & 0x07) << 8) | _nr33)));

            uint8_t current_sample = _wave_pattern_ram[_wave.sample / 2];
            uint8_t sample_data = (uint8_t) (_wave.sample % 2);

            if(_wave.volume == 0) {
                _wave.output = 0;
            } else {
                _wave.output = (uint8_t) (((current_sample >> (4 * (1 - sample_data))) & 0x0F) >> (_wave.volume - 1));
            }

            _wave.sample = (uint8_t) ((_wave.sample + 1) % (_WAVE_PATTERN_RAM_SIZE * 2));
        }
    } else {
        _wave.sample = 0;
        _wave.output = 0;
    }
}

/*
 * Audio source 4: Noise
 */
static void noise_trigger(bool lc_enabled)
{
    uint8_t r = (uint8_t) ((_nr43 & 0x07) * 2);
    if(r == 0) r = 1;
    uint8_t s = (uint8_t) ((_nr43 & 0xF0) >> 4);

    _nr52 |= 0x08;

    _noise.length.enabled = lc_enabled;
    if(_noise.length.timer == 0) {
        _noise.length.timer = 256;
    }

    _noise.timer = (uint16_t) (8 * (r << (s + 1)));

    _noise.envelope.period = (uint8_t) (_nr42 & 0x07);
    _noise.envelope.timer = _noise.envelope.period;
    _noise.envelope.direction = (_nr42 & 0x08) == 0x08;
    _noise.envelope.volume = (uint8_t) ((_nr42 & 0xF0) >> 4);
    _noise.envelope.enabled = (_noise.envelope.period != 0);

    _noise.lfsr.shift_reg = 0x7FFF;

    // Check if DAC is on
    if((_nr42 & 0xF8) == 0) {
        noise_untrigger();
    }
}

static void noise_step(void)
{
    if(noise_is_triggered()) {
        if(--_noise.timer == 0) {
            uint8_t r = (uint8_t) ((_nr43 & 0x07) * 2);
            if(r == 0) r = 1;
            uint8_t s = (uint8_t) ((_nr43 & 0xF0) >> 4);

            _noise.timer = (uint16_t) (8 * (r << (s + 1)));

            if(s <= 13) {
                int bit0 = _noise.lfsr.shift_reg & 0x01;
                int bit1 = (_noise.lfsr.shift_reg >> 1) & 0x01;
                int xor = (bit0 ^ bit1);

                _noise.lfsr.shift_reg = (uint16_t) ((xor << 14) | ((_noise.lfsr.shift_reg >> 1) & 0x3FFF));
                if(_nr43 & 0x08) {
                    _noise.lfsr.shift_reg = (uint16_t) ((xor << 6) | ((_noise.lfsr.shift_reg >> 1) & 0x7FBF));
                }
            }

            if(_noise.lfsr.shift_reg & 0x01) {
                _noise.output = 0;
            } else {
                _noise.output = _noise.envelope.volume;
            }
        }
    } else {
        _noise.output = 0;
    }
}

uint8_t sound_read_byte(uint16_t address)
{
    if(_WAVE_PATTERN_RAM_OFFSET <= address && address < _WAVE_PATTERN_RAM_OFFSET_END) {
        return _wave_pattern_ram[address - _WAVE_PATTERN_RAM_OFFSET];
    }

    if (address == NR52_ADDRESS) {
        return (uint8_t) (_nr52 | 0x70);
    }

    if (_nr52 & 0x80) {
        if (address == NR10_ADDRESS) {
            return (uint8_t) (_nr10 | 0x80);
        } else if (address == NR11_ADDRESS) {
            return (uint8_t) (_nr11 | 0x3F);
        } else if (address == NR12_ADDRESS) {
            return _nr12;
        } else if (address == NR13_ADDRESS) {
            return 0xFF;
        } else if (address == NR14_ADDRESS) {
            return (uint8_t) (_nr14 | 0xBF);
        } else if (address == NR21_ADDRESS) {
            return (uint8_t) (_nr21 | 0x3F);
        } else if (address == NR22_ADDRESS) {
            return _nr22;
        } else if (address == NR23_ADDRESS) {
            return 0xFF;
        } else if (address == NR24_ADDRESS) {
            return (uint8_t) (_nr24 | 0xBF);
        } else if (address == NR30_ADDRESS) {
            return (uint8_t) (_nr30 | 0x7F);
        } else if (address == NR31_ADDRESS) {
            return 0xFF;
        } else if (address == NR32_ADDRESS) {
            return (uint8_t) (_nr32 | 0x9F);
        } else if (address == NR33_ADDRESS) {
            return 0xFF;
        } else if (address == NR34_ADDRESS) {
            return (uint8_t) (_nr34 | 0xBF);
        } else if (address == NR41_ADDRESS) {
            return 0xFF;
        } else if (address == NR42_ADDRESS) {
            return _nr42;
        } else if (address == NR43_ADDRESS) {
            return _nr43;
        } else if (address == NR44_ADDRESS) {
            return (uint8_t) (_nr44 | 0xBF);
        } else if (address == NR50_ADDRESS) {
            return _nr50;
        } else if (address == NR51_ADDRESS) {
            return _nr51;
        }
    }

    return 0xFF;
}

void sound_write_byte(uint16_t address, uint8_t value)
{
    if (_WAVE_PATTERN_RAM_OFFSET <= address && address < _WAVE_PATTERN_RAM_OFFSET_END) {
        _wave_pattern_ram[address - _WAVE_PATTERN_RAM_OFFSET] = value;
    }

    if (address == NR52_ADDRESS) {
        _nr52 = (uint8_t) ((value & 0x80) | (_nr52 & 0x0F));
        if(value & 0x80) {
            audio_enable();
            _frame_seq = 0;
            _square_1.duty = 0;
            _square_2.duty = 0;
            _wave.sample = 0;
        } else {
            audio_disable();
            reset_regs();
        }
    }

    if (_nr52 & 0x80) {
        if (address == NR10_ADDRESS) {
            _nr10 = value;
        } else if (address == NR11_ADDRESS) {
            _nr11 = value;
            _square_1.length.timer = (uint16_t) (64 - (value & 0x3F));
        } else if (address == NR12_ADDRESS) {
            _nr12 = value;
        } else if (address == NR13_ADDRESS) {
            _nr13 = value;
        } else if (address == NR14_ADDRESS) {
            _nr14 = value;
            if (value & 0x80) {
                square_1_trigger((value & 0x40) == 0x40);
            }
        } else if (address == NR21_ADDRESS) {
            _nr21 = value;
            _square_2.length.timer = (uint16_t) (64 - (value & 0x3F));
        } else if (address == NR22_ADDRESS) {
            _nr22 = value;
        } else if (address == NR23_ADDRESS) {
            _nr23 = value;
        } else if (address == NR24_ADDRESS) {
            _nr24 = value;
            if (value & 0x80) {
                square_2_trigger((value & 0x40) == 0x40);
            }
        } else if (address == NR30_ADDRESS) {
            _nr30 = value;
        } else if (address == NR31_ADDRESS) {
            _nr31 = value;
            _wave.length.timer = (uint16_t) (256 - (value & 0x3F));
        } else if (address == NR32_ADDRESS) {
            _nr32 = value;
        } else if (address == NR33_ADDRESS) {
            _nr33 = value;
        } else if (address == NR34_ADDRESS) {
            _nr34 = value;
            if (value & 0x80) {
                wave_trigger((value & 0x40) == 0x40);
            }
        } else if (address == NR41_ADDRESS) {
            _nr41 = value;
            _noise.length.timer = (uint16_t) (64 - (value & 0x3F));
        } else if (address == NR42_ADDRESS) {
            _nr42 = value;
        } else if (address == NR43_ADDRESS) {
            _nr43 = value;
        } else if (address == NR44_ADDRESS) {
            _nr44 = value;
            if (value & 0x80) {
                noise_trigger((value & 0x40) == 0x40);
            }
        } else if (address == NR50_ADDRESS) {
            _nr50 = value;
        } else if (address == NR51_ADDRESS) {
            _nr51 = value;
        }
    }
}

void audio_update(uint8_t clk_tics)
{
    struct sound _sound;

    // Save old clock
    uint32_t old_timer_clk = _timer_clk;

    // Update clock
    _timer_clk += clk_tics;

    if(_nr52 & 0x80) {
        for(int i = 0; i < (_timer_clk - old_timer_clk); i++) {
            if((_timer_clk + i) % _512HZ_DIV == 0) {
                if( (_frame_seq % 2) == 0 ) {
                    length_counter_step();
                }
                if( (_frame_seq % 8) == 7 ) {
                    volume_envelope_step();
                }
                if( (_frame_seq % 4) == 2 ) {
                    frequency_sweep_step();
                }
                _frame_seq = (uint8_t) ((_frame_seq + 1) % 8);
            }

            square_1_step();
            square_2_step();
            wave_step();
            noise_step();

            if((_timer_clk + i) % 8 != 0) {
                continue;
            }

            float dac1 = (_nr52 & 0x01 ? ((float)_square_1.output / 7.5f) - 1.0f : 0.0f);
            float dac2 = (_nr52 & 0x02 ? ((float)_square_2.output / 7.5f) - 1.0f : 0.0f);
            float dac3 = (_nr52 & 0x04 ? ((float)_wave.output / 7.5f) - 1.0f : 0.0f);
            float dac4 = (_nr52 & 0x08 ? ((float)_noise.output / 7.5f) - 1.0f : 0.0f);

            float mixer1 = 0.0f;
            float mixer2 = 0.0f;

            // S01 Mixing
            if (_nr51 & 0x01) {
                // Output sound 1 to SO1
                mixer1 += dac1;
            }
            if (_nr51 & 0x02) {
                // Output sound 2 to SO1
                mixer1 += dac2;
            }
            if (_nr51 & 0x04) {
                // Output sound 3 to SO1
                mixer1 += dac3;
            }
            if (_nr51 & 0x08) {
                // Output sound 4 to SO1
                mixer1 += dac4;
            }

            // S02 Mixing
            if (_nr51 & 0x10) {
                // Output sound 1 to SO2
                mixer2 += dac1;
            }
            if (_nr51 & 0x20) {
                // Output sound 2 to SO2
                mixer2 += dac2;
            }
            if (_nr51 & 0x40) {
                // Output sound 3 to SO2
                mixer2 += dac3;
            }
            if (_nr51 & 0x80) {
                // Output sound 4 to SO2
                mixer2 += dac4;
            }

            _sound.mix_left = (int8_t) (mixer1 * 0x20);
            _sound.mix_right = (int8_t) (mixer2 * 0x20);

            _sound.vin_left = 0;
            _sound.vin_right = 0;

            if (_nr50 & 0x08) {
                // Output Vin to SO1
                _sound.vin_left = get_vin();
            }
            if (_nr50 & 0x80) {
                // Output Vin to SO2
                _sound.vin_right = get_vin();
            }

            _sound.volume_left = (int8_t) ((_nr50 & 0x07) + 1);
            _sound.volume_right = (int8_t) (((_nr50 >> 4) & 0x07) + 1);

            audio_play(&_sound);
        }
    }

    _timer_clk %= CPU_CLK_SPEED;
}

void audio_reset(void)
{
    reset_regs();

    _timer_clk = 0;
    _frame_seq = 0;
}