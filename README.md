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
