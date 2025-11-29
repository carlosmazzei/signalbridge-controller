#!/bin/bash

# Post-creation script for project-specific setup
# This runs after the container is created and the workspace is mounted

set -e

echo "=== Post-Creation Setup ==="

# Navigate to workspace regardless of runtime (VS Code devcontainer or CI)
WORKSPACE_DIR=""
if [ -n "${CONTAINERWORKSPACEFOLDER:-}" ] && [ -d "${CONTAINERWORKSPACEFOLDER}" ]; then
    WORKSPACE_DIR="${CONTAINERWORKSPACEFOLDER}"
else
    CURRENT_DIR="$(pwd)"
    DEFAULT_WORKSPACE="/workspaces/$(basename "$CURRENT_DIR")"
    if [ -d "$DEFAULT_WORKSPACE" ]; then
        WORKSPACE_DIR="$DEFAULT_WORKSPACE"
    else
        WORKSPACE_DIR="$CURRENT_DIR"
    fi
fi

cd "$WORKSPACE_DIR"

# Ensure downstream commands that rely on CONTAINERWORKSPACEFOLDER keep working
export CONTAINERWORKSPACEFOLDER="${CONTAINERWORKSPACEFOLDER:-$WORKSPACE_DIR}"

# GitHub-hosted runners mount /workspace with root ownership, so mark it safe for git
if [ -d ".git" ]; then
    if ! git config --global --get-all safe.directory | grep -Fx "$WORKSPACE_DIR" >/dev/null 2>&1; then
        git config --global --add safe.directory "$WORKSPACE_DIR"
    fi
fi

# Check if submodules are properly initialized
if [ -f ".gitmodules" ]; then
    echo "Checking submodule initialization..."
    
    # Check if Pico SDK submodule is empty
    if [ ! -f "lib/pico-sdk/CMakeLists.txt" ]; then
        echo "Pico SDK submodule appears empty, initializing..."
        git submodule update --init --recursive lib/pico-sdk
    fi
    
    # Check if FreeRTOS submodule is empty
    if [ ! -f "lib/FreeRTOS-Kernel/CMakeLists.txt" ]; then
        echo "FreeRTOS submodule appears empty, initializing..."
        git submodule update --init --recursive lib/FreeRTOS-Kernel
    fi
    
    # Ensure all submodules are up to date
    echo "Ensuring all submodules are up to date..."
    git submodule update --recursive
    
    echo "✅ Submodules are properly initialized"
else
    echo "⚠️  No .gitmodules found, skipping submodule initialization"
fi

# Verify Pico SDK path
if [ -d "lib/pico-sdk" ]; then
    export PICO_SDK_PATH="$(pwd)/lib/pico-sdk"
    echo "Pico SDK found at: $PICO_SDK_PATH"
else
    echo "Warning: Pico SDK not found at lib/pico-sdk"
    echo "Make sure to clone with --recurse-submodules or run:"
    echo "git submodule update --init --recursive"
fi

# Verify FreeRTOS path
if [ -d "lib/FreeRTOS-Kernel" ]; then
    export FREERTOS_KERNEL_PATH="$(pwd)/lib/FreeRTOS-Kernel"
    echo "FreeRTOS found at: $FREERTOS_KERNEL_PATH"
else
    echo "Warning: FreeRTOS not found at lib/FreeRTOS-Kernel"
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Install MISRA addon for cppcheck
echo "Installing MISRA C:2012 addon for cppcheck..."
if [ -f ".devcontainer/install_misra_addon.sh" ]; then
    chmod +x .devcontainer/install_misra_addon.sh
    .devcontainer/install_misra_addon.sh
else
    echo "⚠️  MISRA addon installation script not found"
fi

# Verify VS Code configuration files
echo "Verifying VS Code configuration..."

if [ -f ".vscode/settings.json" ]; then
    echo "✅ VS Code settings.json found"
else
    echo "⚠️  VS Code settings.json not found"
fi

if [ -f ".vscode/c_cpp_properties.json" ]; then
    echo "✅ C/C++ properties configuration found"
else
    echo "⚠️  C/C++ properties configuration not found"
fi

if [ -f ".vscode/tasks.json" ]; then
    echo "✅ VS Code tasks configuration found"
else
    echo "⚠️  VS Code tasks configuration not found"
