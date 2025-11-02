#!/bin/bash
# Script to run cppcheck with MISRA C:2012 addon
# This script should be run from the project root directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Workspace for cppcheck artefacts (ignored by git)
OUTPUT_DIR="$PROJECT_ROOT/.cppcheck"
DUMP_DIR="$OUTPUT_DIR/dumps"
LOG_FILE="$OUTPUT_DIR/cppcheck.log"

mkdir -p "$DUMP_DIR"
: > "$LOG_FILE"

# Mirror console output to log file stored under .cppcheck/
exec > >(tee "$LOG_FILE") 2>&1

collect_dumps() {
    [ -d "$PROJECT_ROOT" ] || return 0

    local git_available=false
    if command -v git >/dev/null 2>&1 && git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git_available=true
    fi

    find "$PROJECT_ROOT" -type f -name '*.dump' \
        -not -path "$PROJECT_ROOT/.git/*" \
        -not -path "$DUMP_DIR/*" \
        -print0 | while IFS= read -r -d '' dumpfile; do
            rel_path="${dumpfile#$PROJECT_ROOT/}"

            if [ "$git_available" = true ] && git -C "$PROJECT_ROOT" ls-files --error-unmatch "$rel_path" >/dev/null 2>&1; then
                # Leave tracked dump artefacts (e.g. checked-in vendor files) untouched
                continue
            fi

            dest="$DUMP_DIR/$rel_path"
            mkdir -p "$(dirname "$dest")"
            mv -f "$dumpfile" "$dest" || true
        done
}

# Sweep any leftover dumps from previous runs before starting and ensure we
# always relocate new dumps into .cppcheck/dumps on exit.
collect_dumps
trap collect_dumps EXIT

echo "=== Signalbridge Controller - Cppcheck with MISRA C:2012 ==="
echo "Project root: $PROJECT_ROOT"

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    echo "❌ cppcheck is not installed or not in PATH"
    echo "In DevContainer: cppcheck should be pre-installed"
    echo "On host system: install with 'apt install cppcheck' or 'brew install cppcheck'"
    exit 1
fi

echo "✅ cppcheck version: $(cppcheck --version)"

# Check if MISRA addon is available
MISRA_ADDON_PATH=""
POSSIBLE_PATHS=(
    "/usr/share/cppcheck/addons/misra.py"
    "/usr/local/share/cppcheck/addons/misra.py"
    "/opt/homebrew/share/cppcheck/addons/misra.py"
    "$(dirname $(which cppcheck))/../share/cppcheck/addons/misra.py"
)

for path in "${POSSIBLE_PATHS[@]}"; do
    if [ -f "$path" ]; then
        MISRA_ADDON_PATH="$path"
        echo "✅ Found MISRA addon at: $MISRA_ADDON_PATH"
        break
    fi
done

if [ -z "$MISRA_ADDON_PATH" ]; then
    echo "⚠️  MISRA addon not found in standard locations."
    echo "💡 To install MISRA addon in DevContainer:"
    echo "   1. Rebuild DevContainer (runs .devcontainer/install_misra_addon.sh)"
    echo "   2. Or manually run: .devcontainer/install_misra_addon.sh"
    echo "   3. Or run: ./scripts/test_misra_addon.sh to verify installation"
    echo ""
    echo "🔍 Searching for any Python addons..."
    
    # Try to find any addon files
    ADDON_FILES=$(find /usr -name "*.py" -path "*/cppcheck/addons/*" 2>/dev/null | head -5)
    if [ -n "$ADDON_FILES" ]; then
        echo "Found cppcheck addon files:"
        echo "$ADDON_FILES"
    else
        echo "No cppcheck addon files found."
    fi
    
    ENABLE_MISRA=false
else
    ENABLE_MISRA=true
    
    # Verify the addon is executable
    if python3 "$MISRA_ADDON_PATH" --help >/dev/null 2>&1; then
        echo "✅ MISRA addon is executable and functional"
    else
        echo "⚠️  MISRA addon found but may have issues"
        echo "   Try running: python3 $MISRA_ADDON_PATH --help"
    fi
