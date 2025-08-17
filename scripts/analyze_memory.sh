#!/bin/bash

# Memory Placement Analyzer for Raspberry Pi Pico
# Usage: ./analyze_memory.sh <elf_file> <variable_name>

ELF_FILE=$1
VAR_NAME=$2

if [ $# -ne 2 ]; then
    echo "Usage: $0 <elf_file> <variable_name>"
    exit 1
fi

echo "=== Memory Placement Analysis for '$VAR_NAME' ==="
echo

# Extract memory regions from linker script
echo "1. Memory Regions (from ELF headers):"
arm-none-eabi-readelf -S "$ELF_FILE" | grep -E "PROGBITS|NOBITS" | awk '{printf "%-20s Start: 0x%s Size: 0x%s\n", $2, $4, $6}'
echo

# Find the variable's address and section
echo "2. Variable '$VAR_NAME' Information:"
VAR_INFO=$(arm-none-eabi-nm -S "$ELF_FILE" | grep -w "$VAR_NAME")
if [ -z "$VAR_INFO" ]; then
    echo "ERROR: Variable '$VAR_NAME' not found!"
    exit 1
fi

VAR_ADDR=$(echo "$VAR_INFO" | awk '{print $1}')
VAR_SIZE=$(echo "$VAR_INFO" | awk '{print $2}')
VAR_TYPE=$(echo "$VAR_INFO" | awk '{print $3}')

echo "   Address: 0x$VAR_ADDR"
echo "   Size: 0x$VAR_SIZE bytes"
echo "   Type: $VAR_TYPE"

# Determine which section contains the variable
SECTION=$(arm-none-eabi-objdump -t "$ELF_FILE" | grep -w "$VAR_NAME" | awk '{print $4}')
echo "   Section: $SECTION"
echo

# Check memory boundaries
echo "3. Memory Boundary Analysis:"

# Extract RAM boundaries (typical for RP2040)
RAM_START="0x20000000"
RAM_END="0x20042000"  # 264KB RAM
FLASH_START="0x10000000"
FLASH_END="0x10200000"  # 2MB Flash

VAR_ADDR_DEC=$((0x$VAR_ADDR))
VAR_END_DEC=$((0x$VAR_ADDR + 0x$VAR_SIZE))

# Check if in RAM or Flash
if [ $VAR_ADDR_DEC -ge $((0x20000000)) ] && [ $VAR_ADDR_DEC -lt $((0x20042000)) ]; then
    echo "   Location: RAM"
    REMAINING_RAM=$((0x20042000 - VAR_END_DEC))
    echo "   Distance to RAM end: $REMAINING_RAM bytes"
elif [ $VAR_ADDR_DEC -ge $((0x10000000)) ] && [ $VAR_ADDR_DEC -lt $((0x10200000)) ]; then
    echo "   Location: FLASH (XIP)"
    echo "   WARNING: Volatile variable in FLASH!"
else
    echo "   Location: UNKNOWN - Potential problem!"
fi
echo

# Check for nearby symbols that might conflict
echo "4. Nearby Symbols (within 1KB):"
RANGE_START=$(printf "%08x" $((VAR_ADDR_DEC - 512)))
RANGE_END=$(printf "%08x" $((VAR_ADDR_DEC + 512)))

arm-none-eabi-nm -n "$ELF_FILE" | awk -v start="0x$RANGE_START" -v end="0x$RANGE_END" -v var="$VAR_NAME" \
    'BEGIN {start_dec = strtonum(start); end_dec = strtonum(end)} 
     {addr = strtonum("0x" $1)} 
     addr >= start_dec && addr <= end_dec && $3 != var {
         printf "   0x%s %s (distance: %d bytes)\n", $1, $3, addr - strtonum("0x" var_addr)
     }'

echo
echo "5. Critical Memory Regions Check:"

# Get heap and stack info
HEAP_START=$(arm-none-eabi-nm "$ELF_FILE" | grep -w "_heap_start" | awk '{print $1}')
HEAP_END=$(arm-none-eabi-nm "$ELF_FILE" | grep -w "_heap_end" | awk '{print $1}')
STACK_TOP=$(arm-none-eabi-nm "$ELF_FILE" | grep -w "_stack_top" | awk '{print $1}')
STACK_BOTTOM=$(arm-none-eabi-nm "$ELF_FILE" | grep -w "_stack_bottom" | awk '{print $1}')

echo "   Heap:  0x$HEAP_START - 0x$HEAP_END"
echo "   Stack: 0x$STACK_BOTTOM - 0x$STACK_TOP"

# Check for conflicts
if [ ! -z "$HEAP_START" ]; then
    HEAP_START_DEC=$((0x$HEAP_START))
    if [ $VAR_ADDR_DEC -ge $HEAP_START_DEC ] && [ $VAR_ADDR_DEC -lt $((0x$HEAP_END)) ]; then
        echo "   ERROR: Variable overlaps with HEAP!"
    fi
fi

echo
echo "6. FreeRTOS Memory Usage:"
# Check for FreeRTOS heap
FREERTOS_HEAP=$(arm-none-eabi-nm "$ELF_FILE" | grep -E "ucHeap|xHeap" | head -1)
if [ ! -z "$FREERTOS_HEAP" ]; then
    echo "   FreeRTOS Heap: $FREERTOS_HEAP"
fi

# Check for multicore sections
echo
echo "7. Multicore (SMP) Analysis:"
CORE0_SECTIONS=$(arm-none-eabi-objdump -h "$ELF_FILE" | grep -i "core0" || echo "   No core0-specific sections found")
CORE1_SECTIONS=$(arm-none-eabi-objdump -h "$ELF_FILE" | grep -i "core1" || echo "   No core1-specific sections found")
echo "   Core0 sections: $CORE0_SECTIONS"
echo "   Core1 sections: $CORE1_SECTIONS"

# Generate report
echo
echo "=== POTENTIAL PROBLEMS ==="

# Check volatile in flash
if [ $VAR_ADDR_DEC -ge $((0x10000000)) ] && [ $VAR_ADDR_DEC -lt $((0x10200000)) ]; then
    echo "❌ CRITICAL: Volatile variable in FLASH/XIP region!"
fi

# Check alignment for SMP
ALIGNMENT=$((VAR_ADDR_DEC % 4))
if [ $ALIGNMENT -ne 0 ]; then
    echo "⚠️  WARNING: Variable not 4-byte aligned (required for atomic operations)"
fi

# Check proximity to stack
if [ ! -z "$STACK_BOTTOM" ]; then
    STACK_DISTANCE=$((VAR_ADDR_DEC - 0x$STACK_BOTTOM))
    if [ $STACK_DISTANCE -ge 0 ] && [ $STACK_DISTANCE -lt 4096 ]; then
        echo "⚠️  WARNING: Variable very close to stack region ($STACK_DISTANCE bytes)"
    fi
fi

# Check .bss vs .data
if [[ "$SECTION" == ".bss" ]] && [ "$VAR_TYPE" == "D" ]; then
    echo "⚠️  WARNING: Initialized variable in .bss section (should be in .data)"
fi

echo
echo "=== RECOMMENDATIONS ==="
echo "1. For volatile variables in SMP, ensure:"
echo "   - Placed in RAM (0x20000000-0x20042000)"
echo "   - 4-byte aligned for atomic access"
echo "   - Not in cacheable regions"
echo "   - Sufficient distance from heap/stack"
echo
echo "2. Add to linker script if needed:"
echo "   .data : {"
echo "       . = ALIGN(4);"
echo "       *(.volatile_vars)"
echo "   } > RAM"