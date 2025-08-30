#!/bin/bash
# manual_memory_analysis.sh
# Safe manual memory analysis script (no problematic cmake variables)

set -e  # Exit on error

## \brief Path to ELF file to analyze (default: build/src/pi_controller.elf)
## \details You can override by passing as first argument.
ELF_FILE="${1:-build/src/pi_controller.elf}"

## \brief Path to output memory report file (default: build/memory_analysis_report.txt)
## \details You can override by passing as second argument.
REPORT_FILE="${2:-build/memory_analysis_report.txt}"

echo "=== MEMORY ANALYSIS SCRIPT ==="
echo "Analyzing: $ELF_FILE"
echo "Report: $REPORT_FILE"
echo ""

# Check if ELF file exists
if [ ! -f "$ELF_FILE" ]; then
    echo "❌ Error: $ELF_FILE not found!"
    echo "Make sure you've built the project first: make"
    exit 1
fi

# Memory limits - Raspberry Pi Pico (RP2040) specifications
# Raspberry Pi Pico RP2040:
FLASH_SIZE_KB=2048    # 2MB Flash memory
RAM_SIZE_KB=264       # 264KB SRAM (256KB + 8KB cache as RAM)

# Convert to bytes for calculations
FLASH_SIZE_BYTES=$((FLASH_SIZE_KB * 1024))
RAM_SIZE_BYTES=$((RAM_SIZE_KB * 1024))

echo "📊 Memory limits configured:"
echo "   Flash: ${FLASH_SIZE_KB}KB (${FLASH_SIZE_BYTES} bytes)"
echo "   RAM:   ${RAM_SIZE_KB}KB (${RAM_SIZE_BYTES} bytes)"
echo ""

# Start report
cat > "$REPORT_FILE" << EOF
=== MEMORY ANALYSIS REPORT ===
Generated: $(date)
File: $ELF_FILE
File size: $(ls -la "$ELF_FILE" | awk '{print $5}') bytes

MEMORY LIMITS CONFIGURED:
Flash: ${FLASH_SIZE_KB}KB (${FLASH_SIZE_BYTES} bytes)
RAM:   ${RAM_SIZE_KB}KB (${RAM_SIZE_BYTES} bytes)

EOF

# 1. Basic size information with percentage calculations
echo "📊 Getting basic memory usage..."
echo "=== BASIC MEMORY USAGE WITH PERCENTAGES ===" >> "$REPORT_FILE"

