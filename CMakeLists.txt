# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CALCULATOR_VERSION)
string(STRIP "${CALCULATOR_VERSION}" CALCULATOR_VERSION)
project(Calculator VERSION "${CALCULATOR_VERSION}" LANGUAGES C CXX)

include(CTest)

option(CALCULATOR_BUILD_DESKTOP "Build the GTK desktop application" ON)

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

set(INFILTRATR_COMMON_EXPECTED_VERSION "1.19.3")
set(INFILTRATR_COMMON_EXPECTED_COMMIT "de7251ce12ed176048df1bad05ef7e4d0db7e9ec")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION STREQUAL INFILTRATR_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Calculator requires Common ${INFILTRATR_COMMON_EXPECTED_VERSION}; "
        "found ${INFILTRATR_COMMON_VERSION}")
endif()

# A source archive has no Git metadata, so VERSION is the strongest check
# available there. In a normal repository checkout, also require the immutable
# release commit so a same-version but wrong submodule revision cannot build.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C
                "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common"
                rev-parse HEAD
        RESULT_VARIABLE INFILTRATR_COMMON_GIT_RESULT
        OUTPUT_VARIABLE INFILTRATR_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT INFILTRATR_COMMON_GIT_RESULT EQUAL 0 OR
       NOT INFILTRATR_COMMON_COMMIT STREQUAL INFILTRATR_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Calculator requires Common commit "
            "${INFILTRATR_COMMON_EXPECTED_COMMIT}; found "
            "${INFILTRATR_COMMON_COMMIT}")
    endif()
endif()

# Common is an implementation dependency, not part of Calculator's install
# surface. EXCLUDE_FROM_ALL keeps unreferenced Common targets and install rules
# out of Calculator packages while calculator-core still pulls Portable in.
add_subdirectory(src/infiltratr-common EXCLUDE_FROM_ALL)

add_library(calculator-core STATIC
    src/core/calculator.cpp
    src/core/session.cpp
    src/core/programmer.cpp
)

target_include_directories(calculator-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)

target_link_libraries(calculator-core PUBLIC
    InfiltratrCommon::Portable
)

add_library(calculator-ui STATIC
    src/ui/calculator_ui_controller.cpp
)

target_include_directories(calculator-ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

target_link_libraries(calculator-ui PUBLIC
    calculator-core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(calculator-ui PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(calculator-core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    )
endif()

if(CALCULATOR_BUILD_DESKTOP)
    if(WIN32)
        # Windows has a native shell so the release executable has no GTK/GLib
        # runtime closure and depends only on Windows system components.
        add_executable(infiltrator-calc WIN32
            src/app/windows_main.cpp
        )
        target_link_libraries(infiltrator-calc PRIVATE
            calculator-ui
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
            calculator-ui
            PkgConfig::GTK4
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(infiltrator-calc PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow
            )
        endif()
    endif()

    install(TARGETS infiltrator-calc RUNTIME DESTINATION bin)
    install(FILES LICENSE DESTINATION share/doc/infiltrator-calculator OPTIONAL)
endif()

add_executable(calculator-test
    tests/test_calculator.cpp
)
target_link_libraries(calculator-test PRIVATE calculator-core)
add_test(NAME calculator-core COMMAND calculator-test)

add_executable(calculator-session-test
    tests/test_session.cpp
)
target_link_libraries(calculator-session-test PRIVATE calculator-core)
add_test(NAME calculator-session COMMAND calculator-session-test)

add_executable(calculator-programmer-test
    tests/test_programmer.cpp
)
target_link_libraries(calculator-programmer-test PRIVATE calculator-core)
add_test(NAME calculator-programmer COMMAND calculator-programmer-test)

add_executable(calculator-ui-contract-test
    tests/test_ui_contract.cpp
)
target_link_libraries(calculator-ui-contract-test PRIVATE calculator-ui)
add_test(NAME calculator-ui-contract COMMAND calculator-ui-contract-test)

add_executable(calculator-ui-controller-test
    tests/test_ui_controller.cpp
)
target_link_libraries(calculator-ui-controller-test PRIVATE calculator-ui)
add_test(NAME calculator-ui-controller COMMAND calculator-ui-controller-test)

