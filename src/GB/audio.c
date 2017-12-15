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

static SDL_AudioDeviceID _audio_device;

static SDL_AudioSpec _have;

void audio_setup(void)
{
    const SDL_AudioSpec _want = {
            .freq = 48000,
            .format = AUDIO_S8,
            .channels = 2,
            .samples = 1,
            .callback = NULL,
            .userdata = NULL
    };

    _audio_device = SDL_OpenAudioDevice(NULL, 0, &_want, &_have, 0);
    if(_audio_device == 0) {
        log_error("Could not retrieve a valid audio device.");
        GB_exit();
        return;
    }
}

void audio_enable(void)
{
    SDL_PauseAudioDevice(_audio_device, 0);
}

void audio_disable(void)
{
    SDL_PauseAudioDevice(_audio_device, 1);
}

void audio_play(struct sound *_sound)
{
    SDL_QueueAudio(_audio_device, _sound, sizeof(struct sound));
}

void audio_teardown(void)
{
    if(_audio_device != 0) {
        SDL_CloseAudioDevice(_audio_device);
    }
}