# Get memory usage using size command
if command -v arm-none-eabi-size >/dev/null 2>&1; then
    SIZE_OUTPUT=$(arm-none-eabi-size "$ELF_FILE" 2>/dev/null)
    if [ $? -eq 0 ]; then
        echo "$SIZE_OUTPUT" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # Parse the size output to extract text, data, bss values
        # Skip header line and get the data line
        SIZE_LINE=$(echo "$SIZE_OUTPUT" | tail -n 1)
        TEXT_SIZE=$(echo "$SIZE_LINE" | awk '{print $1}')
        DATA_SIZE=$(echo "$SIZE_LINE" | awk '{print $2}')
        BSS_SIZE=$(echo "$SIZE_LINE" | awk '{print $3}')
        
        # Calculate totals
        FLASH_USED=$TEXT_SIZE  # Flash = text section
        RAM_USED=$((DATA_SIZE + BSS_SIZE))  # RAM = data + bss
        
        # Calculate percentages
        if [ "$FLASH_USED" -gt 0 ] && [ "$FLASH_SIZE_BYTES" -gt 0 ]; then
            FLASH_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($FLASH_USED/$FLASH_SIZE_BYTES)*100}")
        else
            FLASH_PERCENT="N/A"
        fi
        
        if [ "$RAM_USED" -gt 0 ] && [ "$RAM_SIZE_BYTES" -gt 0 ]; then
            RAM_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($RAM_USED/$RAM_SIZE_BYTES)*100}")
        else
            RAM_PERCENT="N/A"
        fi
        
        # Calculate remaining memory
        FLASH_FREE=$((FLASH_SIZE_BYTES - FLASH_USED))
        RAM_FREE=$((RAM_SIZE_BYTES - RAM_USED))
        
        # Display detailed breakdown
        echo "=== MEMORY USAGE BREAKDOWN ===" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        printf "%-15s %10s %10s %8s %10s\n" "MEMORY TYPE" "USED" "TOTAL" "PERCENT" "FREE" >> "$REPORT_FILE"
        printf "%-15s %10s %10s %8s %10s\n" "---------------" "----------" "----------" "--------" "----------" >> "$REPORT_FILE"
        printf "%-15s %10d %10d %7s%% %10d\n" "Flash (text)" "$FLASH_USED" "$FLASH_SIZE_BYTES" "$FLASH_PERCENT" "$FLASH_FREE" >> "$REPORT_FILE"
        printf "%-15s %10d %10d %7s%% %10d\n" "RAM (data+bss)" "$RAM_USED" "$RAM_SIZE_BYTES" "$RAM_PERCENT" "$RAM_FREE" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # Add visual indicators
        echo "=== MEMORY STATUS INDICATORS ===" >> "$REPORT_FILE"
        
        # Flash status
        if [ "$FLASH_PERCENT" != "N/A" ]; then
            FLASH_NUM=$(echo "$FLASH_PERCENT" | cut -d'.' -f1)
            if [ "$FLASH_NUM" -gt 90 ]; then
                FLASH_STATUS="🚨 CRITICAL"
            elif [ "$FLASH_NUM" -gt 75 ]; then
                FLASH_STATUS="⚠️  WARNING"
            elif [ "$FLASH_NUM" -gt 50 ]; then
                FLASH_STATUS="📊 MODERATE"
            else
                FLASH_STATUS="✅ GOOD"
            fi
            echo "Flash Usage: ${FLASH_PERCENT}% - $FLASH_STATUS" >> "$REPORT_FILE"
        fi
        
        # RAM status
        if [ "$RAM_PERCENT" != "N/A" ]; then
            RAM_NUM=$(echo "$RAM_PERCENT" | cut -d'.' -f1)
            if [ "$RAM_NUM" -gt 90 ]; then
                RAM_STATUS="🚨 CRITICAL"
            elif [ "$RAM_NUM" -gt 75 ]; then
                RAM_STATUS="⚠️  WARNING"
            elif [ "$RAM_NUM" -gt 50 ]; then
                RAM_STATUS="📊 MODERATE"
            else
                RAM_STATUS="✅ GOOD"
            fi
            echo "RAM Usage: ${RAM_PERCENT}% - $RAM_STATUS" >> "$REPORT_FILE"
        fi
        echo "" >> "$REPORT_FILE"
        
        # Section breakdown
        echo "=== SECTION BREAKDOWN ===" >> "$REPORT_FILE"
        if [ "$TEXT_SIZE" -gt 0 ]; then
            TEXT_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($TEXT_SIZE/$FLASH_SIZE_BYTES)*100}")
            echo "text (code):           $TEXT_SIZE bytes (${TEXT_PERCENT}% of Flash)" >> "$REPORT_FILE"
        fi
        if [ "$DATA_SIZE" -gt 0 ]; then
            DATA_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($DATA_SIZE/$RAM_SIZE_BYTES)*100}")
            echo "data (init vars):      $DATA_SIZE bytes (${DATA_PERCENT}% of RAM)" >> "$REPORT_FILE"
        fi
        if [ "$BSS_SIZE" -gt 0 ]; then
            BSS_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($BSS_SIZE/$RAM_SIZE_BYTES)*100}")
            echo "bss (uninit vars):     $BSS_SIZE bytes (${BSS_PERCENT}% of RAM)" >> "$REPORT_FILE"
        fi
        
    else
        echo "arm-none-eabi-size failed" >> "$REPORT_FILE"
    fi
elif command -v size >/dev/null 2>&1; then
    size "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || echo "size command failed" >> "$REPORT_FILE"
    echo "Note: Install arm-none-eabi-gcc for detailed percentage analysis" >> "$REPORT_FILE"
else
    echo "No size command available" >> "$REPORT_FILE"
    echo "Install arm-none-eabi-gcc for memory analysis" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 2. Detailed size information (try different formats)
echo "📊 Getting detailed memory usage..."
echo "=== DETAILED MEMORY USAGE ===" >> "$REPORT_FILE"
if command -v arm-none-eabi-size >/dev/null 2>&1; then
    # Try different format options
    arm-none-eabi-size -A "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || \
    arm-none-eabi-size -B "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || \
    arm-none-eabi-size "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || \
    echo "All size formats failed" >> "$REPORT_FILE"