fi

# Verify configuration files exist
CONFIG_FILE="$PROJECT_ROOT/.cppcheck_config"
SUPPRESSIONS_FILE="$PROJECT_ROOT/.cppcheck_suppressions"
MISRA_CONFIG="$PROJECT_ROOT/misra.json"
MISRA_RULES="$PROJECT_ROOT/misra.txt"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Configuration file not found: $CONFIG_FILE"
    exit 1
fi

if [ ! -f "$SUPPRESSIONS_FILE" ]; then
    echo "❌ Suppressions file not found: $SUPPRESSIONS_FILE"
    exit 1
fi

echo "✅ Configuration file: $CONFIG_FILE"
echo "✅ Suppressions file: $SUPPRESSIONS_FILE"

# Check for compile_commands.json
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
USE_COMPILE_DB=false

if [ -f "$COMPILE_COMMANDS" ]; then
    echo "✅ Found compile_commands.json"
    USE_COMPILE_DB=true
else
    echo "⚠️  compile_commands.json not found, using manual configuration"
fi

# Build cppcheck command
CPPCHECK_CMD=(
    cppcheck
    --std=c11
    --enable=all
    --inline-suppr
    --force
    --verbose
    --check-level=exhaustive
    -j$(nproc)
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]'
    --suppressions-list="$SUPPRESSIONS_FILE"
)

# Add MISRA addon if available
if [ "$ENABLE_MISRA" = true ] && [ -f "$MISRA_CONFIG" ] && [ -f "$MISRA_RULES" ]; then
    echo "✅ MISRA configuration: $MISRA_CONFIG"
    echo "✅ MISRA rules text: $MISRA_RULES"
    CPPCHECK_CMD+=(--addon="$MISRA_CONFIG")
    # Enable dump for MISRA addon analysis
    CPPCHECK_CMD+=(--dump)
    echo "ℹ️  Dump files enabled for MISRA analysis"
else
    echo "⚠️  Running without MISRA addon (no dump files will be generated)"
    if [ ! -f "$MISRA_CONFIG" ]; then
        echo "   Missing: $MISRA_CONFIG"
    fi
    if [ ! -f "$MISRA_RULES" ]; then
        echo "   Missing: $MISRA_RULES"
    fi
fi

# Load manual configuration for defines and critical includes
echo "✅ Loading configuration from: $CONFIG_FILE"
while IFS= read -r line; do
    # Skip empty lines and comments
    if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*# ]]; then
        # Only add defines and include directives (not include paths, they're in compile_commands.json)
        if [[ "$line" =~ ^-D || "$line" =~ ^--include= ]]; then
            CPPCHECK_CMD+=("$line")
        fi
    fi
done < "$CONFIG_FILE"

# Use compile_commands.json if available, otherwise use manual config
if [ "$USE_COMPILE_DB" = true ]; then
    echo "✅ Using compile_commands.json for accurate compiler settings"
    CPPCHECK_CMD+=(--project="$COMPILE_COMMANDS")
    # Filter to only analyze project source files (not SDK/libraries)
    # CPPCHECK_CMD+=(--file-filter="$PROJECT_ROOT/src/*.c")

    echo ""
    echo "🚀 Running cppcheck analysis with compilation database..."
    echo "Cppcheck command: ${CPPCHECK_CMD[*]}"
    echo ""

    # Run cppcheck with compilation database and filter output
    CPPCHECK_OUTPUT=$(mktemp)
    "${CPPCHECK_CMD[@]}" > "$CPPCHECK_OUTPUT" 2>&1
    EXIT_CODE=$?

    # Display only errors from src/ and include/ folders (exclude lib/test/unit)
    grep -v -E "/(lib|test|unit)/" "$CPPCHECK_OUTPUT" || true
    rm -f "$CPPCHECK_OUTPUT"

    if [ $EXIT_CODE -eq 0 ]; then
        collect_dumps
        echo ""
        echo "✅ Cppcheck analysis completed successfully"
        exit 0
    else
        EXIT_CODE=$?
        collect_dumps
        echo ""
        echo "⚠️  Cppcheck analysis completed with warnings/errors (exit code: $EXIT_CODE)"
        echo ""
        echo "💡 Tips for resolving issues:"
        echo "   - Check .cppcheck_suppressions for known suppressions"
        echo "   - For MISRA violations, add appropriate suppressions with justification"
        echo "   - External library issues should be suppressed with DEVIATION (D5)"
        exit $EXIT_CODE
    fi
