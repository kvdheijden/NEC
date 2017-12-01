cmake_minimum_required(VERSION 3.2)
project(NEC VERSION 0.0.0.0 LANGUAGES C)

# Ansi c
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Set output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Check if debug
set(IS_DEBUG 1)
if (CMAKE_BUILD_TYPE EQUAL "RELEASE")
    set(IS_DEBUG 0)
endif (CMAKE_BUILD_TYPE EQUAL "RELEASE")

# Endianness check
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
configure_file(
        "${CMAKE_SOURCE_DIR}/config.h.in"
        "${CMAKE_SOURCE_DIR}/include/config.h"
)

# Setup testing framework
option(NEC_TESTING "" ON)
if(NEC_TESTING)
    enable_testing()
    find_package(GTest)
    if(GTEST_FOUND)
        include_directories(${GTEST_INCLUDE_DIRS})
    else(GTEST_FOUND)
        message("Testing without GTEST")
    endif(GTEST_FOUND)
endif(NEC_TESTING)

# Go into source directory
add_subdirectory(src)