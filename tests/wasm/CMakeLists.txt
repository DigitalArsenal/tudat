# WASM-specific tests that don't require Boost.Test
# These tests can be run with Node.js after building with Emscripten
#
# This test suite includes comprehensive propagation tests that work without
# external SPICE kernels by using analytical ephemerides and self-contained
# dynamical systems (CR3BP, two-body, mass propagation, etc.)

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

# Full propagation library dependencies for comprehensive WASM testing
# These libraries enable orbit propagation, environment setup, and dynamics simulation
# without requiring external SPICE kernels (using analytical ephemerides instead)
target_link_libraries(tudat_wasm_test
    PRIVATE
    # Simulation and propagation setup
    tudat_propagation_setup
    tudat_environment_setup
    tudat_estimation_setup

    # Core propagators and dynamics
    tudat_propagators
    tudat_numerical_integrators

    # Astrodynamics and physics
    tudat_basic_astrodynamics
    tudat_gravitation
    tudat_ephemerides
    tudat_aerodynamics
    tudat_propulsion
    tudat_electromagnetism
    tudat_relativity

    # Supporting libraries
    tudat_reference_frames
    tudat_basic_mathematics
    tudat_interpolators
    tudat_root_finders
    tudat_statistics
    tudat_numerical_quadrature
    tudat_filters

    # System and geometry
    tudat_system_models
    tudat_geometric_shapes
    tudat_ground_stations

    # Mission analysis
    tudat_mission_segments
    tudat_shape_based_methods
    tudat_low_thrust_trajectories

    # I/O and utilities
    tudat_input_output
    tudat_basics
    tudat_data

    # External interfaces (SOFA for high-precision calculations, SPICE for ephemeris)
    tudat_sofa_interface
    tudat_spice_interface
    ${Sofa_LIBRARIES}
    ${CSpice_LIBRARIES}
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
