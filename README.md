# NEC
Nintendo Emulator Collection

Currently in development:
* GameBoy

## TODO List
* Tidy PPU (Specifically Pixel Pipeline)
  * Contains a lot of magic numbers with should be changed to #defines
* Implement sound using SDL
* Update display to work with more OpenGL versions and OpenGL ES (or maybe using SDL, might be more convenient)
* Some sort of testing/debugging might be nice

## Known bugs
* Cmake: Compiling requires static libraries in ./lib and shared libraries in ./bin
  * Currently used libraries are glew and SDL
  * Should be changed to using find_package
* Sound is just weird noise
* Pokemon Red: After some time of running the emulator it starts hanging -> Memory leak, using 2GB RAM after some time
* Pokemon Red: GB hangs when losing a fight and blacking out