else
    echo "arm-none-eabi-size not available" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 3. Section headers
echo "📊 Getting section information..."
echo "=== SECTION INFORMATION ===" >> "$REPORT_FILE"
if command -v arm-none-eabi-objdump >/dev/null 2>&1; then
    arm-none-eabi-objdump -h "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || echo "objdump failed" >> "$REPORT_FILE"
elif command -v objdump >/dev/null 2>&1; then
    objdump -h "$ELF_FILE" >> "$REPORT_FILE" 2>/dev/null || echo "objdump failed" >> "$REPORT_FILE"
else
    echo "No objdump command available" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 4. Symbol analysis
echo "📊 Getting symbol information..."
echo "=== LARGEST SYMBOLS ===" >> "$REPORT_FILE"
if command -v arm-none-eabi-nm >/dev/null 2>&1; then
    echo "Top 20 largest symbols:" >> "$REPORT_FILE"
    arm-none-eabi-nm --print-size --size-sort --radix=d "$ELF_FILE" 2>/dev/null | tail -20 >> "$REPORT_FILE" || echo "Symbol analysis failed" >> "$REPORT_FILE"
elif command -v nm >/dev/null 2>&1; then
    echo "Using system nm (limited functionality):" >> "$REPORT_FILE"
    nm "$ELF_FILE" 2>/dev/null | head -20 >> "$REPORT_FILE" || echo "nm failed" >> "$REPORT_FILE"
else
    echo "No nm command available" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 5. Stack usage analysis - ENHANCED FORMATTING
echo "📊 Getting stack usage information..."
echo "=== STACK USAGE ANALYSIS ===" >> "$REPORT_FILE"

# Find all .su files
SU_FILES=$(find build -name "*.su" -type f 2>/dev/null)
if [ -n "$SU_FILES" ]; then
    STACK_COUNT=$(echo "$SU_FILES" | wc -l | tr -d ' ')
else
    STACK_COUNT=0
fi

