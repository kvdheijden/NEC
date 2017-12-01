cmake_minimum_required(VERSION 3.2)

include_directories(${NEC_SOURCE_DIR}/include)

if(ANDROID)
    set(GRAPHICS_LIBRARIES GLESv2)
else()
    find_package(OpenGL REQUIRED)
    if(OPENGL_FOUND)
        include_directories(${OPENGL_INCLUDE_DIR})
    endif(OPENGL_FOUND)

    find_package(GLEW REQUIRED)
    #add_definitions(-DGLEW_STATIC)
    if(GLEW_FOUND)
        include_directories(${GLEW_INCLUDE_DIR})
    endif(GLEW_FOUND)

    set(GRAPHICS_LIBRARIES ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES})
endif()

add_subdirectory(GB)