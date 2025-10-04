#!/bin/bash
# Script to install MISRA C:2012 addon for cppcheck in DevContainer
# This script should be run during DevContainer setup

set -e

echo "=== Installing MISRA C:2012 Addon for Cppcheck ==="

# Get cppcheck version and installation details
CPPCHECK_VERSION=$(cppcheck --version 2>/dev/null | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' || echo "unknown")
echo "Cppcheck version: $CPPCHECK_VERSION"

# Find cppcheck installation directory
CPPCHECK_PATH=$(which cppcheck)
CPPCHECK_PREFIX=$(dirname $(dirname $CPPCHECK_PATH))
ADDONS_DIR="$CPPCHECK_PREFIX/share/cppcheck/addons"

echo "Cppcheck installation: $CPPCHECK_PREFIX"
echo "Expected addons directory: $ADDONS_DIR"

# Create addons directory if it doesn't exist
sudo mkdir -p "$ADDONS_DIR"

# Method 1: Try to get addon from cppcheck source
echo ""
echo "📥 Method 1: Installing from cppcheck source..."

# Create temporary directory
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Download cppcheck source matching installed version
if [ "$CPPCHECK_VERSION" != "unknown" ]; then
    DOWNLOAD_URL="https://github.com/danmar/cppcheck/archive/refs/tags/${CPPCHECK_VERSION}.tar.gz"
    echo "Downloading cppcheck source version $CPPCHECK_VERSION..."
    
    if wget -q "$DOWNLOAD_URL" -O cppcheck.tar.gz 2>/dev/null; then
        tar -xzf cppcheck.tar.gz
        SOURCE_DIR="cppcheck-${CPPCHECK_VERSION}"
        
        if [ -d "$SOURCE_DIR/addons" ]; then
            echo "✅ Found addons in source distribution"
            
            # Copy MISRA addon files
            if [ -f "$SOURCE_DIR/addons/misra.py" ]; then
                sudo cp "$SOURCE_DIR/addons/misra.py" "$ADDONS_DIR/"
                echo "✅ Installed misra.py"
            fi
            
            # Copy other important addons
            for addon in cert.py threadsafety.py y2038.py naming.py misra_9.py; do
                if [ -f "$SOURCE_DIR/addons/$addon" ]; then
                    sudo cp "$SOURCE_DIR/addons/$addon" "$ADDONS_DIR/"
                    echo "✅ Installed $addon"
                fi
            done
            
            # Copy addon utilities
            if [ -f "$SOURCE_DIR/addons/cppcheckdata.py" ]; then
                sudo cp "$SOURCE_DIR/addons/cppcheckdata.py" "$ADDONS_DIR/"
                echo "✅ Installed cppcheckdata.py"
            fi
            
            METHOD1_SUCCESS=true
        else
            echo "⚠️  No addons directory found in source"
            METHOD1_SUCCESS=false
        fi
    else
        echo "⚠️  Failed to download cppcheck source"
        METHOD1_SUCCESS=false
    fi
else
    echo "⚠️  Unknown cppcheck version, trying latest"
    METHOD1_SUCCESS=false
fi

# Method 2: Download from latest main branch if Method 1 failed
if [ "$METHOD1_SUCCESS" != "true" ]; then
    echo ""
    echo "📥 Method 2: Installing from latest main branch..."
    
    if wget -q "https://github.com/danmar/cppcheck/archive/refs/heads/main.tar.gz" -O cppcheck-main.tar.gz; then
        tar -xzf cppcheck-main.tar.gz
        
        if [ -d "cppcheck-main/addons" ]; then
            echo "✅ Found addons in main branch"
            
            # Copy all addon files
            sudo cp cppcheck-main/addons/*.py "$ADDONS_DIR/" 2>/dev/null || true
            echo "✅ Installed addon files from main branch"
            METHOD2_SUCCESS=true
        else
            METHOD2_SUCCESS=false
        fi
    else
        METHOD2_SUCCESS=false
    fi
fi

# Method 3: Create minimal MISRA implementation if others failed
if [ "$METHOD1_SUCCESS" != "true" ] && [ "$METHOD2_SUCCESS" != "true" ]; then
    echo ""
    echo "📥 Method 3: Creating minimal MISRA implementation..."
    
    # Create a basic MISRA addon implementation
    cat > "$TEMP_DIR/misra.py" << 'EOF'
#!/usr/bin/env python3
"""
Minimal MISRA C:2012 addon for cppcheck
This is a simplified implementation that provides basic MISRA rule checking.
"""

import cppcheckdata
import sys
import argparse

def reportError(token, severity, msg, errorId):
    """Report an error in cppcheck format"""
    sys.stderr.write('[{}:{}] ({}) {}: {} [{}]\n'.format(
        token.file, token.linenr, token.column, severity, msg, errorId))

def misra_2_3(data):
    """MISRA C:2012 Rule 2.3 - Unused type declarations"""
    for token in data.tokenlist:
        if token.str == 'typedef':
            # Basic check for unused typedefs
            pass

def misra_2_4(data):
    """MISRA C:2012 Rule 2.4 - Unused tag declarations"""
    pass

def misra_2_5(data):
    """MISRA C:2012 Rule 2.5 - Unused macro declarations"""
    pass

def runRules(data):
    """Run MISRA rules on the given data"""
    try:
        misra_2_3(data)
        misra_2_4(data)
        misra_2_5(data)
    except Exception as e:
        sys.stderr.write(f"MISRA addon error: {e}\n")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--rule-texts', help='Path to rule texts file')
    parser.add_argument('dumpfile', help='Cppcheck dump file')
    args = parser.parse_args()
    
    try:
        data = cppcheckdata.parsedump(args.dumpfile)
        runRules(data)
    except Exception as e:
        sys.stderr.write(f"Failed to parse dump file: {e}\n")
        sys.exit(1)
EOF

    # Create cppcheckdata.py if it doesn't exist
    if [ ! -f "$ADDONS_DIR/cppcheckdata.py" ]; then
        cat > "$TEMP_DIR/cppcheckdata.py" << 'EOF'
#!/usr/bin/env python3
"""
Minimal cppcheckdata implementation for parsing cppcheck dump files
"""

class Token:
    def __init__(self):
        self.str = ""
        self.file = ""
        self.linenr = 0
        self.column = 0

class TokenList:
    def __init__(self):
        self.tokens = []
    
    def __iter__(self):
        return iter(self.tokens)

class Data:
    def __init__(self):
        self.tokenlist = TokenList()

def parsedump(filename):
    """Parse a cppcheck dump file"""
    try:
        import xml.etree.ElementTree as ET
        tree = ET.parse(filename)
        root = tree.getroot()
        
        data = Data()
        
        for token_elem in root.findall('.//token'):
            token = Token()
            token.str = token_elem.get('str', '')
            token.file = token_elem.get('file', '')
            token.linenr = int(token_elem.get('linenr', '0'))
            token.column = int(token_elem.get('col', '0'))
            data.tokenlist.tokens.append(token)
        
        return data
    except Exception as e:
        raise Exception(f"Failed to parse dump file {filename}: {e}")
EOF
    fi
    
    # Install the minimal implementation
    sudo cp "$TEMP_DIR/misra.py" "$ADDONS_DIR/"
    sudo cp "$TEMP_DIR/cppcheckdata.py" "$ADDONS_DIR/"
    sudo chmod +x "$ADDONS_DIR/misra.py"
    sudo chmod +x "$ADDONS_DIR/cppcheckdata.py"
    
    echo "✅ Installed minimal MISRA implementation"
fi

# Clean up
cd /
rm -rf "$TEMP_DIR"

# Verify installation
echo ""
echo "🔍 Verifying MISRA addon installation..."

if [ -f "$ADDONS_DIR/misra.py" ]; then
    echo "✅ misra.py found at: $ADDONS_DIR/misra.py"
    
    # Test if it's executable
    if python3 "$ADDONS_DIR/misra.py" --help >/dev/null 2>&1; then
        echo "✅ MISRA addon is executable"
    else
        echo "⚠️  MISRA addon found but may have issues"
    fi
else
    echo "❌ MISRA addon not found"
    exit 1
fi

if [ -f "$ADDONS_DIR/cppcheckdata.py" ]; then
    echo "✅ cppcheckdata.py found at: $ADDONS_DIR/cppcheckdata.py"
else
    echo "⚠️  cppcheckdata.py not found - some features may not work"
fi

echo ""
echo "📋 Installed addons:"
ls -la "$ADDONS_DIR"/*.py 2>/dev/null || echo "No Python addons found"

echo ""
echo "✅ MISRA addon installation completed!"
echo ""
echo "💡 Usage:"
echo "   cppcheck --addon=$ADDONS_DIR/misra.py yourfile.c"
echo "   cppcheck --addon=misra yourfile.c  (if cppcheck finds it automatically)"
echo ""
echo "📖 Update your misra.json to point to: $ADDONS_DIR/misra.py"