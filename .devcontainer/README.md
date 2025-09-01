# DevContainer Configuration

This devcontainer provides a complete development environment for the Pico SignalBridge Controller project including:

## 🛠️ Development Tools
- **Build System**: CMake, Ninja, Make
- **Compilers**: GCC, Clang, ARM GCC toolchain
- **Debuggers**: GDB, LLDB, OpenOCD
- **Code Quality**: Uncrustify, Cppcheck, Clang-tidy
- **Documentation**: Doxygen with Graphviz

## 🧪 Testing & Coverage
- **CMocka Test Framework**: Modern C testing with powerful mocking capabilities
- **Coverage Tools**: LCOV, gcovr for HTML reports
- **Test Runners**: CTest integration
- **Mock Framework**: Hardware abstraction layer mocks

## 📚 Libraries & Submodules
- **Pico SDK**: Raspberry Pi Pico development SDK
- **FreeRTOS Kernel**: Real-time operating system
- **CMocka**: Unit testing framework for C with mocking support
- **TinyUSB**: USB device/host stack (via Pico SDK)

## 🎯 VS Code Extensions
- C/C++ IntelliSense and debugging
- CMake Tools integration
- Test Explorer with CMocka support
- Coverage visualization
- ARM Cortex debugging
- Serial monitor support

## ⚡ Quick Commands
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

## 🚀 Getting Started
1. Open project in VS Code with DevContainers extension
2. Container builds automatically with all dependencies
3. Run `Tasks: Initialize Submodules` if needed
4. Start coding with full IntelliSense support!