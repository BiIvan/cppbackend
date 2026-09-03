cmake_minimum_required(VERSION 3.20)

project(seabattle CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

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

target_compile_definitions(seabattle PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)
