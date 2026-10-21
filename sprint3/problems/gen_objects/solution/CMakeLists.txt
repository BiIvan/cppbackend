cmake_minimum_required(VERSION 3.20)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Файлы CMakeDeps от Conan 1 находятся в каталоге build.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

find_package(Boost CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/loot_generator.cpp
    src/loot_generator.h
    src/extra_data.h
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
)

target_link_libraries(game_server PRIVATE
    Boost::log_setup
    Boost::log
    Boost::program_options
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(game_server PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(game_server_tests
        src/model.cpp
        src/loot_generator.cpp
        src/json_loader.cpp
        src/boost_json.cpp
        src/request_handler.cpp
        tests/loot_generator_tests.cpp
        tests/model-tests.cpp
        tests/json_loader_tests.cpp
        tests/api_tests.cpp
    )
    target_include_directories(game_server_tests PRIVATE src)
    target_link_libraries(game_server_tests PRIVATE
        Catch2::Catch2WithMain
        Boost::headers
        Threads::Threads
    )
    if(WIN32)
        target_compile_definitions(game_server_tests PRIVATE
            WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    endif()
    add_test(NAME game_server_tests COMMAND game_server_tests)
endif()

if(WIN32)
    target_link_libraries(game_server PRIVATE
        ws2_32
        mswsock
    )

    if(TARGET game_server_tests)
        target_link_libraries(game_server_tests PRIVATE
            ws2_32
            mswsock
        )
    endif()
endif()
