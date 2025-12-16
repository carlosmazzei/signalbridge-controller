#!/bin/bash
# Simplified cppcheck for quick analysis without MISRA
# Uses compile_commands.json when available
# Run this from the project root directory

echo "=== Running simplified cppcheck analysis ==="

# Set proper working directory
cd "$(dirname "$0")/.." || exit 1
PROJECT_ROOT="$(pwd)"

echo "Project root: $PROJECT_ROOT"

# Load configuration file for defines
CONFIG_FILE="$PROJECT_ROOT/.cppcheck_config"
CPPCHECK_ARGS=()

if [ -f "$CONFIG_FILE" ]; then
    echo "✅ Loading configuration from: $CONFIG_FILE"
    while IFS= read -r line; do
        # Skip empty lines and comments
        if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*# ]]; then
            # Only add defines and include directives
            if [[ "$line" =~ ^-D || "$line" =~ ^--include= ]]; then
                CPPCHECK_ARGS+=("$line")
            fi
        fi
    done < "$CONFIG_FILE"
fi

# Check for compile_commands.json
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"

if [ -f "$COMPILE_COMMANDS" ]; then
    echo "✅ Using compile_commands.json for accurate compiler settings"

    # Run cppcheck with compilation database (no MISRA, no dump files)
    cppcheck \
        --std=c11 \
        --enable=warning,style,performance,portability \
        --inline-suppr \
        --verbose \
        --check-level=exhaustive \
        -j$(nproc) \
        --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
        --suppressions-list=./.cppcheck_suppressions \
        "${CPPCHECK_ARGS[@]}" \
        --project="$COMPILE_COMMANDS" \
        --file-filter="$PROJECT_ROOT/src/*.c"
else
    echo "⚠️  compile_commands.json not found, using basic configuration"

    # Fallback to basic manual configuration (minimal includes)
    cppcheck \
        --std=c11 \
        --enable=warning,style,performance,portability \
        --inline-suppr \
        --verbose \
        --check-level=exhaustive \
        -j$(nproc) \
        --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
        --suppressions-list=./.cppcheck_suppressions \
        "${CPPCHECK_ARGS[@]}" \
        -I./include \
        -I./lib/pico-sdk/src/common/pico_base_headers/include \
        -I./lib/FreeRTOS-Kernel/include \
        ./src/*.c
fi

echo ""
echo "✅ Quick cppcheck analysis completed (no MISRA, no dump files)"