
cmake_minimum_required(VERSION 3.15)

project(book_manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

# Локальный обход некорректной импортной библиотеки
# в установленном Windows-пакете libpq.
if(MINGW AND EXISTS "${CMAKE_BINARY_DIR}/local-libpq/libpq.dll.a")
    set(CONAN_LIB_DIRS_LIBPQ
        "${CMAKE_BINARY_DIR}/local-libpq"
    )
endif()

conan_basic_setup(TARGETS NO_OUTPUT_DIRS)

add_executable(book_manager main.cpp)
 
set_target_properties(book_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL      "${CMAKE_BINARY_DIR}"
)

target_link_libraries(book_manager PRIVATE
    CONAN_PKG::libpqxx
    CONAN_PKG::nlohmann_json
)
