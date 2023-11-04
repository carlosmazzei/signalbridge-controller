# Raspberry Pi Pico FreeRTOS SMP Controller

This controller uses Raspberry Pi Pico and the SMP (Symmetric Multi Processor) version of the FreeRTOS.

In order to compile with VSCode, you need first to set the environment variables in the `settings.json` file (example provided below. Substitute [INSTALLATION FOLDER] with the correct path):

```json
{
    "cmake.environment": {
        "PICO_SDK_PATH":"/[INSTALLATION FOLDER]/pico/pico-sdk",
        "FREERTOS_KERNEL_PATH":"/[INSTALLATION FOLDER]/pico/FreeRTOS-SMP-Demos/FreeRTOS/Source",
        "PICO_TINYUSB_PATH":"/[INSTALLATION FOLDER]/pico/pico-sdk/lib/tinyusb"
    },
}
```

Then, config the CMake Extensions to use Unix Makefiles

*CMake:Generator*: Unix Makefiles

And to build it, run:

```bash
mkdir build
cd build
cmake ..
cd ..
make
```

## FreeRTOS SMP Usage

In order to use the FreeRTOS you need to include the `/FreeRTOS/FreeRTOSConfig.h` file and import the cmake file `include(FreeRTOS_Kernel_import.cmake)` in the top CMakeLists.txt config. After that you can import the desired files and use FreeRTOS directly.

## Documentation

Doxygen documentation is generated automatically from the code comments. Doxygen must be installed in order to generate the documentation correclty. Feel free to change the settings in the `/docs/Doxyfile` to customize documentation output.

## Features

### Idle Hook

The idle task runs at the very lowest priority, so such an idle hook function will only get executed when there are no tasks of higher priority that are able to run. This makes the idle hook function an ideal place to put the processor into a low power state - providing an automatic power saving whenever there is no processing to be performed.
The idle hook will only get called if configUSE_IDLE_HOOK is set to 1 within FreeRTOSConfig.h. When this is set the application must provide the hook function with the following prototype:

```c
void vApplicationIdleHook( void );
```

The idle hook is called repeatedly as long as the idle task is running. It is paramount that the idle hook function does not call any API functions that could cause it to block. Also, if the application makes use of the vTaskDelete() API function then the idle task hook must be allowed to periodically return (this is because the idle task is responsible for cleaning up the resources that were allocated by the RTOS kernel to the task that has been deleted).

During the idle hook the application will get the free heap size.
