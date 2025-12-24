# Architecture Documentation: Signalbridge Controller

## Introduction
The Signalbridge Controller firmware runs on the Raspberry Pi Pico using the symmetric multi-processing build of FreeRTOS. Its design targets low-latency communication between the host over USB and a collection of hardware peripherals such as keypads, rotary encoders, ADC channels, and LED or digit displays.

## Architectural Overview
The system follows a producer–consumer pipeline that spreads work across the two RP2040 cores. Core 0 focuses on USB communication through TinyUSB, while Core 1 manages input processing and application logic. Queues connect the tasks so producers and consumers stay decoupled. Illustrations for the task layout and data flow are available in the assets directory and referenced below.

- Task and queue diagram: `../assets/task_diagram_en.png`
- Data flow diagram: `../assets/data_flow_diagram_en.png`

## Task Architecture
Nine principal tasks divide responsibilities with explicit core affinity:

| Task | Core | Purpose |
| :--- | :---: | :--- |
| CDC task | 0 | Maintains the TinyUSB device stack. |
| CDC write task | 0 | Pulls formatted packets from the transmit queue and delivers them to the host. |
| UART event task | 0 | Receives raw bytes from the host and forwards them to the decoding pipeline. |
| LED status task | 0 | Updates the status LED to reflect system state. |
| Decode reception task | 1 | Decodes COBS packets and validates checksums before dispatching commands. |
| Process outbound task | 1 | Formats outbound events and places them in the transmit queue. |
| ADC read task | 1 | Samples ADC channels, filters values, and generates events. |
| Keypad task | 1 | Scans the keypad matrix and emits key events. |
| Encoder read task | 1 | Tracks rotary encoder movement and emits rotation events. |

Stack sizing reflects workload: communication and processing tasks use triple the minimal stack, hardware readers use four to five times the minimal stack, and the status LED task uses double.

## Queue Architecture
Three queues coordinate data movement and enforce isolation between producers and consumers. Sizes match the constants in `include/app_config.h` and `src/app_inputs.c`:

| Queue | Producer | Consumer | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| Encoded reception queue | UART event task | Decode reception task | 2048 bytes | Buffers COBS-encoded bytes arriving from the host. |
| Data event queue | Keypad, ADC, encoder tasks | Process outbound task | 500 events | Stores multiplexed input events until the host fetches them. |
| CDC transmit queue | Process outbound task and decoding path | CDC write task | 2048 packets | Holds formatted packets until the USB interface is ready. |

## Data Flows
- **Host to device:** The UART event task captures bytes from the host, the decode task reconstructs and validates packets, and the processing logic triggers hardware actions or prepares responses.
- **Device to host:** Hardware tasks enqueue events, the outbound processor formats them, and the CDC write task transmits packets to the host. The queue architecture ensures communication duties on Core 0 remain responsive even when Core 1 is busy.

## Synchronization and Protection
Queues provide thread-safe communication between tasks. Core affinity reduces contention, and each task contributes to watchdog updates to detect hangs. Communication queues use short waits or polling to keep USB paths responsive, while the event queue blocks until the host reads data to avoid dropping user input.

## Error Management and Diagnostics
Fourteen counters track issues such as queue send or receive failures, watchdog timeouts, malformed messages, buffer overflows, and bytes transmitted or received. Critical errors persist in watchdog scratch registers, and the status LED communicates fault categories through distinct blink patterns so that resets can be diagnosed without host connectivity.

## Suggested Improvements
Key recommendations for strengthening the architecture include:
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
