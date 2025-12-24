# Output Device Architecture Documentation

## System Overview
The output subsystem manages LED and digit displays through a unified interface on the shared SPI bus. It uses multiplexed chip selection to address multiple devices efficiently while keeping the controller logic consistent across hardware variations.

## Supported Driver Types
The firmware supports generic LED and digit drivers alongside TM1639- and TM1637-based LED and digit drivers. Each controller slot binds to one driver type at build time through the `DEVICE_CONFIG` table in `include/app_outputs.h`, ensuring predictable behavior per device.

## Architectural Outline
- **Payload handling:** Incoming payloads describe digit values or LED states. Controller logic interprets the payload and routes it to the correct driver.
- **Output controller:** The central module coordinates payload processing, delegates to driver handlers, and manages the multiplexer state that selects the active chip.
- **Driver handlers:** Each controller block references a single driver instance and translates generic commands into device-specific actions.
- **Multiplexer and mutex layer:** A hardware multiplexer expands chip select capability while a mutex ensures only one transaction uses the SPI bus at a time. This combination protects transactions from interference.
- **Drivers:** TM1639 devices use the shared SPI bus, and TM1637 devices reuse the same interface via bit-banging on their dedicated pins. Both maintain buffers for current and prepared output states so updates are staged before hardware commits to avoid flicker.

## Concurrency and Bus Control
SPI access is serialized through a mutex to guarantee exclusive transactions. The multiplexer selects the target device for each operation, allowing up to eight chip select lines with minimal GPIO use. Drivers rely on the controller to manage chip selection so protocol handling stays consistent.

## Buffering Strategy
Drivers keep an active buffer representing what is currently displayed and a preparation buffer for upcoming updates. The controller swaps buffers only after a full update is ready, producing smooth transitions and preventing partial frames from appearing on the displays.

## Error Handling and Diagnostics
Parameter validation and error counters help identify initialization failures, invalid payloads, and semaphore issues. Diagnostic tracking allows the team to spot repeated failures and confirm that concurrency controls are working as expected.

## Usage Flow
1. The system initializes output hardware, configures the SPI multiplexer on GPIO 10, 14, 15 with chip select on GPIO 27, and spins up driver instances for each configured slot.
2. Payloads for digits or LEDs arrive from higher-level logic.
3. The controller identifies the target driver and acquires the SPI mutex.
4. The multiplexer selects the correct chip before the driver updates its preparation buffer.
5. TM1639 handlers commit over SPI; TM1637 handlers drive their DIO and CLK pins directly. The mutex is released when the transaction completes.

## Design Principles
- One controller block references exactly one driver to keep responsibilities clear.
- Driver abstraction allows the controller to remain device-agnostic.
- Mutex-protected SPI access and controlled chip selection prevent contention.
- Double buffering maintains stable displays without flicker.
- Multiplexed access scales to multiple devices while conserving GPIO pins.
- TM1637 handlers reuse the shared infrastructure so mixed driver sets remain consistent.
- Robust error tracking supports diagnostics and maintainability.

## Extensibility
New driver types can be added by extending the driver initialization and handler logic while preserving the existing controller interface. The separation between hardware-specific code and shared controller logic keeps the system maintainable and portable.

## References
- Raspberry Pi Pico SDK Documentation
- TM1639 Datasheet
- FreeRTOS Documentation
