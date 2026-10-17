cmake_minimum_required(VERSION 3.20)
project(game_server LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
find_package(Boost CONFIG REQUIRED COMPONENTS log_setup log program_options)
find_package(Threads REQUIRED)
add_library(game_model STATIC
    src/model.h src/model.cpp src/player.h
    src/loot_generator.h src/loot_generator.cpp
    src/tagged.h src/literals.h src/geom.h
    src/collision_detector.h src/collision_detector.cpp)
target_include_directories(game_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(game_support STATIC
    src/json_loader.h src/json_loader.cpp src/extra_data.h src/boost_json.cpp)
target_link_libraries(game_support PUBLIC game_model Boost::headers)
add_library(game_api STATIC src/application.h src/request_handler.h src/request_handler.cpp)
target_link_libraries(game_api PUBLIC game_support Threads::Threads)
if(WIN32)
    target_compile_definitions(game_api PUBLIC
        NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(game_api PUBLIC ws2_32 mswsock)
endif()
add_executable(game_server
    src/main.cpp src/http_server.h src/http_server.cpp src/logger.h src/ticker.h src/sdk.h)
target_link_libraries(game_server PRIVATE game_api Boost::log_setup Boost::log Boost::program_options)
include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(game_server_tests
        tests/loot_generator_tests.cpp tests/model-tests.cpp
        tests/json_loader_tests.cpp tests/api_tests.cpp tests/find_return_tests.cpp tests/score_tests.cpp)
    target_link_libraries(game_server_tests PRIVATE game_api Catch2::Catch2WithMain)
    add_test(NAME game_server_tests COMMAND game_server_tests)
    # This source defines its own main().
    add_executable(collision_detector_tests tests/collision-detector-tests.cpp)
    target_link_libraries(collision_detector_tests PRIVATE game_model Catch2::Catch2)
    add_test(NAME collision_detector_tests COMMAND collision_detector_tests)
endif()
