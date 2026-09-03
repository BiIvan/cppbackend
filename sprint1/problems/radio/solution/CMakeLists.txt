cmake_minimum_required(VERSION 3.20)

project(Radio CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

find_package(Boost REQUIRED)
find_package(miniaudio REQUIRED)
find_package(Threads REQUIRED)

add_executable(radio
    src/main.cpp
    src/miniaudio_impl.cpp
)

target_link_libraries(radio PRIVATE
    Boost::boost
    miniaudio::miniaudio
    Threads::Threads
)

target_compile_definitions(radio PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)
