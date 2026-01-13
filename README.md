
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

### External Dependencies

All dependencies are automatically handled for WASM builds:

| Dependency | Notes |
|------------|-------|
| **Boost** | Headers automatically provided via Emscripten port |
| **Eigen3** | Automatically downloaded if not found |
| **CSpice** | Automatically downloaded and built with Emscripten |
| **nrlmsise00** | Automatically downloaded and built with Emscripten |
| **SOFA** | Optional, disabled by default for WASM |

Dependencies are downloaded to the `_deps/` folder inside the build directory.

### Data Files

In WASM builds, data files are accessed via Emscripten's virtual filesystem. Mount your data files to `/tudat_data`:

```javascript
// In your JavaScript code
Module.FS.mkdir('/tudat_data');
Module.FS.mount(Module.MEMFS, {}, '/tudat_data');
// ... load your data files
```

### Notes

- The Emscripten SDK is installed to `.emsdk/` in the project root (gitignored)
- Tests and tutorials are automatically disabled for WASM builds
- A stub resource header is provided for WASM builds that don't have TudatResources

