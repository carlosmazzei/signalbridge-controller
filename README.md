# Raspberry Pi Pico FreeRTOS SMP Controller

![build](https://github.com/carlosmazzei/a320-pico-controller-freertos/actions/workflows/build.yml/badge.svg)

This controller uses the Raspberry Pi Pico and the SMP (Symmetric Multi Processor) version of FreeRTOS to enable robust multitasking functionality for embedded applications.

The main objective of this project is to create a low-latency interface (with response times in dozens of milliseconds) to be used in a home simulator setting. The raspberry pi pico is used with a breakboard to interface with LEDs, 7-segment display controllers, ADC converters, key inputs, and rotary inputs.

## Table of Contents

- [1. Getting Started](#1-getting-started)
  - [Prerequisites](#prerequisites)
    - [For Linux](#for-linux)
    - [For macOS (using Homebrew)](#for-macos-using-homebrew)
    - [For Windows](#for-windows)
- [2. Configuring the Environment](#2-configuring-the-environment)
  - [CMake Configuration](#cmake-configuration)
- [3. Building the Project](#3-building-the-project)
- [4. Pico-SDK](#4-pico-sdk)
  - [Setting Up the Pico-SDK Path](#setting-up-the-pico-sdk-path)
- [5. FreeRTOS SMP Usage](#5-freertos-smp-usage)
- [6. Documentation](#6-documentation)
  - [Generating Documentation](#generating-documentation)
- [7. Features](#7-features)
  - [Idle Hook](#idle-hook)
  - [Commands](#commands)
- [8. Code Overview](#8-code-overview)
  - [Task Architecture](#task-architecture)
  - [Key Files](#key-files)
  - [Error Handling](#error-handling)
- [9. Contribution](#9-contribution)

## 1. Getting Started

### Prerequisites

Ensure you have the following installed:

#### For Linux

```bash
sudo apt update
sudo apt install -y cmake doxygen python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

#### For macOS (using Homebrew)

```bash
brew update
brew install cmake doxygen python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

#### For Windows

1. Install [CMake](https://cmake.org/download/).
2. Download and install [GNU Arm Embedded Toolchain](https://developer.arm.com/downloads/-/gnu-rm).
3. Install [Doxygen](https://www.doxygen.nl/download.html).
4. Install Git for Windows from [git-scm.com](https://git-scm.com/).
5. Install [Make for Windows](http://gnuwin32.sourceforge.net/packages/make.htm) or use it through [MinGW](https://sourceforge.net/projects/mingw/).

## 2. Configuring the Environment

### CMake Configuration

To ensure compatibility, configure the CMake generator to use **Unix Makefiles**:

- **CMake:Generator**: Unix Makefiles

## 3. Building the Project

Follow these steps to build the project:

1. Clone the repository:

   ```bash
   git clone --recurse-submodules https://github.com/your-repo/a320-pico-controller-freertos.git
   cd a320-pico-controller-freertos
   ```

2. Initialize submodules (Pico-SDK and FreeRTOS):

   ```bash
   git submodule update --init --recursive
   ```

3. Create a build directory and run CMake:

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

4. Flash the compiled `.uf2` file to your Raspberry Pi Pico by copying it to the Pico's USB mass storage drive.

## 4. Pico-SDK

The Pico-SDK is included as a submodule under `/lib/pico-sdk`. To initialize it:

```bash
git submodule update --init --recursive
```

### Setting Up the Pico-SDK Path

Ensure that the Pico-SDK path is correctly set in your environment. Add the following to the `CMakeLists.txt` file in the root directory:

```cmake
set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/lib/pico-sdk)
```

Alternatively, set the `PICO_SDK_PATH` environment variable:

```bash
export PICO_SDK_PATH=$(pwd)/lib/pico-sdk
```

## 5. FreeRTOS SMP Usage

To use FreeRTOS SMP:

1. Include the `FreeRTOSConfig.h` file in the `/FreeRTOS` directory:

   ```c
   #include "FreeRTOSConfig.h"
   ```

2. Import the FreeRTOS kernel in the top-level `CMakeLists.txt`:

   ```cmake
   include(FreeRTOS_Kernel_import.cmake)
   set(FREERTOS_KERNEL_PATH ${CMAKE_SOURCE_DIR}/lib/FreeRTOS)
   ```

## 6. Documentation

Doxygen documentation is generated automatically from the source code comments.

### Generating Documentation

To generate documentation:

1. Ensure Doxygen is installed:

   ```bash
   brew install doxygen # macOS
   sudo apt-get install doxygen # Ubuntu/Debian
   ```

2. Run the following command in the `/docs` directory:

   ```bash
   doxygen Doxyfile
   ```

3. The documentation will be generated in the specified output directory in `Doxyfile`.

## 7. Features

### Idle Hook

The idle task is implemented using the `vApplicationIdleHook()` function. This function runs whenever no higher-priority tasks are active, allowing low-power state transitions.

To enable the idle hook, set `configUSE_IDLE_HOOK` to `1` in `FreeRTOSConfig.h`.

Example prototype:

```c
void vApplicationIdleHook(void);
```

### Commands

The interface supports multiple commands to interact with inputs and outputs:

- Control LED states
- Send PWM duty cycles
- Read analog or digital inputs
- Report system status

Refer to the documentation for details about available commands and their usage.

## 8. Code Overview

### Task Architecture

The main application initializes FreeRTOS tasks for:

- **USB CDC Communication**: Handles UART communication over USB.
- **Input Processing**: Reads digital and analog inputs.
- **Output Control**: Sends commands to outputs like LEDs or displays.
- **COBS Decoding**: Decodes received data using Consistent Overhead Byte Stuffing.
- **Error Handling**: Monitors and logs errors in real time.

Each task runs in its own thread and communicates via FreeRTOS queues. For example:

- **Encoded Reception Queue**: Manages incoming COBS-encoded data.
- **Data Event Queue**: Handles processed events for outbound communication.

### Key Files

- **`main.c`**: Entry point for initializing tasks and setting up peripherals.
- **`FreeRTOSConfig.h`**: Configuration file for FreeRTOS kernel settings.
- **`cobs.c`**: Implements COBS encoding/decoding algorithms.
- **`outputs.c`**: Manages output signals like LEDs and PWM.
- **`inputs.c`**: Handles input peripherals like ADCs and keypads.

### Error Handling

Errors are tracked using a centralized error counter:

```c
typedef struct error_counters_t {
    uint16_t counters[NUM_ERROR_COUNTERS];
    bool error_state;
} error_counters_t;
```

Critical errors cause the system to enter a blinking LED error state.

## 9. Contribution

Contributions are welcome! Please follow these steps:

1. Fork the repository.
2. Create a new branch for your feature/bugfix.
3. Submit a pull request with a detailed description.

This **README.md** provides comprehensive instructions for setting up, building, and using the Raspberry Pi Pico FreeRTOS SMP Controller. It also includes detailed information about the tools required and the code architecture.
