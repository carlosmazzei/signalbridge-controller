# Raspberry Pi Pico FreeRTOS SMP Controller

This controller uses Raspberry Pi Pico and the SMP (Symmetric Multi Processor) version of the FreeRTOS.

In order to compile with VSCode, you need first to set the environment variables in the `settings.json` file:

```json
{
    "cmake.environment": {
        "PICO_SDK_PATH":"/Users/carlosmazzei/pico/pico-sdk",
        "FREERTOS_KERNEL_PATH":"/Users/carlosmazzei/pico/FreeRTOS-SMP-Demos/FreeRTOS/Source",
        "PICO_TINYUSB_PATH":"/Users/carlosmazzei/pico/pico-sdk/lib/tinyusb"
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