if [ "$STACK_COUNT" -gt 0 ]; then
    echo "Found $STACK_COUNT stack usage files" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    
    # Create temporary file for processing
    TEMP_DATA=$(mktemp)
    
    # Extract and format data from all .su files
    for file in $SU_FILES; do
        if [ -s "$file" ]; then  # Check if file is not empty
            while IFS= read -r line; do
                # Parse format: file:line:column:function_name bytes type
                if echo "$line" | grep -qE '^.+:[0-9]+:[0-9]+:.+[[:space:]]+[0-9]+[[:space:]]+.+'; then
                    # Extract components using awk for better compatibility
                    FILE_PATH=$(echo "$line" | awk -F: '{print $1}')
                    LINE_NUM=$(echo "$line" | awk -F: '{print $2}')
                    FUNCTION=$(echo "$line" | awk '{print $(NF-2)}')
                    BYTES=$(echo "$line" | awk '{print $(NF-1)}')
                    TYPE=$(echo "$line" | awk '{print $NF}')
                    
                    # Extract just filename from full path
                    FILENAME=$(basename "$FILE_PATH")
                    
                    # Format: BYTES|FUNCTION|FILENAME:LINE|TYPE
                    echo "$BYTES|$FUNCTION|$FILENAME:$LINE_NUM|$TYPE" >> "$TEMP_DATA"
                fi
            done < "$file"
        fi
    done
    
    # Sort by bytes (descending) and format nicely
    if [ -s "$TEMP_DATA" ]; then
        echo "=== TOP STACK USERS (sorted by bytes) ===" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # Header
        printf "%-8s %-30s %-25s %-10s\n" "BYTES" "FUNCTION" "LOCATION" "TYPE" >> "$REPORT_FILE"
        printf "%-8s %-30s %-25s %-10s\n" "--------" "------------------------------" "-------------------------" "----------" >> "$REPORT_FILE"
        
        # Sort by bytes (first column, numeric, reverse) and show top 20
        sort -t'|' -k1,1nr "$TEMP_DATA" | head -20 | while IFS='|' read -r bytes function location type; do
            printf "%-8s %-30s %-25s %-10s\n" "$bytes" "$function" "$location" "$type" >> "$REPORT_FILE"
        done
        
        echo "" >> "$REPORT_FILE"
        
        # Analysis sections
        echo "=== ANALYSIS BY CATEGORY ===" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # High usage functions (>200 bytes)
        echo "🚨 HIGH STACK USAGE (>200 bytes):" >> "$REPORT_FILE"
        HIGH_COUNT=0
        sort -t'|' -k1,1nr "$TEMP_DATA" | while IFS='|' read -r bytes function location type; do
            if [ "$bytes" -gt 200 ]; then
                printf "   %s bytes - %s (%s)\n" "$bytes" "$function" "$location" >> "$REPORT_FILE"
                HIGH_COUNT=$((HIGH_COUNT + 1))
            fi
        done
        if [ "$HIGH_COUNT" -eq 0 ]; then
            echo "   None found" >> "$REPORT_FILE"
        fi
        echo "" >> "$REPORT_FILE"
        
        # Medium usage functions (100-200 bytes)
        echo "⚠️  MEDIUM STACK USAGE (100-200 bytes):" >> "$REPORT_FILE"
        MEDIUM_COUNT=0
        sort -t'|' -k1,1nr "$TEMP_DATA" | while IFS='|' read -r bytes function location type; do
            if [ "$bytes" -gt 100 ] && [ "$bytes" -le 200 ]; then
                printf "   %s bytes - %s (%s)\n" "$bytes" "$function" "$location" >> "$REPORT_FILE"
                MEDIUM_COUNT=$((MEDIUM_COUNT + 1))
                # Limit output to avoid very long lists
                if [ "$MEDIUM_COUNT" -ge 10 ]; then
                    echo "   ... (showing top 10 only)" >> "$REPORT_FILE"
                    break
                fi
            fi
        done
        if [ "$MEDIUM_COUNT" -eq 0 ]; then
            echo "   None found" >> "$REPORT_FILE"
        fi
        echo "" >> "$REPORT_FILE"
        
        # Your application functions only
        echo "🎯 YOUR APPLICATION FUNCTIONS:" >> "$REPORT_FILE"
        APP_COUNT=0
        sort -t'|' -k1,1nr "$TEMP_DATA" | while IFS='|' read -r bytes function location type; do
            # Filter for your source files (not library files)
            case "$location" in
                *"main.c"* | *"inputs.c"* | *"outputs.c"* | *"tm1639.c"* | *"cobs.c"* | *"hooks.c"* | *"led_debug.c"*)
                    if [ "$bytes" -gt 200 ]; then
                        RISK="🚨"
                    elif [ "$bytes" -gt 100 ]; then
                        RISK="⚠️ "
                    else
                        RISK="✅"
                    fi
                    printf "   %s %s bytes - %s (%s)\n" "$RISK" "$bytes" "$function" "$location" >> "$REPORT_FILE"
                    APP_COUNT=$((APP_COUNT + 1))
                    ;;
            esac
        done
        if [ "$APP_COUNT" -eq 0 ]; then
            echo "   No application functions found in analysis" >> "$REPORT_FILE"
        fi
        echo "" >> "$REPORT_FILE"
        
        # Statistics
        echo "=== STATISTICS ===" >> "$REPORT_FILE"
        TOTAL_FUNCTIONS=$(wc -l < "$TEMP_DATA" 2>/dev/null || echo "0")
        MAX_BYTES=$(sort -t'|' -k1,1nr "$TEMP_DATA" 2>/dev/null | head -1 | cut -d'|' -f1 2>/dev/null || echo "0")
        MAX_FUNCTION=$(sort -t'|' -k1,1nr "$TEMP_DATA" 2>/dev/null | head -1 | cut -d'|' -f2 2>/dev/null || echo "unknown")
        HIGH_USAGE=$(sort -t'|' -k1,1nr "$TEMP_DATA" 2>/dev/null | awk -F'|' '$1 > 200' | wc -l 2>/dev/null || echo "0")
        MEDIUM_USAGE=$(sort -t'|' -k1,1nr "$TEMP_DATA" 2>/dev/null | awk -F'|' '$1 > 100 && $1 <= 200' | wc -l 2>/dev/null || echo "0")
        LOW_USAGE=$(sort -t'|' -k1,1nr "$TEMP_DATA" 2>/dev/null | awk -F'|' '$1 <= 100' | wc -l 2>/dev/null || echo "0")
        
        echo "Total functions analyzed: $TOTAL_FUNCTIONS" >> "$REPORT_FILE"
        echo "Highest stack usage: $MAX_BYTES bytes ($MAX_FUNCTION)" >> "$REPORT_FILE"
        echo "Functions with high usage (>200 bytes): $HIGH_USAGE" >> "$REPORT_FILE"
        echo "Functions with medium usage (100-200 bytes): $MEDIUM_USAGE" >> "$REPORT_FILE"
        echo "Functions with low usage (≤100 bytes): $LOW_USAGE" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # Task stack analysis
        echo "📏 TASK STACK IMPACT ANALYSIS:" >> "$REPORT_FILE"
        echo "Assuming 4KB (4096 bytes) task stacks:" >> "$REPORT_FILE"
        sort -t'|' -k1,1nr "$TEMP_DATA" | head -5 | while IFS='|' read -r bytes function location type; do
            if [ "$bytes" -gt 0 ]; then
                PERCENTAGE=$(awk "BEGIN {printf \"%.1f\", ($bytes/4096)*100}")
                if awk "BEGIN {exit !($bytes > 300)}"; then
                    IMPACT="HIGH"
                elif awk "BEGIN {exit !($bytes > 150)}"; then
                    IMPACT="MEDIUM"  
                else
                    IMPACT="LOW"
                fi
                echo "   $function: $bytes bytes = ${PERCENTAGE}% of task stack [$IMPACT]" >> "$REPORT_FILE"
            fi
        done
        echo "" >> "$REPORT_FILE"
        
        # Recommendations
        echo "=== OPTIMIZATION RECOMMENDATIONS ===" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        
        # Check for high usage functions
        TOP_OFFENDER=$(sort -t'|' -k1,1nr "$TEMP_DATA" | head -1 | cut -d'|' -f2 2>/dev/null)
        TOP_BYTES=$(sort -t'|' -k1,1nr "$TEMP_DATA" | head -1 | cut -d'|' -f1 2>/dev/null)

        # Only compare if TOP_BYTES is a valid integer
        if [[ "$TOP_BYTES" =~ ^[0-9]+$ ]] && [ "$TOP_BYTES" -gt 200 ]; then
            echo "🎯 IMMEDIATE ACTION NEEDED:" >> "$REPORT_FILE"
            echo "   Function '$TOP_OFFENDER' uses $TOP_BYTES bytes" >> "$REPORT_FILE"
            echo "   Recommendations:" >> "$REPORT_FILE"
            echo "   - Move large local arrays/structs to global scope" >> "$REPORT_FILE"
            echo "   - Use heap allocation (malloc/pvPortMalloc) for large buffers" >> "$REPORT_FILE"
            echo "   - Check for unnecessary large local variables" >> "$REPORT_FILE"
            echo "" >> "$REPORT_FILE"
        fi
        
        echo "📋 GENERAL OPTIMIZATION TIPS:" >> "$REPORT_FILE"
        echo "1. Functions >200 bytes: Move large variables to global/heap" >> "$REPORT_FILE"
        echo "2. Functions >100 bytes: Review local variable usage" >> "$REPORT_FILE"
        echo "3. Stack usage is cumulative (function + called functions)" >> "$REPORT_FILE"
        echo "4. Monitor call chains for deep recursion" >> "$REPORT_FILE"
        echo "5. Use static allocation for known-size buffers" >> "$REPORT_FILE"
        echo "6. Consider increasing task stack sizes if needed" >> "$REPORT_FILE"
        
        # Cleanup
        rm -f "$TEMP_DATA"
        
    else
        echo "No stack usage data could be parsed from .su files" >> "$REPORT_FILE"
    fi
    
