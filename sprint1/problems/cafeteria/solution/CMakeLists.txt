cmake_minimum_required(VERSION 3.20)

project(cafeteria CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
find_package(Boost REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
find_package(Threads REQUIRED)

add_executable(cafeteria
	src/main.cpp
	src/cafeteria.h
	src/result.h
	src/hotdog.h
	src/gascooker.h
	src/ingredients.h
	src/clock.h
)
target_link_libraries(cafeteria PRIVATE
    Boost::boost
    Threads::Threads
)

target_compile_definitions(cafeteria PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)
