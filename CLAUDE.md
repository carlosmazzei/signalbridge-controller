# CLAUDE.md — Signalbridge Controller

## Project Overview

Embedded firmware for Raspberry Pi Pico (RP2040) that acts as a USB-connected I/O panel controller for cockpit simulators. Bridges hardware inputs/outputs (keypad matrix, ADCs, rotary encoders, LED displays) to a host PC over USB CDC serial using COBS-framed messages.

- **Target**: RP2040 (dual Cortex-M0+), Raspberry Pi Pico board
- **RTOS**: FreeRTOS SMP (V11.2.0) — 9 tasks across 2 cores, 2kHz tick
- **SDK**: Pico SDK 2.1.1+ (vendored in `lib/pico-sdk`)
- **Language**: C11 (C++17 set for Pico SDK compatibility)
- **Build**: CMake presets, Unix Makefiles generator
- **License**: GPL v3

## Build Commands

```bash
# First-time setup: initialize submodules
git submodule update --init --recursive

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
- **Targeted**: `ctest --preset host-tests -R <test_name_regex>`
- Tests use `--wrap` linker flags to mock FreeRTOS and hardware functions

### TDD Workflow (Red → Green → Refactor)

1. **Red**: Write a failing, behavior-focused unit test that captures the new requirement or bug.
2. **Green**: Implement the minimal firmware logic to make the test pass.
3. **Refactor**: Clean up names, structure, and duplication without changing behavior; keep tests green.

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
- Prefer explicit widths (`uint8_t`, `uint32_t`), U-suffix literals, and defensive casts
- Prefer small, single-purpose helpers with clear data staging

### Code Examples

```c
// Explicit widths, U-suffix, defensive cast
static inline uint8_t calculate_checksum(const uint8_t *data, uint8_t length)
{
    uint8_t checksum = 0U;
    for (uint8_t i = 0U; i < length; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

// Single-purpose helper with clear data staging
static void send_status(uint8_t index)
{
    uint8_t payload[5] = {0U, 0U, 0U, 0U, 0U};

    if (index < (uint8_t)NUM_STATISTICS_COUNTERS)
    {
        payload[0] = index;
        const uint32_t value = statistics_get_counter((statistics_counter_enum_t)index);
        payload[1] = (uint8_t)((value >> 24U) & 0xFFU);
        payload[2] = (uint8_t)((value >> 16U) & 0xFFU);
        payload[3] = (uint8_t)((value >> 8U) & 0xFFU);
        payload[4] = (uint8_t)(value & 0xFFU);
    }

    app_comm_send_packet(BOARD_ID, PC_ERROR_STATUS_CMD, payload, sizeof(payload));
}
```

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
- Run tests and static analysis before committing to verify nothing is broken
- Update documentation when behavior or logic changes
- Use TDD for new behavior or bug fixes

## Operational Boundaries

**ALWAYS**
- Run `ctest --preset host-tests` before committing
- Run lint/static analysis before finishing
- Update documentation when behavior/logic changes

**ASK FIRST**
- Installing new dependencies
- Modifying persistent storage layout
- Deleting files

**NEVER**
- Commit secrets or credentials
- Create throwaway test scripts
- Refactor outside the scoped change request
