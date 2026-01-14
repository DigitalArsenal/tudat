# Tudat WASM Test Suite

This directory contains the WebAssembly test suite for Tudat, including a browser-based test runner with 3D visualizations.

## Screenshot

The web UI features a modern space-themed design with real-time test results, 3D orbital visualization, and interactive charts:

![Tudat WASM Test Runner](docs/screenshot.png)

## Quick Start

### Build and Run in Browser

```bash
# From the tudat root directory
cd build-wasm

# Configure with Emscripten (if not already done)
emcmake cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the web version and start the server
ninja tudat_wasm_serve
```

This will:

1. Build the WASM test suite for web
2. Start a local HTTP server on port 8080
3. Open your browser to `http://localhost:8080`

### Run Tests in Node.js (CLI)

```bash
cd build-wasm
ninja tudat_wasm_test
node tests/wasm/tudat_wasm_test.js
```

## Build Targets

| Target              | Description                              |
| ------------------- | ---------------------------------------- |
| `tudat_wasm_test`   | Node.js version for CLI testing          |
| `tudat_wasm_web`    | Browser version with web UI              |
| `tudat_wasm_serve`  | Build web version and start HTTP server  |

## Web UI Features

The browser-based test runner includes:

- **Real-time test results** with pass/fail indicators
- **3D orbital visualization** using Three.js
  - TLE orbit visualization (Vanguard 1)
  - LEO test orbit
  - CR3BP trajectory
- **Interactive charts** using Chart.js
  - Test results by category (doughnut chart)
  - Execution time analysis (bar chart)
  - Accuracy/error distribution (scatter/line charts)
  - Position components over time
- **Console output** with syntax highlighting
- **Dark space theme** with animated starfield background

## Directory Structure

```text
tests/wasm/
├── wasmTest.cpp          # Main test implementation
├── CMakeLists.txt        # Build configuration
├── README.md             # This file
├── Agents.md             # Development notes
├── data/                 # Embedded data files (EOP, etc.)
│   └── earth_orientation/
└── web/                  # Browser UI files
    ├── index.html        # Main HTML page
    └── app.js            # JavaScript application
```

## Test Categories

The test suite covers:

1. **Basic Astrodynamics** - Unit conversions, physical constants
2. **Orbital Mechanics** - Keplerian/Cartesian conversions (NASA ODTBX benchmarks)
3. **Anomaly Conversions** - True/eccentric/mean anomaly
4. **Numerical Methods** - RK4 integration, interpolation
5. **Propagation** - CR3BP, two-body, mass propagation
6. **SPICE Interface** - Time conversions, frame rotations
7. **TLE/SGP4** - Two-line element propagation (Vallado benchmark)

## Requirements

- Emscripten SDK (for building)
- Python 3 (for web server)
- Modern web browser with WebAssembly support

## Accuracy Requirements

All tests meet or exceed the original Tudat native test requirements:

| Test              | Requirement    | WASM Result       |
| ----------------- | -------------- | ----------------- |
| TLE/SGP4 Position | < 50 m         | 15.6 m            |
| TLE/SGP4 Velocity | < 0.05 m/s     | 0.021 m/s         |
| Orbital Elements  | 1e-14 relative | Meets requirement |
