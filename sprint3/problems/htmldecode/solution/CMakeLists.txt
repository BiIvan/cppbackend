cmake_minimum_required(VERSION 3.11)

project(htmldecode CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

add_executable(htmldecode
    src/main.cpp
    src/htmldecode.h
    src/htmldecode.cpp
)

add_executable(tests
    tests/tests.cpp
    src/htmldecode.h
    src/htmldecode.cpp
)
target_link_libraries(tests PRIVATE CONAN_PKG::catch2)

add_executable(catch_test_example
    tests/catch_test_main.cpp
    src/leap_year.h
    src/leap_year.cpp
)
target_link_libraries(catch_test_example PRIVATE CONAN_PKG::catch2)

