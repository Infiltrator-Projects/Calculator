# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" INFILTRATOR_CALC_VERSION)
string(STRIP "${INFILTRATOR_CALC_VERSION}" INFILTRATOR_CALC_VERSION)
project(InfiltratorCalc VERSION "${INFILTRATOR_CALC_VERSION}" LANGUAGES C CXX)

include(CTest)

option(INFILTRATOR_CALC_BUILD_DESKTOP "Build the GTK desktop application" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/infiltratr-common)

add_library(infiltrator-calc-core STATIC
    src/core/calculator.cpp
    src/core/session.cpp
    src/core/programmer.cpp
)

target_include_directories(infiltrator-calc-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)

target_link_libraries(infiltrator-calc-core PUBLIC
    InfiltratrCommon::Portable
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(infiltrator-calc-core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    )
endif()

if(INFILTRATOR_CALC_BUILD_DESKTOP)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

    add_executable(infiltrator-calc
        src/app/main.cpp
    )

    target_link_libraries(infiltrator-calc PRIVATE
        infiltrator-calc-core
        PkgConfig::GTK4
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(infiltrator-calc PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
        )
    endif()

    install(TARGETS infiltrator-calc RUNTIME DESTINATION bin)
    install(FILES LICENSE DESTINATION share/doc/infiltrator-calc OPTIONAL)
endif()

add_executable(infiltrator-calc-test
    tests/test_calculator.cpp
)
target_link_libraries(infiltrator-calc-test PRIVATE infiltrator-calc-core)
add_test(NAME calculator-core COMMAND infiltrator-calc-test)

add_executable(infiltrator-calc-session-test
    tests/test_session.cpp
)
target_link_libraries(infiltrator-calc-session-test PRIVATE infiltrator-calc-core)
add_test(NAME calculator-session COMMAND infiltrator-calc-session-test)

add_executable(infiltrator-calc-programmer-test
    tests/test_programmer.cpp
)
target_link_libraries(infiltrator-calc-programmer-test PRIVATE infiltrator-calc-core)
add_test(NAME calculator-programmer COMMAND infiltrator-calc-programmer-test)
