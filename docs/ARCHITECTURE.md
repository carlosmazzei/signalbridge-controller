# Architecture Documentation: Signalbridge Controller

## Introduction
The Signalbridge Controller firmware runs on the Raspberry Pi Pico using the symmetric multi-processing build of FreeRTOS. Its design targets low-latency communication between the host over USB and a collection of hardware peripherals such as keypads, rotary encoders, ADC channels, and LED or digit displays.

## Architectural Overview
The system follows a producer–consumer pipeline that spreads work across the two RP2040 cores. Core 0 focuses on USB communication through TinyUSB, while Core 1 manages input processing and application logic. Queues connect the tasks so producers and consumers stay decoupled. Illustrations for the task layout and data flow are available in the assets directory and referenced below.

- Task and queue diagram: `../assets/task_diagram_en.png`
- Data flow diagram: `../assets/data_flow_diagram_en.png`

## Task Architecture
Eight application tasks and a separate watchdog supervisor divide responsibilities with explicit core affinity. Task-status indices 0–7 report application tasks, index 8 reports idle/heap statistics, and index 9 reports the watchdog supervisor; the supervisor remains outside `task_enum_t` and is not checked by its own task-health loop:

| Task | Core | Purpose |
| :--- | :---: | :--- |
| CDC task | 0 | Maintains the TinyUSB device stack. |
| CDC write task | 0 | Pulls formatted packets from the transmit queue and delivers them to the host. |
| UART event task | 0 | Receives raw bytes from the host and forwards them to the decoding pipeline. |
| LED status task | 1 | Updates the status LED to reflect system state. |
| Decode reception task | 1 | Decodes COBS packets and validates checksums before dispatching commands. |
| Process outbound task | 1 | Formats outbound events and places them in the transmit queue. |
| ADC read task | 1 | Samples ADC channels with µs-resolution settling, oversamples and applies a moving-average filter plus hysteresis deadband, and generates events only on significant change. Tunable via `adc_settling_us`, `adc_oversample`, `adc_hysteresis`, `adc_scan_interval_ms` and `adc_channels` (lower the channel count to scan only active throttle/sidestick axes for higher refresh rate). |
| Keypad task | 1 | Scans the keypad matrix and emits key events. |
| Watchdog supervisor (task-status index 9) | 0 | Checks progress of all eight application tasks and feeds the hardware watchdog only while healthy. |

Encoder polling runs inside the keypad task, between column scans.

Stack sizing reflects workload: communication and processing tasks use triple the minimal stack, hardware readers use four to five times the minimal stack, and the status LED task uses double.

## Queue Architecture
Three queues coordinate data movement and enforce isolation between producers and consumers. Sizes match the constants in `include/app_config.h` and `src/app_inputs.c`:

| Queue | Producer | Consumer | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| Encoded reception queue | UART event task | Decode reception task | 256 frames | Buffers complete COBS-encoded frames arriving from the host. |
| Data event queue | Encoder polling in keypad task | Process outbound task | 500 events | Best-effort relative events tagged with the USB session. |
| CDC transmit queue | Process outbound task and decoding path | CDC write task | 256 packets | Holds formatted packets until the USB interface is ready (about 10 KB of FreeRTOS heap, including session and delivery metadata). |

## Data Flows
- **Host to device:** The UART event task captures bytes from the host, the decode task reconstructs and validates packets, and the processing logic triggers hardware actions or prepares responses.
- **Device to host:** Hardware tasks update absolute input snapshots or enqueue relative encoder events, the outbound processor formats them, and the CDC write task transmits packets to the host. The queue architecture ensures communication duties on Core 0 remain responsive even when Core 1 is busy.

## Synchronization and Protection
Queues transfer complete messages by value. An input snapshot store under FreeRTOS critical sections keeps up to 64 key states and 16 ADC values. Initial ADC readings and stable key states are published even without a transition. Values remain pending until the writer accepts the whole packet into TinyUSB; a failed enqueue/write permits retry, and a new USB session invalidates previous delivery acknowledgements. This confirms local FIFO acceptance, not processing by the host application. Relative encoder events are best effort, never block scanning, and are not replayed across sessions.

The single CDC function requires DTR and RTS. Bus reset, unmount, suspend and line-state transitions invalidate the session. RX resets partial framing and rejects queued frames from old sessions; line-state transitions clear buffered RX bytes before publishing readiness. TX clears stale FIFO fragments on a session change. During backpressure, the writer preserves the current partial packet, sleeps 10 ms between unsuccessful writes and records a stall after one second. It can wait for the host indefinitely without busy-spinning; other tasks remain schedulable.

