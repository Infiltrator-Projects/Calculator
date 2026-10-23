# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CALCULATOR_VERSION)
string(STRIP "${CALCULATOR_VERSION}" CALCULATOR_VERSION)
project(Calculator VERSION "${CALCULATOR_VERSION}" LANGUAGES C CXX)

include(CTest)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/InfiltratorNativeFilesystem.cmake")

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

set(INFILTRATR_COMMON_EXPECTED_VERSION "1.19.57")
set(INFILTRATR_COMMON_EXPECTED_COMMIT "0cfa4a36cf84034fd77d5c17a59990f068d5c1fb")
set(CALCULATOR_MULTIPRECISION_EXPECTED_COMMIT "1f7acd34fe929573c131e869867dc1d12b8080b6")
set(CALCULATOR_MULTIPRECISION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/boost-multiprecision")

if(NOT EXISTS "${CALCULATOR_MULTIPRECISION_DIR}/include/boost/multiprecision/cpp_complex.hpp")
    message(FATAL_ERROR "Calculator requires the pinned Boost.Multiprecision source dependency")
endif()

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

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CALCULATOR_MULTIPRECISION_DIR}" rev-parse HEAD
        RESULT_VARIABLE CALCULATOR_MULTIPRECISION_GIT_RESULT
        OUTPUT_VARIABLE CALCULATOR_MULTIPRECISION_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT CALCULATOR_MULTIPRECISION_GIT_RESULT EQUAL 0 OR
       NOT CALCULATOR_MULTIPRECISION_COMMIT STREQUAL CALCULATOR_MULTIPRECISION_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Calculator requires Boost.Multiprecision commit "
            "${CALCULATOR_MULTIPRECISION_EXPECTED_COMMIT}; found "
            "${CALCULATOR_MULTIPRECISION_COMMIT}")
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
    src/core/evaluation_budget.cpp
    src/core/scientific.cpp
    src/core/advanced_tools.cpp
    src/core/advanced_tools_conversion.cpp
    src/core/advanced_tools_analysis.cpp
    src/core/advanced_tools_exact.cpp
    src/core/advanced_tools_number.cpp
    src/core/session.cpp
    src/core/programmer.cpp
)

target_include_directories(calculator-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    PRIVATE "${CALCULATOR_MULTIPRECISION_DIR}/include"
)
target_compile_definitions(calculator-core PRIVATE BOOST_MP_STANDALONE)

target_link_libraries(calculator-core PUBLIC
    InfiltratrCommon::Portable
)

