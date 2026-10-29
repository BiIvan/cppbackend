cmake_minimum_required(VERSION 3.15)
project(bookypedia LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
elseif(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
endif()
find_package(Threads REQUIRED)
if(TARGET CONAN_PKG::libpqxx)
    set(BOOKYPEDIA_DEPS CONAN_PKG::boost CONAN_PKG::libpqxx CONAN_PKG::libpq)
else()
    find_package(Boost REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
    set(BOOKYPEDIA_DEPS Boost::headers PkgConfig::PQXX)
endif()

add_library(libbookypedia STATIC
    src/menu/menu.cpp
    src/ui/view.cpp
    src/app/use_cases_impl.cpp
    src/app/tags.cpp
    src/domain/author.cpp
    src/util/tagged_uuid.cpp
    src/postgres/postgres.cpp
)
target_link_libraries(libbookypedia PUBLIC ${BOOKYPEDIA_DEPS} Threads::Threads)
if(WIN32)
    target_link_libraries(libbookypedia PUBLIC bcrypt)
endif()
add_executable(bookypedia src/bookypedia.cpp src/main.cpp)
target_link_libraries(bookypedia PRIVATE libbookypedia)
set_target_properties(bookypedia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_executable(ui_contract_tests tests/ui_contract_tests.cpp
        src/ui/view.cpp src/menu/menu.cpp src/app/tags.cpp)
    add_test(NAME ui_contract COMMAND ui_contract_tests)
    if(TARGET CONAN_PKG::catch2)
        add_executable(tests tests/use_case_tests.cpp tests/tagged_uuid_tests.cpp)
        target_link_libraries(tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::gtest libbookypedia)
        add_test(NAME bookypedia_tests COMMAND tests)
    else()
        find_package(Catch2 3 QUIET CONFIG)
        if(Catch2_FOUND)
            add_executable(tests tests/use_case_tests.cpp tests/tagged_uuid_tests.cpp)
            target_link_libraries(tests PRIVATE Catch2::Catch2WithMain libbookypedia)
            add_test(NAME bookypedia_tests COMMAND tests)
        else()
            message(STATUS "Catch2 3 not found: only independent UI contract tests will be built")
        endif()
    endif()
endif()
