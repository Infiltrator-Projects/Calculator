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

# Windows releases use the native Win32 shell and the static MSVC runtime.
# This keeps the published EXE independent of GTK/GLib and VC redistributables.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION STREQUAL "1.19.1")
    message(FATAL_ERROR "Infiltrator Calc requires Infiltratr Common 1.19.1; found ${INFILTRATR_COMMON_VERSION}")
endif()

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

add_library(infiltrator-calc-ui STATIC
    src/ui/calculator_ui_controller.cpp
)

target_include_directories(infiltrator-calc-ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

target_link_libraries(infiltrator-calc-ui PUBLIC
    infiltrator-calc-core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(infiltrator-calc-ui PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(infiltrator-calc-core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    )
endif()

if(INFILTRATOR_CALC_BUILD_DESKTOP)
    if(WIN32)
        # Windows has a native shell so the release executable has no GTK/GLib
        # runtime closure and depends only on Windows system components.
        add_executable(infiltrator-calc WIN32
            src/app/windows_main.cpp
        )
        target_link_libraries(infiltrator-calc PRIVATE
            infiltrator-calc-ui
            user32
            gdi32
            comctl32
            advapi32
        )
        if(MSVC)
            target_compile_options(infiltrator-calc PRIVATE /W4 /utf-8)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(infiltrator-calc PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow
            )
            target_link_options(infiltrator-calc PRIVATE
                -static-libgcc
                -static-libstdc++
            )
        endif()
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

        add_executable(infiltrator-calc
            src/app/main.cpp
        )

        target_link_libraries(infiltrator-calc PRIVATE
            infiltrator-calc-ui
            PkgConfig::GTK4
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(infiltrator-calc PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow
            )
        endif()
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

add_executable(infiltrator-calc-ui-contract-test
    tests/test_ui_contract.cpp
)
target_link_libraries(infiltrator-calc-ui-contract-test PRIVATE infiltrator-calc-ui)
add_test(NAME calculator-ui-contract COMMAND infiltrator-calc-ui-contract-test)

add_executable(infiltrator-calc-ui-controller-test
    tests/test_ui_controller.cpp
)
target_link_libraries(infiltrator-calc-ui-controller-test PRIVATE infiltrator-calc-ui)
add_test(NAME calculator-ui-controller COMMAND infiltrator-calc-ui-controller-test)

