# Performance Analysis & Improvement Plan

**Scope & method:** static analysis of task timing, queue/heap budgets, and driver
transaction costs across the firmware (July 2026). Figures are computed from the
code and datasheets, not profiled on hardware — see [Follow-up measurement
plan](#follow-up-measurement-plan) before undertaking the larger restructures.

## System budget snapshot

- **CPU:** RP2040, 2× Cortex-M0+ (no HW divide), 200 MHz via
  `PICO_USE_FASTEST_SUPPORTED_CLOCK=1` on SDK ≥ 2.x. All code runs from
  flash (XIP); no RAM-resident functions.
- **RTOS:** FreeRTOS SMP, tick 2000 Hz (0.5 ms), preemption + time slicing,
  no tickless idle. 128 KB heap (heap_4).
- **Task layout:** Core 0 — CDC (prio 3), CDC write (2), UART event (2);
  Core 1 — decode (2), outbound (1), ADC (1), keypad (1), LED status (1).
  Four equal-priority tasks round-robin on Core 1, so any busy-wait there
  starves the others.
- **Comm path:** COBS frames ≤ 26 bytes; TinyUSB CDC with 4 KB RX/TX FIFOs;
  TinyUSB runs on the FreeRTOS OSAL (`TINYUSB_OPT_OS=OPT_OS_FREERTOS`
  injected by the top-level CMakeLists).

## Prioritized opportunities

| # | Item | Area | Impact | Risk | Effort | Status | Where |
|---|------|------|--------|------|--------|--------|-------|
| 1 | ADC settling busy-wait 500 → 100 µs | Inputs / Core 1 | **High** (~8 ms → ~1.6 ms CPU spin per scan) | Low (revert knob) | S | **Implemented** | `include/app_inputs.h` `ADC_DEFAULT_SETTLING_US` |
| 2 | CDC TX queue 2048 → 256 slots | Memory | **High** (frees ~48 KB of 128 KB heap) | Low | S | **Implemented** | `include/app_config.h` `CDC_TRANSMIT_QUEUE_SIZE` |
| 3 | TM1639 flush batched into one 17-byte SPI burst | Displays | Medium (removes 16 per-call overheads per flush) | Low (same byte stream, unit-tested) | S | **Implemented** | `src/tm1639.c` `tm1639_flush` |
| 4 | SPI 500 kHz → 1 MHz | Displays | Medium (halves TM1639 bus time & mutex hold) | Low–Med (HW signal integrity) | S | **Implemented** | `include/app_outputs.h` `SPI_FREQUENCY` |
| 5 | TM1637 bit delay 3 → 2 µs | Displays | Medium (~33% faster bit-banged writes under SPI mutex) | Low–Med (HW) | S | **Implemented** | `src/tm1637.c` `TM1637_DELAY_US` (`#ifndef`-overridable) |
| 6 | CDC write loop: yield only on FIFO-full, coalesce flushes, disconnect bail-out | USB TX | Medium (no busy-yield per chunk; fewer small USB transfers; no watchdog reset on mid-packet disconnect) | Low | M | **Implemented** | `src/app_tasks.c` `cdc_write_task` |
| 7 | ~~Disable run-time stats / trace / stats formatting~~ | Scheduler | — | — | S | **Rejected** — consumed by `PC_TASK_STATUS_CMD` | `include/FreeRTOSConfig.h` |
| 8 | Disable empty tick hook | Scheduler | Low–Med (2000 empty calls/s/core removed) | Low | S | **Implemented** | `include/FreeRTOSConfig.h`, `src/hooks.c` |
| 9 | Rate-limit `uxTaskGetStackHighWaterMark` to every 64th loop pass | Scheduler | Low–Med (multi-MB/s of diagnostic stack scans removed) | Low | S | **Implemented** | `src/app_tasks.c`, `src/app_inputs.c` |
| 10 | Input-event enqueue timeout 1000 → 100 ms | Inputs | Low–Med (bounds worst-case keypad stall 10×) | Low | S | **Implemented** | `include/app_config.h` `INPUT_QUEUE_SEND_TIMEOUT_MS` |
| 11 | Remove inert `PICO_HEAP_SZIE` typo define; dedupe FreeRTOSConfig defines | Build hygiene | None (provable no-ops) | None | S | **Implemented** | `src/CMakeLists.txt`, `include/FreeRTOSConfig.h` |
| 12 | ADC: timer/DMA-driven scan instead of task busy-wait | Inputs | **High** (removes remaining ~1.6 ms/scan spin entirely) | Med–High | L | Proposed | `src/app_inputs.c` |
| 13 | Outbound single-copy path (stream buffer / encode-in-place) | Comm | Medium (packet copied ~3× today: memcpy → COBS → queue copy) | Med | L | Proposed | `src/app_comm.c`, `src/app_tasks.c` |
| 14 | Per-address dirty tracking in TM1639/TM1637 (partial flush) | Displays | Medium (single-digit change rewrites whole buffer today) | Med | M | Proposed | `src/tm1639.c`, `src/tm1637.c` |
| 15 | Interrupt-driven keypad/encoder capture | Inputs | Medium (frees ~500 Hz polling loop) | High (HW rework of mux scan) | L | Proposed | `src/app_inputs.c` |
| 16 | Link-time optimization (LTO) for `pico-release` | Build | Low–Med (cross-module inlining) | Med (pico-sdk `--wrap` aliasing history; must be CI-validated) | S | Proposed | `CMakePresets.json` / `src/CMakeLists.txt` |
| 17 | `configUSE_TIMERS=0` (no app software timers exist) | Memory | Low (frees prio-9 daemon + 8 KB stack) | Low (needs link check) | S | Proposed | `include/FreeRTOSConfig.h` |
| 18 | Tick rate 2000 → 1000 Hz, or tickless idle | Scheduler | Medium (halves tick ISR load) | Med (changes all `pdMS_TO_TICKS` granularity assumptions, 0.5 ms debounce windows) | M | Proposed | `include/FreeRTOSConfig.h` |
| 19 | `__not_in_flash_func` / copy-to-RAM for hot paths (COBS, framer, ISRs) | CPU | Low–Med (removes XIP cache-miss jitter) | Med | M | Proposed | `src/cobs.c`, `src/encoded_framer.c` |
| 20 | Remove `select_interface`'s unconditional `sleep_us(1)` (also on deselect) | Displays | Low | Med (HW timing) | S | Proposed | `src/app_outputs.c` |
| 21 | TM1637: hoist `gpio_set_function`/`gpio_set_dir` out of per-bit pin toggles | Displays | Low–Med | Med (needs HW validation of open-drain emulation) | M | Proposed | `src/tm1637.c` |
| 22 | Statistics counters: per-core or atomic increments | Correctness (SMP data race, not perf) | — | Low | M | Proposed | `src/error_management.c` |
| 23 | `-fstack-protector-strong` trade-off review | CPU | Low (per-function canary cost) | Med (safety feature; keep unless profiling says otherwise) | S | Proposed | `src/CMakeLists.txt` |
| 24 | Batched input events (N events per packet) | Comm | Low–Med (fewer per-event packets under burst) | Med (protocol change) | L | Proposed | protocol + `src/app_comm.c` |

## Implemented items — details & revert guide

Each change keeps a single revert knob and documents it at the definition site.

| Define / site | New value | Old value |
|---|---|---|
| `SPI_FREQUENCY` (`include/app_outputs.h`) | `1000U * 1000U` | `500U * 1000U` |
| `ADC_DEFAULT_SETTLING_US` (`include/app_inputs.h`) | `100U` | `500U` |
| `CDC_TRANSMIT_QUEUE_SIZE` (`include/app_config.h`) | `256U` | `2048U` |
| `INPUT_QUEUE_SEND_TIMEOUT_MS` (`include/app_config.h`) | `100U` | `1000U` |
| `TM1637_DELAY_US` (`src/tm1637.c`) | `2` (build-overridable) | `3` |
| `configUSE_TICK_HOOK` (`include/FreeRTOSConfig.h`) | `0` | `1` |

Notes per item:

1. **ADC settling (item 1).** The wait is a `busy_wait_us_32()` spin on Core 1,
   paid per enabled channel per scan (16 channels by default). The 74HC4067
   switches in < 1 µs and the realistic source RC constant (pot wiper + mux
   R\_on into the S/H and trace capacitance) is well under 10 µs, so 100 µs
   retains > 10× margin. Also runtime-tunable via `input_config.adc_settling_us`.
   Raise it back toward 500 µs if adjacent-channel crosstalk appears with
   high-impedance sources. Unit test pins the ≤ 100 µs contract.
2. **CDC TX queue (item 2).** The queue stores 27-byte packets by value; 2048
   slots consumed ~55 KB (43%) of the FreeRTOS heap while only ever feeding a
   4 KB TinyUSB FIFO. 256 slots (~7 KB) still buffer ~1.7× the FIFO; when the
   host stalls long enough to fill them, `app_comm_send_packet` already
   drops-and-counts (`CDC_QUEUE_SEND_ERROR`). Unit test pins a ≤ 16 KB budget.
3. **TM1639 batch flush (item 3).** The address command and 16 display bytes
   now go out as one `spi_write_blocking` burst inside the same STB window —
   identical byte stream, 17× fewer SPI calls. Unit-tested via a capturing SPI
   mock (`test_flush_batches_address_and_data_in_one_spi_write`).
4. **SPI 1 MHz (item 4).** TM1639 datasheet maximum is 1 MHz (min CLK pulse
   400 ns; RP2040 produces 500 ns half-periods at 1 MHz).
5. **TM1637 delay (item 5).** 2 µs half-period = 250 kHz, a 2× margin under the
   500 kHz datasheet max. Override with `-DTM1637_DELAY_US=3` if long/loaded
   wiring needs the slower clock.
6. **CDC write loop (item 6).** Previously the drain loop executed
   `taskYIELD()` on every iteration (even after successful writes) and flushed
   after every 27-byte packet. Now it yields only when the FIFO is actually
   full, drains every queued packet before one flush (latency unchanged — the
   flush still happens as soon as the queue empties), and abandons the packet
   if the host drops the link mid-write instead of spinning until the watchdog
   fires.
7. **Run-time stats (item 7) — rejected.** Initially disabled on the
   assumption nothing consumed them, but the `PC_TASK_STATUS_CMD` handler
   (`send_heap_status` in `src/app_comm.c`) reports per-task and idle CPU
   usage to the host via `ulTaskGetRunTimeCounter/Percent` and
   `ulTaskGetIdleRunTimeCounter/Percent`, all of which require
   `configGENERATE_RUN_TIME_STATS=1`. `configGENERATE_RUN_TIME_STATS`,
   `configUSE_TRACE_FACILITY` and `configUSE_STATS_FORMATTING_FUNCTIONS`
   are therefore left enabled. The per-context-switch timer read is the cost
   of a shipped diagnostic feature, not dead weight.
8-9. **Scheduler trims (items 8–9).** The tick hook was an empty call at
   2000 Hz per core (now disabled); the watermark refresh full-stack scan ran
   every loop pass in every task (~2.5 MB/s from the keypad task alone) and is
   now sampled every 64th pass.
10. **Input enqueue timeout (item 10).** A full data-event queue already ended
   in drop-and-count after blocking the scan for 1 s; the shorter bound only
   limits the input outage while the 500-slot queue provides the real burst
   absorption.
11. **`PICO_HEAP_SZIE` (item 11).** The misspelled define never had any effect.
   It was removed rather than "fixed": a real `PICO_HEAP_SIZE=0x20000` would
   demand a 128 KB minimum C heap alongside the 128 KB FreeRTOS heap in
   264 KB RAM. The C heap is nearly unused (two one-time `pvPortMalloc`
   driver allocations), so no replacement is needed.

## Proposed items — why deferred

- **Timer/DMA ADC (12):** biggest remaining win on Core 1, but needs a redesign
  of the scan loop (alarm-driven mux stepping or free-running ADC + DMA ring)
  and hardware validation of settling behavior.
- **Single-copy TX path (13):** each outbound packet is currently copied ~3×
  (payload memcpy → COBS encode → queue copy-by-value). A FreeRTOS stream
  buffer of encoded bytes, or encoding directly into the TinyUSB FIFO in
  `cdc_write_task`, would remove two copies — an architectural change to the
  queue contract, so it needs its own test round.
- **Dirty tracking (14):** both display drivers rewrite their full buffer on
  any change. Per-address dirty flags plus fixed-address writes
  (`TM1639_CMD_FIXED_ADDR`) would shrink single-digit updates ~8×.
- **LTO (16):** flagged S-effort but must be validated in CI (no local ARM
  toolchain; pico-sdk has history of `--wrap`/alias breakage under `-flto`).
- **Tick rate (18):** halving to 1000 Hz halves tick overhead but coarsens
  every `pdMS_TO_TICKS(1)` sleep to 1 ms and widens debounce windows —
  needs a timing review of keypad/ADC cadences first.
- **Statistics atomicity (22):** `counters[index]++` from both cores is a
  benign-looking SMP data race (lost increments). Fix is per-core counter
  banks summed on read, or `hardware/sync` spinlocks — correctness work,
  tracked here so it isn't mistaken for a perf lever.

## Follow-up measurement plan

Before investing in items 12–14, profile on hardware:

1. Query per-task and idle CPU usage over `PC_TASK_STATUS_CMD` (already backed
   by the enabled run-time stats) and confirm the ADC/keypad/idle split on
   Core 1.
2. Toggle a spare GPIO around the ADC scan and display flush paths and measure
   with a logic analyzer to validate the computed timings in this document.
3. Use the existing statistics counters (`INPUT_QUEUE_FULL_ERROR`,
   `CDC_QUEUE_SEND_ERROR`, `INPUT_HYSTERESIS_SUPPRESSED`) to watch for drops
   under sustained host load after the queue resizing.

---

**See also:** [Docs index](README.md) · [ARCHITECTURE](ARCHITECTURE.md)
