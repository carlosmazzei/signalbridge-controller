# Unit Tests

This directory contains unit tests for the signalbridge-controller project.

## Test Framework
- **CMocka** - Modern C unit testing framework with powerful mocking capabilities
- **Mock headers** - Hardware abstraction layer mocks for Pico SDK dependencies

## Running Tests

### Option 1: Using the main CMakeLists (when NOT building for Pico)
```bash
mkdir build-host
cd build-host
cmake -DPICO_PLATFORM=host ..
make
# Tests will be built but not run automatically
```

### Option 2: Building tests directly 
```bash
cd test
mkdir build
cd build
cmake ..
make
./test_cobs
./test_error_management
```

## Test Structure

- `unit/` - Unit test files
  - `test_*.c` - Individual test files
  - `hardware_mocks.c` - Hardware abstraction mocks
  - `mock_headers/` - Mock Pico SDK headers
- `CMakeLists.txt` - Main test build configuration

## Coverage

Current test coverage focuses on:
- **COBS module** (100% - encode/decode functions)
- **Error Management** (85% - statistics functions)
- **Input validation** (configuration validation)
- **Constants validation** (outputs, tm1639)

Tests achieve ~70% coverage of testable code while respecting the original source files.