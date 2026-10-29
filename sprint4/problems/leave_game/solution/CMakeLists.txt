cmake_minimum_required(VERSION 3.20)

project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Threads REQUIRED)

find_package(Boost CONFIG REQUIRED COMPONENTS
    log
    program_options
)

add_library(game_model STATIC
    src/model.cpp
    src/collision_detector.cpp
    src/loot_generator.cpp
)

target_include_directories(game_model PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(game_model PUBLIC
    Boost::headers
    Threads::Threads
)

# Реализация Boost.JSON уже находится в src/boost_json.cpp.
# Поэтому отдельно Boost::json здесь не подключаем.
add_library(game_json STATIC
    src/boost_json.cpp
)

target_link_libraries(game_json PUBLIC
    Boost::headers
)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/json_loader.cpp
    src/request_handler.cpp
)

target_link_libraries(game_server PRIVATE
    game_model
    game_json
    Boost::log
    Boost::program_options
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(game_server PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_link_libraries(game_server PRIVATE
        ws2_32
        mswsock
    )
endif()

option(BUILD_TESTING "Build unit tests" OFF)

if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 3 CONFIG REQUIRED)

    add_executable(game_server_tests
        tests/state-serialization-tests.cpp
    )

    target_link_libraries(game_server_tests PRIVATE
        game_model
        game_json
        Catch2::Catch2WithMain
    )

    add_test(
        NAME state_serialization
        COMMAND game_server_tests
    )
endif()

