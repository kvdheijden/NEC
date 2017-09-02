project(libGB VERSION 0.1.0 LANGUAGES C)

include_directories(include)

add_library(GB
        src/GB.c
        src/LR35902.c
        src/GPU.c
        src/MMU.c)

target_link_libraries(GB ${OPENGL_gl_LIBRARY})

add_executable(testGB
        test/main.c)

target_link_libraries(testGB GB)
add_test(testGB testGB)