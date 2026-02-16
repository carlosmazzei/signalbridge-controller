# CLAUDE.md — Signalbridge Controller

## Project Overview

Embedded firmware for Raspberry Pi Pico (RP2040) that acts as a USB-connected I/O panel controller for cockpit simulators. Bridges hardware inputs/outputs (keypad matrix, ADCs, rotary encoders, LED displays) to a host PC over USB CDC serial using COBS-framed messages.

- **Target**: RP2040 (dual Cortex-M0+), Raspberry Pi Pico board
- **RTOS**: FreeRTOS SMP (V11.2.0) — 9 tasks across 2 cores, 2kHz tick
- **SDK**: Pico SDK 2.1.1+ (vendored in `lib/pico-sdk`)
- **Language**: C11
- **License**: GPL v3

## Build Commands

```bash
# Embedded release build
cmake --preset pico-release && cmake --build --preset pico-release

# Embedded debug build
cmake --preset pico-debug && cmake --build --preset pico-debug

# Host unit tests (CMocka, runs on x86/x64)
cmake --preset host-tests && cmake --build --preset host-tests && ctest --preset host-tests

# Test coverage (lcov HTML report → build-tests/coverage/)
cmake --build --preset host-tests-coverage
```

Output firmware: `build-release/src/pi_controller.uf2`

## Testing

- **Framework**: CMocka, running on host via FreeRTOS POSIX port
- **Location**: `test/unit/`
- **Hardware mocks**: `test/unit/hardware_mocks.c` and `test/unit/mock_headers/`
- **Run**: `ctest --preset host-tests` (4 parallel jobs, output on failure)
- Tests use `--wrap` linker flags to mock FreeRTOS and hardware functions

## Static Analysis

```bash
# Cppcheck + MISRA C:2012 (requires compile_commands.json from pico-release build)
./scripts/run_cppcheck.sh

# Simplified cppcheck (no MISRA)
./scripts/cppcheck_simple.sh
```

- MISRA config: `misra.json`, rule texts in `misra.txt`
- Suppressions: `.cppcheck_suppressions` (documented per file/line)
- SonarQube: configured in `sonar-project.properties`

## Code Style

- **Formatter**: Uncrustify (`uncrustify.cfg`)
- Format all: `find src include -name "*.c" -o -name "*.h" | xargs uncrustify -c uncrustify.cfg --replace --no-backup`
- Clangd LSP config in `.clangd`

## Project Structure

```
src/           — Application source (main.c, tasks, comm, I/O drivers, COBS, TM163x)
include/       — Public headers (config, commands, data types, FreeRTOS config)
lib/           — Git submodules (pico-sdk, FreeRTOS-Kernel)
test/unit/     — Unit tests + hardware mocks
scripts/       — Analysis and CI helper scripts
docs/          — Architecture, protocol, and setup documentation
assets/        — Logos and diagrams
.github/       — CI workflows, PR template, Dependabot
.devcontainer/ — Dev container (Debian, ARM toolchain, analysis tools)
```

## Architecture Notes

- **Core 0**: USB CDC tasks (read/write/maintenance), status LED
- **Core 1**: Input scanning (keypad, ADC, encoders), output driving (SPI/TM1639/TM1637/PWM), COBS decode
- **Communication**: COBS framing with 0x00 delimiter. 3-byte header + 20-byte payload + checksum
- **SPI bus**: 500 kHz, mutex-protected, with multiplexer routing for multiple display drivers
- **Error handling**: Watchdog (5s timeout), scratch registers persist error info across resets, status LED blink patterns, 22 statistics counters
- **Memory constraints**: 2MB Flash, 264KB RAM, 128KB FreeRTOS heap, stack-usage warnings at 3.5KB

## Key Guidelines

- This is safety-conscious embedded C — respect MISRA C:2012 conventions
- All hardware access goes through Pico SDK APIs (no direct register writes)
- FreeRTOS queues connect tasks; avoid shared mutable state without mutex protection
- Test new logic with CMocka mocks; use `--wrap` for hardware/RTOS function interception
- Keep stack usage low — functions are warned at 3.5KB (`-Wstack-usage=3584`)
- Run `ctest --preset host-tests` before committing to verify nothing is broken
