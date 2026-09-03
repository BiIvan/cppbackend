cmake_minimum_required(VERSION 3.20)

project(seabattle CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(Boost REQUIRED)
find_package(miniaudio REQUIRED)
find_package(Threads REQUIRED)

add_executable(seabattle
    src/main.cpp
)

target_link_libraries(seabattle PRIVATE
    Boost::boost
    miniaudio::miniaudio
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(seabattle PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()
