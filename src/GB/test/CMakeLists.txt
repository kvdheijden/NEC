cmake_minimum_required(VERSION 3.2)
project(testGB VERSION 0.1.0.0 LANGUAGES C)

add_executable(testGB main.c)
add_custom_command(TARGET testGB POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${NEC_SOURCE_DIR}/bin"
        $<TARGET_FILE_DIR:testGB>)

if(GTEST_FOUND)
    target_link_libraries(testGB GB ${GTEST_LIBRARIES})
else()
    target_link_libraries(testGB GB)
endif(GTEST_FOUND)

add_test(TestGB testGB)