Only the supervisor feeds the five-second hardware watchdog. It checks heartbeats every 100 ms, using a four-second deadline. The decode task explicitly marks its indefinite queue wait; a suspended task or a fatal error still prevents feeding. A partial hang therefore resets the board within roughly nine seconds, while loss of the supervisor itself resets it within five seconds. Statistics, health timestamps and the USB session state use sequentially consistent C11 atomics through the `atomic_u32_*` wrappers (`src/atomic_u32.c`), backed by the Pico SDK on RP2040.

The LED status task publishes its heartbeat after every blink, so long error blink patterns do not look like a hang. USB descriptor callbacks do not publish heartbeats; the CDC heartbeat comes from `cdc_task`.

Initialization and queue cleanup APIs are restricted to boot, before the scheduler runs. Output reinitialization releases old driver handles. Display active buffers and brightness state are committed only after a successful driver transfer; failures keep the desired buffer dirty. TM1639 SPI transfer success cannot prove that a physical panel accepted the data.

## Error Management and Diagnostics
Twenty-three counters track issues such as queue send or receive failures, watchdog timeouts, malformed messages, buffer overflows, bytes transmitted or received, and output/input driver errors. Critical errors persist in watchdog scratch registers (a Pico SDK `panic()` is routed to `panic_handler` through `PICO_PANIC_FUNCTION` and recorded as `ERROR_PICO_SDK_PANIC`), and the status LED communicates fault categories through distinct blink patterns so that resets can be diagnosed without host connectivity.

## Known limitations

- The following have not yet been validated on a physical board: USB enumeration on the host operating systems in use, TX saturation and reconnect, electrical ACKs, SMP behaviour under load, input latency, peak task stack usage, and watchdog reset on partial/fatal hangs. Host tests use the POSIX port on a single core and cannot cover these cases; see [`test/README.md`](../test/README.md).
- Command dispatch drives display and LED hardware synchronously inside the decode task (priority 2, Core 1, the same core as the priority-1 input tasks). Decoupling it needs latency measurements and a defined priority/coalescing/drop policy.
- Display controller IDs are 1–7: the three-bit ID field cannot address an eighth slot, and changing that is a protocol compatibility decision. Digit count per driver comes from the type map; there is no generic driver-capability interface.
- Input delivery is confirmed at TinyUSB FIFO acceptance; the protocol has no application-level acknowledgement. Encoder events and command responses are best effort.
- Toolchain reproducibility is limited: the DevContainer pins exact APT versions, but without a Debian snapshot repository old pins can disappear (see [`.devcontainer/README.md`](../.devcontainer/README.md)).
- CI (`.github/workflows/build-all.yml`) has no `pull_request` trigger; on push, it runs only when the listed paths change. The quality checks (format, Cppcheck/MISRA, host tests) are non-blocking unless the repository variable `ENFORCE_QUALITY_GATES=true` is set. Release publication requires `build-firmware` and `unit-tests` to succeed.

## Suggested Improvements
A prioritized performance analysis — including which of these items have already been implemented and which remain proposed — is maintained in [PERFORMANCE.md](PERFORMANCE.md). Key recommendations for strengthening the architecture include:
- Introduce differentiated task priorities so USB communication outranks lower-urgency processing.
- Expand the data event queue capacity to reduce blocking during bursts of hardware activity.
- Favor interrupt-driven input capture for the keypad, ADC, and encoder paths to reduce latency and CPU usage.
- Replace indefinite queue waits with bounded timeouts paired with recovery logic.
- Separate transmission queues by priority to ensure command responses are not delayed by bulk event traffic.
- Apply rate limiting to ADC events to avoid flooding downstream queues when values oscillate.
- Track queue utilization in diagnostics to guide tuning and highlight bottlenecks.
- Consider static memory pools for frequently used data structures to avoid heap fragmentation and improve determinism.
- Upgrade checksums to CRCs for more robust error detection on noisy links.
- Document task state machines with diagrams to simplify maintenance and onboarding.

## Conclusion
The current architecture cleanly separates responsibilities across FreeRTOS tasks and cores, leveraging queues for safe communication. The improvement areas above focus on latency reduction, robustness, and observability so the firmware can scale to heavier workloads while maintaining predictable behavior.

---

**See also:** [Docs index](README.md) · [OUTPUT](OUTPUT.md) · [COMMANDS](COMMANDS.md)
