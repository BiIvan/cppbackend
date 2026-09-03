cmake_minimum_required(VERSION 3.20)

project(restaurant CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
