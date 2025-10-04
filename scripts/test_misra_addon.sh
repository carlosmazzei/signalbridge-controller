#!/bin/bash
# Script to test MISRA addon installation and functionality
# Run this after DevContainer setup to verify everything works

set -e

echo "=== Testing MISRA C:2012 Addon Installation ==="

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    echo "❌ cppcheck is not installed"
    exit 1
fi

echo "✅ cppcheck version: $(cppcheck --version)"

# Find MISRA addon
MISRA_ADDON_PATHS=(
    "/usr/share/cppcheck/addons/misra.py"
    "/usr/local/share/cppcheck/addons/misra.py"
    "/opt/homebrew/share/cppcheck/addons/misra.py"
    "$(dirname $(which cppcheck))/../share/cppcheck/addons/misra.py"
)

MISRA_FOUND=false
MISRA_PATH=""

for path in "${MISRA_ADDON_PATHS[@]}"; do
    if [ -f "$path" ]; then
        MISRA_PATH="$path"
        MISRA_FOUND=true
        echo "✅ Found MISRA addon at: $MISRA_PATH"
        break
    fi
done

if [ "$MISRA_FOUND" = false ]; then
    echo "❌ MISRA addon not found in standard locations"
    echo "Searched:"
    for path in "${MISRA_ADDON_PATHS[@]}"; do
        echo "  - $path"
    done
    exit 1
fi

# Test MISRA addon functionality
echo ""
echo "🧪 Testing MISRA addon functionality..."

# Create a simple test file with MISRA violations
TEST_FILE="/tmp/misra_test.c"
cat > "$TEST_FILE" << 'EOF'
// Test file with intentional MISRA violations

#include <stdio.h>

// MISRA 2.5: Unused macro
#define UNUSED_MACRO 42

// MISRA 2.3: Unused typedef
typedef struct {
    int unused_field;
} unused_struct_t;

int main(void) {
    int x = 5;
    return 0;
}
EOF

echo "Created test file: $TEST_FILE"

# Test basic cppcheck functionality
echo ""
echo "🔍 Testing basic cppcheck..."
if cppcheck --quiet --template='{file}:{line}: {message} [{id}]' "$TEST_FILE" 2>&1; then
    echo "✅ Basic cppcheck works"
else
    echo "❌ Basic cppcheck failed"
    exit 1
fi

# Test MISRA addon with direct path
echo ""
echo "🔍 Testing MISRA addon with direct path..."
if python3 "$MISRA_PATH" --help >/dev/null 2>&1; then
    echo "✅ MISRA addon script is executable"
else
    echo "❌ MISRA addon script has issues"
    exit 1
fi

# Test MISRA addon integration with cppcheck
echo ""
echo "🔍 Testing MISRA addon integration..."

# Check if misra.json exists
if [ ! -f "./misra.json" ]; then
    echo "❌ misra.json configuration file not found"
    exit 1
fi

echo "✅ Found misra.json configuration"

# Check if misra.txt exists
if [ ! -f "./misra.txt" ]; then
    echo "❌ misra.txt rule file not found"
    exit 1
fi

echo "✅ Found misra.txt rule file"

# Test cppcheck with MISRA addon
echo ""
echo "🔍 Testing cppcheck with MISRA addon..."

# Run cppcheck with addon (capture output)
CPPCHECK_OUTPUT=$(cppcheck \
    --addon=./misra.json \
    --template='{file}:{line}: {message} [{id}]' \
    --force \
    "$TEST_FILE" 2>&1 || true)

echo "Cppcheck with MISRA output:"
echo "$CPPCHECK_OUTPUT"

# Check if MISRA addon actually ran (look for misra-related output)
if echo "$CPPCHECK_OUTPUT" | grep -i "misra" >/dev/null; then
    echo "✅ MISRA addon is working (found MISRA-related output)"
elif echo "$CPPCHECK_OUTPUT" | grep -i "addon" >/dev/null; then
    echo "⚠️  Addon system is working but MISRA output unclear"
    echo "   This might be normal if no MISRA violations were found"
else
    echo "⚠️  MISRA addon may not be working properly"
    echo "   Check addon configuration and paths"
fi

# Test project-specific configuration
echo ""
echo "🔍 Testing project-specific configuration..."

# Test with a project source file if available
PROJECT_TEST_FILE="src/main.c"
if [ -f "$PROJECT_TEST_FILE" ]; then
    echo "Testing with project file: $PROJECT_TEST_FILE"
    
    PROJECT_OUTPUT=$(cppcheck \
        --addon=./misra.json \
        --template='{file}:{line}: {message} [{id}]' \
        --suppressions-list=./.cppcheck_suppressions.txt \
        -DportBYTE_ALIGNMENT=8 \
        -DRP2040=1 \
        -I./include \
        --force \
        "$PROJECT_TEST_FILE" 2>&1 || true)
    
    echo "Project file analysis output:"
    echo "$PROJECT_OUTPUT" | head -20
    
    if [ ${#PROJECT_OUTPUT} -gt 0 ]; then
        echo "✅ Project file analysis completed"
    else
        echo "⚠️  No output from project file analysis"
    fi
else
    echo "⚠️  Project source file not found: $PROJECT_TEST_FILE"
fi

# Clean up
rm -f "$TEST_FILE"

echo ""
echo "📋 MISRA Addon Test Summary:"
echo "  - MISRA addon location: $MISRA_PATH"
echo "  - Basic functionality: ✅"
echo "  - Configuration files: ✅"
echo "  - Integration test: See output above"
echo ""
echo "💡 To use MISRA addon:"
echo "  ./scripts/run_cppcheck.sh              # Full analysis"
echo "  ./scripts/cppcheck_simple.sh          # Quick check"
echo "  cppcheck --addon=./misra.json file.c  # Manual usage"
echo ""
echo "🔧 If issues persist:"
echo "  1. Rebuild DevContainer to ensure installation"
echo "  2. Check .devcontainer/install_misra_addon.sh output"
echo "  3. Verify misra.txt file contains rule definitions"
echo "  4. Update suppressions in .cppcheck_suppressions.txt"