fi

if [ -f "uncrustify.cfg" ]; then
    echo "✅ Uncrustify configuration found"
else
    echo "⚠️  Uncrustify configuration not found - code formatting may not work"
fi

# Set up environment variables in .bashrc for persistent access
echo "Setting up environment variables..."
cat >> ~/.bashrc << 'EOF'

# Raspberry Pi Pico FreeRTOS Development Environment
export PICO_SDK_PATH="${CONTAINERWORKSPACEFOLDER}/lib/pico-sdk"
export FREERTOS_KERNEL_PATH="${CONTAINERWORKSPACEFOLDER}/lib/FreeRTOS"
export PICO_TOOLCHAIN_PATH="/usr/bin"

# Add ARM toolchain to PATH (if not already there)
if [[ ":$PATH:" != *":/usr/bin:"* ]]; then
    export PATH="/usr/bin:$PATH"
fi

# Useful aliases for development
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'
alias pico-build='cd build && cmake .. && make'
alias pico-clean='rm -rf build && mkdir build'
alias pico-flash='echo "Copy the .uf2 file from build/ to your Pico when in BOOTSEL mode"'
alias pico-format='find src include -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" | xargs uncrustify -c uncrustify.cfg --replace --no-backup'

EOF

# Source the updated .bashrc
source ~/.bashrc

# Verify toolchain
echo ""
echo "=== Toolchain Verification ==="
if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "✅ ARM GCC: $(arm-none-eabi-gcc --version | head -n1)"
else
    echo "❌ ARM GCC not found"
fi

if command -v cmake >/dev/null 2>&1; then
    echo "✅ CMake: $(cmake --version | head -n1)"
else
    echo "❌ CMake not found"
fi

if command -v uncrustify >/dev/null 2>&1; then
    echo "✅ Uncrustify: $(uncrustify --version)"
else
    echo "❌ Uncrustify not found"
fi

if command -v doxygen >/dev/null 2>&1; then
    echo "✅ Doxygen: $(doxygen --version)"
else
    echo "❌ Doxygen not found"
fi

# Display useful information
echo ""
echo "=== Environment Variables (docker container) ==="
if [ -n "${CI:-}" ]; then
    echo "(environment redacted in CI environment)"
else
    env | sort
fi
echo ""
echo "=== Development Environment Ready ==="
echo "Workspace: $(pwd)"
echo "VS Code Extensions: Automatically installed via devcontainer"
echo ""
echo "=== Configuration Files ==="
echo "📁 .devcontainer/devcontainer.json - Container setup and extensions"
echo "📁 .vscode/settings.json - Workspace settings (CMake, editor, extensions)"
echo "📁 .vscode/c_cpp_properties.json - C/C++ IntelliSense configuration"
echo "📁 .vscode/tasks.json - Build and formatting tasks"
echo "📁 .vscode/launch.json - Debug configurations"
echo "📁 uncrustify.cfg - Code formatting rules"
echo ""
echo "=== Quick Start ==="
echo "1. VS Code will automatically configure itself with the proper settings"
echo "2. Use Ctrl+Shift+P → 'Tasks: Run Task' for various build and format operations"
echo "3. Create build: mkdir -p build && cd build"
echo "4. Configure: cmake .."
echo "5. Build: make"
echo "6. Flash: Copy .uf2 file to Pico in BOOTSEL mode"
echo ""
echo "=== VS Code Tasks Available ==="
echo "• Initialize Submodules - Setup git submodules"
echo "• Build Project - Full CMake + make build"
echo "• Clean Build - Remove and rebuild"
echo "• uncrustify - Format current file"
echo "• uncrustify all C/C++ files - Format entire project"
echo "• Generate Documentation - Create Doxygen docs"
echo ""
echo "=== Useful Commands ==="
echo "pico-build    - Quick build (cmake + make)"
echo "pico-clean    - Clean build directory"
echo "pico-format   - Format all source files with uncrustify"
echo "pico-flash    - Instructions for flashing"
echo ""
echo "=== File Structure ==="
echo "• devcontainer.json: Container setup only"
echo "• settings.json: Workspace behavior (non-C/C++ specific)"
echo "• c_cpp_properties.json: C/C++ language server configuration"
echo "• No conflicting settings between files!"
echo ""
echo "Happy coding! 🚀"