else
    echo "No stack usage files found (.su files)" >> "$REPORT_FILE"
    echo "To generate stack usage data:" >> "$REPORT_FILE"
    echo "1. Add '-fstack-usage' to compiler flags in CMakeLists.txt" >> "$REPORT_FILE"
    echo "2. Rebuild project: make clean && make" >> "$REPORT_FILE"
    echo "3. Re-run this analysis" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 6. Memory map (if exists)
echo "📊 Checking for memory map..."
echo "=== MEMORY MAP INFORMATION ===" >> "$REPORT_FILE"
MAP_FILE="build/src/pi_controller.map"
if [ -f "$MAP_FILE" ]; then
    echo "Memory map file found: $MAP_FILE" >> "$REPORT_FILE"
    echo "Map file size: $(ls -la "$MAP_FILE" | awk '{print $5}') bytes" >> "$REPORT_FILE"
    echo "To view full map: less $MAP_FILE" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    echo "Memory Configuration (excerpt):" >> "$REPORT_FILE"
    head -50 "$MAP_FILE" >> "$REPORT_FILE" 2>/dev/null || echo "Could not read map file" >> "$REPORT_FILE"
else
    echo "No memory map file found" >> "$REPORT_FILE"
    echo "To generate: add 'LINKER:-Map=pi_controller.map' to linker options" >> "$REPORT_FILE"
