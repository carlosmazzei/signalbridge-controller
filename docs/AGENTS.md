# AGENTS.md — Signalbridge Controller Firmware

## 1. Project Overview
Signalbridge Controller Firmware is the embedded runtime for a Raspberry Pi Pico (RP2040) that provides low-latency I/O (keypads, encoders, ADCs, LEDs, and multi-digit displays) over a USB CDC command interface, orchestrated by FreeRTOS SMP. It focuses on deterministic task scheduling, robust queues, and hardware-specific drivers for input/output peripherals.

## 2. Agent Persona
Senior Embedded Systems & Real-Time Firmware Architect (RP2040, FreeRTOS SMP, C/C++).

## 3. Tech Stack
- Raspberry Pi Pico SDK **2.0.0** (submodule branch)
- FreeRTOS Kernel **11.2.0** (submodule branch)
- C **11** and C++ **17** (project standards)
- CMake **presets** with Unix Makefiles generator

## 4. Essential Commands
### Environment setup / install
```sh
git submodule update --init --recursive
```

### Dev build loop (HMR not available for firmware; use incremental rebuilds)
```sh
cmake --preset pico-debug
cmake --build --preset pico-debug -- -j4
```

### Targeted testing
```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests -R <test_name_regex>
```

### Linting / static analysis
```sh
./scripts/cppcheck_simple.sh
# or full MISRA-aware analysis
./scripts/run_cppcheck.sh
```

## 5. Coding Standards & Examples
**Prefer explicit widths, U-suffix literals, and defensive casts.**
```c
static inline uint8_t calculate_checksum(const uint8_t *data, uint8_t length)
{
	uint8_t checksum = 0U;
	for (uint8_t i = 0U; i < length; i++)
	{
		checksum ^= data[i];
	}
	return checksum;
}
```

**Prefer small, single-purpose helpers with clear data staging.**
```c
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

## 6. Operational Boundaries (Always / Ask / Never)
**ALWAYS**
- Run tests and lint/static analysis before finishing.
- Update documentation when behavior/logic changes.
- Use TDD for new behavior or bug fixes.

**ASK FIRST**
- Installing new dependencies.
- Modifying DB schemas (if introduced later) or any persistent storage layout.
- Deleting files.

**NEVER**
- Commit secrets or credentials.
- Create throwaway test scripts (e.g., `test_temp.py`).
- Refactor outside the scoped change request.

## 7. TDD Workflow (Red → Green → Refactor)
1. **Red:** Write a failing, behavior-focused unit test (host tests) that captures the new requirement or bug.
2. **Green:** Implement the minimal firmware logic to make the test pass.
3. **Refactor:** Clean up names, structure, and duplication without changing behavior; keep tests green.
