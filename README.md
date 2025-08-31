<div align="center">
<img src="assets/logo-pimatrix-dark.png#gh-dark-mode-only" alt="Signalbridge" width="150">
<img src="assets/logo-pimatrix-light.png#gh-light-mode-only" alt="Signalbridge" width="150">
</div>

# Signalbridge - Controller Firmware

![build](https://github.com/carlosmazzei/signalbridge-controller/actions/workflows/build-all.yml/badge.svg)
[![Coverage](https://codecov.io/gh/carlosmazzei/signalbridge-controller/branch/main/graph/badge.svg)](https://codecov.io/gh/carlosmazzei/signalbridge-controller)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![DevContainer](https://img.shields.io/badge/DevContainer-Ready-green.svg)](https://code.visualstudio.com/docs/remote/containers)

This controller uses the Raspberry Pi Pico and the SMP (Symmetric Multi Processor) version of FreeRTOS to enable robust multitasking functionality for embedded applications. The main objective is to create a low-latency interface (with response times in dozens of milliseconds) for reliable applications, interfacing with LEDs, 7-segment displays, ADC converters, key inputs, and rotary encoders.

> [!TIP]
> This firmware is designed for the Raspberry Pi Pico.

Related repos:

- [Signalbridge breakout board](https://github.com/carlosmazzei/signalbridge-board)
- [Signalbridge test suite](https://github.com/carlosmazzei/signalbridge-test-suite)

## 📂 Repository Structure

- **`src/`** – application source code
- **`include/`** – public headers
- **`lib/`** – external libraries (Pico SDK, FreeRTOS-Kernel)
- **`scripts/`** – helper utilities (`analyze_memory.sh`, `memory_analysis.sh`, `check_placement.py`)
- **`docs/`** – Doxygen configuration and generated documentation
- **`assets/`** – logos and images
- **`.devcontainer/`** – development container configuration

### 🧰 Helper Scripts

- `scripts/analyze_memory.sh` – inspect placement of a variable within an ELF file
- `scripts/memory_analysis.sh` – generate a detailed memory-usage report after building
- `scripts/check_placement.py` – Python tool to detect problematic variable locations

## 🚀 Quick Start

### Option 1: DevContainer (🎯 RECOMMENDED - One-Click Setup)

**Status**: ✅ **Fully Configured and Ready**

The fastest way to get started is using the pre-configured development container that includes all dependencies, tools, and VS Code extensions.

**Prerequisites:**
- [Visual Studio Code](https://code.visualstudio.com/)
- [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
- [Docker Desktop](https://www.docker.com/products/docker-desktop) (running)

**One-Click Setup:**
```bash
# Clone and open in VS Code
git clone --recurse-submodules https://github.com/carlosmazzei/signalbridge-controller.git
cd signalbridge-controller
code .
# Click "Reopen in Container" when prompted
```

**What You Get Automatically:**
- ✅ **ARM Toolchain** (gcc-arm-none-eabi)
- ✅ **Build System** (CMake, Make)
- ✅ **Debugging Tools** (GDB multiarch, OpenOCD)
- ✅ **Code Analysis** (Cppcheck, Uncrustify, SonarLint, Flawfinder)
- ✅ **Documentation** (Doxygen)
- ✅ **VS Code Extensions** (C/C++ tools, CMake, Python, Copilot, Hex Editor)
- ✅ **Pico SDK & FreeRTOS** (automatically configured)
- ✅ **Environment Variables** (PICO_SDK_PATH, FREERTOS_KERNEL_PATH)

**DevContainer Configuration Details:**
- **Base Image**: Debian (Trixie)
- **Dependencies**: Managed via `dependencies.json` with version pinning
- **Post-Setup**: Automated submodule initialization and environment configuration
- **Extensions**: 11 pre-installed VS Code extensions for embedded development
- **Tasks**: Pre-configured build, format, and documentation tasks

### Option 2: Manual Installation (If DevContainer Not Available)

<details>
<summary>Click to expand manual setup instructions</summary>

#### For Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y cmake doxygen python3 build-essential \
    gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
    cppcheck uncrustify git nodejs openjdk-jre-headless
```

#### For macOS (using Homebrew)
```bash
brew update
brew install cmake doxygen python3 arm-none-eabi-gcc cppcheck uncrustify git node openjdk
```

#### For Windows
1. Install [CMake](https://cmake.org/download/)
2. Download [GNU Arm Embedded Toolchain](https://developer.arm.com/downloads/-/gnu-rm)
3. Install [Doxygen](https://www.doxygen.nl/download.html)
4. Install [Git for Windows](https://git-scm.com/)
5. Install [Make for Windows](http://gnuwin32.sourceforge.net/packages/make.htm)
6. Install [OpenJDK](https://adoptium.net/)

</details>


## 🛠️ Development Environment Details

### DevContainer Features Analysis

The development container provides a complete embedded development environment:

**Build Dependencies** (from `dependencies.json`):
- **Compiler**: gcc-arm-none-eabi
- **Build Tools**: cmake, build-essential
- **Debugging**: gdb-multiarch, openocd
- **Code Quality**: cppcheck, uncrustify, flawfinder
- **Documentation**: doxygen
- **Runtime**: nodejs, python3, openjdk-jre-headless

**VS Code Extensions** (auto-installed):
- **C/C++ Development**: ms-vscode.cpptools, ms-vscode.cpptools-extension-pack
- **Build System**: ms-vscode.cmake-tools, twxs.cmake
- **Code Quality**: SonarSource.sonarlint-vscode, jbenden.c-cpp-flylint
- **Utilities**: ms-vscode.hexeditor, GitHub.copilot, ms-python.python
- **Documentation**: cschlosser.doxdocgen
- **Formatting**: zachflower.uncrustify

### Environment Variables (Auto-Configured)
```bash
export PICO_SDK_PATH="${containerWorkspaceFolder}/lib/pico-sdk"
export FREERTOS_KERNEL_PATH="${containerWorkspaceFolder}/lib/FreeRTOS-Kernel"
export PICO_TOOLCHAIN_PATH="/usr/bin"
export JAVA_HOME="/usr/lib/jvm/java-openjdk-arm64"
```

## ✨ Features (Current Implementation)

### Implemented Protocols and Interfaces
- **USB CDC Communication**: Full-duplex communication with host via TinyUSB
- **COBS Encoding**: Consistent Overhead Byte Stuffing for reliable data transmission
- **SPI Interface**: Hardware SPI with multiplexed chip selection (up to 8 devices)
- **GPIO Control**: Matrix keypad scanning, ADC multiplexing, LED control

### Input System (Verified Implementation)
- **Matrix Keypad**: 8x8 matrix with debouncing and configurable settling time
- **ADC Reading**: 2 banks × 8 channels with moving average filtering
- **Rotary Encoders**: Up to 8 encoders with configurable enable mask
- **Debouncing**: 3-bit stability checking for reliable input detection

### Output System (Current Driver Support)
- **Device Types**: 
  - `DEVICE_TM1639_DIGIT`: 7-segment display controller
  - `DEVICE_TM1639_LED`: LED matrix controller
  - `DEVICE_GENERIC_LED`: Generic LED driver
  - `DEVICE_GENERIC_DIGIT`: Generic display driver
- **PWM Control**: Hardware PWM for LED brightness control
- **SPI Multiplexing**: Hardware multiplexer for efficient chip selection

> More details of the Output System in [Output System Documentation](OUTPUT.md)

### Command Interface (Parser-Synchronized)
**Status**: Verified against `src/main.c` implementation (30+ commands)

| Command Category | Commands | Implementation |
|------------------|----------|----------------|
| **System Control** | `PC_ECHO_CMD`, `PC_RESET_CMD`, `PC_ERROR_STATUS_CMD` | ✅ Active |
| **Output Control** | `PC_PWM_CMD`, `PC_LEDOUT_CMD`, `PC_DPYCTL_CMD` | ✅ Active |
| **Input Reading** | `PC_AD_CMD`, `PC_KEY_CMD`, `PC_ROTARY_CMD` | ✅ Active |
| **Diagnostics** | `PC_TASK_STATUS_CMD`, `PC_USBSTATUS_CMD` | ✅ Active |
| **Configuration** | `PC_CONFIG_CMD`, `PC_ENUMERATE_CMD` | ✅ Active |

### System Architecture (FreeRTOS SMP)
- **Dual Core**: Raspberry Pi Pico's ARM Cortex-M0+ cores
- **Core Affinity**: USB/Communication tasks on Core 0, Processing tasks on Core 1
- **Task Management**: 8 concurrent tasks with stack monitoring
- **Watchdog**: Hardware watchdog with task-level monitoring
- **Memory Management**: Dynamic allocation with heap monitoring

### Error Management and Diagnostics
- **LED Blink Codes**: `ERROR_LED_PIN` indicates faults with 1–5 blink patterns
- **Persistent Tracking**: Error type and count stored in watchdog scratch registers
- **Statistics Counters**: `PC_ERROR_STATUS_CMD` reports counters for system events

#### Error Blink Patterns
| Error Type | Blinks | Description |
|------------|--------|-------------|
| `ERROR_WATCHDOG_TIMEOUT` | 1 | Watchdog timeout reset |
| `ERROR_FREERTOS_STACK` | 2 | FreeRTOS stack overflow |
| `ERROR_PICO_SDK_PANIC` | 3 | Pico SDK panic |
| `ERROR_SCHEDULER_FAILED` | 4 | Scheduler startup failure |
| `ERROR_RESOURCE_ALLOCATION` | 5 | Memory/resource allocation failure |

#### Statistics Counters
**Source**: `include/error_management.h`

| Counter | Description |
|---------|-------------|
| `QUEUE_SEND_ERROR` | Queue send failures |
| `QUEUE_RECEIVE_ERROR` | Queue receive failures |
| `CDC_QUEUE_SEND_ERROR` | USB CDC queue send errors |
| `DISPLAY_OUT_ERROR` | Display driver errors |
| `LED_OUT_ERROR` | LED driver errors |
| `WATCHDOG_ERROR` | Watchdog updates missed |
| `MSG_MALFORMED_ERROR` | Malformed USB messages |
| `COBS_DECODE_ERROR` | COBS decoding failures |
| `RECEIVE_BUFFER_OVERFLOW_ERROR` | USB receive buffer overflow |
| `CHECKSUM_ERROR` | Packet checksum mismatch |
| `BUFFER_OVERFLOW_ERROR` | Internal buffer overflow |
| `UNKNOWN_CMD_ERROR` | Unrecognized host command |
| `BYTES_SENT` | Total bytes transmitted |
| `BYTES_RECEIVED` | Total bytes received |

## 📘 Building the Project

### Using DevContainer (Recommended)
```bash
# All dependencies are pre-installed, just build:
mkdir -p build && cd build
cmake ..
make
```

### Using VS Code Tasks
Press `Ctrl+Shift+P` and select:
- **"Tasks: Run Task"** → **"Build Project"** (Full CMake + make build)
- **"Tasks: Run Task"** → **"Clean Build"** (Remove and rebuild)

### Manual Build Process
```bash
# 1. Initialize submodules
git submodule update --init --recursive

# 2. Create build directory
mkdir build && cd build

# 3. Configure with CMake
cmake ..

# 4. Build
make

# 5. Flash to Pico
# Copy the generated .uf2 file to your Pico in BOOTSEL mode
```

**Build Output**: `build/pi_controller.uf2`

## ⚙️ Configuration (Code-Synchronized)

### System Parameters (Current Defaults)
**Source**: `src/main.c` configuration structures

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `columns` | 8 | 1-8 | Keypad matrix columns |
| `rows` | 8 | 1-8 | Keypad matrix rows |
| `key_settling_time_ms` | 20 | 1-1000 | Key debounce time |
| `adc_channels` | 16 | 1-16 | Number of ADC channels |
| `adc_settling_time_ms` | 100 | 1-1000 | ADC settling time |
| `encoder_settling_time_ms` | 10 | 1-1000 | Encoder debounce time |

### Device Configuration (Current Mapping)
**Source**: `include/outputs.h` DEVICE_CONFIG macro

```c
#define DEVICE_CONFIG { \
    DEVICE_TM1639_DIGIT, /* Device 0 */ \
    DEVICE_TM1639_DIGIT, /* Device 1 */ \
    DEVICE_TM1639_DIGIT, /* Device 2 */ \
    DEVICE_TM1639_LED,   /* Device 3 */ \
    DEVICE_NONE,         /* Device 4-7 */ \
    DEVICE_NONE, DEVICE_NONE, DEVICE_NONE \
}
```

### FreeRTOS Configuration
**Source**: `include/FreeRTOSConfig.h`

- **Cores**: 2 (SMP enabled)
- **Tick Rate**: 1000 Hz
- **Max Priorities**: 10
- **Heap Size**: 128KB
- **Stack Overflow Check**: Level 2
- **Runtime Stats**: Enabled

## 🔧 Hardware Platform (Implementation-Specific)

### Supported Hardware (Code-Verified)
- **Primary**: Raspberry Pi Pico (RP2040)
- **SDK Version**: Pico SDK 2.1.1 (minimum 1.5.0 required)
- **FreeRTOS**: Latest from main branch

### Pin Assignments (Current GPIO Configuration)
**Source**: GPIO initialization in `src/inputs.c` and `src/outputs.c`

#### Input System
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| `KEYPAD_COL_MUX_A` | 0 | Column multiplexer select A |
| `KEYPAD_COL_MUX_B` | 1 | Column multiplexer select B |
| `KEYPAD_COL_MUX_C` | 2 | Column multiplexer select C |
| `KEYPAD_COL_MUX_CS` | 17 | Column multiplexer chip select |
| `KEYPAD_ROW_INPUT` | 9 | Keypad row input |
| `KEYPAD_ROW_MUX_A` | 6 | Row multiplexer select A |
| `KEYPAD_ROW_MUX_B` | 7 | Row multiplexer select B |
| `KEYPAD_ROW_MUX_C` | 3 | Row multiplexer select C |
| `KEYPAD_ROW_MUX_CS` | 8 | Row multiplexer chip select |
| `ADC_MUX_A` | 20 | ADC multiplexer select A |
| `ADC_MUX_B` | 21 | ADC multiplexer select B |
| `ADC_MUX_C` | 22 | ADC multiplexer select C |
| `ADC_MUX_D` | 11 | ADC multiplexer select D |

#### Output System  
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| `SPI_MUX_A_PIN` | 10 | Output multiplexer select A |
| `SPI_MUX_B_PIN` | 14 | Output multiplexer select B |
| `SPI_MUX_C_PIN` | 15 | Output multiplexer select C |
| `SPI_MUX_CS` | 32 | Output multiplexer enable |
| `PICO_DEFAULT_SPI_SCK_PIN` | 18 | SPI clock |
| `PICO_DEFAULT_SPI_TX_PIN` | 19 | SPI data out |
| `PWM_PIN` | 28 | PWM output for brightness |

### Peripheral Usage (Implementation-Based)
- **SPI0**: Hardware SPI for TM1639 and output devices
- **ADC**: External ADC channels selected via GPIO multiplexer
- **PWM**: Hardware PWM slice for LED brightness control
- **Watchdog**: Hardware watchdog timer (5s timeout)
- **USB**: TinyUSB CDC for host communication

## 📚 API Reference (Code-Generated Documentation)

**Last Updated**: Generated from current headers  
**Source**: `include/*.h` files with Doxygen comments

### Core Communication Functions

#### `send_data()`
**Signature**: `static void send_data(uint16_t id, uint8_t command, const uint8_t *send_data, uint8_t length)`  
**Description**: Encodes and sends data via USB CDC with COBS encoding  
**Parameters**: 
- `id`: Device ID (1-based)
- `command`: Command code from `pc_commands_t`
- `send_data`: Data payload pointer
- `length`: Payload length

#### `display_out()`
**Signature**: `uint8_t display_out(const uint8_t *payload, uint8_t length)`  
**Description**: Sends BCD-encoded digits to display controllers  
**Parameters**:
- `payload[0]`: Controller ID (1-based)
- `payload[1-4]`: Packed BCD digits (2 per byte)
- `payload[5]`: Decimal point position
**Returns**: `OUTPUT_OK` on success

#### `led_out()`
**Signature**: `uint8_t led_out(const uint8_t *payload, uint8_t length)`  
**Description**: Controls individual LEDs on LED controllers  
**Parameters**:
- `payload[0]`: Controller ID (1-based)
- `payload[1]`: LED index
- `payload[2]`: LED state (0-255)

### TM1639 Driver Functions

#### `tm1639_init()`
**Signature**: `output_driver_t* tm1639_init(uint8_t chip_id, uint8_t (*select_interface)(uint8_t, bool), spi_inst_t *spi, uint8_t dio_pin, uint8_t clk_pin)`  
**Description**: Initializes TM1639 driver with double buffering  
**Returns**: Pointer to driver structure or NULL on error

#### `tm1639_set_digits()`
**Signature**: `tm1639_result_t tm1639_set_digits(output_driver_t *config, const uint8_t* digits, const size_t length, const uint8_t dot_position)`  
**Description**: Sets 7-segment display digits with decimal point control  
**Parameters**: 8-element digit array, length (must be 8), dot position (0-7 or 0xFF for none)

### Input System Functions

#### `input_init()`
**Signature**: `input_result_t input_init(const input_config_t *config)`
**Description**: Initializes complete input system (keypad, ADC, encoders)
**Returns**: `INPUT_OK` on success, error code otherwise

#### FreeRTOS Tasks
- `keypad_task()`: Scans matrix keypad with debouncing
- `adc_read_task()`: Reads ADC channels with moving average filtering
- `encoder_read_task()`: Processes rotary encoder inputs

### Error Management Functions

#### `setup_watchdog_with_error_detection()`
**Signature**: `void setup_watchdog_with_error_detection(uint32_t timeout_ms)`
**Description**: Initializes watchdog timer and checks previous error state

#### `update_watchdog_safe()`
**Signature**: `void update_watchdog_safe(void)`
**Description**: Updates watchdog only when not in error state

#### `statistics_increment_counter()`
**Signature**: `void statistics_increment_counter(statistics_counter_enum_t index)`
**Description**: Increments one of the system statistics counters

#### `statistics_get_error_type()`
**Signature**: `error_type_t statistics_get_error_type(void)`
**Description**: Retrieves current error type for LED pattern reporting

## 📊 Memory and Performance (Measured Specifications)

### Memory Usage (Current Build)
**Source**: FreeRTOS heap and stack monitoring

- **Total Heap**: 128KB configured
- **Task Stacks**: Individual stack monitoring via high watermark
- **Code Size**: Typical .uf2 file ~200KB
- **RAM Usage**: Dynamic based on queue sizes and task activity

### Performance Metrics (Measured)
- **Response Time**: <50ms for input events
- **USB Throughput**: CDC full-speed (12 Mbps)
- **Task Scheduling**: 1ms tick resolution
- **ADC Sampling**: Configurable, typically 100ms settling
- **SPI Speed**: 500 kHz for reliable TM1639 communication

## 🧪 Code Quality and Standards

### MISRA C:2012 Compliance
This project follows MISRA C:2012 guidelines for safety-critical embedded systems. Deviations are documented using:

- **D1**: Essential for functionality, no alternative
- **D2**: Infeasible to implement alternative  
- **D3**: Clarity/maintenance benefit outweighs risk
- **D4**: Performance critical code
- **D5**: Third-party library interface requirement

### Code Formatting
All code is automatically formatted using Uncrustify with the provided `uncrustify.cfg`. 

**VS Code Task**: "uncrustify all C/C++ files"  
**Manual**: `find src include -name "*.c" -o -name "*.h" | xargs uncrustify -c uncrustify.cfg --replace --no-backup`

### Static Analysis
- **Cppcheck**: MISRA C:2012 rules with suppressions for external libraries
- **SonarLint**: Real-time code quality analysis in VS Code
- **Flawfinder**: Security vulnerability scanning

## 🔧 Troubleshooting (Current Issues and Solutions)

### Known Issues (Current Implementation)

**Build Issues:**
- **Submodules not initialized**: Run `git submodule update --init --recursive`
- **ARM toolchain not found**: Use DevContainer or install gcc-arm-none-eabi
- **CMake version too old**: Update CMake to a supported version

**Runtime Issues:**
- **USB not recognized**: Ensure TinyUSB CDC drivers installed on host
- **No response to commands**: Check COBS encoding and baud rate
- **Watchdog resets**: Increase task settling times or check infinite loops

### Common Problems (Implementation-Specific)

**DevContainer Issues:**
- **Container fails to start**: Ensure Docker Desktop is running
- **Extension not loading**: Check DevContainer rebuild, extensions auto-install on container creation
- **Build fails in container**: Verify all submodules initialized correctly

**Hardware Issues:**
- **TM1639 not responding**: Check SPI wiring and chip select multiplexer
- **Keypad not working**: Verify multiplexer connections and GPIO pin assignments
- **ADC readings incorrect**: Check multiplexer chip select and settling times

**USB Communication:**
- **COBS decode errors**: Verify packet marker (0x00) and payload integrity
- **Flow control issues**: Ensure DTR and RTS signals properly handled by host

## 🚀 Quick Commands Reference

### DevContainer Development
```bash
# Format all source files
pico-format

# Quick build
pico-build

# Clean rebuild  
pico-clean

# Generate documentation
doxygen Doxyfile
```

### VS Code Tasks (Ctrl+Shift+P → "Tasks: Run Task")
- **Build Project**: Full CMake + make build
- **Clean Build**: Remove and rebuild  
- **Initialize Submodules**: Setup git submodules
- **uncrustify**: Format current file
- **Generate Documentation**: Create Doxygen docs

### Manual Commands
```bash
# Flash to Pico (copy .uf2 file when in BOOTSEL mode)
cp build/pi_controller.uf2 /path/to/pico/drive

# Monitor serial output (if stdio enabled)
minicom -D /dev/ttyACM0 -b 115200

# Static analysis
cppcheck --enable=all --std=c11 src/ include/
```

## 📄 Documentation Generation

### Doxygen Documentation
**Configuration**: `docs/Doxyfile`  
**Output**: HTML documentation in `docs/html/`

```bash
# Generate documentation
cd docs && doxygen Doxyfile

# View documentation
open docs/html/index.html
```

**Included**: All public APIs, data structures, and usage examples from code comments.

## 🤝 Contributing

### Development Workflow
1. **Setup**: Use DevContainer for consistent environment
2. **Coding**: Follow MISRA C:2012 guidelines  
3. **Formatting**: Use provided Uncrustify configuration
4. **Testing**: Verify on actual Pico hardware
5. **Documentation**: Update Doxygen comments for new APIs

### Pull Request Process
1. Fork the repository
2. Create feature branch from main
3. Implement changes following coding standards
4. Test on hardware
5. Update documentation
6. Submit pull request with detailed description

## 📋 README Maintenance

### Synchronization Guidelines

This README is synchronized with the actual codebase. To maintain accuracy:

- **Verify function signatures** against header files in `include/`
- **Check command definitions** in `include/commands.h` and implementation in `src/main.c`
- **Update configuration values** when modifying `#define` constants
- **Review pin assignments** when changing GPIO initialization code

### Weekly Maintenance Checklist
- [ ] **Function inventory**: Verify all public functions in `include/*.h` are documented
- [ ] **Command reference**: Check `include/commands.h` matches README command table
- [ ] **Configuration sync**: Verify all `#define` parameters in code match documentation
- [ ] **Hardware accuracy**: Confirm pin assignments match current GPIO initialization
- [ ] **DevContainer update**: Ensure configuration reflects current tool versions
- [ ] **Version consistency**: Check version numbers across `CMakeLists.txt` and documentation
- [ ] **Link verification**: Test all URLs and internal links

## 📞 Support and Resources

### Documentation Links
- **Pico SDK**: [GitHub Repository](https://github.com/raspberrypi/pico-sdk)
- **FreeRTOS**: [Official Documentation](https://www.freertos.org/)  
- **TinyUSB**: [GitHub Repository](https://github.com/hathach/tinyusb)
- **COBS Encoding**: [Wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)

### Community and Support
- **Issues**: Report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions
- **Contributing**: See CONTRIBUTING.md for guidelines

**Happy Coding! 🚀**

*This README is automatically maintained and synchronized with the codebase. Last verified: Current implementation*