fi
echo "" >> "$REPORT_FILE"

# 7. Summary and recommendations
echo "📊 Generating summary..."
cat >> "$REPORT_FILE" << EOF
=== ANALYSIS SUMMARY ===

MEMORY USAGE INTERPRETATION:
- text: Program code (Flash memory)
- data: Initialized variables (RAM)
- bss:  Uninitialized variables (RAM)
- Total Flash = text section
- Total RAM = data + bss sections

MEMORY PERCENTAGE THRESHOLDS:
✅ GOOD:     0-50% usage
📊 MODERATE: 51-75% usage  
⚠️  WARNING: 76-90% usage
🚨 CRITICAL: 91-100% usage

RECOMMENDATIONS:
1. Monitor stack usage files (.su) for functions >256 bytes
2. Check symbol sizes for unexpectedly large functions/variables
3. Review memory map for layout optimization
4. If Flash >75%: Consider code optimization or larger MCU
5. If RAM >75%: Review global variables and buffer sizes
6. Use 'arm-none-eabi-objdump -d' for disassembly if needed

MEMORY LIMIT CONFIGURATION:
Current limits: Flash=${FLASH_SIZE_KB}KB, RAM=${RAM_SIZE_KB}KB
To update: Edit FLASH_SIZE_KB and RAM_SIZE_KB variables in this script

COMMANDS FOR FURTHER ANALYSIS:
- arm-none-eabi-size $ELF_FILE
- arm-none-eabi-objdump -h $ELF_FILE
- arm-none-eabi-nm --print-size --size-sort $ELF_FILE
- less $MAP_FILE

=== END OF REPORT ===
EOF

echo "✅ Analysis complete!"
echo "📄 Report saved to: $REPORT_FILE"
echo ""
echo "📊 Quick summary:"
if command -v arm-none-eabi-size >/dev/null 2>&1; then
    SIZE_OUTPUT=$(arm-none-eabi-size "$ELF_FILE" 2>/dev/null)
    if [ $? -eq 0 ]; then
        echo "$SIZE_OUTPUT"
        
        # Parse and show percentages in terminal
        SIZE_LINE=$(echo "$SIZE_OUTPUT" | tail -n 1)
        TEXT_SIZE=$(echo "$SIZE_LINE" | awk '{print $1}')
        DATA_SIZE=$(echo "$SIZE_LINE" | awk '{print $2}')
        BSS_SIZE=$(echo "$SIZE_LINE" | awk '{print $3}')
        
        FLASH_USED=$TEXT_SIZE
        RAM_USED=$((DATA_SIZE + BSS_SIZE))
        
        if [ "$FLASH_USED" -gt 0 ] && [ "$FLASH_SIZE_BYTES" -gt 0 ]; then
            FLASH_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($FLASH_USED/$FLASH_SIZE_BYTES)*100}")
            echo "💾 Flash: $FLASH_USED bytes (${FLASH_PERCENT}% of ${FLASH_SIZE_KB}KB)"
        fi
        
        if [ "$RAM_USED" -gt 0 ] && [ "$RAM_SIZE_BYTES" -gt 0 ]; then
            RAM_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($RAM_USED/$RAM_SIZE_BYTES)*100}")
            echo "🧠 RAM:   $RAM_USED bytes (${RAM_PERCENT}% of ${RAM_SIZE_KB}KB)"
        fi
    else
        echo "Could not get size info"
    fi
else
    echo "Install arm-none-eabi-gcc for detailed analysis"
fi

echo ""
echo "🔍 To view full report:"
echo "  cat $REPORT_FILE"
echo "  less $REPORT_FILE"