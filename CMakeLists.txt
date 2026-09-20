# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CALCULATOR_VERSION)
string(STRIP "${CALCULATOR_VERSION}" CALCULATOR_VERSION)
project(Calculator VERSION "${CALCULATOR_VERSION}" LANGUAGES C CXX)

include(CTest)

option(CALCULATOR_BUILD_DESKTOP "Build the GTK desktop application" ON)
option(
    CALCULATOR_ENABLE_MPFR_ORACLE_TESTS
    "Compare Calculator transcendental results with a high-precision MPFR oracle"
    OFF)

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

set(INFILTRATR_COMMON_EXPECTED_VERSION "1.19.10")
set(INFILTRATR_COMMON_EXPECTED_COMMIT "33e69c0a462b56d388881d89c4eb49f72fa0b0fe")

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
include(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common/cmake/InfiltratrTypographyAssets.cmake")

add_library(calculator-core STATIC
    src/core/calculator.cpp
    src/core/advanced_tools.cpp
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
    # Calculator deliberately imposes the strict no-fallback option permitted
    # by Common's typography contract. Common owns immutable asset provenance;
    # Calculator only fetches/packages the exact verified resources.
    set(CALCULATOR_FONT_ARCHIVE
        "${CMAKE_CURRENT_BINARY_DIR}/calculator-mb-corpo-fonts.tar.xz")
    file(DOWNLOAD
        "${INFILTRATR_MB_CORPO_ARCHIVE_URL}"
        "${CALCULATOR_FONT_ARCHIVE}"
        EXPECTED_HASH
            "SHA256=${INFILTRATR_MB_CORPO_ARCHIVE_SHA256}"
        TLS_VERIFY ON
        STATUS CALCULATOR_FONT_DOWNLOAD_STATUS)
    list(GET CALCULATOR_FONT_DOWNLOAD_STATUS 0 CALCULATOR_FONT_DOWNLOAD_CODE)
    if(NOT CALCULATOR_FONT_DOWNLOAD_CODE EQUAL 0)
        list(GET CALCULATOR_FONT_DOWNLOAD_STATUS 1 CALCULATOR_FONT_DOWNLOAD_MESSAGE)
        message(FATAL_ERROR
            "Unable to obtain the canonical Calculator font archive: "
            "${CALCULATOR_FONT_DOWNLOAD_MESSAGE}")
    endif()

    set(CALCULATOR_FONT_DIR "${CMAKE_CURRENT_BINARY_DIR}/calculator-fonts")
    file(REMOVE_RECURSE "${CALCULATOR_FONT_DIR}")
    file(MAKE_DIRECTORY "${CALCULATOR_FONT_DIR}")
    file(ARCHIVE_EXTRACT
        INPUT "${CALCULATOR_FONT_ARCHIVE}"
        DESTINATION "${CALCULATOR_FONT_DIR}")

    set(CALCULATOR_FONT_A_COND_REGULAR
        "${CALCULATOR_FONT_DIR}/${INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE}")
    set(CALCULATOR_FONT_S_BOLD
        "${CALCULATOR_FONT_DIR}/${INFILTRATR_MB_CORPO_UI_BOLD_FILE}")
    set(CALCULATOR_FONT_S_REGULAR
        "${CALCULATOR_FONT_DIR}/${INFILTRATR_MB_CORPO_UI_REGULAR_FILE}")
    set(CALCULATOR_FONT_FILES
        "${CALCULATOR_FONT_A_COND_REGULAR}"
        "${CALCULATOR_FONT_S_BOLD}"
        "${CALCULATOR_FONT_S_REGULAR}")
    set(CALCULATOR_FONT_EXPECTED_SHA256
        "${INFILTRATR_MB_CORPO_BRAND_REGULAR_SHA256}"
        "${INFILTRATR_MB_CORPO_UI_BOLD_SHA256}"
        "${INFILTRATR_MB_CORPO_UI_REGULAR_SHA256}")
    foreach(CALCULATOR_FONT_INDEX RANGE 0 2)
        list(GET CALCULATOR_FONT_FILES
             ${CALCULATOR_FONT_INDEX} CALCULATOR_FONT_FILE)
        list(GET CALCULATOR_FONT_EXPECTED_SHA256
             ${CALCULATOR_FONT_INDEX} CALCULATOR_FONT_SHA256_EXPECTED)
        file(SHA256
             "${CALCULATOR_FONT_FILE}" CALCULATOR_FONT_SHA256_ACTUAL)
        if(NOT CALCULATOR_FONT_SHA256_ACTUAL
           STREQUAL CALCULATOR_FONT_SHA256_EXPECTED)
            message(FATAL_ERROR
                "Calculator font hash mismatch: ${CALCULATOR_FONT_FILE}")
        endif()
    endforeach()

    if(WIN32)
        # Windows has a native shell so the release executable has no GTK/GLib
        # runtime closure and depends only on Windows system components.
        add_executable(infiltrator-calc WIN32
            src/app/windows_main.cpp
        )

        file(TO_NATIVE_PATH
             "${CALCULATOR_FONT_S_REGULAR}" CALCULATOR_FONT_S_REGULAR_RC)
        file(TO_NATIVE_PATH
             "${CALCULATOR_FONT_S_BOLD}" CALCULATOR_FONT_S_BOLD_RC)
        file(TO_NATIVE_PATH
             "${CALCULATOR_FONT_A_COND_REGULAR}" CALCULATOR_FONT_A_COND_REGULAR_RC)
        string(REPLACE "\\" "\\\\"
               CALCULATOR_FONT_S_REGULAR_RC "${CALCULATOR_FONT_S_REGULAR_RC}")
        string(REPLACE "\\" "\\\\"
               CALCULATOR_FONT_S_BOLD_RC "${CALCULATOR_FONT_S_BOLD_RC}")
        string(REPLACE "\\" "\\\\"
               CALCULATOR_FONT_A_COND_REGULAR_RC "${CALCULATOR_FONT_A_COND_REGULAR_RC}")
        set(CALCULATOR_WINDOWS_ICON
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows/calculator.ico")
        if(NOT EXISTS "${CALCULATOR_WINDOWS_ICON}")
            message(FATAL_ERROR "Calculator Windows icon is missing")
        endif()
        file(TO_NATIVE_PATH
             "${CALCULATOR_WINDOWS_ICON}" CALCULATOR_WINDOWS_ICON_RC)
        string(REPLACE "\\" "\\\\"
               CALCULATOR_WINDOWS_ICON_RC "${CALCULATOR_WINDOWS_ICON_RC}")

        set(CALCULATOR_WINDOWS_FONT_RC
            "${CMAKE_CURRENT_BINARY_DIR}/calculator-fonts.rc")
        file(WRITE "${CALCULATOR_WINDOWS_FONT_RC}"
            "1001 ICON \"${CALCULATOR_WINDOWS_ICON_RC}\"\n"
            "1101 RCDATA \"${CALCULATOR_FONT_S_REGULAR_RC}\"\n"
            "1102 RCDATA \"${CALCULATOR_FONT_S_BOLD_RC}\"\n"
            "1103 RCDATA \"${CALCULATOR_FONT_A_COND_REGULAR_RC}\"\n")
        target_sources(infiltrator-calc PRIVATE "${CALCULATOR_WINDOWS_FONT_RC}")
        target_compile_definitions(infiltrator-calc PRIVATE
            CALCULATOR_ICON_RESOURCE=1001
            CALCULATOR_FONT_RESOURCE_REGULAR=1101
            CALCULATOR_FONT_RESOURCE_BOLD=1102
            CALCULATOR_FONT_RESOURCE_CONDENSED=1103)

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
        add_executable(infiltrator-calc-cli
            src/app/cli_main.cpp
        )
        target_link_libraries(infiltrator-calc-cli PRIVATE calculator-core)
        install(TARGETS infiltrator-calc-cli RUNTIME DESTINATION bin)

        target_link_libraries(infiltrator-calc PRIVATE
            calculator-ui
            PkgConfig::GTK4
        )

        install(FILES ${CALCULATOR_FONT_FILES}
            DESTINATION share/fonts/truetype/infiltrator-calculator)

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

add_executable(calculator-advanced-tools-test
    tests/test_advanced_tools.cpp
)
target_link_libraries(calculator-advanced-tools-test PRIVATE calculator-core)
add_test(NAME calculator-advanced-tools COMMAND calculator-advanced-tools-test)

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

if(CALCULATOR_ENABLE_MPFR_ORACLE_TESTS)
    find_path(CALCULATOR_MPFR_INCLUDE_DIR mpfr.h REQUIRED)
    find_library(CALCULATOR_MPFR_LIBRARY NAMES mpfr REQUIRED)
    add_executable(calculator-mpfr-oracle-test
        tests/test_mpfr_oracle.cpp
    )
    target_include_directories(
        calculator-mpfr-oracle-test PRIVATE
        "${CALCULATOR_MPFR_INCLUDE_DIR}")
    target_link_libraries(
        calculator-mpfr-oracle-test PRIVATE
        calculator-core
        "${CALCULATOR_MPFR_LIBRARY}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(
            calculator-mpfr-oracle-test PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    endif()
    add_test(
        NAME calculator-mpfr-oracle
        COMMAND calculator-mpfr-oracle-test)
endif()

