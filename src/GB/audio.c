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

#include "audio.h"
#include "GB.h"

#include <SDL2/SDL.h>

#define AUDIO_SRC_FREQ      524288
#define AUDIO_SRC_FORMAT    AUDIO_S8
#define AUDIO_SRC_CHANNELS  2
#define AUDIO_SRC_SAMPLES   8

static SDL_AudioDeviceID _audio_device;
static SDL_AudioStream *_audio_stream;

static void audio_callback(void *unused, Uint8 *stream, int len)
{
    // TODO: Empty buffer (for some part)
    // Too many samples are generated if VSYNC is turned off.
    // This means we're filling the buffer as soon as we're using fast forward
    // We should playback quicker if this happens
    SDL_AudioStreamGet(_audio_stream, stream, len);
}

void audio_setup(void)
{
    const SDL_AudioSpec _want = {
            .freq = AUDIO_SRC_FREQ,
            .format = AUDIO_SRC_FORMAT,
            .channels = AUDIO_SRC_CHANNELS,
            .samples = AUDIO_SRC_SAMPLES,
            .callback = audio_callback,
            .userdata = NULL
    };
    SDL_AudioSpec _have;

    _audio_device = SDL_OpenAudioDevice(NULL, 0, &_want, &_have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if(_audio_device == 0) {
        log_error("Could not retrieve a valid audio device: %s.\n", SDL_GetError());
        GB_exit();
        return;
    }

    _audio_stream = SDL_NewAudioStream(AUDIO_SRC_FORMAT, AUDIO_SRC_CHANNELS, AUDIO_SRC_FREQ, _have.format, _have.channels, _have.freq);
    if(_audio_stream == NULL) {
        log_error("Could not initialize audio stream: %s.\n", SDL_GetError());
        GB_exit();
        return;
    }
}

void audio_enable(void)
{
    SDL_AudioStreamClear(_audio_stream);
    SDL_ClearQueuedAudio(_audio_device);
    SDL_PauseAudioDevice(_audio_device, 0);
}

void audio_disable(void)
{
    SDL_AudioStreamClear(_audio_stream);
    SDL_ClearQueuedAudio(_audio_device);
    SDL_PauseAudioDevice(_audio_device, 1);
}

void audio_play(struct sound *_sound)
{
    if(SDL_GetAudioDeviceStatus(_audio_device) != SDL_AUDIO_PLAYING) {
        return;
    }

    int8_t data[2] = {0, 0};

    SDL_MixAudioFormat((Uint8 *) &data[0], (const Uint8 *) &_sound->vin_left, AUDIO_SRC_FORMAT, 1, _sound->volume_left * 0x10);
    SDL_MixAudioFormat((Uint8 *) &data[0], (const Uint8 *) &_sound->mix_left, AUDIO_SRC_FORMAT, 1, _sound->volume_left * 0x10);

    SDL_MixAudioFormat((Uint8 *) &data[1], (const Uint8 *) &_sound->vin_right, AUDIO_SRC_FORMAT, 1, _sound->volume_right * 0x10);
    SDL_MixAudioFormat((Uint8 *) &data[1], (const Uint8 *) &_sound->mix_right, AUDIO_SRC_FORMAT, 1, _sound->volume_right * 0x10);

    if(SDL_AudioStreamPut(_audio_stream, data, 2) < 0) {
        log_warning("Invalid write to stream: %s\n", SDL_GetError());
    }

}

void audio_teardown(void)
{
    if(_audio_stream != NULL) {
        SDL_AudioStreamClear(_audio_stream);
        SDL_FreeAudioStream(_audio_stream);
    }

    if(_audio_device != 0) {
        SDL_CloseAudioDevice(_audio_device);
    }
}

