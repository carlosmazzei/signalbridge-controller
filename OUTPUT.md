# Output Device Architecture Documentation

## System Overview

This section describes the architecture of the output device control system. The architecture is designed to manage multiple output devices (such as LEDs and digital displays) through a unified interface while allowing for hardware-specific implementations. The system specifically uses SPI bus communication with multiplexed chip selection for efficient hardware control.

## Driver types

These are the driver types supported by the system:

- `DEVICE_NONE`: No device connected
- `DEVICE_GENERIC_LED`: Generic LED device
- `DEVICE_GENERIC_DIGIT`: Generic digit display
- `DEVICE_TM1639_LED`: TM1639-based LED device
- `DEVICE_TM1639_DIGIT`: TM1639-based digit display

The device configuration is defined at compile time and determines which driver is instantiated for each controller slot.

## Architecture Diagram

```mermaid
graph LR
    PAYLOAD["Payload <br/> - DIGITS <br/> - LEDs"] -->|input| MP
    
    subgraph OUTPUTS["OUTPUTS.C"]
        MP["Output <br/> Controller"]
        subgraph CONTROLLERS["Controller Blocks"]
            Comp1["Driver Handler"]
            Comp2["Driver Handler"]
            Comp3["Driver Handler"]
            Comp4["Driver Handler"]
            Comp5["Driver Handler"]
            Comp6["Driver Handler"]
            Comp7["Driver Handler"]
            Comp8["Driver Handler"]
        end
        MP --- Comp1
        MP --- Comp2
        MP --- Comp3
        MP --- Comp4
        MP --- Comp5
        MP --- Comp6
        MP --- Comp7
        MP --- Comp8
        MUX["MUX control"]
    end
    
    MUX --> MUTEX["MUTEX"]
    MUTEX --> HWLAYER["Hardware Layer <br/> Concurrency <br/> Control"]
    
    subgraph DRIVERS["Drivers (LED or DIGIT)"]
        subgraph LED1["LED Driver"]
            BUFF1["ID <br/> BUFFER"]
            INIT1["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph TM1["TM1639 Driver"]
            BUFF2["ID <br/> BUFFER"]
            INIT2["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph LED2["LED Driver"]
            BUFF3["ID <br/> BUFFER"]
            INIT3["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph TM2["TM1639 Driver"]
            BUFF4["ID <br/> BUFFER"]
            INIT4["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph LED3["LED Driver"]
            BUFF5["ID <br/> BUFFER"]
            INIT5["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph TM3["TM1639 Driver"]
            BUFF6["ID <br/> BUFFER"]
            INIT6["init <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph LED4["LED Driver"]
            BUFF7["ID <br/> BUFFER"]
            INIT7["init  <br/> disp <br/> led <br/> cs-select"]
        end
        
        subgraph TM4["TM1639 Driver"]
            BUFF8["ID <br/> BUFFER"]
            INIT8["init <br/> disp <br/> led <br/> cs-select"]
        end
    end
    
    Comp1 -.->|handler references| LED1
    Comp2 -.->|handler references| TM1
    Comp3 -.->|handler references| LED2
    Comp4 -.->|handler references| TM2
    Comp5 -.->|handler references| LED3
    Comp6 -.->|handler references| TM3
    Comp7 -.->|handler references| LED4
    Comp8 -.->|handler references| TM4
```

## Component Descriptions

### Payload

The payload contains the output data to be displayed, primarily consisting of:

- **DIGITS**: Packed BCD data for 7-segment displays (8 digits per controller).
- **LEDs**: Index and state for LED outputs.

### OUTPUTS.C

This is the central coordination module of the architecture, containing:

1. **Output Controller**: The multiplexer and input processing unit that handles incoming payload data.

2. **Controller Blocks**: Up to 8 controller blocks, each containing a driver handler. These blocks are responsible for directing data to the appropriate driver. Each controller can only reference one driver at a time.

3. **MUX control**: Manages the physical multiplexer on the SPI bus that selects the appropriate chip select line. This component directly controls which physical device on the SPI bus is active at any given time.

4. **MUTEX**: A mutex layer that provides concurrency control for the SPI bus access, ensuring that only one controller can access the bus at a time.

### Driver Handlers

Each controller block contains a driver handler which:

- References exactly one driver instance
- Routes data from the controller to the specific driver
- Translates generic commands into driver-specific operations

### Hardware Layer Concurrency Control with MUTEX

The mutex layer provides hardware-level concurrency control for the SPI bus access. It ensures that:

- Only one controller can access the SPI bus at a time
- Chip select operations are atomic and cannot be interrupted
- The multiplexer state cannot be changed while a transaction is in progress

This component provides lower-level hardware access control mechanisms, ensuring that hardware resources (particularly the SPI bus and multiplexer) are accessed safely and efficiently.

### Drivers

The architecture supports multiple driver types, including:

1. **LED Drivers**: For controlling LED displays
2. **TM1639 Drivers**: A specific type of driver for digit displays using the TM1639 chipset

Each driver contains:

- **ID BUFFER**: Contains:
  - The controller ID that corresponds to the physical chip select pin number
  - An active buffer with current display state
  - A temporary buffer to prepare updates without causing display flickering

- **Initialization Handlers**: Functions including:
  - init: Initialize the driver and configure its operating parameters
  - disp: Display control functions
  - led: LED control functions
  - cs-select: Pointer to chip select function for implementing device-specific protocols

#### TM1639 Driver

