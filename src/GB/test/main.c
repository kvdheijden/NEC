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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <SDL2/SDL.h>

#include "config.h"
#include "../GB.h"

#define PROGRAM_NAME    "NEC-GameBoy"
#define V_SYNC           1

static SDL_Window* window;
static SDL_GLContext gl_context;

static void sdl_die(const char *msg)
{
    fprintf( stderr, "%s: %s\n", msg, SDL_GetError());
    SDL_Quit();
    exit(EXIT_FAILURE);
}

static void sdl_check_error(int line)
{
#if defined(DEBUG) && DEBUG
    const char *error = SDL_GetError();
    if (*error != '\0') {
        fprintf( stderr, "SDL Error: %s\n", error);
        if (line != -1)
            fprintf( stderr, " + line: %i\n", line);
        SDL_ClearError();
    }
#endif
}

void serial_transfer_initiate(uint8_t data)
{

}

static void do_key_down(SDL_KeyboardEvent event)
{
    switch (event.keysym.sym) {
        case SDLK_d:
        case SDLK_RIGHT:
            key_pressed(RIGHT);
            break;
        case SDLK_a:
        case SDLK_LEFT:
            key_pressed(LEFT);
            break;
        case SDLK_w:
        case SDLK_UP:
            key_pressed(UP);
            break;
        case SDLK_s:
        case SDLK_DOWN:
            key_pressed(DOWN);
            break;
        case SDLK_z:
            key_pressed(A);
            break;
        case SDLK_x:
            key_pressed(B);
            break;
        case SDLK_BACKSPACE:
            key_pressed(SELECT);
            break;
        case SDLK_ESCAPE:
        case SDLK_RETURN:
            key_pressed(START);
            break;
        case SDLK_SPACE:
            SDL_GL_SetSwapInterval(0);
            break;
        default:
            break;
    }
}

static void do_key_up(SDL_KeyboardEvent event)
{
    switch (event.keysym.sym) {
        case SDLK_d:
        case SDLK_RIGHT:
            key_released(RIGHT);
            break;
        case SDLK_a:
        case SDLK_LEFT:
            key_released(LEFT);
            break;
        case SDLK_w:
        case SDLK_UP:
            key_released(UP);
            break;
        case SDLK_s:
        case SDLK_DOWN:
            key_released(DOWN);
            break;
        case SDLK_z:
            key_released(A);
            break;
        case SDLK_x:
            key_released(B);
            break;
        case SDLK_BACKSPACE:
            key_released(SELECT);
            break;
        case SDLK_ESCAPE:
        case SDLK_RETURN:
            key_released(START);
            break;
        case SDLK_SPACE:
            SDL_GL_SetSwapInterval(1);
            break;
        default:
            break;
    }
}

void sync_frame(void)
{
    SDL_Event event;

    // Swap buffers
    SDL_GL_SwapWindow(window);

    // Check for key events
    if( SDL_PollEvent(&event) ) {
        switch (event.type) {
            case SDL_KEYDOWN:
                do_key_down(event.key);
                break;
            case SDL_KEYUP:
                do_key_up(event.key);
                break;
            case SDL_QUIT:
                GB_stop();
                break;
            default:
                break;
        }
    }
}

void set_title(const char *title)
{
    char buffer[32] = {0};
    sprintf(buffer, "%s: %s", PROGRAM_NAME, title);
    SDL_SetWindowTitle(window, buffer);
}

static void init_window(void)
{
    // Initialise SDL
    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
        sdl_die( "Failed to initialize SDL" );

    /* Request opengl 3.2 context.
     * SDL doesn't have the ability to choose which profile at this time of writing,
     * but it should default to the core profile */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    /* Turn on double buffering with a 24bit Z buffer.
     * You may need to change this to 16 or 32 for your system */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Open a window and create its OpenGL context
    int width = 800;
    int height = 720;
    window = SDL_CreateWindow(PROGRAM_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if( window == NULL )
        sdl_die("Unable to create window");

    // Clear SDL Error
    sdl_check_error(__LINE__);

    // Create OpenGL Context
    gl_context = SDL_GL_CreateContext(window);
    sdl_check_error(__LINE__);

    // Enable/Disable Vsync
    SDL_GL_SetSwapInterval(V_SYNC);
}

static void destroy_window(void)
{
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for(int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    if(argc < 2) {
        printf("Please specify the BIOS file as first argument.\n");
        return EXIT_FAILURE;
    }

    printf("\nStarting NEC-GB Emulator.\n\n");

    init_window();

    GB_load_bios(argv[1]);
    if(argc == 2) {
        GB_load_cartridge(NULL, NULL);
    } else if(argc == 3) {
        GB_load_cartridge(argv[2], NULL);
    } else if(argc == 4) {
        GB_load_cartridge(argv[2], argv[3]);
    }

    GB_start();

    destroy_window();

    return GB_exit_code();
}

void log_error(char *format, ...)
{
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}

void log_warning(char *format, ...)
{
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}