else
    # Fallback to manual configuration
    echo "✅ Loading manual configuration from: $CONFIG_FILE"
    # Read the config file and add each line as a separate argument
    while IFS= read -r line; do
        # Skip empty lines and comments
        if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*# ]]; then
            CPPCHECK_CMD+=("$line")
        fi
    done < "$CONFIG_FILE"

    # Provide include paths for the in-tree headers when the compilation
    # database is unavailable.  This keeps cppcheck focused on project sources
    # without requiring local stub copies of external SDKs.
    MANUAL_INCLUDE_PATHS=(
        "-I$PROJECT_ROOT/include"
        "-I$PROJECT_ROOT/lib/FreeRTOS-Kernel/include"
        "-I$PROJECT_ROOT/lib/FreeRTOS-Kernel/portable/GCC/ARM_CM0"
        "-I$PROJECT_ROOT/lib/pico-sdk/src/common/pico_base_headers/include"
        "-I$PROJECT_ROOT/lib/pico-sdk/src/rp2_common/hardware_gpio/include"
        "-I$PROJECT_ROOT/test/unit/mock_headers"
        "-I$PROJECT_ROOT/test/unit/mock_headers/hardware"
        "-I$PROJECT_ROOT/test/unit/mock_headers/pico"
    )
    CPPCHECK_CMD+=("${MANUAL_INCLUDE_PATHS[@]}")

    # When we cannot rely on the compile database, external SDK headers may
    # legitimately be unavailable.  Suppress the corresponding diagnostics so
    # that analysis of the project code can proceed without local stub copies.
    CPPCHECK_CMD+=(
        --suppress=missingInclude
        --suppress=missingIncludeSystem
    )

    # Add source files to analyze
    SOURCE_FILES=($(find src -name "*.c" -not -path "*/build/*" | sort))

    if [ ${#SOURCE_FILES[@]} -eq 0 ]; then
        echo "❌ No source files found in src/ directory"
        exit 1
    fi

    echo ""
    echo "📁 Source files to analyze:"
    for file in "${SOURCE_FILES[@]}"; do
        echo "   - $file"
    done

    echo ""
    echo "🚀 Running cppcheck analysis with manual configuration..."
    echo ""

    # Run cppcheck with manual config and filter output
    CPPCHECK_OUTPUT=$(mktemp)
    "${CPPCHECK_CMD[@]}" "${SOURCE_FILES[@]}" > "$CPPCHECK_OUTPUT" 2>&1
    EXIT_CODE=$?

    # Display only errors from src/ and include/ folders (exclude lib/test/unit)
    grep -v -E "/(lib|test|unit)/" "$CPPCHECK_OUTPUT" || true
    rm -f "$CPPCHECK_OUTPUT"

    if [ $EXIT_CODE -eq 0 ]; then
        collect_dumps
        echo ""
        echo "✅ Cppcheck analysis completed successfully"
        exit 0
    else
        collect_dumps
        echo ""
        echo "⚠️  Cppcheck analysis completed with warnings/errors (exit code: $EXIT_CODE)"
        echo ""
        echo "💡 Tips for resolving issues:"
        echo "   - Check .cppcheck_suppressions for known suppressions"
        echo "   - Verify include paths in .cppcheck_config"
        echo "   - For MISRA violations, add appropriate suppressions with justification"
        echo "   - External library issues should be suppressed with DEVIATION (D5)"
        exit $EXIT_CODE
    fi
fi
