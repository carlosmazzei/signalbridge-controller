# DevContainer Picotool Fix

## Problem
Picotool was built for the host architecture (macOS arm64) but devcontainer runs Linux, causing "Exec format error".

## Solution Options

### Option 1: Clean Rebuild (Recommended)
```bash
# In devcontainer terminal
rm -rf build-release build-debug build-tests
mkdir build-release && cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

### Option 2: Force Picotool Rebuild
```bash
# In devcontainer, remove cached picotool
cd build-release
rm -rf _deps/picotool*
make -j4
```

### Option 3: Add Devcontainer Flag (if issues persist)
Add to CMakeLists.txt after line 1:
```cmake
# Detect devcontainer environment
if(DEFINED ENV{DEVCONTAINER})
    set(PICO_NO_HARDWARE_SCAN 1)
endif()
```

## Explanation
The Pico SDK automatically downloads and builds picotool as an ExternalProject. When the build directory is copied between host and container, the cached picotool executable has wrong architecture. A clean rebuild forces picotool to be compiled for the container's architecture.

## Verification
After rebuild, check that picotool works:
```bash
./_deps/picotool/picotool version
```
Should show version without "Exec format error".