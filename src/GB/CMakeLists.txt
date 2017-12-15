cmake_minimum_required(VERSION 3.2)
project(GB VERSION 0.1.0.0 LANGUAGES C)

add_library(GB GB.c LR35902.c MMU.c PPU.c timer.c sound.c joypad.c serial.c cartridge.c display.c audio.c)
target_link_libraries(GB ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${SDL2_LIBRARIES})

option(NEC_GB_TESTING "" ${NEC_TESTING})

if(NEC_TESTING AND NEC_GB_TESTING)
    add_subdirectory(test)
endif(NEC_TESTING AND NEC_GB_TESTING)