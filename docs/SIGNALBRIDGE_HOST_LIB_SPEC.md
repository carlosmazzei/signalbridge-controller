# Signalbridge Host Library — Implementation Specification

This document fully specifies a host-side communication library for Signalbridge Controller boards. It is the single reference an implementor needs; reading the firmware source is not required.

---

## 1. Purpose and Scope

### What the library does

- Opens USB CDC (virtual COM port) serial connections to one or more Signalbridge boards
- Implements the COBS binary framing protocol (encode, decode, checksum)
- Dispatches received input events (keypad, ADC, rotary encoder) to the consuming application via callbacks or an event system
- Provides methods to send output commands (PWM, LED matrix, 7-segment displays)
- Exposes a diagnostics API (error counters, task statistics, echo/ping)
- Manages connection lifecycle (connect, disconnect, reconnect, board enumeration)

### What the library does NOT do

- Render any UI — the consuming application owns all presentation
- Persist configuration — the library is stateless between sessions
- Implement flight simulator logic — it is a transport and device abstraction layer
- Manage non-Signalbridge devices — the UI application can compose this library alongside other interface libraries

### Design constraints

- The library must be safe to use from a UI thread via an event/callback pattern (no blocking calls on the main thread)
- All serial I/O should run on a background thread or async loop, with thread-safe event dispatch to the caller
- The library must support multiple simultaneous board connections, each identified by its 11-bit Board ID
- The library should be usable from any language/platform that supports USB serial (Python, TypeScript/Node.js, C#, C++, Rust, etc.) — the spec is language-agnostic

---

## 2. Transport Layer

### USB CDC serial

- **Interface**: USB CDC ACM (virtual COM port)
- **No baud rate negotiation** — USB full-speed/high-speed is handled by the USB stack
- **Flow control**: None required beyond DTR/RTS (see below)
- **Buffer sizes**: Host should use at minimum 4096-byte read/write buffers

### USB device identification

| Field | Value |
|---|---|
| Vendor ID (VID) | `0xCAFE` |
| Product ID (PID) | `0x4001` |
| Manufacturer string | `Signalbridge` |
| Product string | `Signalbridge Device` |
| Serial number | RP2040 unique board ID (16 hex characters, unique per chip) |
| CDC interface | Interface 0 (`ITF_NUM_CDC_0`) |

The serial number is derived from the RP2040's hardware-unique ID and can be used to persistently identify a specific physical board across USB reconnections.

### DTR/RTS requirement (critical)

The firmware gates all communication on the DTR and RTS line states. The host **must assert both DTR and RTS** after opening the serial port. If either is not asserted, the firmware will silently discard all outbound traffic (device will not transmit to host).

```
After opening the serial port:
  1. Assert DTR (Data Terminal Ready)
  2. Assert RTS (Request To Send)
  3. Wait briefly for the firmware to recognize the line state change
  4. Begin communication
```

Most serial libraries assert DTR automatically on port open, but RTS may need explicit assertion. Always set both.

---

## 3. COBS Framing Protocol

### Overview

All data exchanged between host and device is COBS-encoded (Consistent Overhead Byte Stuffing). COBS transforms arbitrary byte sequences into zero-free representations, allowing `0x00` to serve as an unambiguous packet delimiter.

### Wire format

```
[COBS-encoded bytes] [0x00]
```

- The `0x00` byte is the **packet delimiter** (packet marker)
- It is appended after the COBS-encoded data
- It is NOT part of the COBS-encoded payload — strip it before decoding

### COBS encoding algorithm

Input bytes are divided into groups separated by zero bytes. Each group is prefixed with a code byte indicating the number of non-zero bytes that follow (plus one). A code byte of 0x01 represents an original zero byte with no following data bytes.

**Encoding steps:**
1. Scan the input for zero bytes, splitting into groups
2. For each group, write a code byte equal to (group length + 1), followed by the group bytes
3. If the last group has no trailing zero, write the code byte and group normally
4. Worst-case overhead: 1 byte per 254 input bytes

**Decoding steps:**
1. Read a code byte `c`
2. If `c == 0x00`, decoding is complete (delimiter reached)
3. Copy the next `c - 1` bytes to output
4. If `c < 0xFF`, append a `0x00` to output (implicit zero), unless this is the last group
5. Repeat from step 1

**Example:**
```
Original:    [0xAA] [0xBB] [0x00] [0xCC]
Encoded:     [0x03] [0xAA] [0xBB] [0x02] [0xCC]
On wire:     [0x03] [0xAA] [0xBB] [0x02] [0xCC] [0x00]
                                                   ^-- packet delimiter
```

### Receiving packets

1. Read bytes from the serial port continuously
2. Buffer bytes until a `0x00` is encountered
3. The bytes before the `0x00` form one COBS-encoded packet
4. COBS-decode the buffered bytes to recover the raw packet
5. Validate and process the raw packet (see section 4)
6. Repeat

### Sending packets

1. Build the raw packet (header + payload + checksum) per section 4
2. COBS-encode the raw packet
3. Append a `0x00` delimiter byte
4. Write the result to the serial port

---

## 4. Packet Structure

### Raw packet layout (before COBS encoding)

```
Byte 0       : ID high       (upper 8 bits of the 11-bit Board ID)
Byte 1       : ID low + cmd  (bits 7-5: lower 3 bits of Board ID, bits 4-0: command)
Byte 2       : payload length (N, range 0–20)
Byte 3..3+N-1: payload data
Byte 3+N     : XOR checksum
```

### Board ID encoding (11-bit)

The Board ID is an 11-bit value split across bytes 0 and 1:

```
Board ID = (byte0 << 3) | ((byte1 >> 5) & 0x07)
```

To encode a Board ID into the header:

```
byte0 = (board_id >> 3) & 0xFF
byte1 = ((board_id & 0x07) << 5) | (command & 0x1F)
```

The default Board ID is `0x01`. The firmware only processes packets whose Board ID matches its configured value.

### Command encoding (5-bit)

```
command = byte1 & 0x1F
```

Valid command range: `0x01` to `0x1E` (30 commands).

### Payload constraints

- Maximum payload length: **20 bytes**
- Payload length of 0 is valid (header-only messages like echo with no data)

### Checksum

- **Algorithm**: XOR of all bytes from byte 0 through the last payload byte
- **Position**: Immediately after the payload (byte 3+N)

```
checksum = byte0 ^ byte1 ^ byte2 ^ payload[0] ^ payload[1] ^ ... ^ payload[N-1]
```

### Maximum packet sizes

| Stage | Size |
|---|---:|
| Max payload | 20 bytes |
| Header | 3 bytes |
| Checksum | 1 byte |
| Max raw packet | 24 bytes |
| Max COBS-encoded | 26 bytes (24 + ceil(24/254)) |
| Max on-wire (with delimiter) | 27 bytes |

### Packet validation (receiving)

When receiving a packet from the device, validate in this order:

1. COBS-decode succeeds (no malformed code bytes)
2. Decoded length >= 4 (minimum: 3-byte header + 1-byte checksum)
3. Payload length field (byte 2) matches actual decoded length minus 4
4. Payload length <= 20
5. XOR checksum matches
6. Board ID matches expected value
7. Command is a known value

If any check fails, discard the packet and optionally log the error.

---

## 5. Command Reference

### 5.1 Command ID Table

| Name | ID | Direction | Status | Description |
|---|---:|---|---|---|
| `PWM` | `0x01` | Host → Device | Implemented | Set global PWM brightness |
| `LED_OUT` | `0x02` | Host → Device | Implemented | Update LED matrix column |
| `AD` | `0x03` | Device → Host | Implemented | ADC value change event |
| `KEY` | `0x04` | Device → Host | Implemented | Keypad press/release event |
| `DISPLAY` | `0x05` | Host → Device | Reserved | 7-segment display (not implemented) |
| `ROTARY` | `0x06` | Device → Host | Implemented | Rotary encoder rotation event |
| `TRIM` | `0x07` | — | Reserved | Trim wheel event |
| `OPTO` | `0x08` | — | Reserved | Opto-coupler input |
| `RELAY` | `0x09` | — | Reserved | Relay control |
| `DISPLAY_CTL` | `0x0A` | Host → Device | Implemented | Display control (digits + brightness) |
| `TCAS` | `0x0B` | — | Reserved | TCAS indicator |
| `FCU` | `0x0C` | — | Reserved | Flight Control Unit |
| `SET_VALUE` | `0x0D` | — | Reserved | Generic set-value |
| `DEBUG` | `0x10` | — | Reserved | Debug data |
| `DEBUG_CTL1` | `0x11` | — | Reserved | Debug control channel 1 |
| `DEBUG_CTL2` | `0x12` | — | Reserved | Debug control channel 2 |
| `DEBUG_CTL3` | `0x13` | — | Reserved | Debug control channel 3 |
| `ECHO` | `0x14` | Bidirectional | Implemented | Echo request/response |
| `ID_TABLE` | `0x15` | — | Reserved | Identification table |
| `IO_ERROR_STATUS` | `0x16` | — | Reserved | I/O error summary |
| `ERROR_STATUS` | `0x17` | Bidirectional | Implemented | Error counter query |
| `TASK_STATUS` | `0x18` | Bidirectional | Implemented | FreeRTOS task status query |
| `USB_STATUS` | `0x19` | — | Reserved | USB link status |
| `ID_CONFIRM_NODE` | `0x1A` | — | Reserved | Node confirmation |
| `ID_CONFIRM` | `0x1B` | — | Reserved | Confirmation response |
| `ID_REQUEST` | `0x1C` | — | Reserved | Identification request |
| `CONFIG` | `0x1D` | — | Reserved | Configuration update |
| `ENUMERATE` | `0x1E` | — | Reserved | Enumeration trigger |

**Reserved** commands are defined in the firmware enum but have no handler. The library should define constants for all command IDs but only implement send/receive logic for commands marked **Implemented**.

---

### 5.2 Inbound Commands (Host → Device)

#### 5.2.1 PWM Brightness — `0x01`

Sets the global PWM duty cycle that controls the LED brightness rail for all annunciators.

| Field | Value |
|---|---|
| Command ID | `0x01` |
| Direction | Host → Device |
| Payload length | 1 byte |

**Payload:**

| Byte | Description | Range |
|---:|---|---|
| 0 | Duty cycle | `0x00` (off) – `0xFF` (full brightness) |

The firmware applies a squared nonlinearity internally (`duty² / 255`) for perceptual linearity. The library should expose the raw 0–255 value and document this behavior.

**Example raw packet** (Board ID `0x01`, duty cycle `0x80`):
```
Bytes: [0x00] [0x21] [0x01] [0x80] [0xA0]
        id_hi  id_lo  len    duty   checksum
              +cmd
```

---

#### 5.2.2 LED Matrix Update — `0x02`

Updates a single column of an 8×8 LED matrix attached to a specific controller.

| Field | Value |
|---|---|
| Command ID | `0x02` |
| Direction | Host → Device |
| Payload length | 3 bytes |

**Payload:**

| Byte | Description | Range |
|---:|---|---|
| 0 | Controller ID | `1`–`8` (1-based) |
| 1 | Column index | `0`–`7` |
| 2 | LED bitmask | `0x00`–`0xFF` |

**LED bitmask encoding:**
- Bits 3–0: Drive SEG1–SEG4 (written to low nibble of address `column × 2`)
- Bits 7–4: Drive SEG9–SEG12 (written to low nibble of address `column × 2 + 1`)
- `1` = LED on, `0` = LED off

**Example raw packet** (Board ID `0x01`, controller 1, column 3, LEDs 0xFF):
```
Bytes: [0x00] [0x22] [0x03] [0x01] [0x03] [0xFF] [0xDC]
```

---

#### 5.2.3 Display Control — `0x0A`

Controls 7-segment digit displays. Two sub-commands are supported: set digits and set brightness.

| Field | Value |
|---|---|
| Command ID | `0x0A` |
| Direction | Host → Device |

**Payload header byte** (`payload[0]`):
```
Bits 7–5: Controller ID (1-based, value 1–8 stored as 1–7 in 3 bits)
Bits 4–0: Display sub-command
```

```
controller_id = (payload[0] >> 5) & 0x07
display_cmd   = payload[0] & 0x1F
```

##### Sub-command 0x00: Set Digits

| Field | Value |
|---|---|
| Payload length | 6 bytes |
| Display sub-command | `0x00` |

| Byte | Description |
|---:|---|
| 0 | Header: `(controller_id << 5) \| 0x00` |
| 1 | Digit pair 0-1: high nibble = digit 0, low nibble = digit 1 |
| 2 | Digit pair 2-3: high nibble = digit 2, low nibble = digit 3 |
| 3 | Digit pair 4-5: high nibble = digit 4, low nibble = digit 5 |
| 4 | Digit pair 6-7: high nibble = digit 6, low nibble = digit 7 |
| 5 | Dot position (0–7 for position, `0xFF` for no dot) |

Digits are BCD-encoded: `0x0`–`0xF`. Values `0x0`–`0x9` display the corresponding numeral. Values `0xA`–`0xF` are driver-specific (some drivers display hex characters, others blank).

**Example**: Controller 1, digits "12345678", dot at position 2:
```
payload: [0x20] [0x12] [0x34] [0x56] [0x78] [0x02]
```

##### Sub-command 0x01: Set Brightness

| Field | Value |
|---|---|
| Payload length | 2 bytes |
| Display sub-command | `0x01` |

| Byte | Description |
|---:|---|
| 0 | Header: `(controller_id << 5) \| 0x01` |
| 1 | Brightness level: `0` = off, `1`–`7` = on |

**Example**: Controller 1, brightness 4:
```
payload: [0x21] [0x04]
```

---

#### 5.2.4 Echo — `0x14`

Sends arbitrary data to the device, which echoes it back unchanged. Used for connectivity testing and round-trip latency measurement.

| Field | Value |
|---|---|
| Command ID | `0x14` |
| Direction | Bidirectional |
| Payload length | 0–20 bytes (any) |

The device responds with the same command ID and identical payload.

---

#### 5.2.5 Error Status Query — `0x17`

Requests the value of a specific statistics counter from the device.

| Field | Value |
|---|---|
| Command ID | `0x17` |
| Direction | Host → Device (request), Device → Host (response) |

**Request payload** (1 byte):

| Byte | Description | Range |
|---:|---|---|
| 0 | Counter index | `0`–`22` |

**Response payload** (5 bytes):

| Byte | Description |
|---:|---|
| 0 | Counter index (echoed) |
| 1 | Counter value bits 31–24 (MSB) |
| 2 | Counter value bits 23–16 |
| 3 | Counter value bits 15–8 |
| 4 | Counter value bits 7–0 (LSB) |

**Statistics counter indices:**

| Index | Name | Description |
|---:|---|---|
| 0 | `QUEUE_SEND_ERROR` | FreeRTOS queue send failures |
| 1 | `QUEUE_RECEIVE_ERROR` | FreeRTOS queue receive failures |
| 2 | `CDC_QUEUE_SEND_ERROR` | CDC transmit queue send failures |
| 3 | `DISPLAY_OUT_ERROR` | Display driver output errors |
| 4 | `LED_OUT_ERROR` | LED driver output errors |
| 5 | `WATCHDOG_ERROR` | Watchdog timeout events |
| 6 | `MSG_MALFORMED_ERROR` | Malformed inbound messages |
| 7 | `COBS_DECODE_ERROR` | COBS decoding failures |
| 8 | `RECEIVE_BUFFER_OVERFLOW_ERROR` | Receive buffer overflows |
| 9 | `CHECKSUM_ERROR` | Checksum validation failures |
| 10 | `BUFFER_OVERFLOW_ERROR` | Generic buffer overflow events |
| 11 | `UNKNOWN_CMD_ERROR` | Unknown command IDs received |
| 12 | `BYTES_SENT` | Total bytes transmitted to host |
| 13 | `BYTES_RECEIVED` | Total bytes received from host |
| 14 | `RESOURCE_ALLOCATION_ERROR` | Memory/queue allocation failures |
| 15 | `OUTPUT_CONTROLLER_ID_ERROR` | Invalid controller ID in output command |
| 16 | `OUTPUT_INIT_ERROR` | Output subsystem initialization failures |
| 17 | `OUTPUT_DRIVER_INIT_ERROR` | Individual driver initialization failures |
| 18 | `OUTPUT_INVALID_PARAM_ERROR` | Invalid parameter in output command |
| 19 | `INPUT_QUEUE_INIT_ERROR` | Input event queue initialization failures |
| 20 | `INPUT_QUEUE_FULL_ERROR` | Input event queue full (events dropped) |
| 21 | `INPUT_INIT_ERROR` | Input subsystem initialization failures |
| 22 | `INPUT_HYSTERESIS_SUPPRESSED` | ADC events suppressed by hysteresis filter |

---

#### 5.2.6 Task Status Query — `0x18`

Requests runtime metrics for a specific FreeRTOS task.

| Field | Value |
|---|---|
| Command ID | `0x18` |
| Direction | Host → Device (request), Device → Host (response) |

**Request payload** (1 byte):

| Byte | Description | Range |
|---:|---|---|
| 0 | Task index | `0`–`8` |

If the task index exceeds the number of tasks, the response is a single byte `0xFF`.

**Response payload** (13 bytes):

| Byte | Description |
|---:|---|
| 0 | Task index (echoed) |
| 1–4 | Runtime counter (big-endian, 32-bit) |
| 5–8 | Runtime percentage (big-endian, 32-bit) |
| 9–12 | Stack high watermark in bytes (big-endian, 32-bit) |

When `index == 8` (equal to `NUM_TASKS`), the response returns idle task statistics with bytes 9–12 containing the minimum free heap size instead of a stack watermark.

**Task indices:**

| Index | Task | Core |
|---:|---|---|
| 0 | CDC device task | 0 |
| 1 | CDC write task | 0 |
| 2 | UART event task | 0 |
| 3 | Decode reception task | 1 |
| 4 | Process outbound task | 1 |
| 5 | ADC read task | 1 |
| 6 | Keypad task | 1 |
| 7 | LED status task | 0 |
| 8 | System (idle task + heap info) | — |

---

### 5.3 Outbound Events (Device → Host)

These are unsolicited messages generated by the device whenever input state changes. The library must continuously listen for these and dispatch them to registered callbacks.

#### 5.3.1 Keypad Event — `0x04`

Generated when a key is pressed or released. The device debounces with a 3-sample stability window.

| Field | Value |
|---|---|
| Command ID | `0x04` |
| Direction | Device → Host |
| Payload length | 1 byte |

**Payload:**

| Byte | Description |
|---:|---|
| 0 | `(column << 4) \| (row << 1) \| state` |

**Decoding:**
```
column = (payload[0] >> 4) & 0x0F     // range 0–7
row    = (payload[0] >> 1) & 0x07     // range 0–7
state  = payload[0] & 0x01            // 1 = pressed, 0 = released
```

**Hardware context:**
- 8×8 matrix = 64 possible keys
- Some positions may be mapped to rotary encoders and will not generate key events
- Debounce: ~4 ms press detection, ~6 ms release detection
- Scan rate: ~500 Hz

---

#### 5.3.2 ADC Event — `0x03`

Generated when an ADC channel's filtered value changes beyond the hysteresis threshold.

| Field | Value |
|---|---|
| Command ID | `0x03` |
| Direction | Device → Host |
| Payload length | 3 bytes |

**Payload:**

| Byte | Description |
|---:|---|
| 0 | ADC channel index (0–15) |
| 1 | Value high byte (MSB) |
| 2 | Value low byte (LSB) |

**Decoding:**
```
channel = payload[0]                    // range 0–15
value   = (payload[1] << 8) | payload[2]  // 12-bit ADC value, range 0–4095
```

**Hardware context:**
- 16 channels behind a 74HC4067 analog multiplexer
- 12-bit ADC (Pico built-in)
- 4-tap moving average filter
- Default hysteresis: 8 LSBs (events suppressed if change < 8)
- Events are only sent when value changes significantly
- Default scan interval: 1 ms per full 16-channel sweep

---

#### 5.3.3 Rotary Encoder Event — `0x06`

Generated when a rotary encoder detects one detent of rotation.

| Field | Value |
|---|---|
| Command ID | `0x06` |
| Direction | Device → Host |
| Payload length | 2 bytes |

**Payload:**

| Byte | Description |
|---:|---|
| 0 | `(encoder_index << 4)` — encoder index in upper nibble |
| 1 | Direction: `1` = clockwise, `0` = counter-clockwise |

**Decoding:**
```
encoder_index = (payload[0] >> 4) & 0x0F  // range 0–7
direction     = payload[1]                 // 1 = CW, 0 = CCW
```

**Hardware context:**
- Up to 8 rotary encoders
- Quadrature decoding with lookup table
- Encoders are wired into the keypad matrix (2 adjacent columns per encoder)
- Those matrix positions are excluded from keypad scanning

---

## 6. Hardware Capabilities Summary

This section describes the physical I/O available on a Signalbridge board so the library can expose meaningful abstractions and the UI can present appropriate controls.

### Inputs

| Input | Count | Resolution | Notes |
|---|---:|---|---|
| Keypad keys | 64 (8×8) | Binary | Debounced, press/release events |
| ADC channels | 16 | 12-bit (0–4095) | Filtered, hysteresis-suppressed |
| Rotary encoders | Up to 8 | Detent steps | Quadrature, CW/CCW direction |

### Outputs

| Output | Count | Resolution | Notes |
|---|---:|---|---|
| PWM brightness | 1 (global) | 8-bit (0–255) | Squared for perceptual linearity |
| LED matrices | Up to 8 | 8×8 per controller | Column-addressable, 8 segments per column |
| 7-segment displays | Up to 8 | 8 digits per controller | BCD digits, decimal point per digit |
| Display brightness | Per controller | 3-bit (0–7) | 0 = off, 1–7 = on |

### Controller slots

The device has 8 controller slots (IDs 1–8) accessible via an SPI multiplexer. Each slot can host one display/LED device. The default configuration has a TM1639 digit controller in slot 1; slots 2–8 are empty. The slot configuration is fixed at firmware compile time.

### Supported device types

| Type ID | Name | Protocol |
|---:|---|---|
| 0 | None | — |
| 1 | Generic LED | — |
| 2 | Generic Digit | — |
| 3 | TM1639 LED | SPI |
| 4 | TM1639 Digit | SPI |
| 5 | TM1637 LED | Bit-bang |
| 6 | TM1637 Digit | Bit-bang |

---

## 7. Library API Design

This section defines the public API surface the library must expose. The design is event-driven and non-blocking.

### 7.1 Board Connection

```
BoardConnection
  ├── connect(port_path) → BoardHandle
  ├── disconnect(handle)
  ├── list_serial_ports() → PortInfo[]
  ├── is_connected(handle) → bool
  └── on_connection_state_changed(callback)
```

**`connect(port_path)`**: Opens the USB CDC serial port, starts background read/write threads, and returns a handle. The connection is not "verified" until the first successful echo or received event.

**`disconnect(handle)`**: Closes the serial port and stops background threads. Pending outbound packets are flushed before closing.

**`list_serial_ports()`**: Returns a list of available serial ports with metadata (path, VID/PID, serial number, description). The library should provide a `discover_boards()` convenience method that filters for devices matching VID `0xCAFE` / PID `0x4001` and returns only Signalbridge boards. The USB serial number (16-character RP2040 unique ID) enables persistent board identification across reconnections.

**Connection state transitions:**
```
Disconnected → Connecting → Connected → Disconnected
                    ↓
               Connection Failed
```

### 7.2 Input Event Listeners

The library should support registering callbacks for each input event type. All callbacks are dispatched on a non-serial-port thread (e.g., main thread, event loop, or a dedicated dispatch thread).

```
board.on_key_event(callback(board_id, column, row, pressed))
board.on_adc_event(callback(board_id, channel, value))
board.on_rotary_event(callback(board_id, encoder_index, direction))
board.on_echo_response(callback(board_id, payload, round_trip_ms))
board.on_error_status_response(callback(board_id, counter_index, counter_name, value))
board.on_task_status_response(callback(board_id, task_index, task_name, runtime, percent, watermark))
board.on_raw_packet(callback(board_id, command, payload))  // catch-all for unhandled commands
```

### 7.3 Output Commands

All send methods are non-blocking. They encode the packet and enqueue it for the background write thread.

```
board.set_pwm(duty_cycle: 0–255)
board.set_led(controller_id: 1–8, column: 0–7, bitmask: 0x00–0xFF)
board.set_digits(controller_id: 1–8, digits: uint8[8], dot_position: 0–7 or NONE)
board.set_display_brightness(controller_id: 1–8, brightness: 0–7)
board.send_echo(payload: bytes)
board.query_error_counter(counter_index: 0–22)
board.query_task_status(task_index: 0–8)
```

### 7.4 Diagnostics

```
board.query_all_error_counters() → Future<dict[counter_name, value]>
board.query_all_task_stats() → Future<list[TaskStatus]>
board.ping() → Future<round_trip_ms>       // convenience wrapper around echo
```

`query_all_error_counters()` sends 23 individual error status queries (indices 0–22) and collects the responses. The library should handle response correlation by matching the counter index in the response payload to the original request.

`query_all_task_stats()` sends 9 individual task status queries (indices 0–8) and collects the responses.

### 7.5 Multi-Board Management

```
SignalbridgeManager
  ├── add_board(port_path, board_id: 0x001–0x7FF) → BoardHandle
  ├── remove_board(handle)
  ├── get_board(board_id) → BoardHandle
  ├── list_boards() → BoardHandle[]
  ├── on_board_added(callback)
  ├── on_board_removed(callback)
  └── on_any_event(callback)   // receives events from all boards
```

The `board_id` parameter is optional at connection time. If not provided, the library should accept packets with any Board ID from that port. If provided, the library should validate that received packets match the expected Board ID and discard mismatches.

---

## 8. Connection Lifecycle

### Connect flow

1. Open USB CDC serial port (no baud rate negotiation needed)
2. Assert DTR and RTS line states (required — firmware will not transmit without both)
3. Start background reader thread
4. Start background writer thread
5. (Optional) Send an echo request to verify connectivity
6. Notify application of connection state change
7. Begin dispatching received events

### Disconnect flow

1. Stop accepting new outbound packets
2. Flush pending outbound queue (with timeout)
3. Stop background reader and writer threads
4. Close serial port
5. Notify application of connection state change

### Error recovery

If the serial port becomes unavailable (USB disconnect, device reset, etc.):

1. Detect read/write failure on the background thread
2. Transition to `Disconnected` state
3. Notify application via `on_connection_state_changed` callback
4. The application decides whether to attempt reconnection — the library does not auto-reconnect by default

The library should provide an optional auto-reconnect mode:
- Periodically scan for the device on the last known port
- Exponential backoff: 1s, 2s, 4s, 8s, max 30s
- Notify application on each reconnect attempt and on success

### Device reset detection

The Signalbridge board has a 5-second watchdog timer. If the watchdog fires, the device resets and re-enumerates on USB. The host should detect the serial port disappearing and reappearing. After reconnection, all device state (PWM, LEDs, displays) is reset to defaults.

---

## 9. Error Handling

### Protocol-level errors

| Error | Detection | Library behavior |
|---|---|---|
| COBS decode failure | Malformed code bytes | Discard packet, increment internal error counter |
| Checksum mismatch | Computed XOR ≠ received | Discard packet, increment internal error counter |
| Payload too large | Length field > 20 | Discard packet, increment internal error counter |
| Unknown command | Command ID not in table | Dispatch to `on_raw_packet` callback, log warning |
| Board ID mismatch | Received ID ≠ expected | Discard packet silently (multi-board scenarios) |

### Transport-level errors

| Error | Detection | Library behavior |
|---|---|---|
| Serial port open failure | OS error on open | Throw/return error, transition to Disconnected |
| Read failure | OS error or timeout | Transition to Disconnected, notify application |
| Write failure | OS error | Transition to Disconnected, notify application |
| USB device disconnected | Port disappears | Transition to Disconnected, notify application |

### Library internal counters

The library should maintain its own set of diagnostic counters:

- Packets sent
- Packets received
- Packets discarded (checksum, COBS, overflow)
- Bytes sent
- Bytes received
- Connection attempts
- Connection failures
- Reconnections

---

## 10. Example Wire-Format Messages

All examples use Board ID `0x01`.

### 10.1 Sending: PWM brightness to 50% (duty = 0x80)

**Raw packet construction:**
```
byte0 = 0x00                      // Board ID 0x01: (0x01 >> 3) = 0x00
byte1 = 0x21                      // (0x01 & 0x07) << 5 | 0x01 = 0x21
byte2 = 0x01                      // payload length
payload = [0x80]                  // duty cycle
checksum = 0x00 ^ 0x21 ^ 0x01 ^ 0x80 = 0xA0

Raw:  [0x00, 0x21, 0x01, 0x80, 0xA0]
```

**COBS encoding** (0x00 in raw data at position 0):
```
Group 1: [0x00] → code 0x01 (zero at position 0)
Group 2: [0x21, 0x01, 0x80, 0xA0] → code 0x05 (4 bytes + 1)

COBS: [0x01, 0x05, 0x21, 0x01, 0x80, 0xA0]
Wire: [0x01, 0x05, 0x21, 0x01, 0x80, 0xA0, 0x00]
```

### 10.2 Receiving: Keypad event — column 1, row 0, pressed

**Wire bytes received:**
```
[... bytes ...] [0x00]    ← packet delimiter
```

**After COBS decode:**
```
[0x00, 0x24, 0x01, 0x11, 0x34]
```

**Parse:**
```
board_id = (0x00 << 3) | ((0x24 >> 5) & 0x07) = 0x01
command  = 0x24 & 0x1F = 0x04 (KEY event)
length   = 0x01
payload  = [0x11]
checksum = 0x00 ^ 0x24 ^ 0x01 ^ 0x11 = 0x34 ✓

column = (0x11 >> 4) & 0x0F = 1
row    = (0x11 >> 1) & 0x07 = 0
state  = 0x11 & 0x01 = 1 (pressed)
```

### 10.3 Receiving: ADC event — channel 3, value 2748

**After COBS decode:**
```
[0x00, 0x23, 0x03, 0x03, 0x0A, 0xBC, checksum]
```

**Parse:**
```
command = 0x03 (AD event)
channel = 0x03 = 3
value   = (0x0A << 8) | 0xBC = 2748
```

### 10.4 Sending: Display digits "12345678" with dot at position 2, controller 1

**Payload construction:**
```
payload[0] = (1 << 5) | 0x00 = 0x20   // controller 1, set digits
payload[1] = 0x12                       // digits 0-1
payload[2] = 0x34                       // digits 2-3
payload[3] = 0x56                       // digits 4-5
payload[4] = 0x78                       // digits 6-7
payload[5] = 0x02                       // dot at position 2

Raw: [0x00, 0x2A, 0x06, 0x20, 0x12, 0x34, 0x56, 0x78, 0x02, checksum]
```

### 10.5 Querying error counter 9 (CHECKSUM_ERROR)

**Request raw:**
```
[0x00, 0x37, 0x01, 0x09, checksum]
```

**Response raw (if counter = 42 = 0x0000002A):**
```
[0x00, 0x37, 0x05, 0x09, 0x00, 0x00, 0x00, 0x2A, checksum]
```

---

## 11. Testing Strategy

The library should include tests for:

### Unit tests
- COBS encode/decode round-trip with edge cases (empty, all zeros, 254+ bytes, max packet)
- Packet construction and parsing for every implemented command
- Checksum calculation verification against the example packets in this spec
- Board ID encoding/decoding with various 11-bit values
- Payload validation (length bounds, field ranges)

### Integration tests
- Connect to a real or emulated Signalbridge board
- Send echo request and verify identical response
- Send PWM command and verify no error response
- Query all error counters and verify reasonable values
- Query all task statuses and verify valid responses
- Monitor keypad/ADC/encoder events with physical interaction (manual test)

### Compliance tests
- Verify all example byte sequences in section 10 produce correct wire output
- Verify parser accepts all example responses and decodes correctly
- Verify parser rejects malformed packets (bad checksum, oversized payload, truncated header)

---

## 12. Implementation Notes

### Threading model

- **Reader thread**: Continuously reads from the serial port, buffers bytes, splits on `0x00` delimiters, COBS-decodes, validates, and dispatches to callbacks
- **Writer thread**: Dequeues outbound packets, COBS-encodes, appends delimiter, writes to serial port
- **Dispatch thread** (optional): If callbacks must run on a specific thread (e.g., UI thread), use a thread-safe queue to marshal events

### Performance considerations

- The device can generate events at up to 500 Hz (keypad) and 1000 Hz (ADC) per channel
- With 16 ADC channels and hysteresis, typical event rate is much lower
- The library should handle at least 1000 events/second per board without dropping packets
- Outbound commands are rate-limited by USB latency (~1 ms per USB frame); batching multiple commands in rapid succession is fine

### Byte order

All multi-byte integer values in the protocol are **big-endian** (network byte order):
- ADC values: `(payload[1] << 8) | payload[2]`
- Error counter values: 32-bit big-endian across 4 bytes
- Task statistics: 32-bit big-endian across 4 bytes

### Platform considerations

- **Windows**: Serial ports appear as `COMx` — use device manager or registry to find CDC ACM devices
- **macOS**: Serial ports appear as `/dev/tty.usbmodemXXXX` or `/dev/cu.usbmodemXXXX`
- **Linux**: Serial ports appear as `/dev/ttyACMx` — udev rules can create stable symlinks

---

## 13. Glossary

| Term | Definition |
|---|---|
| **Board ID** | 11-bit identifier uniquely identifying a Signalbridge board (default: `0x01`) |
| **COBS** | Consistent Overhead Byte Stuffing — encoding that eliminates zero bytes for framing |
| **Controller ID** | 1-based identifier (1–8) for SPI-attached display/LED devices on a board |
| **CDC** | USB Communications Device Class — virtual serial port over USB |
| **BCD** | Binary-Coded Decimal — one decimal digit per nibble (4 bits) |
| **Detent** | A physical click position on a rotary encoder |
| **Hysteresis** | Minimum change threshold that must be exceeded before a new ADC event is emitted |
| **TM1639 / TM1637** | LED/display driver ICs from Titan Micro Electronics |
| **Annunciator** | A single indicator light on a cockpit panel |

---

**See also:** [ARCHITECTURE](ARCHITECTURE.md) · [COMMANDS](COMMANDS.md) · [OUTPUT](OUTPUT.md)
