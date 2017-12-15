cmake_minimum_required(VERSION 3.2)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
set(SDL2_LIBRARIES mingw32 SDL2main SDL2)

set(GLEW_LIBRARIES glew32)

add_subdirectory(GB)