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

## Repository Structure

- **`src/`** – application source code
- **`include/`** – public headers
- **`lib/`** – external libraries (Pico SDK, FreeRTOS-Kernel)
- **`scripts/`** – helper utilities (`analyze_memory.sh`, `memory_analysis.sh`, `check_placement.py`)
- **`docs/`** – Doxygen configuration and generated documentation
- **`assets/`** – logos and images
- **`.devcontainer/`** – development container configuration

### Helper Scripts

- `scripts/analyze_memory.sh` – inspect placement of a variable within an ELF file
- `scripts/memory_analysis.sh` – generate a detailed memory-usage report after building
- `scripts/check_placement.py` – Python tool to detect problematic variable locations

## Quick Start

### DevContainer (recommended)
Status: fully configured and ready for one-click setup.

**Prerequisites**
- Visual Studio Code
- Dev Containers extension
- Docker Desktop (running)

**One-click flow**
- Clone the repository with submodules.
- Open the folder in VS Code.
- Use the prompt to reopen in the container.

**What the container provides**
- Arm GCC toolchain, CMake, Make, and supporting build tools.
- Debugging utilities (GDB multiarch and OpenOCD).
- Static analysis and formatting tools (cppcheck, uncrustify, Flawfinder, SonarLint).
- Doxygen for API documentation generation.
- Preinstalled VS Code extensions for C/C++, CMake, Python, and documentation workflows.

**Configuration details**
- Debian-based base image with dependencies pinned in dependencies.json.
- Post-creation setup initializes submodules and environment variables.
- Predefined tasks for build, clean, formatting, and documentation generation.

**Environment variables**
- PICO_SDK_PATH and FREERTOS_KERNEL_PATH are set to the lib submodule locations.
- PICO_TOOLCHAIN_PATH points to the container toolchain.
- JAVA_HOME points to the bundled OpenJDK runtime for analysis tooling.

### Manual setup
If containers are not available, install CMake, Doxygen, Python 3, the GNU Arm Embedded Toolchain, git, cppcheck, uncrustify, OpenOCD, and a recent Java runtime. Use Homebrew on macOS or package managers on Linux. On Windows, combine the same toolchain with Make and Git for Windows.

## Building
- Initialize submodules before first build to pull the Pico SDK and FreeRTOS Kernel.
- Create a `build` directory, configure with CMake from that directory, and build with Make. VS Code tasks named **Build Project** and **Clean Build** provide the same workflow.
- The resulting UF2 image appears under `build/pi_controller.uf2`; copy it to the Pico while it is in BOOTSEL mode.

## ✨ Features and Architecture

### System layout
- Raspberry Pi Pico running FreeRTOS SMP at a 1 kHz tick with dual-core scheduling.
- Nine tasks with explicit core affinity: USB CDC maintenance, CDC read/write, UART ingestion, packet decoding, outbound processing, status LED handling, keypad scanning, ADC sampling, and encoder tracking.
- Queues sized per `include/app_config.h`: encoded reception queue (2048 bytes), CDC transmit queue (2048 packets), and data event queue (500 events).

### Input subsystem
- Eight-by-eight keypad matrix with three-bit stability masks for debouncing and configurable settling delays.
- Sixteen ADC channels behind a four-bit multiplexer with moving-average filtering to smooth readings.
- Up to eight rotary encoders controlled through a run-time mask and independent sampling delays.
- GPIO assignments from `include/app_inputs.h`: keypad multiplexers on GPIO 0/1/2/17 and 6/7/3/8, ADC multiplexer selects on GPIO 20/21/22/11, and keypad sampling on GPIO 9.

### Output subsystem
- Driver set defined in `include/app_outputs.h` supports generic LEDs and digits plus TM1639 and TM1637 devices.
- Default mapping uses TM1639 digits on the first and third slots, a TM1637 digit set on the second slot, and a TM1639 LED matrix on the fourth slot; remaining positions are empty and can be reassigned at compile time.
- Shared SPI fabric with multiplexer selects on GPIO 10, 14, and 15 and chip-select enable on GPIO 27. GPIO 28 provides PWM-based brightness control. TM1637 devices reuse the same infrastructure via bit-banged DIO and CLK pins.
- A mutex protects SPI transfers, and drivers stage updates in preparation buffers before committing to hardware to avoid flicker.

### Command interface
`include/commands.h` defines the command set: PWM and LED updates, analog readings, keypad events, rotary encoder data, display control, diagnostic queries (task status, USB status, error counters), configuration updates, and debug channels. Message parsing lives in `src/app_comm.c` and connects host requests to the processing pipeline.
For protocol framing, payload layouts, and pre-COBS example frames, see `docs/COMMANDS.md`.

### Diagnostics and resilience
- Statistics counters in `include/error_management.h` track queue send/receive errors, CDC transmit issues, output and input initialization errors, malformed messages, COBS decode failures, buffer overruns, checksum problems, unknown commands, and bytes sent/received. A resource allocation counter highlights critical setup failures.
- The status LED indicates error categories using blink patterns while the watchdog records persistent fault types. Counters and error types feed the `PC_ERROR_STATUS_CMD` response for host-side inspection.

## Hardware Platform
- Target: Raspberry Pi Pico (RP2040) with Pico SDK 2.1.1 or newer and the current FreeRTOS Kernel.
- Output SPI clock uses the Pico default pins 18 (SCK) and 19 (TX). Input multiplexers and PWM run on the GPIO assignments listed above.
- The watchdog enforces a five-second timeout, and the SPI bus operates at 500 kHz for TM1639 reliability.

## Code Quality and Documentation
- MISRA C:2012 guidance informs implementation choices; suppressions are documented for external dependencies and hardware access.
- Formatting uses Uncrustify with `uncrustify.cfg`. VS Code tasks cover formatting, builds, and documentation generation.
- Static analysis relies on cppcheck with the MISRA addon, SonarLint, and Flawfinder. Helper scripts under `scripts/` wrap common checks.
- Doxygen configuration lives in `docs/Doxyfile`, and generated HTML output is written to `docs/html/`.

## Additional Documentation
- System architecture overview: `docs/ARCHITECTURE.md`.
- Output subsystem details: `docs/OUTPUT.md`.
- Cppcheck and MISRA workflow: `docs/CPPCHECK_SETUP.md`.
- Documentation update guidance: `docs/PROMPT.md`.

## Troubleshooting
- If builds fail, confirm submodules are initialized and the Arm toolchain is available. Recreate the DevContainer if dependencies are missing.
- USB connection issues often trace back to TinyUSB drivers or missing COBS framing markers on the host side.
- For unresponsive peripherals, verify multiplexer wiring matches the GPIO assignments and that queue sizes are sufficient for bursty traffic.

## Support and Resources
- Documentation and SDK references: Pico SDK, FreeRTOS, TinyUSB, and COBS documentation.
- Use GitHub Issues for bug reports and Discussions for questions. Contributing guidelines follow standard fork-and-branch workflows with code formatting and documentation updates expected alongside code changes.

*This README is synchronized with the current codebase and omits code listings in favor of behavior-focused descriptions.*
