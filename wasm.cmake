# Tudat WASM build, test, and serve script
# Usage: cmake -P wasm.cmake
#
# This script configures, builds, tests, and serves the WASM version of Tudat.

cmake_minimum_required(VERSION 3.20)

# Get the directory containing this script (the tudat root)
get_filename_component(TUDAT_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

set(WASM_BUILD_DIR "${TUDAT_ROOT}/build-wasm")
set(WASM_TOOLCHAIN "${TUDAT_ROOT}/.emsdk/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

# Check toolchain exists
if(NOT EXISTS "${WASM_TOOLCHAIN}")
    message(FATAL_ERROR "Emscripten toolchain not found at ${WASM_TOOLCHAIN}\nRun a native cmake configure first to download the emsdk.")
endif()

message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  Tudat WASM Build, Test, and Serve")
message(STATUS "============================================================")
message(STATUS "")

# Create build directory
file(MAKE_DIRECTORY "${WASM_BUILD_DIR}")

# Configure (skip if already configured)
if(EXISTS "${WASM_BUILD_DIR}/CMakeCache.txt")
    message(STATUS "Build already configured, skipping configure step...")
    message(STATUS "(Delete build-wasm/CMakeCache.txt to force reconfigure)")
else()
    message(STATUS "Configuring WASM build...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -G Ninja
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_TOOLCHAIN_FILE=${WASM_TOOLCHAIN}
            "${TUDAT_ROOT}"
        WORKING_DIRECTORY "${WASM_BUILD_DIR}"
        RESULT_VARIABLE CONFIG_RESULT
    )
    if(NOT CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "CMake configuration failed")
    endif()
endif()

# Build Node.js test
message(STATUS "")
message(STATUS "Building WASM (Node.js test)...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build . --target tudat_wasm_test
    WORKING_DIRECTORY "${WASM_BUILD_DIR}"
    RESULT_VARIABLE BUILD_RESULT
)
if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Build failed")
endif()

# Build web version
message(STATUS "")
message(STATUS "Building WASM (web version)...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build . --target tudat_wasm_web
    WORKING_DIRECTORY "${WASM_BUILD_DIR}"
    RESULT_VARIABLE BUILD_RESULT
)
if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Build failed")
endif()

# Run tests via Node.js
message(STATUS "")
message(STATUS "Running WASM tests via Node.js...")
find_program(NODE_EXECUTABLE NAMES node nodejs)
if(NODE_EXECUTABLE)
    execute_process(
        COMMAND ${NODE_EXECUTABLE} "${WASM_BUILD_DIR}/tests/wasm/tudat_wasm_test.js"
        WORKING_DIRECTORY "${WASM_BUILD_DIR}/tests/wasm"
        RESULT_VARIABLE TEST_RESULT
    )
    if(NOT TEST_RESULT EQUAL 0)
        message(FATAL_ERROR "Tests failed")
    endif()
    message(STATUS "Tests passed!")
else()
    message(WARNING "Node.js not found - skipping tests")
endif()

# Start web server
message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  Starting web server at http://localhost:8080")
message(STATUS "  Press Ctrl+C to stop")
message(STATUS "============================================================")
message(STATUS "")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${WASM_BUILD_DIR}/tests/wasm/web/start_server.py"
    WORKING_DIRECTORY "${WASM_BUILD_DIR}/tests/wasm/web"
)
