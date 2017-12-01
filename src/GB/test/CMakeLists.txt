cmake_minimum_required(VERSION 3.2)
project(testGB VERSION 0.1.0.0 LANGUAGES C)

include_directories(../)
link_directories(${NEC_SOURCE_DIR}/lib)

add_executable(testGB main.c)

if(GTEST_FOUND)
    target_link_libraries(testGB GB glfw ${GTEST_LIBRARIES})
else()
    target_link_libraries(testGB GB glfw3)
endif(GTEST_FOUND)

add_test(TestGB testGB)