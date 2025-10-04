#!/bin/bash
# Simplified cppcheck command to fix the specific errors mentioned
# Run this from the project root directory

echo "=== Running cppcheck with fixed configuration ==="

# Set proper working directory
cd "$(dirname "$0")/.." || exit 1
PROJECT_ROOT="$(pwd)"

echo "Project root: $PROJECT_ROOT"

# Run cppcheck with fixed configuration
cppcheck \
    --std=c11 \
    --enable=all \
    --force \
    --verbose \
    --template='{file}:{line}: {severity}: {message} [{id}]' \
    --suppressions-list=./.cppcheck_suppressions.txt \
    --addon=./misra.json \
    -DportBYTE_ALIGNMENT=8 \
    -DRP2040=1 \
    -D__ARM_ARCH_6M__=1 \
    -DconfigUSE_16_BIT_TICKS=0 \
    -DconfigNUM_CORES=2 \
    -DconfigTOTAL_HEAP_SIZE=131072 \
    -DconfigMAX_PRIORITIES=10 \
    -DconfigMINIMAL_STACK_SIZE=128 \
    -DconfigTICK_RATE_HZ=1000 \
    -I./include \
    -I./lib/pico-sdk/src/common/pico_stdlib/include \
    -I./lib/pico-sdk/src/rp2_common/hardware_gpio/include \
    -I./lib/pico-sdk/src/rp2040/pico_platform/include \
    -I./lib/pico-sdk/src/common/pico_base/include \
    -I./lib/FreeRTOS-Kernel/include \
    -I./lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040 \
    --force-include=./include/FreeRTOSConfig.h \
    ./src/main.c \
    ./src/app_comm.c \
    ./src/app_context.c \
    ./src/app_setup.c \
    ./src/app_tasks.c \
    ./src/error_management.c \
    ./src/hooks.c \
    ./src/inputs.c \
    ./src/outputs.c \
    ./src/tm1637.c \
    ./src/tm1639.c \
    ./src/cobs.c

echo ""
echo "Cppcheck analysis completed"