add_library(calculator-ui STATIC
    src/ui/calculator_ui_controller.cpp
    src/ui/calculator_ui_state.cpp
    src/ui/calculator_ui_presentation.cpp
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
        -Werror=missing-field-initializers -Werror=unused-parameter
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(calculator-core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Werror=missing-field-initializers -Werror=unused-parameter
    )
endif()

if(CALCULATOR_BUILD_DESKTOP)
    if(WIN32)
        # The Windows executable owns verified private font resources, while
        # Linux uses the persistent /System/Resources/Fonts cache. The source
        # revision is immutable: the manifest and every selected TTF come from
        # the same audited 23-face InfiltratorOS inventory.
        set(CALCULATOR_EXTENDED_FONTS_COMMIT "66c17fe9ea3ea9678b4028ffea1862a37d488f79")
        set(CALCULATOR_FONT_DIR "${CMAKE_CURRENT_BINARY_DIR}/calculator-fonts")
        file(MAKE_DIRECTORY "${CALCULATOR_FONT_DIR}")
        set(CALCULATOR_FONT_BASE
            "https://raw.githubusercontent.com/Infiltrator-Projects/InfiltratorOS/${CALCULATOR_EXTENDED_FONTS_COMMIT}/assets/fonts")
        set(CALCULATOR_FONT_MANIFEST "${CALCULATOR_FONT_DIR}/SHA256SUMS")
        file(DOWNLOAD "${CALCULATOR_FONT_BASE}/SHA256SUMS"
            "${CALCULATOR_FONT_MANIFEST}" TLS_VERIFY ON
            STATUS CALCULATOR_FONT_DOWNLOAD_STATUS)
        list(GET CALCULATOR_FONT_DOWNLOAD_STATUS 0 CALCULATOR_FONT_DOWNLOAD_CODE)
        if(NOT CALCULATOR_FONT_DOWNLOAD_CODE EQUAL 0)
            message(FATAL_ERROR "Unable to retrieve pinned 23-face font inventory")
        endif()
        file(STRINGS "${CALCULATOR_FONT_MANIFEST}" CALCULATOR_FONT_MANIFEST_LINES)
        list(LENGTH CALCULATOR_FONT_MANIFEST_LINES CALCULATOR_FONT_MANIFEST_COUNT)
        if(NOT CALCULATOR_FONT_MANIFEST_COUNT EQUAL 23)
            message(FATAL_ERROR "Incomplete canonical 23-face font inventory")
        endif()

        set(CALCULATOR_WINDOWS_FONT_ROLES
            "1101|mb_corpo_s_text_regular.ttf"
            "1102|mb_corpo_s_text_bold.ttf"
            "1103|mb_corpo_a_title_cond_regular.ttf"
            "1104|mb_corpo_s_text_cond_bold.ttf"
            "1105|mb_corpo_a_screen_2_cond_regular.ttf")
        set(CALCULATOR_WINDOWS_FONT_RC_LINES "")
        foreach(CALCULATOR_FONT_ROLE IN LISTS CALCULATOR_WINDOWS_FONT_ROLES)
            string(REPLACE "|" ";" CALCULATOR_FONT_PARTS "${CALCULATOR_FONT_ROLE}")
            list(GET CALCULATOR_FONT_PARTS 0 CALCULATOR_FONT_ID)
            list(GET CALCULATOR_FONT_PARTS 1 CALCULATOR_FONT_NAME)
            set(CALCULATOR_FONT_EXPECTED_SHA "")
            foreach(CALCULATOR_MANIFEST_LINE IN LISTS CALCULATOR_FONT_MANIFEST_LINES)
                string(REPLACE "  " ";" CALCULATOR_MANIFEST_PARTS "${CALCULATOR_MANIFEST_LINE}")
                list(LENGTH CALCULATOR_MANIFEST_PARTS CALCULATOR_MANIFEST_PART_COUNT)
                if(CALCULATOR_MANIFEST_PART_COUNT EQUAL 2)
                    list(GET CALCULATOR_MANIFEST_PARTS 1 CALCULATOR_MANIFEST_NAME)
                    if(CALCULATOR_MANIFEST_NAME STREQUAL CALCULATOR_FONT_NAME)
                        list(GET CALCULATOR_MANIFEST_PARTS 0 CALCULATOR_FONT_EXPECTED_SHA)
                    endif()
                endif()
            endforeach()
            string(LENGTH "${CALCULATOR_FONT_EXPECTED_SHA}" CALCULATOR_FONT_SHA_LENGTH)
            if(NOT CALCULATOR_FONT_SHA_LENGTH EQUAL 64)
                message(FATAL_ERROR "Missing checksum for ${CALCULATOR_FONT_NAME}")
            endif()
            set(CALCULATOR_FONT_FILE "${CALCULATOR_FONT_DIR}/${CALCULATOR_FONT_NAME}")
            file(DOWNLOAD "${CALCULATOR_FONT_BASE}/${CALCULATOR_FONT_NAME}"
                "${CALCULATOR_FONT_FILE}" TLS_VERIFY ON
                EXPECTED_HASH "SHA256=${CALCULATOR_FONT_EXPECTED_SHA}"
                STATUS CALCULATOR_FONT_DOWNLOAD_STATUS)
            list(GET CALCULATOR_FONT_DOWNLOAD_STATUS 0 CALCULATOR_FONT_DOWNLOAD_CODE)
            if(NOT CALCULATOR_FONT_DOWNLOAD_CODE EQUAL 0)
                message(FATAL_ERROR "Unable to verify font ${CALCULATOR_FONT_NAME}")
            endif()
            file(TO_NATIVE_PATH "${CALCULATOR_FONT_FILE}" CALCULATOR_FONT_RC_PATH)
            string(REPLACE "\\" "\\\\" CALCULATOR_FONT_RC_PATH "${CALCULATOR_FONT_RC_PATH}")
            string(APPEND CALCULATOR_WINDOWS_FONT_RC_LINES
                "${CALCULATOR_FONT_ID} RCDATA \"${CALCULATOR_FONT_RC_PATH}\"\n")
        endforeach()

        # Windows has a native shell so the release executable has no GTK/GLib
        # runtime closure and depends only on Windows system components.
        add_executable(calculator WIN32
            src/app/windows_main.cpp
            src/app/windows_platform.cpp
            src/app/windows_persistence.cpp
        )

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
            "${CALCULATOR_WINDOWS_FONT_RC_LINES}")
        target_sources(calculator PRIVATE "${CALCULATOR_WINDOWS_FONT_RC}")
        target_compile_definitions(calculator PRIVATE
            CALCULATOR_ICON_RESOURCE=1001
            CALCULATOR_FONT_RESOURCE_REGULAR=1101
            CALCULATOR_FONT_RESOURCE_BOLD=1102
            CALCULATOR_FONT_RESOURCE_CONDENSED=1103
            CALCULATOR_FONT_RESOURCE_COMPACT=1104
            CALCULATOR_FONT_RESOURCE_RESULT=1105)

        target_link_libraries(calculator PRIVATE
            calculator-ui
            user32
            gdi32
            comctl32
            advapi32
        )
        if(MSVC)
            target_compile_options(calculator PRIVATE /W4 /utf-8)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(calculator PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow
                -Werror=shadow -Werror=deprecated-declarations
            )
            target_link_options(calculator PRIVATE
                -static-libgcc
                -static-libstdc++
            )
        endif()
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

        # Calculator has one process identity. The small Linux dispatcher owns
        # mode selection; the existing GTK shell remains a cohesive translation
        # unit whose entry symbol is renamed only within that source file.
        add_executable(calculator
            src/app/linux_entry.cpp
            src/app/main.cpp
            src/app/cli_mode.cpp
            src/app/linux_persistence.cpp
        )
        set_source_files_properties(
            src/app/main.cpp
            PROPERTIES COMPILE_DEFINITIONS "main=calculator_gui_main"
        )

        target_link_libraries(calculator PRIVATE
            calculator-ui
            InfiltratrCommon::Common
            PkgConfig::GTK4
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(calculator PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow
            )
        endif()

        if(BUILD_TESTING)
            add_executable(calculator-linux-persistence-test
                tests/test_linux_persistence.cpp
                src/app/linux_persistence.cpp
                src/app/cli_mode.cpp)
            target_link_libraries(calculator-linux-persistence-test PRIVATE
                calculator-ui InfiltratrCommon::Common PkgConfig::GTK4)
            add_test(NAME calculator-linux-persistence
                COMMAND calculator-linux-persistence-test)
            add_test(
                NAME calculator-cli-mode
                COMMAND calculator /cli "2+2"
            )
            set_tests_properties(
                calculator-cli-mode PROPERTIES PASS_REGULAR_EXPRESSION "^4"
            )
        endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        infiltrator_install_native_filesystem_skeleton()
        install(TARGETS calculator RUNTIME DESTINATION "/Programs")
        install(FILES support/resources/infiltrator-native-path.sh
            DESTINATION "/etc/profile.d"
            RENAME "91-infiltrator-calculator-path.sh")
    else()
        install(TARGETS calculator RUNTIME DESTINATION bin)
    endif()
    install(FILES LICENSE DESTINATION share/doc/calculator OPTIONAL)
endif()

if(BUILD_TESTING)
add_executable(calculator-test
    tests/test_calculator.cpp
)
target_link_libraries(calculator-test PRIVATE calculator-core)
add_test(NAME calculator-core COMMAND calculator-test)

add_executable(calculator-standard-math-test
    tests/test_standard_math.cpp
)
target_link_libraries(calculator-standard-math-test PRIVATE calculator-core)
add_test(NAME calculator-standard-math COMMAND calculator-standard-math-test)

add_executable(calculator-scientific-test
    tests/test_scientific.cpp
)
target_link_libraries(calculator-scientific-test PRIVATE calculator-core)
add_test(NAME calculator-scientific COMMAND calculator-scientific-test)

add_executable(calculator-scientific-math-test
    tests/test_scientific_math.cpp
)
target_link_libraries(calculator-scientific-math-test PRIVATE calculator-core)
add_test(NAME calculator-scientific-math COMMAND calculator-scientific-math-test)

add_executable(calculator-advanced-tools-test
    tests/test_advanced_tools.cpp
)
target_link_libraries(calculator-advanced-tools-test PRIVATE calculator-core)
add_test(NAME calculator-advanced-tools COMMAND calculator-advanced-tools-test)

add_executable(calculator-financial-test tests/test_financial.cpp)
target_link_libraries(calculator-financial-test PRIVATE calculator-core)
add_test(NAME calculator-financial COMMAND calculator-financial-test)
set_tests_properties(calculator-financial PROPERTIES TIMEOUT 30)

add_executable(calculator-evaluation-budget-test
    tests/test_evaluation_budget.cpp
)
target_link_libraries(calculator-evaluation-budget-test PRIVATE calculator-core)
add_test(NAME calculator-evaluation-budget COMMAND calculator-evaluation-budget-test)

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

    find_path(CALCULATOR_MPC_INCLUDE_DIR mpc.h REQUIRED)
    find_library(CALCULATOR_MPC_LIBRARY NAMES mpc REQUIRED)
    add_executable(calculator-precision-oracle-test
        tests/test_precision_oracle.cpp
    )
    target_include_directories(
        calculator-precision-oracle-test PRIVATE
        "${CALCULATOR_MPFR_INCLUDE_DIR}"
        "${CALCULATOR_MPC_INCLUDE_DIR}")
    target_link_libraries(
        calculator-precision-oracle-test PRIVATE
        calculator-core
        "${CALCULATOR_MPC_LIBRARY}"
        "${CALCULATOR_MPFR_LIBRARY}"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(
            calculator-precision-oracle-test PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
            -Wconversion -Wsign-conversion)
    endif()
    add_test(
        NAME calculator-precision-oracle
        COMMAND calculator-precision-oracle-test)

    add_executable(calculator-scientific-oracle-test
        tests/test_scientific_oracle.cpp
    )
    target_include_directories(
        calculator-scientific-oracle-test PRIVATE
        "${CALCULATOR_MPFR_INCLUDE_DIR}"
        "${CALCULATOR_MPC_INCLUDE_DIR}")
    target_link_libraries(
        calculator-scientific-oracle-test PRIVATE
        calculator-core
        "${CALCULATOR_MPC_LIBRARY}"
        "${CALCULATOR_MPFR_LIBRARY}"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(
            calculator-scientific-oracle-test PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow
            -Wconversion -Wsign-conversion)
    endif()
    add_test(
        NAME calculator-scientific-oracle
        COMMAND calculator-scientific-oracle-test)
endif()
endif()
