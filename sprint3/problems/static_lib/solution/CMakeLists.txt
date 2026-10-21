cmake_minimum_required(VERSION 3.20)

project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Файлы CMakeDeps, созданные Conan.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

find_package(Boost CONFIG REQUIRED)
find_package(Threads REQUIRED)

# ------------------------------------------------------------
# Игровая модель.
# Не зависит от JSON, HTTP и сетевого транспорта.
# ------------------------------------------------------------

add_library(game_model STATIC
    src/model.h
    src/model.cpp
    src/player.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
    src/literals.h
)

target_include_directories(game_model PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ------------------------------------------------------------
# JSON-инфраструктура и загрузка конфигурации.
# Проверяется json_loader_tests.cpp.
# ------------------------------------------------------------

add_library(game_support STATIC
    src/json_loader.h
    src/json_loader.cpp
    src/extra_data.h
    src/boost_json.cpp
)

target_link_libraries(game_support PUBLIC
    game_model
    Boost::headers
)

# ------------------------------------------------------------
# Приложение и обработчик запросов.
# Проверяется api_tests.cpp.
# Сетевой listener и HttpSession сюда не входят.
# ------------------------------------------------------------

add_library(game_api STATIC
    src/application.h
    src/request_handler.h
    src/request_handler.cpp
)

target_link_libraries(game_api PUBLIC
    game_support
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(game_api PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )

    target_link_libraries(game_api PUBLIC
        ws2_32
        mswsock
    )
endif()

# ------------------------------------------------------------
# Сервер: запуск, соединения, логирование, автоматические тики.
# ------------------------------------------------------------

add_executable(game_server
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/logger.h
    src/ticker.h
    src/sdk.h
)

target_link_libraries(game_server PRIVATE
    game_api
    Boost::log_setup
    Boost::log
    Boost::program_options
)

# ------------------------------------------------------------
# Catch2-тесты.
# Исходники библиотек здесь повторно не перечисляются.
# ------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)

    add_executable(game_server_tests
        tests/loot_generator_tests.cpp
        tests/model-tests.cpp
        tests/json_loader_tests.cpp
        tests/api_tests.cpp
    )

    target_link_libraries(game_server_tests PRIVATE
        game_api
        Catch2::Catch2WithMain
    )

    add_test(
        NAME game_server_tests
        COMMAND game_server_tests
    )
endif()
