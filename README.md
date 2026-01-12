
# tudat

Repository with the core functionality of the TU Delft Astrodynamics Toolbox (Tudat) ecosystem.
* For more details, we refer to the [project website](https://docs.tudat.space/en/latest/) and our [project Github main page](https://github.com/tudat-team)
* For developers, this repository is best built as part of the [tudat-bundle](https://github.com/tudat-team/tudat-bundle)
* Conda package for this repository are available on [anaconda](anaconda.org/tudat-team/tudat/), which is built through the [tudat-feedstock](https://github.com/tudat-team/tudat-feedstock) on [Azure](https://dev.azure.com/tudat-team/feedstock-builds/_build?definitionId=2)

## Building for WebAssembly

Tudat can be compiled to WebAssembly using Emscripten. The Emscripten SDK is automatically downloaded and installed by CMake when using the provided toolchain file.

### Quick Start

```bash
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain-emscripten.cmake
cmake --build build-wasm
```

### Configuration Options

Specify a different Emscripten version:
```bash
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain-emscripten.cmake -DEMSDK_VERSION=3.1.52
```

### Managing Emscripten SDK

Update the Emscripten SDK:
```bash
cmake --build build-wasm --target update-emscripten
```

List available Emscripten versions:
```bash
cmake --build build-wasm --target list-emscripten-versions
```

### Notes

- The Emscripten SDK is installed to `.emsdk/` in the project root (gitignored)
- Tests and tutorials are automatically disabled for WASM builds
- External dependencies (Boost, CSpice, nrlmsise00) must be built with Emscripten and made available to CMake

