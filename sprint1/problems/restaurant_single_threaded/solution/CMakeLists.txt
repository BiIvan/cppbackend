cmake_minimum_required(VERSION 3.20)

project(restaurant CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_executable(restaurant
    src/main.cpp
)

target_link_libraries(restaurant PRIVATE
    Boost::boost
    Threads::Threads
)

target_compile_definitions(restaurant PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)
