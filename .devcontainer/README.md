# DevContainer Configuration

This devcontainer provides a complete development environment for the Pico SignalBridge Controller project including:

## Development Tools
- **Build System**: CMake, Ninja, Make
- **Compilers**: GCC, Clang, ARM GCC toolchain
- **Debuggers**: GDB, LLDB, OpenOCD
- **Code Quality**: Uncrustify, Cppcheck, Clang-tidy
- **Documentation**: Doxygen with Graphviz

## Testing & Coverage
- **CMocka Test Framework**: Modern C testing with powerful mocking capabilities
- **Coverage Tools**: LCOV, gcovr for HTML reports
- **Test Runners**: CTest integration
- **Mock Framework**: Hardware abstraction layer mocks

## Libraries & Submodules
- **Pico SDK**: Raspberry Pi Pico development SDK
- **FreeRTOS Kernel**: Real-time operating system
- **CMocka**: Unit testing framework for C with mocking support
- **TinyUSB**: USB device/host stack (via Pico SDK)

## VS Code Extensions
- C/C++ IntelliSense and debugging
- CMake Tools integration
- Test Explorer with CMocka support
- Coverage visualization
- ARM Cortex debugging
- Serial monitor support

## Quick Commands
Access via Command Palette (Ctrl+Shift+P) → "Tasks: Run Task":

### Build Tasks
- **Build Project** - Standard embedded build
- **Clean Build** - Full rebuild
- **Generate Documentation** - Doxygen HTML docs

### Test Tasks  
- **Build Tests** - Compile unit tests
- **Run All Tests** - Execute complete test suite
- **Run COBS Tests** - Test COBS encoding/decoding
- **Run Error Management Tests** - Test statistics & error handling
- **Generate Coverage Report** - HTML coverage analysis
- **Clean Tests** - Remove test build artifacts

### Format Tasks
- **uncrustify** - Format current file
- **uncrustify all C/C++ files** - Format entire codebase

## Getting Started
1. Open project in VS Code with DevContainers extension
2. Container builds automatically with all dependencies
3. Run `Tasks: Initialize Submodules` if needed
4. Start coding with full IntelliSense support!

## Rebuild and pinned APT versions

`dependencies.json` pins exact Debian package versions. The Docker build fails
if a pinned version is no longer available in the configured Trixie repositories.
A `Version ... was not found` error means the manifest needs an update; it is
not a VS Code or Docker startup failure. Security updates can replace older
versions in these repositories.

Refresh the package lists in a working Trixie container, then run the existing
updater from the repository root and inspect the manifest diff:

```sh
sudo apt-get update
.devcontainer/update-dependencies.sh --no-backup
```

The updater needs `jq`, Python and APT, already included in the development
image. It uses the container's package candidates, not packages from macOS.
A package pin can be a string shared by all architectures or an object with
`arm64` and `amd64` versions. The updater preserves entries for other architectures;
the image build fails if its architecture has no pinned version.
Validate the resulting image before retrying **Dev Containers: Rebuild Container**:

```sh
docker build --progress=plain -t signalbridge-devcontainer .devcontainer
```

On 2026-10-11, the manifest was refreshed for `bash`, `curl`, `gpg`, `nodejs`
and `jq`, after querying the Debian ARM64 and AMD64 package indexes. The
`uncrustify` pin was split by architecture because its Debian binary rebuild
version differs between ARM64 and AMD64.
Exact version pins against changing repositories do not provide historical
reproducibility; that requires a Debian snapshot repository as well.

Validation: a complete ARM64 image build, installed-version checks for all 25
manifest packages, all 15 CTest entries (including coverage and tooling), the
Pico release build and Cppcheck passed. AMD64 pins were checked against the
Debian package index; an AMD64 image build was not run.
