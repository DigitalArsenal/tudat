# WASM-specific tests that don't require Boost.Test
# These tests can be run with Node.js after building with Emscripten

add_executable(tudat_wasm_test wasmTest.cpp)

target_include_directories(tudat_wasm_test
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
)

target_include_directories(tudat_wasm_test
    SYSTEM PRIVATE
    ${EIGEN3_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(tudat_wasm_test
    PRIVATE
    tudat_basic_astrodynamics
    tudat_basic_mathematics
    tudat_interpolators
    tudat_numerical_integrators
    tudat_statistics
    tudat_reference_frames
)

set_target_properties(tudat_wasm_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests/wasm"
)

# For Emscripten, we need to configure for Node.js execution
if(EMSCRIPTEN)
    set_target_properties(tudat_wasm_test PROPERTIES
        SUFFIX ".js"
        LINK_FLAGS "-sENVIRONMENT=node -sEXIT_RUNTIME=1 -sNODERAWFS=0 -sMODULARIZE=0 -sINVOKE_RUN=1"
    )

    # Register test with CTest for running via Node.js
    # The test output is tudat_wasm_test.js in the build directory
    find_program(NODE_EXECUTABLE NAMES node nodejs)
    if(NODE_EXECUTABLE)
        add_test(
            NAME tudat_wasm_test
            COMMAND ${NODE_EXECUTABLE} "${PROJECT_BINARY_DIR}/tests/wasm/tudat_wasm_test.js"
            WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/tests/wasm"
        )
        set_tests_properties(tudat_wasm_test PROPERTIES
            LABELS "wasm"
        )
    else()
        message(WARNING "Node.js not found - WASM tests cannot be run via CTest. Install Node.js or run manually.")
    endif()
endif()
