cmake_minimum_required(VERSION 3.2)
project(NEC VERSION 0.0.0.0 LANGUAGES C)

# Ansi c
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

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