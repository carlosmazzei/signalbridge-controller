#!/bin/bash
# Script to run cppcheck with MISRA C:2012 addon
# Optimized for CI and VS Code visibility

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Workspace for cppcheck artefacts
OUTPUT_DIR="$PROJECT_ROOT/.cppcheck"
DUMP_DIR="$OUTPUT_DIR/dumps"
BUILD_DIR="$OUTPUT_DIR/build"
LOG_FILE="$OUTPUT_DIR/cppcheck.log"

mkdir -p "$DUMP_DIR"
mkdir -p "$BUILD_DIR" 
: > "$LOG_FILE"

# Mirror console output to log file stored under .cppcheck/
exec > >(tee "$LOG_FILE") 2>&1

collect_dumps() {
    [ -d "$PROJECT_ROOT/src" ] || return 0

    local git_available=false
    if command -v git >/dev/null 2>&1 && git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git_available=true
    fi

    find "$PROJECT_ROOT/src" -type f \( -name '*.dump' -o -name '*.ctu-info' \) \
        -print0 | while IFS= read -r -d '' dumpfile; do
            rel_path="${dumpfile#$PROJECT_ROOT/}"

            if [ "$git_available" = true ] && git -C "$PROJECT_ROOT" ls-files --error-unmatch "$rel_path" >/dev/null 2>&1; then
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

# Check dependencies
if ! command -v cppcheck &> /dev/null; then
    echo "❌ cppcheck is not installed or not in PATH"
    exit 1
fi
echo "✅ cppcheck version: $(cppcheck --version)"

# Check MISRA addon
MISRA_ADDON_PATH=""
POSSIBLE_PATHS=(
    "/usr/share/cppcheck/addons/misra.py"
    "/usr/local/share/cppcheck/addons/misra.py"
    "/opt/homebrew/share/cppcheck/addons/misra.py"
    "$(dirname "$(which cppcheck)")/../share/cppcheck/addons/misra.py"
)

# Debian/Ubuntu package the addons under the multiarch libdir rather than
# /usr/share, so glob those in too before giving up.
while IFS= read -r path; do
    [ -n "$path" ] && POSSIBLE_PATHS+=("$path")
done < <(ls -1 /usr/lib/*/cppcheck/addons/misra.py \
                /usr/local/lib/*/cppcheck/addons/misra.py 2>/dev/null || true)

for path in "${POSSIBLE_PATHS[@]}"; do
    if [ -f "$path" ]; then
        MISRA_ADDON_PATH="$path"
        echo "✅ Found MISRA addon at: $MISRA_ADDON_PATH"
        break
    fi
done

if [ -z "$MISRA_ADDON_PATH" ]; then
    echo "⚠️  MISRA addon not found. Running standard checks only."
    ENABLE_MISRA=false
    exit 1 # Exit if MISRA addon is mandatory
else
    ENABLE_MISRA=true
fi

# Config Files
CONFIG_FILE="$PROJECT_ROOT/.cppcheck_config"
SUPPRESSIONS_FILE="$PROJECT_ROOT/.cppcheck_suppressions"
MISRA_CONFIG="$PROJECT_ROOT/misra.json"
MISRA_RULES="$PROJECT_ROOT/misra.txt"

[ ! -f "$CONFIG_FILE" ] && { echo "❌ Missing config: $CONFIG_FILE"; exit 1; }
[ ! -f "$SUPPRESSIONS_FILE" ] && { echo "❌ Missing suppressions: $SUPPRESSIONS_FILE"; exit 1; }

echo "✅ Configuration loaded"

# Build Base Command
CPPCHECK_CMD=(
    cppcheck
    --error-exitcode=2
    --inline-suppr
    --exitcode-suppressions="$PROJECT_ROOT/.cppcheck_advisory"
    --include="$(arm-none-eabi-gcc -print-file-name=include)/stdatomic.h"
    --std=c11
    --enable=all
    --platform=unix32
    --check-level=exhaustive
    -j$(nproc)
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]'
    --suppressions-list="$SUPPRESSIONS_FILE"
    --cppcheck-build-dir="$BUILD_DIR"
    --checkers-report="$OUTPUT_DIR/checkers_report.txt"
)

if [ "$ENABLE_MISRA" = true ] && [ -f "$MISRA_CONFIG" ] && [ -f "$MISRA_RULES" ]; then
    CPPCHECK_CMD+=(--addon="$MISRA_CONFIG")
    CPPCHECK_CMD+=(--dump)
    echo "ℹ️  MISRA enabled"
fi

# Rule 8.7 deviation D2: cross-translation-unit findings ignore inline
# suppressions, so suppress the declaration that follows each marker comment.
while IFS=: read -r marker_file marker_line _; do
    CPPCHECK_CMD+=("--suppress=misra-c2012-8.7:*${marker_file}:$((marker_line + 1))")
done < <(cd "$PROJECT_ROOT" && grep -rn "MISRA Deviation D2 (Rule 8.7)" src)

# Load defines from config
while IFS= read -r line; do
    if [[ -n "$line" && ! "$line" =~ ^[[:space:]]*# ]]; then
        if [[ "$line" =~ ^-D || "$line" =~ ^--include= ]]; then
            CPPCHECK_CMD+=("$line")
        fi
    fi
done < "$CONFIG_FILE"

COMPILE_COMMANDS="${CPPCHECK_COMPILE_COMMANDS:-$PROJECT_ROOT/build-release/compile_commands.json}"

if [ -f "$COMPILE_COMMANDS" ]; then
    echo "✅ Using compile_commands.json"
    python3 "$SCRIPT_DIR/prepare_cppcheck.py" "$COMPILE_COMMANDS" "$OUTPUT_DIR/compile_commands.json"
    CPPCHECK_CMD+=(--project="$OUTPUT_DIR/compile_commands.json")
    CPPCHECK_CMD+=(--file-filter="$PROJECT_ROOT/src/*")
    CPPCHECK_CMD+=(-I"$PROJECT_ROOT/include")
    CPPCHECK_CMD+=('-Dbi_decl(x)=')

    echo "🚀 Executing Cppcheck..."
    echo "CMD: ${CPPCHECK_CMD[*]}"
    echo "---------------------------------------------------"

    # Execute DIRECTLY to stdout/stderr
    EXIT_CODE=0
    "${CPPCHECK_CMD[@]}" || EXIT_CODE=$?

    collect_dumps
    if [ $EXIT_CODE -ne 0 ]; then
        echo "---------------------------------------------------"
        echo "❌ Cppcheck failed with errors (Exit Code: $EXIT_CODE)"
        exit $EXIT_CODE
    fi
else
    echo "❌  compile_commands.json not found. Please generate it using your build system."
    exit 1
fi

echo ""
echo "✅ Cppcheck passed successfully"
exit 0