- Controls TM1639-based 7-segment and LED displays.
- Implements double buffering (active and preparation buffers) to prevent flicker.
- Provides methods for initialization, display updates, brightness, and key scanning.
- Uses a function pointer for chip select via the multiplexer.

## Driver Implementation Details

Each driver must implement the following:

1. **digit_out Method**: This method receives a payload and processes it according to the specific protocol required by the hardware device. The method signature typically looks like:

    ```c
    void digit_out(driver_t* driver, payload_t* payload);
    ```

2. **Double Buffering**: Drivers maintain two buffers:
   - Active Buffer: Currently displayed on the hardware
   - Temporary Buffer: Used to prepare the next display state

   This prevents flickering during updates, as the temporary buffer is prepared completely before being atomically swapped with the active buffer.  
3. **Chip Select Mechanism**: Each driver contains a pointer to a chip select function that it can call to implement its specific communication protocol:

    ```c
    typedef void (*cs_select_fn)(uint8_t id, bool state);
    ```

    The driver calls this function to control its chip select line (identified by its ID) during SPI transactions.

## SPI Bus and Multiplexer Control

The SPI bus in this architecture is shared among all output devices, with chip selection managed through a multiplexer:

1. When a controller needs to communicate with its driver:
   - It acquires the MUTEX to gain exclusive access to the SPI bus
   - The MUX control sets the multiplexer to select the appropriate chip select line
   - The driver executes its protocol using the chip select function
   - The MUTEX is released when the transaction is complete

2. The multiplexer allows the system to control up to 8 different chip select lines using fewer GPIO pins, making efficient use of limited microcontroller resources.

## Key Architecture Principles

1. **One-to-One Relationship**: Each controller block references exactly one driver instance, maintaining a clean separation of concerns.

2. **Driver Abstraction**: The architecture allows different driver implementations (LED, TM1639, etc.) to be used interchangeably from the controller's perspective.

3. **Concurrency Control**: The MUTEX and hardware layer concurrency control ensure safe resource sharing on the SPI bus.

4. **Double Buffering**: Prevents display flickering by preparing complete frames before displaying them.

5. **Multiplexed Hardware Access**: Uses a hardware multiplexer to expand the number of controllable devices beyond the available GPIO pins.

6. **Protocol Encapsulation**: Each driver implements its own protocol through the digit_out method, keeping hardware-specific details isolated from the rest of the system.

## SPI Bus and Multiplexer Control

- The SPI bus is shared among all output devices.
- The 74HC138 multiplexer selects which device is active via three GPIO pins (MUX_A_PIN, MUX_B_PIN, MUX_C_PIN) and an enable pin (MUX_ENABLE).
- The `select_interface` function sets the multiplexer state for each transaction.
- The `cs_select` function is called by drivers to set the chip select line for their specific device.
- The FreeRTOS mutex (`spi_mutex`) ensures only one transaction occurs at a time.

## Double Buffering

- Each driver maintains an active buffer (current display state) and a preparation buffer (next state).
- Updates are staged in the prep buffer and atomically swapped to the active buffer to prevent flicker.

## Error Handling and Statistics

- All functions validate parameters and update error counters on failure.
- Error codes are defined for initialization, display output, invalid parameters, and semaphore failures.
- The `out_statistics_counters` structure tracks error occurrences for diagnostics.

## Usage Flow

1. **Initialization**: `output_init()` sets up all hardware and drivers.
2. **Payload Reception**: Payloads for digits or LEDs are received by the controller.
3. **Routing**: The controller block identifies the correct driver based on the controller ID.
4. **Mutex Acquisition**: The SPI mutex is taken to ensure exclusive access.
5. **Multiplexer Selection**: The correct chip is selected via the multiplexer.
6. **Driver Call**: The driver's `set_digits` or `set_leds` function is called.
7. **Buffer Update**: Data is written to the preparation buffer, then flushed to the device.
8. **Mutex Release**: The SPI mutex is released, allowing other transactions.

---

## Example: Sending Digits to a Display

```c
uint8_t payload[6] = {
    1,      // Controller ID (1-based)
    0x12,   // Digits 1 and 2 (packed BCD)
    0x34,   // Digits 3 and 4
    0x56,   // Digits 5 and 6
    0x78,   // Digits 7 and 8
    2       // Dot position
};
display_out(payload, 6);
```

---

## Design Principles

- **One-to-One Mapping**: Each controller block references exactly one driver.
- **Driver Abstraction**: Controllers use function pointers for device-agnostic output.
- **Concurrency Control**: Mutexes and atomic chip select ensure safe SPI sharing.
- **Double Buffering**: Prevents flicker and ensures atomic display updates.
- **Multiplexed Access**: Efficiently expands the number of controllable devices.
- **Error Tracking**: Robust error handling and statistics for diagnostics.
- **Protocol Encapsulation**: Each driver implements its own communication protocol, isolating hardware-specific logic.

## Extensibility

- The architecture allows for easy addition of new driver types.
- Generic driver support can be implemented by extending the driver initialization and handler logic.
- The system is designed for portability and maintainability, with clear separation between hardware abstraction, driver logic, and application interface.

## References

- [Raspberry Pi Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [TM1639 Datasheet](https://datasheetspdf.com/pdf-file/1404122/ETC/TM1639/1)
- [FreeRTOS Documentation](https://www.freertos.org/)

This architecture provides a flexible framework for controlling various output devices while maintaining clean interfaces, proper resource management, and efficient hardware utilization.
