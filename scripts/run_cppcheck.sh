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
    [ -d "$PROJECT_ROOT/src" ] || return 0
    find "$PROJECT_ROOT/src" -type f -name '*.dump' \
        -not -path "$DUMP_DIR/*" \
        -print0 | while IFS= read -r -d '' dumpfile; do
            rel_path="${dumpfile#$PROJECT_ROOT/}"
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
SUPPRESSIONS_FILE="$PROJECT_ROOT/.cppcheck_suppressions.txt"
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

# Build cppcheck command
CPPCHECK_CMD=(
    cppcheck
    --std=c11
    --enable=all
    --inline-suppr
    --force
    --verbose
    --template='{file}:{line}: {severity}: {message} [{id}]'
    --suppressions-list="$SUPPRESSIONS_FILE"
)

# Add MISRA addon if available
if [ "$ENABLE_MISRA" = true ] && [ -f "$MISRA_CONFIG" ] && [ -f "$MISRA_RULES" ]; then
    echo "✅ MISRA configuration: $MISRA_CONFIG"
    echo "✅ MISRA rules text: $MISRA_RULES"
    CPPCHECK_CMD+=(--addon="$MISRA_CONFIG")
    # Enable dump for MISRA addon analysis
    CPPCHECK_CMD+=(--dump)
else
    echo "⚠️  Running without MISRA addon"
    if [ ! -f "$MISRA_CONFIG" ]; then
        echo "   Missing: $MISRA_CONFIG"
    fi
    if [ ! -f "$MISRA_RULES" ]; then
        echo "   Missing: $MISRA_RULES"
    fi
fi

# Add include paths and defines from config file
if [ -f "$CONFIG_FILE" ]; then
    echo "✅ Loading configuration from: $CONFIG_FILE"
    # Read the config file and add each line as a separate argument
    while IFS= read -r line; do
        # Skip empty lines and comments
        if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*# ]]; then
            CPPCHECK_CMD+=("$line")
        fi
    done < "$CONFIG_FILE"
fi

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
echo "🚀 Running cppcheck analysis..."
echo ""

# Run cppcheck with error handling
if "${CPPCHECK_CMD[@]}" "${SOURCE_FILES[@]}" 2>&1; then
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
    echo "   - Check .cppcheck_suppressions.txt for known suppressions"
    echo "   - Verify include paths in .cppcheck_config"
    echo "   - For MISRA violations, add appropriate suppressions with justification"
    echo "   - External library issues should be suppressed with DEVIATION (D5)"
    exit $EXIT_CODE
fi
